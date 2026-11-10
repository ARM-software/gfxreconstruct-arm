/*
** Copyright (c) 2026 LunarG, Inc.
** Copyright (c) 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and associated documentation files (the "Software"),
** to deal in the Software without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Software, and to permit persons to whom the
** Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
*/
#include "vulkan_memory_diff_modifier.h"

#include "format/format.h"
#include <cstdint>
#include <vector>
#include "format/api_call_id.h"
#include "util/defines.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

#define PAGE_SIZE 4096

std::vector<VulkanMemoryDiffModifier::ByteDiff>
VulkanMemoryDiffModifier::diff_the_vectors(const uint8_t* new_data, const uint8_t* old_data, uint64_t length)
{
    std::vector<ByteDiff> diffs;

    for (uint64_t i = 0; i < length; i += options_.memory_diff_granularity)
    {
        // The final chunk may be smaller than the requested granularity.
        const uint64_t chunk_size = std::min<uint64_t>(options_.memory_diff_granularity, length - i);

        // A changed chunk is replaced in full, even if only some of its bytes differ.
        if (memcmp(new_data + i, old_data + i, chunk_size) != 0)
        {
            // Extend the previous diff when changed chunks are adjacent so that they're contained in the same resulting
            // FillMemoryCommand later on.
            if (!diffs.empty() && (diffs.back().offset + diffs.back().data.size()) == i)
            {
                diffs.back().data.insert(diffs.back().data.end(), new_data + i, new_data + i + chunk_size);
            }
            else
            {
                ByteDiff diff;
                diff.offset = i;
                diff.data.insert(diff.data.end(), new_data + i, new_data + i + chunk_size);

                diffs.push_back(std::move(diff));
            }
        }
    }

    return diffs;
}

void VulkanMemoryDiffModifier::WriteFillMemoryCmd(uint64_t       memory_id,
                                                  uint64_t       fill_offset,
                                                  uint64_t       fill_size,
                                                  const uint8_t* fill_data)
{
    format::FillMemoryCommandHeader header;

    header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
    header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(header) + fill_size;
    header.meta_header.meta_data_id =
        format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kFillMemoryCommand);
    header.thread_id = 1;

    header.memory_id     = memory_id;
    header.memory_offset = fill_offset;
    header.memory_size   = fill_size;

    auto new_call = CreatePreCall();

    new_call->type = util::CallModifierBase::NewCallDataType::MetaDataCall;
    new_call->parameter_buffer.Write(&header, sizeof(header));
    new_call->parameter_buffer.Write(fill_data, fill_size);
}

void VulkanMemoryDiffModifier::ProcessFillMemoryCommand(uint64_t       memory_id,
                                                        uint64_t       offset,
                                                        uint64_t       size,
                                                        const uint8_t* data)
{
    if (!IsModificationPass())
    {
        return;
    }

    if ((offset % PAGE_SIZE) != 0 || (size % PAGE_SIZE) != 0)
    {
        return;
    }

    uint64_t              number_of_pages = size / PAGE_SIZE;
    auto&                 memory_pages    = memory_map[memory_id];
    std::vector<uint64_t> new_page_indices;
    std::vector<ByteDiff> replacement_diffs;

    for (uint64_t i = 0; i < number_of_pages; i++)
    {
        const uint64_t page_offset = offset + PAGE_SIZE * i;
        auto           page_entry  = memory_pages.find(page_offset);

        if (page_entry == memory_pages.end())
        {
            new_page_indices.push_back(i);
            continue;
        }

        if (memcmp(data + PAGE_SIZE * i, page_entry->second.data(), PAGE_SIZE) != 0)
        {
            auto diff_struct = diff_the_vectors(data + PAGE_SIZE * i, page_entry->second.data(), PAGE_SIZE);
            for (auto& diff : diff_struct)
            {
                diff.offset += page_offset;
                if (!replacement_diffs.empty() &&
                    (replacement_diffs.back().offset + replacement_diffs.back().data.size()) == diff.offset)
                {
                    replacement_diffs.back().data.insert(
                        replacement_diffs.back().data.end(), diff.data.begin(), diff.data.end());
                }
                else
                {
                    replacement_diffs.push_back(std::move(diff));
                }
            }
        }
    }

    if (!replacement_diffs.empty())
    {
        SetDeleteCurrentCall();

        for (const auto& diff : replacement_diffs)
        {
            WriteFillMemoryCmd(memory_id, diff.offset, diff.data.size(), diff.data.data());
        }

        for (uint64_t page_index : new_page_indices)
        {
            WriteFillMemoryCmd(memory_id, offset + PAGE_SIZE * page_index, PAGE_SIZE, data + PAGE_SIZE * page_index);
        }
    }

    for (uint64_t i = 0; i < number_of_pages; i++)
    {
        const uint64_t page_offset  = offset + PAGE_SIZE * i;
        auto [page_entry, inserted] = memory_pages.emplace(page_offset, std::vector<uint8_t>(PAGE_SIZE));
        memcpy(page_entry->second.data(), data + PAGE_SIZE * i, PAGE_SIZE);
    }
}

bool VulkanMemoryDiffModifier::CanOptimize()
{
    return options_.memory_diff;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
