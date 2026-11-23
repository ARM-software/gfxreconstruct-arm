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

#include "vulkan_arm_trace_helpers_modifier.h"

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "format/format.h"
#include "format/format_arm.h"
#include "generated/generated_vulkan_struct_decoders.h"
#include "util/defines.h"
#include "util/logging.h"
#include "util/memory_output_stream.h"
#include "graphics/vulkan_struct_get_pnext.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

bool VulkanArmTraceHelpersModifier::CanOptimize()
{
    previous_fill_memory_commands.clear();

    bool result = struct_to_propagate_.size();

    if (result)
    {
        GFXRECON_LOG_INFO("This capture has been optimized for arm trace helpers.");
    }

    return result;
}

void VulkanArmTraceHelpersModifier::Process_vkCreateDevice(const ApiCallInfo& call_info, args::CreateDevice& args)
{
    if (IsModificationPass())
    {
        return;
    }

    // Keep track of devices requesting trace helpers support
    auto pCreateInfoDec = args.pCreateInfo.GetMetaStructPointer()->decoded_value;
    if (pCreateInfoDec->enabledExtensionCount)
    {

        std::vector<std::string> extensions_vector(pCreateInfoDec->ppEnabledExtensionNames,
                                                   pCreateInfoDec->ppEnabledExtensionNames +
                                                       pCreateInfoDec->enabledExtensionCount);

        if (std::find(extensions_vector.begin(), extensions_vector.end(), VK_ARM_TRACE_HELPERS_EXTENSION_NAME) !=
            extensions_vector.end())
        {
            devices_using_helpers.push_back(*args.pDevice.GetPointer());
        }
    }
}

void VulkanArmTraceHelpersModifier::Process_vkAllocateMemory(const ApiCallInfo& call_info, args::AllocateMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    // Keep track of memories of device requesting trace helpers support
    if (std::find(devices_using_helpers.begin(), devices_using_helpers.end(), args.device) !=
        devices_using_helpers.end())
    {
        memories_using_helpers.push_back(*args.pMemory.GetPointer());
    }
}

VkDeviceSize VulkanArmTraceHelpersModifier::GetDataSizeBasedOnHelpersType(const VkMarkingTypeARM&    marking_types,
                                                                          const VkMarkingSubTypeARM& sub_types)
{
    switch (marking_types)
    {
        case VK_MARKING_TYPE_DEVICE_ADDRESS_ARM:
            return sizeof(VkDeviceAddress);
            break;
        case VK_MARKING_TYPE_SHADER_GROUP_HANDLE_ARM:
            // Hardcoded size for KHR variant from documentation
            return 32 * sizeof(uint8_t);
            break;
        default:
            GFXRECON_LOG_ERROR("Unhandled Helpers Code Path. Abort");
            return 0;
    }

    return 0;
}

void VulkanArmTraceHelpersModifier::ProcessTraceHelpersDataCommand(
    const format::TraceHelpersDataCommandHeader& header, const std::vector<format::TraceHelpersDataInfos>& infos)
{
    if (!IsModificationPass())
    {
        return;
    }
    // Delete the old call to avoid duplication
    SetDeleteCurrentCall();
}

void VulkanArmTraceHelpersModifier::ProcessFillMemoryCommand(uint64_t       memory_id,
                                                             uint64_t       offset,
                                                             uint64_t       size,
                                                             const uint8_t* data)
{
    // If device using the input memory hasn't requested the extension, ignore call
    if (std::find(memories_using_helpers.begin(), memories_using_helpers.end(), memory_id) ==
        memories_using_helpers.end())
    {
        return;
    }

    if (!IsModificationPass())
    {
        previous_fill_memory_commands.push_back({ memory_id, offset, { data, data + size } });
        return;
    }

    std::vector<format::TraceHelpersDataInfos> offsets_found;

    for (auto& el : struct_to_propagate_)
    {
        if (el.memory != memory_id)
        {
            continue;
        }

        if (el.absolute_offset >= offset && el.absolute_offset + el.capture_time_data_propagate.size() <= offset + size)
        {
            uint64_t relative_offset = el.absolute_offset - offset;
            if (memcmp(el.capture_time_data_propagate.data(),
                       data + relative_offset,
                       el.capture_time_data_propagate.size()) == 0)
            {
                offsets_found.push_back(
                    { (uint64_t)el.marking_types, el.sub_types.reserved, (uint64_t)relative_offset });
            }
        }
    }
    if (!offsets_found.empty())
    {
        auto new_call       = CreatePreCall();
        new_call->type      = NewCallDataType::MetaDataCall;
        new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
        new_call->thread_id = 1;

        format::TraceHelpersDataCommandHeader command_header;
        command_header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
        command_header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(command_header) +
                                                       (offsets_found.size() * sizeof(format::TraceHelpersDataInfos));
        command_header.meta_header.meta_data_id = format::MakeMetaDataId(
            format::ApiFamilyId::ApiFamily_Vulkan, format::arm::MetaDataType::kTraceHelpersDataCommand);
        command_header.thread_id = 0;
        command_header.count     = offsets_found.size();

        new_call->parameter_buffer.Write(&command_header, sizeof(format::TraceHelpersDataCommandHeader));
        new_call->parameter_buffer.Write(offsets_found.data(),
                                         offsets_found.size() * sizeof(format::TraceHelpersDataInfos));
    }
}

void VulkanArmTraceHelpersModifier::Process_vkFlushMappedMemoryRanges(const ApiCallInfo&             call_info,
                                                                      args::FlushMappedMemoryRanges& args)
{
    if (IsModificationPass())
    {
        return;
    }

    auto in_pMemoryRanges     = args.pMemoryRanges.GetPointer();
    auto in_pMemoryRangesMeta = args.pMemoryRanges.GetMetaStructPointer();
    for (uint32_t i = 0; i < args.memoryRangeCount; i++)
    {
        if (auto address_offset_arm =
                gfxrecon::graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(&(in_pMemoryRanges[i]));
            address_offset_arm != nullptr)
        {
            // Find closest FillMemoryCmd that modified the data to save the original data to replace

            format::HandleId     memory               = in_pMemoryRangesMeta[i].memory;
            VkDeviceSize         memory_range_offset  = in_pMemoryRanges[i].offset;
            VkDeviceSize         absolute_offset      = 0;
            VkDeviceSize         size_of_modification = 0;
            std::vector<uint8_t> data_to_modify;

            for (uint64_t j = 0; j < address_offset_arm->count; j++)
            {
                absolute_offset      = memory_range_offset + address_offset_arm->pOffsets[j];
                size_of_modification = GetDataSizeBasedOnHelpersType(address_offset_arm->pMarkingTypes[j],
                                                                     address_offset_arm->pSubTypes[j]);

                if (size_of_modification == 0)
                {
                    continue;
                }

                bool previous_relevant_fill_memory_command_exists = false;
                for (auto it = previous_fill_memory_commands.rbegin(); it != previous_fill_memory_commands.rend(); ++it)
                {
                    if (it->memory != memory)
                    {
                        continue;
                    }

                    // Check if ranges don't overlap
                    if (!((it->offset <= absolute_offset + size_of_modification) &&
                          (absolute_offset <= it->offset + it->data.size())))
                    {
                        continue;
                    }

                    // ranges overlap
                    previous_relevant_fill_memory_command_exists = true;

                    std::vector<uint8_t> data_to_propagate(size_of_modification);

                    memcpy(data_to_propagate.data(),
                           it->data.data() + (absolute_offset - it->offset),
                           size_of_modification);

                    struct_to_propagate_.push_back({
                        it->memory,
                        absolute_offset,
                        data_to_propagate,
                        address_offset_arm->pMarkingTypes[j],
                        address_offset_arm->pSubTypes[j],
                    });

                    // This break lives on the assumption that the data to be replaced is never used until this flush
                    // happens, therefore the closest fill memory that modifies it is the only one that we care about
                    break;
                }
                // If this hits, the data was never written prior to the flush call. This shouldn't happen
                GFXRECON_ASSERT(previous_relevant_fill_memory_command_exists == true);
            }
        }
    }
}
GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
