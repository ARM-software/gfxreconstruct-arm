/*
** Copyright (c) 2025 LunarG, Inc.
** Copyright (c) 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "vulkan_descriptor_buffer_modifier.h"

#include <cstdint>
#include <unordered_map>
#include <algorithm>

#include "format/format.h"
#include "format/format_arm.h"
#include "decode/vulkan_optimize_options.h"
#include "util/defines.h"
#include "util/logging.h"
#include "util/memory_output_stream.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanDescriptorBufferModifier::VulkanDescriptorBufferModifier(const VulkanOptimizationOptions& options) :
    options_(options){};

bool VulkanDescriptorBufferModifier::CanOptimize()
{
    return true;
}

VulkanDescriptorBufferModifier::DescriptorTrie::DescriptorTrie()
{
    nodes_.emplace_back();
}

uint32_t VulkanDescriptorBufferModifier::DescriptorTrie::AddNode()
{
    const uint32_t index = static_cast<uint32_t>(nodes_.size());
    nodes_.emplace_back();
    return index;
}

size_t VulkanDescriptorBufferModifier::DescriptorTrie::CountCommonBytes(const uint8_t* p1,
                                                                        const uint8_t* p2,
                                                                        size_t         size) const
{
    size_t i = 0;
    while (i < size && p1[i] == p2[i])
    {
        ++i;
    }
    return i;
}

void VulkanDescriptorBufferModifier::DescriptorTrie::Insert(const uint8_t*        data,
                                                            uint64_t              data_size,
                                                            const DescriptorInfo& version)
{
    if (data_size == 0)
    {
        return;
    }

    // Start the search with the root
    uint32_t node_idx    = 0;
    size_t   data_offset = 0;
    while (data_offset < data_size)
    {
        auto& children = nodes_[node_idx].children;
        auto  child =
            std::lower_bound(children.begin(), children.end(), data[data_offset], [](const Edge& edge, uint8_t byte) {
                return edge.label.front() < byte;
            });
        const auto child_offset = static_cast<size_t>(child - children.begin());

        if (child == children.end() || child->label.front() != data[data_offset])
        {
            // Maintain the root lookup mask
            if (node_idx == 0)
            {
                mask_[data[data_offset]] = true;
            }
            // not found, insert
            uint32_t new_child = AddNode();
            nodes_[new_child].descriptors.emplace_back(version);

            nodes_[node_idx].children.insert(
                nodes_[node_idx].children.begin() + child_offset,
                Edge(std::vector<uint8_t>(data + data_offset, data + data_size), new_child));
            return;
        }
        else
        {
            const size_t common_bytes = CountCommonBytes(
                data + data_offset, child->label.data(), std::min(data_size - data_offset, child->label.size()));

            // If it was a full match
            if (common_bytes == nodes_[node_idx].children[child_offset].label.size())
            {
                // Continue matching from the child
                data_offset += common_bytes;
                node_idx = nodes_[node_idx].children[child_offset].child;
                if (data_offset == data_size)
                {
                    nodes_[node_idx].descriptors.emplace_back(version);
                    return;
                }
                continue;
            }

            // It is not a full match, we need to split the node
            // One of the nodes spawned from the split should have this label
            std::vector<uint8_t> leftover_label(nodes_[node_idx].children[child_offset].label.begin() + common_bytes,
                                                nodes_[node_idx].children[child_offset].label.end());

            // Leftover suffix
            uint32_t split_node_idx = AddNode();
            nodes_[split_node_idx].children.emplace_back(
                Edge{ std::move(leftover_label), nodes_[node_idx].children[child_offset].child });

            // Current node gets its label reduced to common prefix

            nodes_[node_idx].children[child_offset].label.resize(common_bytes);
            nodes_[node_idx].children[child_offset].child = split_node_idx;

            data_offset += common_bytes;
            if (data_offset == data_size)
            {
                nodes_[split_node_idx].descriptors.emplace_back(version);
                return;
            }

            // The remaining data is added to the new node.
            uint32_t new_node = AddNode();
            nodes_[new_node].descriptors.emplace_back(version);
            Edge  new_edge(std::vector<uint8_t>(data + data_offset, data + data_size), new_node);
            auto& split_children = nodes_[split_node_idx].children;
            auto  insert_at      = std::lower_bound(split_children.begin(),
                                              split_children.end(),
                                              new_edge.label.front(),
                                              [](const Edge& edge, uint8_t byte) { return edge.label.front() < byte; });
            split_children.insert(insert_at, std::move(new_edge));
            return;
        }
    }
}

void VulkanDescriptorBufferModifier::DescriptorTrie::Match(const uint8_t*                      data,
                                                           uint64_t                            data_size,
                                                           std::vector<const DescriptorInfo*>& candidates)
{
    candidates.clear();

    if (data_size == 0)
    {
        return;
    }

    if (!mask_[data[0]])
    {
        return;
    }

    uint32_t node_idx    = 0;
    size_t   data_offset = 0;

    match_scratch_.clear();

    while (data_offset < data_size)
    {
        const auto& children = nodes_[node_idx].children;
        auto        child =
            std::lower_bound(children.begin(), children.end(), data[data_offset], [](const Edge& edge, uint8_t byte) {
                return edge.label.front() < byte;
            });
        if (child == children.end() || child->label.front() != data[data_offset] ||
            child->label.size() > data_size - data_offset ||
            util::platform::MemoryCompare(data + data_offset, child->label.data(), child->label.size()) != 0)
        {
            break;
        }

        data_offset += child->label.size();
        node_idx = child->child;

        match_scratch_.push_back(node_idx);
    }

    for (auto node = match_scratch_.rbegin(); node != match_scratch_.rend(); ++node)
    {
        const auto& descriptors = nodes_[*node].descriptors;

        for (auto descriptor = descriptors.rbegin(); descriptor != descriptors.rend(); ++descriptor)
        {
            candidates.push_back(&(*descriptor));
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const DescriptorInfo* lhs, const DescriptorInfo* rhs) {
        return lhs->parent_call_index > rhs->parent_call_index;
    });
}

std::vector<format::DescriptorDataLocationInfo> VulkanDescriptorBufferModifier::GetDescriptorsInFillMemory(
    uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data)
{
    std::vector<format::DescriptorDataLocationInfo> locations;

    auto entry = device_memory_descriptor_locations.find(memory_id);
    if (entry != device_memory_descriptor_locations.end())
    {
        uint64_t end_offset = offset + size;

        for (const auto& loc_map : entry->second)
        {
            auto seen = descriptor_versions_seen_.find(loc_map.first);
            if (seen == descriptor_versions_seen_.end())
            {
                continue;
            }

            for (auto location = loc_map.second.rbegin(); location != loc_map.second.rend(); ++location)
            {
                auto loc_info = *location;
                if (loc_info.descriptor_generation > seen->second)
                {
                    continue;
                }

                const auto& descriptor = GetDescriptorPayload(loc_info);
                if (loc_info.descriptor_offset_in_mapped_memory >= offset &&
                    (loc_info.descriptor_offset_in_mapped_memory + loc_info.orig_size) <= end_offset)
                {
                    uint64_t    descriptor_offset_in_memory = loc_info.descriptor_offset_in_mapped_memory - offset;
                    const void* dest                        = data + descriptor_offset_in_memory;
                    // found out the loc_info to be a descriptor in this filled-Memory range
                    if (util::platform::MemoryCompare(descriptor.data(), dest, descriptor.size()) == 0)
                    {
                        loc_info.descriptor_offset_in_memory = descriptor_offset_in_memory;
                        locations.emplace_back(loc_info);
                        GFXRECON_LOG_DEBUG("Found descriptor in Filled Memory(%" PRIu64
                                           ", pageoffset 0x%lx) at relative offset 0x%lx.",
                                           memory_id,
                                           offset,
                                           descriptor_offset_in_memory);
                        break;
                    }
                }
            }
        }
    }

    FindCopiedDescriptors(memory_id, offset, size, data, &locations);
    return locations;
}

void VulkanDescriptorBufferModifier::RecordDescriptor(uint64_t       call_index,
                                                      uint64_t       descriptor_addr,
                                                      size_t         data_size,
                                                      const uint8_t* data)
{
    auto&    history = descriptor_histories_[descriptor_addr];
    uint64_t version = history.size() + 1;
    history.emplace_back(data_size);
    util::platform::MemoryCopy(history.back().data(), data_size, data, data_size);

    descriptor_trie_.Insert(data, data_size, DescriptorInfo{ descriptor_addr, version, data_size, call_index });
}

const std::vector<uint8_t>&
VulkanDescriptorBufferModifier::GetDescriptorPayload(const format::DescriptorDataLocationInfo& location) const
{
    GFXRECON_ASSERT(location.descriptor_generation > 0);
    const auto& history = descriptor_histories_.at(location.descriptor_addr);
    GFXRECON_ASSERT(location.descriptor_generation <= history.size());
    const auto& descriptor = history.at(location.descriptor_generation - 1);
    GFXRECON_ASSERT(descriptor.size() == location.orig_size);
    return descriptor;
}

void VulkanDescriptorBufferModifier::FindCopiedDescriptors(uint64_t                                         memory_id,
                                                           uint64_t                                         offset,
                                                           uint64_t                                         size,
                                                           const uint8_t*                                   data,
                                                           std::vector<format::DescriptorDataLocationInfo>* locations)
{
    auto memory = memory_binding_entries_.find(memory_id);
    if (memory == memory_binding_entries_.end() || size == 0)
    {
        return;
    }

    struct Range
    {
        uint64_t begin;
        uint64_t end;
    };

    std::vector<Range> occupied_ranges;
    for (const auto& location : *locations)
    {
        const auto& payload = GetDescriptorPayload(location);
        occupied_ranges.push_back(
            { location.descriptor_offset_in_memory, location.descriptor_offset_in_memory + payload.size() });
    }

    std::sort(occupied_ranges.begin(), occupied_ranges.end(), [](const Range& lhs, const Range& rhs) {
        return lhs.begin < rhs.begin || (lhs.begin == rhs.begin && lhs.end < rhs.end);
    });

    std::vector<Range> merged_occupied_ranges;
    merged_occupied_ranges.reserve(occupied_ranges.size());
    for (const auto& range : occupied_ranges)
    {
        if (merged_occupied_ranges.empty() || range.begin > merged_occupied_ranges.back().end)
        {
            merged_occupied_ranges.emplace_back(range);
        }
        else
        {
            merged_occupied_ranges.back().end = std::max(merged_occupied_ranges.back().end, range.end);
        }
    }

    struct BoundBufferRange
    {
        uint64_t          begin;
        uint64_t          end;
        const BufferInfo* buffer;
    };

    const uint64_t fill_begin = memory->second.mapping.offset + offset;
    const uint64_t fill_end   = fill_begin + size;
    GFXRECON_ASSERT(fill_begin >= memory->second.mapping.offset && fill_end >= fill_begin);

    std::vector<BoundBufferRange> bound_buffers;
    std::vector<Range>            scan_ranges;
    bound_buffers.reserve(memory->second.memory_binding_records_.size());
    scan_ranges.reserve(memory->second.memory_binding_records_.size());

    for (const auto& binding : memory->second.memory_binding_records_)
    {
        auto buffer = buffer_entries_.find(binding.handle);
        if (!binding.isBuffer || buffer == buffer_entries_.end() || buffer->second.creation_index >= block_index_ ||
            buffer->second.destruction_index <= block_index_)
        {
            continue;
        }

        const uint64_t buffer_begin = binding.offset;
        const uint64_t buffer_end   = buffer_begin + buffer->second.size;
        GFXRECON_ASSERT(buffer_end >= buffer_begin);

        const uint64_t search_begin = std::max(fill_begin, buffer_begin);
        const uint64_t search_end   = std::min(fill_end, buffer_end);
        if (search_begin < search_end)
        {
            bound_buffers.emplace_back(BoundBufferRange{ buffer_begin, buffer_end, &buffer->second });
            scan_ranges.emplace_back(Range{ search_begin, search_end });
        }
    }

    std::sort(scan_ranges.begin(), scan_ranges.end(), [](const Range& lhs, const Range& rhs) {
        return lhs.begin < rhs.begin || (lhs.begin == rhs.begin && lhs.end < rhs.end);
    });

    std::vector<Range> merged_scan_ranges;
    merged_scan_ranges.reserve(scan_ranges.size());
    for (const auto& range : scan_ranges)
    {
        if (merged_scan_ranges.empty() || range.begin > merged_scan_ranges.back().end)
        {
            merged_scan_ranges.emplace_back(range);
        }
        else
        {
            merged_scan_ranges.back().end = std::max(merged_scan_ranges.back().end, range.end);
        }
    }

    size_t                             occupied_range_idx = 0;
    std::vector<const DescriptorInfo*> candidates;
    for (const auto& scan_range : merged_scan_ranges)
    {
        uint64_t memory_offset = scan_range.begin;
        while (memory_offset < scan_range.end)
        {
            const uint64_t data_offset = memory_offset - fill_begin;

            while (occupied_range_idx < merged_occupied_ranges.size() &&
                   merged_occupied_ranges[occupied_range_idx].end <= data_offset)
            {
                ++occupied_range_idx;
            }

            if (occupied_range_idx < merged_occupied_ranges.size() &&
                merged_occupied_ranges[occupied_range_idx].begin <= data_offset)
            {
                const uint64_t occupied_end = fill_begin + merged_occupied_ranges[occupied_range_idx].end;
                GFXRECON_ASSERT(occupied_end >= fill_begin);
                memory_offset = std::min(scan_range.end, occupied_end);
                continue;
            }

            descriptor_trie_.Match(data + data_offset, scan_range.end - memory_offset, candidates);
            if (candidates.empty())
            {
                ++memory_offset;
                continue;
            }

            bool found_descriptor = false;
            for (const auto& candidate : candidates)
            {
                auto seen = descriptor_versions_seen_.find(candidate->descriptor_addr);
                if (seen == descriptor_versions_seen_.end() || candidate->version > seen->second)
                {
                    continue;
                }

                if (candidate->size > scan_range.end - memory_offset)
                {
                    continue;
                }

                const uint64_t descriptor_end = data_offset + candidate->size;
                GFXRECON_ASSERT(descriptor_end >= data_offset);
                if (occupied_range_idx < merged_occupied_ranges.size() &&
                    merged_occupied_ranges[occupied_range_idx].begin < descriptor_end)
                {
                    continue;
                }

                auto bound_buffer = std::find_if(
                    bound_buffers.begin(), bound_buffers.end(), [memory_offset, &candidate](const auto& range) {
                        return memory_offset >= range.begin && memory_offset < range.end &&
                               candidate->size <= range.end - memory_offset;
                    });
                if (bound_buffer == bound_buffers.end())
                {
                    continue;
                }

                format::DescriptorDataLocationInfo location{};
                location.descriptor_offset_in_mapped_memory = memory_offset - memory->second.mapping.offset;
                location.descriptor_offset_in_buffer        = memory_offset - bound_buffer->begin;
                location.descriptor_offset_in_memory        = data_offset;
                location.descriptor_addr                    = candidate->descriptor_addr;
                location.orig_size                          = candidate->size;
                location.descriptor_generation              = candidate->version;

                locations->emplace_back(location);
                GFXRECON_LOG_DEBUG("Found descriptor generation %" PRIu64 " from 0x%lx in Filled Memory(%" PRIu64
                                   ") at relative offset 0x%lx.",
                                   candidate->version,
                                   candidate->descriptor_addr,
                                   memory_id,
                                   data_offset);

                memory_offset += candidate->size;
                found_descriptor = true;
                break;
            }

            if (!found_descriptor)
            {
                ++memory_offset;
            }
        }
    }
}

void VulkanDescriptorBufferModifier::WriteFixDescriptorDataCmd(format::HandleId                    memory_id,
                                                               uint64_t                            num_of_locations,
                                                               format::DescriptorDataLocationInfo* desc_locations)
{
    auto new_call       = CreatePreCall();
    new_call->type      = NewCallDataType::MetaDataCall;
    new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
    new_call->thread_id = 1;

    format::FixDescriptorDataCommandHeader fix_cmd_header;

    fix_cmd_header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
    fix_cmd_header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(fix_cmd_header) +
                                                   num_of_locations * sizeof(format::DescriptorDataLocationInfo);
    fix_cmd_header.meta_header.meta_data_id = format::MakeMetaDataId(
        format::ApiFamilyId::ApiFamily_Vulkan, format::arm::MetaDataType::kFixDescriptorDataCommand);
    fix_cmd_header.memory_id        = memory_id;
    fix_cmd_header.num_of_locations = num_of_locations;

    GFXRECON_LOG_DEBUG("This capture has been optimized for descriptor buffer with %" PRIu64 " locations.",
                       num_of_locations);
    for (uint64_t i = 0; i < num_of_locations; i++)
    {
        GFXRECON_LOG_DEBUG("    offset in mapped memory %" PRIu64 ".",
                           desc_locations[i].descriptor_offset_in_mapped_memory);
        GFXRECON_LOG_DEBUG("    offset in buffer %" PRIu64 ".", desc_locations[i].descriptor_offset_in_buffer);
        GFXRECON_LOG_DEBUG("    offset in filled memory %" PRIu64 ".", desc_locations[i].descriptor_offset_in_memory);
        GFXRECON_LOG_DEBUG("    descriptor addr 0x%lx.", desc_locations[i].descriptor_addr);
        GFXRECON_LOG_DEBUG("    orig size %" PRIu64 ".", desc_locations[i].orig_size);
    }

    new_call->parameter_buffer.Write(&fix_cmd_header, sizeof(format::FixDescriptorDataCommandHeader));
    new_call->parameter_buffer.Write(desc_locations, num_of_locations * sizeof(format::DescriptorDataLocationInfo));
}

void VulkanDescriptorBufferModifier::ProcessFillMemoryCommand(uint64_t       memory_id,
                                                              uint64_t       offset,
                                                              uint64_t       size,
                                                              const uint8_t* data)
{
    if (!IsModificationPass())
    {
        return;
    }

    // Find descriptor data inside data parameter, and generate FixDescriptorDataCommand meta block
    std::vector<format::DescriptorDataLocationInfo> descriptor_locations =
        GetDescriptorsInFillMemory(memory_id, offset, size, data);
    if (descriptor_locations.size())
    {
        WriteFixDescriptorDataCmd(memory_id, descriptor_locations.size(), descriptor_locations.data());
    }
}

void VulkanDescriptorBufferModifier::ProcessFixDescriptorDataCommand(
    const format::FixDescriptorDataCommandHeader& header, const std::vector<format::DescriptorDataLocationInfo>& infos)
{
    if (IsModificationPass())
    {
        // delete the old fixed meta command, it will be inserted new fixed meta command
        SetDeleteCurrentCall();
        return;
    }
}

void VulkanDescriptorBufferModifier::ProcessFixShadowMemoryCommand(format::HandleId memory_id,
                                                                   uint64_t         map_memory,
                                                                   uint64_t         shadow_memory)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(memory_id) != memory_binding_entries_.end())
    {
        assert(map_memory == memory_binding_entries_[memory_id].mapping.map_memory);
        memory_binding_entries_[memory_id].mapping.shadow_memory = shadow_memory;
    }
}

void VulkanDescriptorBufferModifier::Process_vkCreateBuffer(const ApiCallInfo& call_info, args::CreateBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle                   = *args.pBuffer.GetPointer();
    buffer_entries_[handle].handle            = handle;
    buffer_entries_[handle].size              = args.pCreateInfo.GetPointer()->size;
    buffer_entries_[handle].usage             = args.pCreateInfo.GetPointer()->usage;
    buffer_entries_[handle].flags             = args.pCreateInfo.GetPointer()->flags;
    buffer_entries_[handle].creation_index    = call_info.index;
    buffer_entries_[handle].destruction_index = UINT64_MAX;
}

void VulkanDescriptorBufferModifier::Process_vkDestroyBuffer(const ApiCallInfo& call_info, args::DestroyBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (buffer_entries_.find(args.buffer) != buffer_entries_.end())
    {
        buffer_entries_[args.buffer].destruction_index = call_info.index;
    }
}

void VulkanDescriptorBufferModifier::Process_vkAllocateMemory(const ApiCallInfo& call_info, args::AllocateMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle                           = *args.pMemory.GetPointer();
    memory_binding_entries_[handle].handle            = handle;
    memory_binding_entries_[handle].size              = args.pAllocateInfo.GetPointer()->allocationSize;
    memory_binding_entries_[handle].type_index        = args.pAllocateInfo.GetPointer()->memoryTypeIndex;
    memory_binding_entries_[handle].creation_index    = call_info.index;
    memory_binding_entries_[handle].destruction_index = UINT64_MAX;
    if (args.pAllocateInfo.GetPointer()->pNext)
    {
        VkMemoryAllocateFlagsInfo* info = (VkMemoryAllocateFlagsInfo*)(args.pAllocateInfo.GetPointer()->pNext);
        if (info->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO)
        {
            memory_binding_entries_[handle].flags = info->flags;
        }
    }
}

void VulkanDescriptorBufferModifier::Process_vkFreeMemory(const ApiCallInfo& call_info, args::FreeMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[args.memory].destruction_index = call_info.index;
    }
}

void VulkanDescriptorBufferModifier::Process_vkMapMemory(const ApiCallInfo& call_info, args::MapMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        if (args.size == VK_WHOLE_SIZE)
        {
            assert(args.offset <= memory_binding_entries_[args.memory].size);
            args.size = memory_binding_entries_[args.memory].size - args.offset;
        }

        memory_binding_entries_[args.memory].mapping.offset        = args.offset;
        memory_binding_entries_[args.memory].mapping.size          = args.size;
        memory_binding_entries_[args.memory].mapping.map_memory    = *args.ppData.GetPointer();
        memory_binding_entries_[args.memory].mapping.shadow_memory = 0;
    }
}

void VulkanDescriptorBufferModifier::Process_vkMapMemory2(const ApiCallInfo& call_info, args::MapMemory2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    VkMemoryMapInfo*         map_info      = args.pMemoryMapInfo.GetPointer();
    Decoded_VkMemoryMapInfo* map_meta_info = args.pMemoryMapInfo.GetMetaStructPointer();

    format::HandleId memory = map_meta_info->memory;
    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        VkDeviceSize in_size = map_info->size;
        if (map_info->size == VK_WHOLE_SIZE)
        {
            assert(map_info->offset <= memory_binding_entries_[memory].size);
            in_size = memory_binding_entries_[memory].size - map_info->offset;
        }

        memory_binding_entries_[memory].mapping.offset        = map_info->offset;
        memory_binding_entries_[memory].mapping.size          = in_size;
        memory_binding_entries_[memory].mapping.map_memory    = *args.ppData.GetPointer();
        memory_binding_entries_[memory].mapping.shadow_memory = 0;
    }
}

void VulkanDescriptorBufferModifier::Process_vkUnmapMemory(const ApiCallInfo& call_info, args::UnmapMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[args.memory].mapping.offset        = 0;
        memory_binding_entries_[args.memory].mapping.size          = 0;
        memory_binding_entries_[args.memory].mapping.map_memory    = 0;
        memory_binding_entries_[args.memory].mapping.shadow_memory = 0;
    }
}

void VulkanDescriptorBufferModifier::Process_vkUnmapMemory2(const ApiCallInfo& call_info, args::UnmapMemory2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    Decoded_VkMemoryUnmapInfo* unmap_meta_info = args.pMemoryUnmapInfo.GetMetaStructPointer();

    format::HandleId memory = unmap_meta_info->memory;
    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[memory].mapping.offset        = 0;
        memory_binding_entries_[memory].mapping.size          = 0;
        memory_binding_entries_[memory].mapping.map_memory    = 0;
        memory_binding_entries_[memory].mapping.shadow_memory = 0;
    }
}

void VulkanDescriptorBufferModifier::Process_vkBindBufferMemory(const ApiCallInfo&      call_info,
                                                                args::BindBufferMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[args.memory].memory_binding_records_.push_back(
            { args.buffer, true, args.memoryOffset });
    }
}

void VulkanDescriptorBufferModifier::Process_vkBindBufferMemory2(const ApiCallInfo&       call_info,
                                                                 args::BindBufferMemory2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkBindBufferMemoryInfo*         bind_infos      = args.pBindInfos.GetPointer();
    const Decoded_VkBindBufferMemoryInfo* bind_meta_infos = args.pBindInfos.GetMetaStructPointer();

    for (uint32_t i = 0; i < args.bindInfoCount; ++i)
    {
        if (memory_binding_entries_.find(bind_meta_infos[i].memory) != memory_binding_entries_.end())
        {
            memory_binding_entries_[bind_meta_infos[i].memory].memory_binding_records_.push_back(
                { bind_meta_infos[i].buffer, true, bind_infos[i].memoryOffset });
        }
    }
}

void VulkanDescriptorBufferModifier::Process_vkAllocateCommandBuffers(const ApiCallInfo&            call_info,
                                                                      args::AllocateCommandBuffers& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkCommandBufferAllocateInfo* allocate_info = args.pAllocateInfo.GetPointer();
    for (uint32_t i = 0; i < allocate_info->commandBufferCount; i++)
    {
        format::HandleId handle = args.pCommandBuffers.GetPointer()[i];
        command_buffer_entries_.try_emplace(handle, handle, args.device, allocate_info->level, 0, call_info.index);
    }
}

void VulkanDescriptorBufferModifier::Process_vkBeginCommandBuffer(const ApiCallInfo&        call_info,
                                                                  args::BeginCommandBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkCommandBufferBeginInfo* begin_info            = args.pBeginInfo.GetPointer();
    command_buffer_entries_.at(args.commandBuffer).usage_ = begin_info->flags;
}

void VulkanDescriptorBufferModifier::Process_vkFreeCommandBuffers(const ApiCallInfo&        call_info,
                                                                  args::FreeCommandBuffers& args)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t i = 0; i < args.commandBufferCount; i++)
    {
        format::HandleId handle = args.pCommandBuffers.GetPointer()[i];
        if (handle == format::kNullHandleId)
        {
            GFXRECON_LOG_WARNING("Skipping vkFreeCommandBuffers for null command buffer handle at "
                                 "call index %" PRIu64 ".",
                                 call_info.index);
            continue;
        }

        auto entry = command_buffer_entries_.find(handle);
        if (entry == command_buffer_entries_.end())
        {
            GFXRECON_LOG_WARNING("Skipping vkFreeCommandBuffers for untracked command buffer handle %" PRIu64
                                 " at call index %" PRIu64 ".",
                                 handle,
                                 call_info.index);
            continue;
        }

        entry->second.destruction_index_ = call_info.index;
    }
}

void VulkanDescriptorBufferModifier::Process_vkCmdCopyBuffer(const ApiCallInfo& call_info, args::CmdCopyBuffer& args) {}

void VulkanDescriptorBufferModifier::Process_vkCmdCopyBuffer2(const ApiCallInfo& call_info, args::CmdCopyBuffer2& args)
{}

void VulkanDescriptorBufferModifier::Process_vkCmdCopyBuffer2KHR(const ApiCallInfo&       call_info,
                                                                 args::CmdCopyBuffer2KHR& args)
{}

void VulkanDescriptorBufferModifier::Process_vkCmdUpdateBuffer(const ApiCallInfo&     call_info,
                                                               args::CmdUpdateBuffer& args)
{
    if (!IsModificationPass())
    {
        return;
    }

    const CommandBufferInfo& command_buffer_info = command_buffer_entries_.at(args.commandBuffer);

    format::HandleId device_id = command_buffer_info.device_id_;
    auto             data      = args.pData.GetPointer();
    // reserved for descriptor buffer to WriteFixDescriptorDataCmd
}

void VulkanDescriptorBufferModifier::Process_vkCmdPushConstants(const ApiCallInfo&      call_info,
                                                                args::CmdPushConstants& args)
{
    if (!IsModificationPass())
    {
        return;
    }
    const CommandBufferInfo& command_buffer_info = command_buffer_entries_.at(args.commandBuffer);

    format::HandleId device_id = command_buffer_info.device_id_;
    auto             data      = args.pValues.GetPointer();
    // reserved for descriptor buffer to WriteFixDescriptorDataCmd
}

void VulkanDescriptorBufferModifier::Process_vkGetDescriptorEXT(const ApiCallInfo&      call_info,
                                                                args::GetDescriptorEXT& args)
{
    uint64_t desc_addr = args.pDescriptor.GetAddress();
    if (IsModificationPass())
    {
        uint64_t version = ++descriptor_versions_seen_[desc_addr];
        GFXRECON_ASSERT(version <= descriptor_histories_.at(desc_addr).size());
        return;
    }

    RecordDescriptor(call_info.index, desc_addr, args.dataSize, args.pDescriptor.GetPointer());
    bool found = false;

    format::DescriptorDataLocationInfo location{};
    location.orig_size             = args.dataSize;
    location.descriptor_generation = descriptor_histories_.at(desc_addr).size();
    location.descriptor_addr       = desc_addr;

    for (auto& entry : memory_binding_entries_)
    {
        format::HandleId mem_id  = entry.first;
        DeviceMemoryInfo mem_obj = entry.second;

        uint64_t mem_base_addr =
            (mem_obj.mapping.shadow_memory == 0) ? mem_obj.mapping.map_memory : mem_obj.mapping.shadow_memory;
        uint64_t mem_end_addr = mem_base_addr + mem_obj.mapping.size;

        // pointer address of descriptor in this memory mapped range
        if (desc_addr >= mem_base_addr && desc_addr < mem_end_addr)
        {
            for (auto& binding : mem_obj.memory_binding_records_)
            {
                if (binding.isBuffer && buffer_entries_.find(binding.handle) != buffer_entries_.end() &&
                    buffer_entries_[binding.handle].destruction_index > call_info.index &&
                    buffer_entries_[binding.handle].creation_index < call_info.index)
                {
                    uint64_t binding_base_addr = mem_base_addr + (binding.offset - mem_obj.mapping.offset);
                    uint64_t binding_end_addr  = binding_base_addr + buffer_entries_[binding.handle].size;

                    // pointer address in this buffer range
                    if (desc_addr >= binding_base_addr && desc_addr < binding_end_addr)
                    {
                        // found in buffer's mapped pointer
                        found = true;

                        location.descriptor_offset_in_mapped_memory = desc_addr - mem_base_addr;
                        location.descriptor_offset_in_buffer =
                            desc_addr - mem_base_addr - (binding.offset - mem_obj.mapping.offset);

                        auto& descriptor_locations = device_memory_descriptor_locations[mem_id][desc_addr];
                        auto  existing_location =
                            std::find_if(descriptor_locations.begin(),
                                         descriptor_locations.end(),
                                         [this, &args](const format::DescriptorDataLocationInfo& location) {
                                             const auto& descriptor = GetDescriptorPayload(location);
                                             return descriptor.size() == args.dataSize &&
                                                    util::platform::MemoryCompare(descriptor.data(),
                                                                                  args.pDescriptor.GetPointer(),
                                                                                  args.dataSize) == 0;
                                         });
                        if (existing_location == descriptor_locations.end())
                        {
                            descriptor_locations.emplace_back(location);
                        }

                        GFXRECON_LOG_DEBUG("GetDescriptorEXT into buffer(%" PRIu64
                                           ", at 0x%lx), bound in memory(%" PRIu64 ", 0x%lx).",
                                           binding.handle,
                                           desc_addr,
                                           mem_id,
                                           mem_base_addr);
                        break;
                    }
                }
            }
        }
        if (found)
            break;
    }
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
