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

#include "vulkan_raytracing_modifier.h"

#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "format/format.h"
#include "format/format_arm.h"
#include "generated/generated_vulkan_struct_decoders.h"
#include "decode/vulkan_optimize_options.h"
#include "util/defines.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/memory_output_stream.h"
#include "encode/parameter_buffer.h"
#include "encode/struct_pointer_encoder.h"
#include "graphics/vulkan_struct_get_pnext.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanRayTracingModifier::VulkanRayTracingModifier(const VulkanOptimizationOptions& options) : options_(options){};

bool VulkanRayTracingModifier::CanOptimize()
{
    return true;
}

void VulkanRayTracingModifier::Process_vkGetBufferDeviceAddress(const ApiCallInfo&            call_info,
                                                                args::GetBufferDeviceAddress& args)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                     = args.pInfo.GetMetaStructPointer()->buffer;
    buffer_device_addresses_[args.result]     = buffer_id;
    buffer_entries_[buffer_id].device_address = args.result;
}

void VulkanRayTracingModifier::Process_vkGetBufferDeviceAddressKHR(const ApiCallInfo&               call_info,
                                                                   args::GetBufferDeviceAddressKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                     = args.pInfo.GetMetaStructPointer()->buffer;
    buffer_device_addresses_[args.result]     = buffer_id;
    buffer_entries_[buffer_id].device_address = args.result;
}

void VulkanRayTracingModifier::Process_vkGetBufferDeviceAddressEXT(const ApiCallInfo&               call_info,
                                                                   args::GetBufferDeviceAddressEXT& args)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                     = args.pInfo.GetMetaStructPointer()->buffer;
    buffer_device_addresses_[args.result]     = buffer_id;
    buffer_entries_[buffer_id].device_address = args.result;
}

void VulkanRayTracingModifier::Process_vkGetAccelerationStructureDeviceAddressKHR(
    const ApiCallInfo& call_info, args::GetAccelerationStructureDeviceAddressKHR& args)
{
    const auto& as_id = args.pInfo.GetMetaStructPointer()->accelerationStructure;
    if (IsModificationPass())
    {
        if (options_.remove_rt)
        {
            SetDeleteCurrentCall();
            return;
        }

        if (acceleration_structure_build_infos_.count(as_id) &&
            acceleration_structure_build_infos_[as_id].is_meta_copy &&
            block_index_ < acceleration_structure_build_infos_[as_id].process_compacted_as_index)
        {
            // delete the compacted AS function,
            // it will be inserted new AS function before ProcessVulkanCopyAccelerationStructuresCommand
            SetDeleteCurrentCall();

            gfxrecon::encode::ParameterEncoder encoder(
                &acceleration_structure_build_infos_[as_id].get_address_parameter_buffer);
            encoder.EncodeHandleIdValue(args.device);
            encoder.EncodeStructPtrPreamble(args.pInfo.GetPointer());
            encoder.EncodeEnumValue(args.pInfo.GetPointer()->sType);
            encode::EncodePNextStruct(&encoder, args.pInfo.GetPointer()->pNext);
            encoder.EncodeHandleIdValue(as_id);
            encoder.EncodeHandleIdValue(args.result);
        }
        return;
    }

    auto [it, inserted] =
        acceleration_structure_device_addresses_.try_emplace(args.result, std::unordered_set<format::HandleId>{});
    it->second.insert(as_id);

    acceleration_structure_entries_[as_id].device_address = args.result;

    if (acceleration_structure_entries_.find(as_id) != acceleration_structure_entries_.end())
    {
        if (buffer_device_addresses_.find(args.result) != buffer_device_addresses_.end())
        {
            buffer_device_addresses_.erase(args.result);
        }
    }
}

void VulkanRayTracingModifier::Process_vkGetRayTracingShaderGroupHandlesKHR(
    const ApiCallInfo& call_info, args::GetRayTracingShaderGroupHandlesKHR& args)
{
    if (IsModificationPass())
    {
        if (options_.remove_rt)
        {
            SetDeleteCurrentCall();
            return;
        }
        return;
    }

    const auto single_entry_size = args.dataSize / args.groupCount;
    assert(single_entry_size == format::kMaxShaderGroupHandleSize);

    for (int group = 0; group < args.groupCount; group++)
    {
        uint8_t* ptr = (uint8_t*)args.pData.GetPointer() + group * single_entry_size;

        if (!std::all_of(ptr, ptr + single_entry_size, [](uint8_t v) { return v == 0; }))
        {
            format::ShaderHandleLocationInfo loc{};
            loc.id         = args.pipeline;
            loc.group      = args.firstGroup + group;
            loc.group_size = single_entry_size;
            memcpy(loc.original_handles, ptr, single_entry_size);
            shader_group_handle_entries_[gfxrecon::util::hash::GenerateCheckSum<uint64_t>(
                                             loc.original_handles, format::kMaxShaderGroupHandleSize)]
                .push_back(loc);
        }
    }
}

std::vector<format::AddressLocationInfo> VulkanRayTracingModifier::GetBufferDeviceAddressesInFillMemory(
    std::vector<format::AddressLocationInfo>& as_locations, const void* data, size_t size)
{
    if (buffer_device_addresses_.empty())
    {
        return {};
    }

    auto [min, max] = std::minmax_element(buffer_device_addresses_.begin(),
                                          buffer_device_addresses_.end(),
                                          [](const auto& a, const auto& b) { return a.first < b.first; });

    if (buffer_entries_.find(max->second) == buffer_entries_.end())
    {
        return {};
    }

    std::vector<format::AddressLocationInfo> locations;
    const VkDeviceAddress                    min_addr = min->first;
    const VkDeviceAddress                    max_addr = max->first + buffer_entries_[max->second].size;
    uint64_t*                                start    = (uint64_t*)data;
    for (int i = 0; i < size / sizeof(VkDeviceAddress); i++)
    {
        uint64_t*      ptr               = start + i;
        const uint64_t value             = *ptr;
        const uint64_t offset_in_memory  = (uint64_t)ptr - (uint64_t)start;
        bool           value_is_in_range = value >= min_addr && value <= max_addr;
        if (!value_is_in_range)
        {
            continue;
        }

        // If there is acceleration structure address at the same offset, buffer address will no longer be found.
        auto as_location =
            std::find_if(as_locations.begin(), as_locations.end(), [offset_in_memory](auto& as_location) {
                return (offset_in_memory == as_location.offset_in_memory);
            });

        if (as_location != as_locations.end())
        {
            continue;
        }

        auto entry =
            std::find_if(buffer_device_addresses_.begin(), buffer_device_addresses_.end(), [value, this](auto& entry) {
                auto it = buffer_entries_.find(entry.second);
                if (it == buffer_entries_.end())
                {
                    return false;
                }
                const auto& buffer_entry = it->second;
                return (value >= entry.first) && (value <= entry.first + buffer_entry.size);
            });

        if (entry == buffer_device_addresses_.end())
        {
            continue;
        }

        format::AddressLocationInfo loc{};
        loc.id               = entry->second;
        loc.original_address = entry->first;
        loc.adjusted_address = value;
        loc.size             = buffer_entries_[entry->second].size;
        loc.offset_in_memory = offset_in_memory;
        locations.push_back(loc);
    }
    return locations;
}

std::vector<format::AddressLocationInfo>
VulkanRayTracingModifier::GetAccelerationStructureDeviceAddressesInFillMemory(const void* data, size_t size)
{
    if (options_.remove_rt)
    {
        return {};
    }

    if (acceleration_structure_device_addresses_.empty())
    {
        return {};
    }

    auto [min, max] = std::minmax_element(acceleration_structure_device_addresses_.begin(),
                                          acceleration_structure_device_addresses_.end(),
                                          [](const auto& a, const auto& b) { return a.first < b.first; });

    if (acceleration_structure_entries_.find(*max->second.begin()) == acceleration_structure_entries_.end())
    {
        return {};
    }

    format::HandleId buffer_id = acceleration_structure_entries_[*max->second.begin()].buf_handle;
    if (buffer_entries_.find(buffer_id) == buffer_entries_.end())
    {
        return {};
    }

    std::vector<format::AddressLocationInfo> locations;
    const VkDeviceAddress                    min_addr = min->first;
    const VkDeviceAddress                    max_addr = max->first + buffer_entries_[buffer_id].size;
    uint64_t*                                start    = (uint64_t*)data;
    for (int i = 0; i < size / sizeof(VkDeviceAddress); i++)
    {
        uint64_t*      ptr               = start + i;
        const uint64_t value             = *ptr;
        bool           value_is_in_range = value >= min_addr && value <= max_addr;
        if (!value_is_in_range)
        {
            continue;
        }

        auto ids = acceleration_structure_device_addresses_.find(value);
        if (ids == acceleration_structure_device_addresses_.end())
        {
            continue;
        }

        auto entry = acceleration_structure_entries_.end();

        for (auto& id : ids->second)
        {
            entry = acceleration_structure_entries_.find(id);
            if (entry == acceleration_structure_entries_.end())
            {
                continue;
            }
            if (entry->second.creation_index > block_index_ || entry->second.destruction_index < block_index_)
            {
                continue;
            }
            if (buffer_entries_.at(entry->second.buf_handle).destruction_index < block_index_)
            {
                continue;
            }
            break;
        }

        if (entry == acceleration_structure_entries_.end())
        {
            continue;
        }

        format::AddressLocationInfo loc{};
        loc.id               = entry->second.as_handle;
        loc.original_address = entry->second.device_address;
        loc.adjusted_address = value;
        loc.size             = entry->second.size;
        loc.offset_in_memory = (uint64_t)ptr - (uint64_t)start;
        locations.push_back(loc);
    }
    return locations;
}

std::vector<format::ShaderHandleLocationInfo>
VulkanRayTracingModifier::GetShaderGroupHandlesInFillMemory(const void* data, size_t size)
{
    const uint32_t single_shader_group_size = format::kMaxShaderGroupHandleSize;
    assert(data != nullptr);

    if (options_.remove_rt)
    {
        return {};
    }

    if (shader_group_handle_entries_.empty())
    {
        return {};
    }

    std::vector<format::ShaderHandleLocationInfo> locations_in_fill_memory;
    uint8_t*                                      start = (uint8_t*)data;
    for (int i = 0; i < size / single_shader_group_size; i++)
    {
        uint8_t* ptr = start + i * single_shader_group_size;
        if (std::all_of(ptr, ptr + single_shader_group_size, [](uint8_t v) { return v == 0; }))
        {
            continue;
        }
        auto locations = shader_group_handle_entries_.find(
            gfxrecon::util::hash::GenerateCheckSum<uint64_t>(ptr, single_shader_group_size));

        if (locations != shader_group_handle_entries_.end())
        {
            for (auto& location : locations->second)
            {
                const auto& pipeline_info = pipelines_[location.id];
                if (pipeline_info.destruction_index < block_index_)
                {
                    continue;
                }
                if (0 == memcmp(ptr, location.original_handles, single_shader_group_size))
                {
                    location.offset_in_memory = (uint64_t)ptr - (uint64_t)start;
                    locations_in_fill_memory.push_back(location);
                    break;
                }
            }
        }
    }
    return locations_in_fill_memory;
}

void VulkanRayTracingModifier::WriteFixShaderGroupHandleCmd(format::HandleId                  relation_id,
                                                            uint64_t                          num_of_locations,
                                                            format::ShaderHandleLocationInfo* locations)
{
    if (skip_address_replacement == true)
    {
        return;
    }
    auto new_call       = CreatePreCall();
    new_call->type      = NewCallDataType::MetaDataCall;
    new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
    new_call->thread_id = 1;

    format::FixShaderGroupHandleCommandHeader fix_cmd;
    fix_cmd.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
    fix_cmd.meta_header.block_header.size =
        format::GetMetaDataBlockBaseSize(fix_cmd) + (num_of_locations * sizeof(format::ShaderHandleLocationInfo));
    fix_cmd.meta_header.meta_data_id = format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan,
                                                              format::arm::MetaDataType::kFixShaderGroupHandleCommand);
    fix_cmd.relation_id              = relation_id;
    fix_cmd.num_of_locations         = num_of_locations;

    GFXRECON_LOG_INFO_ONCE("This capture has been optimized for ray tracing pipeline shader group handle.");
    new_call->parameter_buffer.Write(&fix_cmd, sizeof(format::FixShaderGroupHandleCommandHeader));
    new_call->parameter_buffer.Write(locations, num_of_locations * sizeof(format::ShaderHandleLocationInfo));
}

void VulkanRayTracingModifier::WriteFixDeviceAddressCmd(format::HandleId             relation_id,
                                                        uint64_t                     num_of_as_locations,
                                                        format::AddressLocationInfo* as_locations,
                                                        uint64_t                     num_of_buf_locations,
                                                        format::AddressLocationInfo* buf_locations)
{
    if (skip_address_replacement == true)
    {
        return;
    }
    uint64_t num_of_locations = num_of_as_locations + num_of_buf_locations;
    auto     new_call         = CreatePreCall();
    new_call->type            = NewCallDataType::MetaDataCall;
    new_call->call_id         = gfxrecon::format::ApiCallId::ApiCall_Unknown;
    new_call->thread_id       = 1;

    format::FixDeviceAddressCommandHeader fix_cmd;
    fix_cmd.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
    fix_cmd.meta_header.block_header.size =
        format::GetMetaDataBlockBaseSize(fix_cmd) + (num_of_locations * sizeof(format::AddressLocationInfo));
    fix_cmd.meta_header.meta_data_id =
        format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kFixDeviceAddressCommand);
    fix_cmd.relation_id      = relation_id;
    fix_cmd.num_of_locations = num_of_locations;

    new_call->parameter_buffer.Write(&fix_cmd, sizeof(format::FixDeviceAddressCommandHeader));
    if (num_of_as_locations)
    {
        GFXRECON_LOG_INFO_ONCE("This capture has been optimized for acceleration structure device address.");
        new_call->parameter_buffer.Write(as_locations, num_of_as_locations * sizeof(format::AddressLocationInfo));
    }
    if (num_of_buf_locations)
    {
        GFXRECON_LOG_INFO_ONCE("This capture has been optimized for buffer device address.");
        new_call->parameter_buffer.Write(buf_locations, num_of_buf_locations * sizeof(format::AddressLocationInfo));
    }
}

void VulkanRayTracingModifier::WriteInitBufferDataFixCmd()
{
    if (init_buffer_entries_.size())
    {
        for (auto& init_buffer : init_buffer_entries_)
        {
            auto& object = init_buffer.second;
            if (object.shader_handle_locations.size())
            {
                WriteFixShaderGroupHandleCmd(
                    object.buffer_id, object.shader_handle_locations.size(), object.shader_handle_locations.data());
            }

            if (object.device_address_locations.size())
            {
                WriteFixDeviceAddressCmd(object.buffer_id,
                                         object.device_address_locations.size(),
                                         object.device_address_locations.data(),
                                         0,
                                         nullptr);
            }

            if (object.init_buffer_data.size())
            {
                size_t                          data_size = static_cast<size_t>(object.init_buffer_data.size());
                format::InitBufferCommandHeader init_buffer_cmd;

                init_buffer_cmd.meta_header.block_header.type = format::kMetaDataBlock;
                init_buffer_cmd.meta_header.block_header.size =
                    format::GetMetaDataBlockBaseSize(init_buffer_cmd) + data_size;
                init_buffer_cmd.meta_header.meta_data_id = format::MakeMetaDataId(
                    format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kInitBufferCommand);
                init_buffer_cmd.thread_id = 1;
                init_buffer_cmd.device_id = object.device_id;
                init_buffer_cmd.buffer_id = object.buffer_id;
                init_buffer_cmd.data_size = data_size;

                auto new_call       = CreatePreCall();
                new_call->type      = NewCallDataType::MetaDataCall;
                new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
                new_call->thread_id = 1;
                new_call->parameter_buffer.Write(&init_buffer_cmd, sizeof(init_buffer_cmd));
                new_call->parameter_buffer.Write(object.init_buffer_data.data(), data_size);
            }
        }
        init_buffer_entries_.clear();
    }
}

void VulkanRayTracingModifier::ProcessFillMemoryCommand(uint64_t       memory_id,
                                                        uint64_t       offset,
                                                        uint64_t       size,
                                                        const uint8_t* data)
{
    if (!IsModificationPass())
    {
        fill_memory_indices_per_submit_.push_back(block_index_);
        return;
    }

    // Find as and buffer device address values inside data parameter, and generate FixDeviceAddressCommand meta block
    std::vector<format::AddressLocationInfo> as_address_locations =
        GetAccelerationStructureDeviceAddressesInFillMemory(data, size);
    std::vector<format::AddressLocationInfo> buffer_address_locations;
    if (!fill_memory_indices_to_inspect_.empty() && block_index_ == fill_memory_indices_to_inspect_.front())
    {
        buffer_address_locations = GetBufferDeviceAddressesInFillMemory(as_address_locations, data, size);
        fill_memory_indices_to_inspect_.pop_front();
    }
    // Find shader group handle values inside data parameter, and generate FixShaderGroupHandleCommand meta block
    auto shader_handle_locations = GetShaderGroupHandlesInFillMemory(data, size);
    if (shader_handle_locations.size())
    {
        WriteFixShaderGroupHandleCmd(memory_id, shader_handle_locations.size(), shader_handle_locations.data());
    }

    if (as_address_locations.size() || buffer_address_locations.size())
    {
        WriteFixDeviceAddressCmd(memory_id,
                                 as_address_locations.size(),
                                 as_address_locations.data(),
                                 buffer_address_locations.size(),
                                 buffer_address_locations.data());
    }
}

void VulkanRayTracingModifier::ProcessInitBufferCommand(format::HandleId device_id,
                                                        format::HandleId buffer_id,
                                                        uint64_t         data_size,
                                                        const uint8_t*   data)
{
    if (!IsModificationPass())
    {
        return;
    }

    InitBufferInfo init_buffer_object;
    init_buffer_object.device_id = device_id;
    init_buffer_object.buffer_id = buffer_id;

    // Find shader group handle values inside data
    auto shader_handle_locations = GetShaderGroupHandlesInFillMemory(data, data_size);
    if (shader_handle_locations.size())
    {
        init_buffer_object.shader_handle_locations = shader_handle_locations;
    }

    // Here only find as device address values inside data
    auto as_address_locations = GetAccelerationStructureDeviceAddressesInFillMemory(data, data_size);
    if (as_address_locations.size())
    {
        init_buffer_object.device_address_locations = as_address_locations;
    }

    if (shader_handle_locations.size() || as_address_locations.size())
    {
        init_buffer_object.init_buffer_data.resize(data_size);
        std::memcpy(init_buffer_object.init_buffer_data.data(), data, data_size);

        init_buffer_entries_.emplace(std::make_pair(buffer_id, init_buffer_object));

        SetDeleteCurrentCall();
    }
}

void VulkanRayTracingModifier::ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader&    header,
                                                              const std::vector<format::AddressLocationInfo>& infos)
{
    if (IsModificationPass())
    {
        // delete the old fixed meta command, it will be inserted new fixed meta command
        SetDeleteCurrentCall();
        return;
    }
}

void VulkanRayTracingModifier::ProcessFixShaderGroupHandleCommand(
    const format::FixShaderGroupHandleCommandHeader& header, const std::vector<format::ShaderHandleLocationInfo>& infos)
{
    if (IsModificationPass())
    {
        // delete the old fixed meta command, it will be inserted new fixed meta command
        SetDeleteCurrentCall();
        return;
    }
}

void VulkanRayTracingModifier::ProcessAccelerationStructureCompactionDependencyCommand(
    format::HandleId parent, const std::vector<format::HandleId>& children)
{
    if (IsModificationPass())
    {
        // delete the old compaction dependency meta command, it will be inserted new fixed meta command
        SetDeleteCurrentCall();
        return;
    }
}

void VulkanRayTracingModifier::ProcessSetOpaqueAddressCommand(format::HandleId device_id,
                                                              format::HandleId object_id,
                                                              uint64_t         address)
{
    if (IsModificationPass())
    {
        if (acceleration_structure_build_infos_.count(object_id) &&
            acceleration_structure_build_infos_[object_id].is_meta_copy &&
            block_index_ < acceleration_structure_build_infos_[object_id].process_compacted_as_index)
        {
            assert(acceleration_structure_entries_.count(object_id) > 0);
            acceleration_structure_entries_[object_id].device_address = address;
            SetDeleteCurrentCall();
        }
        return;
    }
}

void VulkanRayTracingModifier::Process_vkCreateBuffer(const ApiCallInfo& call_info, args::CreateBuffer& args)
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

void VulkanRayTracingModifier::Process_vkDestroyBuffer(const ApiCallInfo& call_info, args::DestroyBuffer& args)
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

void VulkanRayTracingModifier::Process_vkAllocateMemory(const ApiCallInfo& call_info, args::AllocateMemory& args)
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

void VulkanRayTracingModifier::Process_vkFreeMemory(const ApiCallInfo& call_info, args::FreeMemory& args)
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

void VulkanRayTracingModifier::Process_vkBindBufferMemory(const ApiCallInfo& call_info, args::BindBufferMemory& args)
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

    auto buffer_entry = buffer_entries_.find(args.buffer);
    GFXRECON_ASSERT(buffer_entry != buffer_entries_.end());
    GFXRECON_ASSERT(buffer_entry->second.memory_handle_id == 0);
    buffer_entry->second.memory_handle_id = args.memory;
    buffer_entry->second.memory_offset    = args.memoryOffset;
}

void VulkanRayTracingModifier::Process_vkBindBufferMemory2(const ApiCallInfo& call_info, args::BindBufferMemory2& args)
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

        auto buffer_entry = buffer_entries_.find(bind_meta_infos[i].buffer);
        GFXRECON_ASSERT(buffer_entry != buffer_entries_.end());
        GFXRECON_ASSERT(buffer_entry->second.memory_handle_id == 0);
        buffer_entry->second.memory_handle_id = bind_meta_infos[i].memory;
        buffer_entry->second.memory_offset    = bind_infos[i].memoryOffset;
    }
}

void VulkanRayTracingModifier::Process_vkBindBufferMemory2KHR(const ApiCallInfo&          call_info,
                                                              args::BindBufferMemory2KHR& args)
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

        auto buffer_entry = buffer_entries_.find(bind_meta_infos[i].buffer);
        GFXRECON_ASSERT(buffer_entry != buffer_entries_.end());
        GFXRECON_ASSERT(buffer_entry->second.memory_handle_id == 0);
        buffer_entry->second.memory_handle_id = bind_meta_infos[i].memory;
        buffer_entry->second.memory_offset    = bind_infos[i].memoryOffset;
    }
}

void VulkanRayTracingModifier::Process_vkBindImageMemory(const ApiCallInfo& call_info, args::BindImageMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[args.memory].memory_binding_records_.push_back(
            { args.image, false, args.memoryOffset });
    }
}

void VulkanRayTracingModifier::Process_vkBindImageMemory2(const ApiCallInfo& call_info, args::BindImageMemory2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkBindImageMemoryInfo*         bind_infos      = args.pBindInfos.GetPointer();
    const Decoded_VkBindImageMemoryInfo* bind_meta_infos = args.pBindInfos.GetMetaStructPointer();

    for (uint32_t i = 0; i < args.bindInfoCount; ++i)
    {
        if (memory_binding_entries_.find(bind_meta_infos[i].memory) != memory_binding_entries_.end())
        {
            memory_binding_entries_[bind_meta_infos[i].memory].memory_binding_records_.push_back(
                { bind_meta_infos[i].image, false, bind_infos[i].memoryOffset });
        }
    }
}

void VulkanRayTracingModifier::Process_vkCreateAccelerationStructureKHR(const ApiCallInfo&                    call_info,
                                                                        args::CreateAccelerationStructureKHR& args)
{
    format::HandleId handle = *args.pAccelerationStructure.GetPointer();
    if (!IsModificationPass())
    {
        auto [it, inserted]          = acceleration_structure_entries_.try_emplace(handle, AccelerationStructureInfo{});
        it->second.as_handle         = handle;
        it->second.buf_handle        = args.pCreateInfo.GetMetaStructPointer()->buffer;
        it->second.size              = args.pCreateInfo.GetPointer()->size;
        it->second.offset            = args.pCreateInfo.GetPointer()->offset;
        it->second.type              = args.pCreateInfo.GetPointer()->type;
        it->second.bind_descriptor   = false;
        it->second.creation_index    = call_info.index;
        it->second.destruction_index = UINT64_MAX;
        VkDeviceAddress storage_buffer_address =
            buffer_entries_[args.pCreateInfo.GetMetaStructPointer()->buffer].device_address;
        if (storage_buffer_address != 0)
        {
            it->second.device_address = storage_buffer_address + it->second.offset;
            acceleration_structure_device_addresses_[it->second.device_address].insert(handle);
        }
        return;
    }

    if (options_.remove_rt)
    {
        SetDeleteCurrentCall();
        return;
    }

    GFXRECON_ASSERT(acceleration_structure_entries_.contains(handle));

    const AccelerationStructureInfo& created_object = acceleration_structure_entries_[handle];

    std::unordered_set<uint64_t> prim_counts_quieried;
    for (const auto& [id, donor_candidate_info] : acceleration_structure_entries_)
    {
        const BufferInfo& buffer_entry_current = buffer_entries_.find(created_object.buf_handle)->second;
        const BufferInfo& buffer_entry_donor   = buffer_entries_.find(donor_candidate_info.buf_handle)->second;

        if (buffer_entry_current.memory_handle_id != buffer_entry_donor.memory_handle_id)
        {
            continue;
        }

        if (created_object.device_address == 0 || donor_candidate_info.device_address == 0)
        {

            if (buffer_entry_current.memory_offset != buffer_entry_donor.memory_offset)
            {
                continue;
            }
        }
        else if (created_object.device_address != donor_candidate_info.device_address)
        {
            continue;
        }

        if (created_object.type != donor_candidate_info.type)
        {
            continue;
        }

        if (buffer_entry_current.creation_index >= buffer_entry_donor.destruction_index)
        {
            continue;
        }

        auto build_info_candidate = acceleration_structure_build_infos_.find(id);
        if (build_info_candidate == acceleration_structure_build_infos_.end())
        {
            continue;
        }

        const std::vector<uint32_t>& prim_counts = build_info_candidate->second.primitive_counts;
        uint32_t                     total       = std::accumulate(prim_counts.begin(), prim_counts.end(), 0);
        if (total > 0 && !prim_counts_quieried.contains(total))
        {
            prim_counts_quieried.insert(total);
            EncodeVkGetAccelerationStructureBuildSizesKHR(args.device, build_info_candidate->second);
        }
    }

    auto build_info = acceleration_structure_build_infos_.find(handle);
    if (build_info == acceleration_structure_build_infos_.end())
    {
        return;
    }

    if (build_info->second.is_first_built)
    {
        EncodeVkGetAccelerationStructureBuildSizesKHR(args.device, build_info->second);
        return;
    }

    if (build_info->second.is_meta_copy && block_index_ < build_info->second.process_compacted_as_index)
    {
        // delete the compacted AS function,
        // it will be inserted new AS function before ProcessVulkanCopyAccelerationStructuresCommand
        SetDeleteCurrentCall();

        auto* pCreateInfoDec = args.pCreateInfo.GetPointer();

        gfxrecon::encode::ParameterEncoder encoder(
            &acceleration_structure_build_infos_[handle].create_parameter_buffer);
        encoder.EncodeHandleIdValue(args.device);
        encoder.EncodeStructPtrPreamble(pCreateInfoDec);
        encoder.EncodeEnumValue(pCreateInfoDec->sType);
        encode::EncodePNextStruct(&encoder, pCreateInfoDec->pNext);
        encoder.EncodeFlagsValue(pCreateInfoDec->createFlags);
        encoder.EncodeHandleIdValue(args.pCreateInfo.GetMetaStructPointer()->buffer);
        encoder.EncodeUInt64Value(pCreateInfoDec->offset);
        encoder.EncodeUInt64Value(pCreateInfoDec->size);
        encoder.EncodeEnumValue(pCreateInfoDec->type);
        encoder.EncodeUInt64Value(pCreateInfoDec->deviceAddress);
        encode::EncodeStructPtr(&encoder, args.pAllocator.GetPointer());
        encoder.EncodeHandleIdPtr(args.pAccelerationStructure.GetPointer());
        encoder.EncodeEnumValue(args.result);
        return;
    }

    if (build_info->second.source_of_compaction != format::kNullHandleId)
    {
        format::ParentToChildDependencyHeader header;

        header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
        header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(header) + sizeof(format::HandleId);
        header.meta_header.meta_data_id      = format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan,
                                                                 format::MetaDataType::kParentToChildDependency);
        header.thread_id                     = 1;
        header.dependency_type = format::ParentToChildDependencyType::kAccelerationStructureCompactionDependency;
        header.parent_id       = build_info->second.source_of_compaction;
        header.child_count     = 1;

        auto new_call     = CreatePreCall();
        new_call->type    = util::CallModifierBase::NewCallDataType::MetaDataCall;
        new_call->call_id = gfxrecon::format::ApiCallId::ApiCall_Unknown;
        new_call->parameter_buffer.Write(&header, sizeof(header));
        new_call->parameter_buffer.Write(&handle, sizeof(format::HandleId));
        return;
    }
}

void VulkanRayTracingModifier::Process_vkGetAccelerationStructureBuildSizesKHR(
    const ApiCallInfo& call_info, args::GetAccelerationStructureBuildSizesKHR& args)
{
    if (IsModificationPass())
    {
        SetDeleteCurrentCall();
    }
}

void VulkanRayTracingModifier::Process_vkDestroyAccelerationStructureKHR(const ApiCallInfo& call_info,
                                                                         args::DestroyAccelerationStructureKHR& args)
{
    if (IsModificationPass())
    {
        if (options_.remove_rt)
        {
            SetDeleteCurrentCall();
            return;
        }
        return;
    }

    if (acceleration_structure_entries_.find(args.accelerationStructure) != acceleration_structure_entries_.end())
    {
        acceleration_structure_entries_[args.accelerationStructure].destruction_index = call_info.index;
    }
}

void VulkanRayTracingModifier::ProcessBuildAccelerationStructures(
    uint32_t                                                                   infoCount,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   ppBuildRangeInfos)
{
    for (uint32_t info_index = 0; info_index < infoCount; ++info_index)
    {
        const VkAccelerationStructureBuildGeometryInfoKHR& geometry_info = pInfos->GetPointer()[info_index];
        const VkAccelerationStructureBuildRangeInfoKHR* build_range_info = ppBuildRangeInfos->GetPointer()[info_index];
        const auto&                                     geometry_meta_info = pInfos->GetMetaStructPointer()[info_index];
        const VkBuildAccelerationStructureModeKHR       mode               = geometry_info.mode;
        const format::HandleId&                         dst_as_id = geometry_meta_info.dstAccelerationStructure;

        auto [build_info, inserted] =
            acceleration_structure_build_infos_.try_emplace(dst_as_id, AccelerationStructureBuildInfo{});

        // If the Acceleration Structure has already been build, we need update the build info on the need-to basis
        // Also, this approach makes much more sense for TLAS then for BLAS, so we restrict it to what we know should
        // work
        if (!inserted && geometry_info.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
        {
            uint32_t total_cached = std::accumulate(
                build_info->second.primitive_counts.begin(), build_info->second.primitive_counts.end(), 0);
            uint32_t total_candidate = 0;
            for (uint32_t g = 0; g < geometry_info.geometryCount; ++g)
            {
                total_candidate += build_range_info[g].primitiveCount;
            }

            if (total_candidate < total_cached)
            {
                continue;
            }
        }

        build_info->second.is_first_built             = true;
        build_info->second.is_meta_copy               = false;
        build_info->second.process_compacted_as_index = 0;
        build_info->second.source_of_compaction       = format::kNullHandleId;
        build_info->second.info                       = geometry_info;

        build_info->second.geometries.resize(geometry_info.geometryCount);
        build_info->second.primitive_counts.resize(geometry_info.geometryCount);

        for (uint32_t g = 0; g < geometry_info.geometryCount; ++g)
        {
            const auto& geometry_data     = geometry_info.pGeometries[g];
            const auto& geometry_metadata = geometry_meta_info.pGeometries->GetMetaStructPointer()[g];
            if (geometry_data.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
            {
                continue;
            }

            build_info->second.geometries[g]       = geometry_data;
            build_info->second.primitive_counts[g] = build_range_info[g].primitiveCount;

            switch (geometry_data.geometryType)
            {
                case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                {
                    const auto& triangles = geometry_data.geometry.triangles;
                    if (triangles.pNext)
                    {
                        const auto omm_info =
                            reinterpret_cast<const VkAccelerationStructureTrianglesOpacityMicromapEXT*>(
                                triangles.pNext);
                        if (omm_info->sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT)
                        {
                            break;
                        }
                        const auto meta_omm_info =
                            reinterpret_cast<const Decoded_VkAccelerationStructureTrianglesOpacityMicromapEXT*>(
                                geometry_metadata.geometry->triangles->pNext->GetMetaStructPointer());
                        build_info->second.omm_infos[g]           = *omm_info;
                        build_info->second.geometry_omm_id_map[g] = meta_omm_info->micromap;

                        for (uint32_t usage_index = 0; usage_index < omm_info->usageCountsCount; ++usage_index)
                        {
                            build_info->second.usage_infos[g].push_back(omm_info->pUsageCounts[usage_index]);
                        }
                    }
                    break;
                }
                case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                {
                    instance_buffer_ranges_.emplace(geometry_data.geometry.instances.data.deviceAddress,
                                                    geometry_data.geometry.instances.data.deviceAddress +
                                                        geometry_info.geometryCount *
                                                            sizeof(VkAccelerationStructureInstanceKHR));
                    break;
                }
                case VK_GEOMETRY_TYPE_AABBS_KHR:
                {
                    break;
                }
                default:
                {
                    GFXRECON_LOG_ERROR("Unexpected geometry type");
                    break;
                }
            }
        }
    }
}

void VulkanRayTracingModifier::Process_vkCmdBuildAccelerationStructuresKHR(
    const ApiCallInfo& call_info, args::CmdBuildAccelerationStructuresKHR& args)
{
    GFXRECON_UNREFERENCED_PARAMETER(call_info);

    if (IsModificationPass())
    {
        if (options_.remove_rt)
        {
            SetDeleteCurrentCall();
            return;
        }
        return;
    }

    ProcessBuildAccelerationStructures(args.infoCount, &args.pInfos, &args.ppBuildRangeInfos);
}

void VulkanRayTracingModifier::ProcessVulkanBuildAccelerationStructuresCommand(
    format::HandleId                                                           device_id,
    uint32_t                                                                   info_count,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* geometry_infos,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   range_infos,
    std::vector<std::vector<VkAccelerationStructureInstanceKHR>>&              instance_buffers_data)
{
    GFXRECON_UNREFERENCED_PARAMETER(instance_buffers_data);

    if (IsModificationPass())
    {
        // Update device addresses and shader group handles before first build_as meta command for fastforward
        if (options_.remove_rt)
        {
            SetDeleteCurrentCall();
            return;
        }
        WriteInitBufferDataFixCmd();
        return;
    }

    ProcessBuildAccelerationStructures(info_count, geometry_infos, range_infos);
}

void VulkanRayTracingModifier::Process_vkCmdCopyAccelerationStructureKHR(const ApiCallInfo& call_info,
                                                                         args::CmdCopyAccelerationStructureKHR& args)
{
    if (IsModificationPass())
    {
        if (options_.remove_rt)
        {
            SetDeleteCurrentCall();
            return;
        }
        return;
    }

    const auto& info = args.pInfo.GetMetaStructPointer();
    if (acceleration_structure_build_infos_.count(info->src) != 0)
    {
        if (acceleration_structure_build_infos_.count(info->dst) == 0)
        {
            if (info->decoded_value->mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR)
            {
                acceleration_structure_build_infos_[info->dst].info                       = {};
                acceleration_structure_build_infos_[info->dst].geometries                 = {};
                acceleration_structure_build_infos_[info->dst].omm_infos                  = {};
                acceleration_structure_build_infos_[info->dst].usage_infos                = {};
                acceleration_structure_build_infos_[info->dst].primitive_counts           = {};
                acceleration_structure_build_infos_[info->dst].is_first_built             = false;
                acceleration_structure_build_infos_[info->dst].is_meta_copy               = false;
                acceleration_structure_build_infos_[info->dst].process_compacted_as_index = 0;
                acceleration_structure_build_infos_[info->dst].source_of_compaction       = info->src;
            }
            else
            {
                acceleration_structure_build_infos_.emplace(
                    std::make_pair(info->dst, acceleration_structure_build_infos_[info->src]));
            }
        }
    }
}

void VulkanRayTracingModifier::ProcessVulkanCopyAccelerationStructuresCommand(
    format::HandleId device_id, StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* copy_infos)
{
    if (IsModificationPass())
    {
        if (options_.remove_rt)
        {
            SetDeleteCurrentCall();
            return;
        }

        for (uint32_t index = 0; index < copy_infos->GetLength(); ++index)
        {
            format::HandleId dst_id = copy_infos->GetMetaStructPointer()[index].dst;
            if (acceleration_structure_build_infos_.count(dst_id) &&
                acceleration_structure_build_infos_[dst_id].is_meta_copy)
            {
                format::SetOpaqueAddressCommand opaque_address_cmd;
                opaque_address_cmd.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
                opaque_address_cmd.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(opaque_address_cmd);
                opaque_address_cmd.meta_header.meta_data_id      = format::MakeMetaDataId(
                    format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kSetOpaqueAddressCommand);
                opaque_address_cmd.thread_id = 1;
                opaque_address_cmd.device_id = device_id;
                opaque_address_cmd.object_id = dst_id;
                opaque_address_cmd.address   = acceleration_structure_entries_[dst_id].device_address;

                auto new_opaque_address_call       = CreatePreCall();
                new_opaque_address_call->type      = NewCallDataType::MetaDataCall;
                new_opaque_address_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
                new_opaque_address_call->thread_id = 1;
                new_opaque_address_call->parameter_buffer.Write(&opaque_address_cmd, sizeof(opaque_address_cmd));

                format::ParentToChildDependencyHeader dependency_header;
                dependency_header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
                dependency_header.meta_header.block_header.size =
                    format::GetMetaDataBlockBaseSize(dependency_header) + sizeof(format::HandleId);
                dependency_header.meta_header.meta_data_id = format::MakeMetaDataId(
                    format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kParentToChildDependency);
                dependency_header.thread_id = 1;
                dependency_header.dependency_type =
                    format::ParentToChildDependencyType::kAccelerationStructureCompactionDependency;
                dependency_header.parent_id   = acceleration_structure_build_infos_[dst_id].source_of_compaction;
                dependency_header.child_count = 1;

                auto new_dependency_call       = CreatePreCall();
                new_dependency_call->type      = util::CallModifierBase::NewCallDataType::MetaDataCall;
                new_dependency_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
                new_dependency_call->thread_id = 1;
                new_dependency_call->parameter_buffer.Write(&dependency_header, sizeof(dependency_header));
                new_dependency_call->parameter_buffer.Write(&dst_id, sizeof(format::HandleId));

                auto new_create_as_call       = CreatePreCall();
                new_create_as_call->type      = NewCallDataType::ApiCall;
                new_create_as_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_vkCreateAccelerationStructureKHR;
                new_create_as_call->thread_id = 1;

                auto& create_buffer = acceleration_structure_build_infos_[dst_id].create_parameter_buffer;
                new_create_as_call->parameter_buffer.Write(create_buffer.GetData(), create_buffer.GetDataSize());

                auto new_get_address_call  = CreatePreCall();
                new_get_address_call->type = NewCallDataType::ApiCall;
                new_get_address_call->call_id =
                    gfxrecon::format::ApiCallId::ApiCall_vkGetAccelerationStructureDeviceAddressKHR;
                new_get_address_call->thread_id = 1;

                auto& address_buffer = acceleration_structure_build_infos_[dst_id].get_address_parameter_buffer;
                new_get_address_call->parameter_buffer.Write(address_buffer.GetData(), address_buffer.GetDataSize());

                acceleration_structure_build_infos_[dst_id].is_meta_copy = false;
            }
        }
        return;
    }

    for (uint32_t index = 0; index < copy_infos->GetLength(); ++index)
    {
        format::HandleId                   src_id = copy_infos->GetMetaStructPointer()[index].src;
        format::HandleId                   dst_id = copy_infos->GetMetaStructPointer()[index].dst;
        VkCopyAccelerationStructureModeKHR mode   = copy_infos->GetMetaStructPointer()[index].decoded_value->mode;

        if (acceleration_structure_build_infos_.count(src_id) != 0)
        {
            if (acceleration_structure_build_infos_.count(dst_id) == 0)
            {
                if (mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR)
                {
                    acceleration_structure_build_infos_[dst_id].info                       = {};
                    acceleration_structure_build_infos_[dst_id].geometries                 = {};
                    acceleration_structure_build_infos_[dst_id].omm_infos                  = {};
                    acceleration_structure_build_infos_[dst_id].usage_infos                = {};
                    acceleration_structure_build_infos_[dst_id].primitive_counts           = {};
                    acceleration_structure_build_infos_[dst_id].is_first_built             = false;
                    acceleration_structure_build_infos_[dst_id].is_meta_copy               = true;
                    acceleration_structure_build_infos_[dst_id].process_compacted_as_index = block_index_;
                    acceleration_structure_build_infos_[dst_id].source_of_compaction       = src_id;
                }
                else
                {
                    acceleration_structure_build_infos_.emplace(
                        std::make_pair(dst_id, acceleration_structure_build_infos_[src_id]));
                }
            }
        }
    }
}

void VulkanRayTracingModifier::Process_vkCreateComputePipelines(const ApiCallInfo&            call_info,
                                                                args::CreateComputePipelines& args)
{
    if (IsModificationPass())
    {
        auto in_p_create_infos = args.pCreateInfos.GetPointer();
        for (uint32_t i = 0; i < args.createInfoCount; i++)
        {
            auto adress_offset_arm =
                graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(&(in_p_create_infos[i].stage));
            const VkSpecializationInfo* specialization_info = in_p_create_infos[i].stage.pSpecializationInfo;
            if ((adress_offset_arm != nullptr) && (specialization_info != nullptr))
            {
                skip_address_replacement = true;
            }
        }
        return;
    }

    for (uint32_t i = 0; i < args.createInfoCount; i++)
    {
        format::HandleId handle = args.pPipelines.GetPointer()[i];
        pipelines_[handle]      = PipelineInfo{ handle, call_info.index, std::numeric_limits<uint64_t>::max() };
    }
}

void VulkanRayTracingModifier::Process_vkAllocateCommandBuffers(const ApiCallInfo&            call_info,
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

void VulkanRayTracingModifier::Process_vkBeginCommandBuffer(const ApiCallInfo&        call_info,
                                                            args::BeginCommandBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkCommandBufferBeginInfo* begin_info            = args.pBeginInfo.GetPointer();
    command_buffer_entries_.at(args.commandBuffer).usage_ = begin_info->flags;
}

void VulkanRayTracingModifier::Process_vkCmdBindPipeline(const ApiCallInfo& call_info, args::CmdBindPipeline& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (args.pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        command_buffers_with_compute_.insert(args.commandBuffer);
    }
}

void VulkanRayTracingModifier::Process_vkUpdateDescriptorSets(const ApiCallInfo&          call_info,
                                                              args::UpdateDescriptorSets& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkWriteDescriptorSet* writes = args.pDescriptorWrites.GetPointer();
    for (uint32_t i = 0; i < args.descriptorWriteCount; i++)
    {
        const auto& write      = args.pDescriptorWrites.GetPointer()[i];
        const auto& meta_write = args.pDescriptorWrites.GetMetaStructPointer()[i];

        for (uint32_t index = 0; index < write.descriptorCount; index++)
        {
            if (write.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR && write.pNext)
            {
                const auto* meta_structure =
                    GetPNextMetaStruct<Decoded_VkWriteDescriptorSetAccelerationStructureKHR>(meta_write.pNext);
                VkWriteDescriptorSetAccelerationStructureKHR* structure =
                    (VkWriteDescriptorSetAccelerationStructureKHR*)(write.pNext);
                if (structure->sType != VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR)
                {
                    continue;
                }

                for (uint32_t as_index = 0; as_index < structure->accelerationStructureCount; as_index++)
                {
                    auto handle = meta_structure->pAccelerationStructures.GetPointer()[as_index];
                    if (acceleration_structure_entries_.find(handle) != acceleration_structure_entries_.end())
                    {
                        acceleration_structure_entries_[handle].bind_descriptor = true;
                    }
                }
            }
            else if (write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                     write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                const BufferInfo& dst_entry =
                    buffer_entries_.at(meta_write.pBufferInfo->GetMetaStructPointer()->buffer);
                if (dst_entry.device_address != 0)
                {
                    if (write.pBufferInfo->range == VK_WHOLE_SIZE)
                    {
                        transfer_ranges_.insert({ dst_entry.device_address + write.pBufferInfo->offset,
                                                  dst_entry.device_address + write.pBufferInfo->offset +
                                                      dst_entry.size - write.pBufferInfo->offset });
                    }
                    else
                    {
                        transfer_ranges_.insert(
                            { dst_entry.device_address + write.pBufferInfo->offset,
                              dst_entry.device_address + write.pBufferInfo->offset + write.pBufferInfo->range });
                    }
                }
            }
        }
    }
}

void VulkanRayTracingModifier::Process_vkCmdBindDescriptorSets(const ApiCallInfo&           call_info,
                                                               args::CmdBindDescriptorSets& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (args.pipelineBindPoint != VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        return;
    }

    for (uint32_t i = 0; i < args.descriptorSetCount; i++)
    {
        format::HandleId handle = args.pDescriptorSets.GetPointer()[i];
    }
}

void VulkanRayTracingModifier::Process_vkCmdCopyBuffer(const ApiCallInfo& call_info, args::CmdCopyBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }
    else
    {
        const BufferInfo& dst_entry = buffer_entries_.at(args.dstBuffer);
        if (dst_entry.device_address != 0)
        {
            for (uint32_t i = 0; i < args.regionCount; ++i)
            {
                const VkBufferCopy& r = args.pRegions.GetPointer()[i];
                transfer_ranges_.insert(
                    { dst_entry.device_address + r.dstOffset, dst_entry.device_address + r.dstOffset + r.size });
            }
        }
    }
}

void VulkanRayTracingModifier::Process_vkCmdCopyBuffer2(const ApiCallInfo& call_info, args::CmdCopyBuffer2& args)
{
    if (IsModificationPass())
    {
        return;
    }
    else
    {
        const VkCopyBufferInfo2* copy_info      = args.pCopyBufferInfo.GetPointer();
        const auto*              meta_copy_info = args.pCopyBufferInfo.GetMetaStructPointer();

        const BufferInfo& dst_entry = buffer_entries_.at(meta_copy_info->dstBuffer);
        if (dst_entry.device_address != 0)
        {
            for (uint32_t i = 0; i < copy_info->regionCount; ++i)
            {
                const VkBufferCopy2& r = copy_info->pRegions[i];
                transfer_ranges_.insert(
                    { dst_entry.device_address + r.dstOffset, dst_entry.device_address + r.dstOffset + r.size });
            }
        }
    }
}

void VulkanRayTracingModifier::Process_vkCmdCopyBuffer2KHR(const ApiCallInfo& call_info, args::CmdCopyBuffer2KHR& args)
{
    if (IsModificationPass())
    {
        return;
    }
    else
    {
        const VkCopyBufferInfo2KHR* copy_info      = args.pCopyBufferInfo.GetPointer();
        const auto*                 meta_copy_info = args.pCopyBufferInfo.GetMetaStructPointer();

        const BufferInfo& dst_entry = buffer_entries_.at(meta_copy_info->dstBuffer);
        if (dst_entry.device_address != 0)
        {
            for (uint32_t i = 0; i < copy_info->regionCount; ++i)
            {
                const VkBufferCopy2KHR& r = copy_info->pRegions[i];
                transfer_ranges_.insert(
                    { dst_entry.device_address + r.dstOffset, dst_entry.device_address + r.dstOffset + r.size });
            }
        }
    }
}

void VulkanRayTracingModifier::Process_vkCmdExecuteCommands(const ApiCallInfo&        call_info,
                                                            args::CmdExecuteCommands& args)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t i = 0; i < args.commandBufferCount; i++)
    {
        format::HandleId handle = args.pCommandBuffers.GetPointer()[i];
    }
}

void VulkanRayTracingModifier::Process_vkFreeCommandBuffers(const ApiCallInfo&        call_info,
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
            GFXRECON_LOG_WARNING("Skipping vkFreeCommandBuffers for null command buffer handle at call index %" PRIu64
                                 ".",
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
        command_buffers_with_compute_.erase(handle);
    }
}

void VulkanRayTracingModifier::Process_vkCmdUpdateBuffer(const ApiCallInfo& call_info, args::CmdUpdateBuffer& args)
{
    if (!IsModificationPass())
    {
        return;
    }

    const CommandBufferInfo& command_buffer_info = command_buffer_entries_.at(args.commandBuffer);

    format::HandleId device_id = command_buffer_info.device_id_;
    auto             data      = args.pData.GetPointer();
    // Find shader group handle values inside data parameter, and generate FixShaderGroupHandleCommand meta block
    auto shader_handle_locations = GetShaderGroupHandlesInFillMemory(data, args.dataSize);
    if (shader_handle_locations.size())
    {
        WriteFixShaderGroupHandleCmd(device_id, shader_handle_locations.size(), shader_handle_locations.data());
    }

    // Find as and buffer device address values inside data parameter, and generate FixDeviceAddressCommand meta block
    std::vector<format::AddressLocationInfo> as_address_locations =
        GetAccelerationStructureDeviceAddressesInFillMemory(data, args.dataSize);
    std::vector<format::AddressLocationInfo> buffer_address_locations =
        GetBufferDeviceAddressesInFillMemory(as_address_locations, data, args.dataSize);
    if (as_address_locations.size() || buffer_address_locations.size())
    {
        WriteFixDeviceAddressCmd(device_id,
                                 as_address_locations.size(),
                                 as_address_locations.data(),
                                 buffer_address_locations.size(),
                                 buffer_address_locations.data());
    }
}

void VulkanRayTracingModifier::ProcessCmdPushConstants(format::HandleId         commandBuffer,
                                                       format::HandleId         layout,
                                                       VkShaderStageFlags       stageFlags,
                                                       uint32_t                 offset,
                                                       uint32_t                 size,
                                                       PointerDecoder<uint8_t>* pValues)
{
    const CommandBufferInfo& command_buffer_info = command_buffer_entries_.at(commandBuffer);

    format::HandleId device_id = command_buffer_info.device_id_;
    auto             data      = pValues->GetPointer();
    // Find shader group handle values inside data parameter, and generate FixShaderGroupHandleCommand meta block
    auto shader_handle_locations = GetShaderGroupHandlesInFillMemory(data, size);
    if (shader_handle_locations.size())
    {
        WriteFixShaderGroupHandleCmd(device_id, shader_handle_locations.size(), shader_handle_locations.data());
    }

    // Find as and buffer device address values inside data parameter, and generate FixDeviceAddressCommand meta block
    std::vector<format::AddressLocationInfo> as_address_locations =
        GetAccelerationStructureDeviceAddressesInFillMemory(data, size);
    std::vector<format::AddressLocationInfo> buffer_address_locations =
        GetBufferDeviceAddressesInFillMemory(as_address_locations, data, size);
    if (as_address_locations.size() || buffer_address_locations.size())
    {
        WriteFixDeviceAddressCmd(device_id,
                                 as_address_locations.size(),
                                 as_address_locations.data(),
                                 buffer_address_locations.size(),
                                 buffer_address_locations.data());
    }
}

void VulkanRayTracingModifier::Process_vkCmdPushConstants(const ApiCallInfo& call_info, args::CmdPushConstants& args)
{
    if (!IsModificationPass())
    {
        return;
    }

    ProcessCmdPushConstants(args.commandBuffer, args.layout, args.stageFlags, args.offset, args.size, &args.pValues);
}

void VulkanRayTracingModifier::Process_vkCmdPushConstants2(const ApiCallInfo& call_info, args::CmdPushConstants2& args)
{
    Decoded_VkPushConstantsInfo* meta_info = args.pPushConstantsInfo.GetMetaStructPointer();
    VkPushConstantsInfo*         info      = args.pPushConstantsInfo.GetPointer();
    GFXRECON_ASSERT(meta_info != nullptr && info != nullptr);

    if (!IsModificationPass())
    {
        if (auto address_offset_arm = gfxrecon::graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(info);
            address_offset_arm != nullptr)
        {
            skip_address_replacement = true;
        }

        return;
    }

    ProcessCmdPushConstants(
        args.commandBuffer, meta_info->layout, info->stageFlags, info->offset, info->size, &meta_info->pValues);
}

void VulkanRayTracingModifier::Process_vkCmdPushConstants2KHR(const ApiCallInfo&          call_info,
                                                              args::CmdPushConstants2KHR& args)
{
    Decoded_VkPushConstantsInfo* meta_info = args.pPushConstantsInfo.GetMetaStructPointer();
    VkPushConstantsInfo*         info      = args.pPushConstantsInfo.GetPointer();
    GFXRECON_ASSERT(meta_info != nullptr && info != nullptr);

    if (!IsModificationPass())
    {
        if (auto address_offset_arm = gfxrecon::graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(info);
            address_offset_arm != nullptr)
        {
            skip_address_replacement = true;
        }

        return;
    }

    ProcessCmdPushConstants(
        args.commandBuffer, meta_info->layout, info->stageFlags, info->offset, info->size, &meta_info->pValues);
}

void VulkanRayTracingModifier::Process_vkQueueSubmit(const ApiCallInfo& call_info, args::QueueSubmit& args)
{
    for (uint32_t info_index = 0; info_index < args.submitCount; info_index++)
    {
        const auto& submit_info      = args.pSubmits.GetPointer()[info_index];
        const auto& submit_meta_info = args.pSubmits.GetMetaStructPointer()[info_index];

        bool should_inspect = false;
        for (uint32_t cmd_buffer_index = 0; cmd_buffer_index < submit_info.commandBufferCount; cmd_buffer_index++)
        {

            const format::HandleId command_buffer = submit_meta_info.pCommandBuffers.GetPointer()[cmd_buffer_index];
            should_inspect                        = should_inspect || HeuristicCheck(command_buffer);
        }

        // This submit contains interesting work. We should inspect FillMemory commands associated with this submit
        // Store all the indices of FillMemory tied to this submit so we can process it on the modification pass
        if (should_inspect)
        {
            fill_memory_indices_to_inspect_.insert(fill_memory_indices_to_inspect_.end(),
                                                   fill_memory_indices_per_submit_.begin(),
                                                   fill_memory_indices_per_submit_.end());
        }
    }
    fill_memory_indices_per_submit_.clear();
}

void VulkanRayTracingModifier::Process_vkQueueSubmit2(const ApiCallInfo& call_info, args::QueueSubmit2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t info_index = 0; info_index < args.submitCount; info_index++)
    {
        auto& submit_info      = args.pSubmits.GetPointer()[info_index];
        auto& submit_meta_info = args.pSubmits.GetMetaStructPointer()[info_index];

        bool should_inspect = false;
        for (uint32_t cmd_buffer_index = 0; cmd_buffer_index < submit_info.commandBufferInfoCount; cmd_buffer_index++)
        {

            const format::HandleId command_buffer =
                submit_meta_info.pCommandBufferInfos->GetMetaStructPointer()[cmd_buffer_index].commandBuffer;
            should_inspect = should_inspect || HeuristicCheck(command_buffer);
        }

        // This submit contains interesting work. We should inspect FillMemory commands associated with this submit
        // Store all the indices of FillMemory tied to this submit so we can process it on the modification pass
        if (should_inspect)
        {
            fill_memory_indices_to_inspect_.insert(fill_memory_indices_to_inspect_.end(),
                                                   fill_memory_indices_per_submit_.begin(),
                                                   fill_memory_indices_per_submit_.end());
        }
    }
    fill_memory_indices_per_submit_.clear();
}

void VulkanRayTracingModifier::Process_vkQueueSubmit2KHR(const ApiCallInfo& call_info, args::QueueSubmit2KHR& args)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t info_index = 0; info_index < args.submitCount; info_index++)
    {
        auto& submit_info      = args.pSubmits.GetPointer()[info_index];
        auto& submit_meta_info = args.pSubmits.GetMetaStructPointer()[info_index];

        bool should_inspect = false;
        for (uint32_t cmd_buffer_index = 0; cmd_buffer_index < submit_info.commandBufferInfoCount; cmd_buffer_index++)
        {

            const format::HandleId command_buffer =
                submit_meta_info.pCommandBufferInfos->GetMetaStructPointer()[cmd_buffer_index].commandBuffer;
            should_inspect = should_inspect || HeuristicCheck(command_buffer);
        }

        // This submit contains interesting work. We should inspect FillMemory commands associated with this submit
        // Store all the indices of FillMemory tied to this submit so we can process it on the modification pass
        if (should_inspect)
        {
            fill_memory_indices_to_inspect_.insert(fill_memory_indices_to_inspect_.end(),
                                                   fill_memory_indices_per_submit_.begin(),
                                                   fill_memory_indices_per_submit_.end());
        }
    }
    fill_memory_indices_per_submit_.clear();
}

void VulkanRayTracingModifier::Process_vkCmdWriteAccelerationStructuresPropertiesKHR(
    const ApiCallInfo& call_info, args::CmdWriteAccelerationStructuresPropertiesKHR& args)
{
    if (IsModificationPass())
    {
        if (options_.remove_rt)
        {
            SetDeleteCurrentCall();
            return;
        }
        return;
    }
}

bool VulkanRayTracingModifier::HeuristicCheck(format::HandleId command_buffer)
{
    if (command_buffers_with_compute_.erase(command_buffer) > 0)
    {
        return true;
    }

    for (const auto& a : instance_buffer_ranges_)
    {
        if (transfer_ranges_.intersection(a))
        {
            return true;
        }
    }

    return false;
}

void VulkanRayTracingModifier::EncodeVkGetAccelerationStructureBuildSizesKHR(format::HandleId                device,
                                                                             AccelerationStructureBuildInfo& build_info)
{
    VkAccelerationStructureBuildGeometryInfoKHR pBuildInfo = build_info.info;
    pBuildInfo.pGeometries                                 = build_info.geometries.data();
    for (auto& [geometry, omm_info] : build_info.omm_infos)
    {
        if (omm_info.usageCountsCount > 0)
        {
            omm_info.pUsageCounts = build_info.usage_infos[geometry].data();
        }
        const_cast<VkAccelerationStructureGeometryKHR*>(pBuildInfo.pGeometries)[geometry].geometry.triangles.pNext =
            (void*)&omm_info;
    }

    uint32_t*                                max_primitive_counts = build_info.primitive_counts.data();
    VkAccelerationStructureBuildSizesInfoKHR pSizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0
    };

    auto new_call       = CreatePreCall();
    new_call->type      = NewCallDataType::ApiCall;
    new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_vkGetAccelerationStructureBuildSizesKHR;
    new_call->thread_id = 1;
    gfxrecon::encode::ParameterEncoder encoder(&new_call->parameter_buffer);
    encoder.EncodeHandleIdValue(device);
    encoder.EncodeEnumValue(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR); // TODO: hardcoding device build,
                                                                              // could be host or host_or_device
    // encode::EncodeStructPtr(&encoder, &pBuildInfo);

    // Manually encoding, identical to generated except for pnext of triangles
    encoder.EncodeStructPtrPreamble(&pBuildInfo, false, false);

    encoder.EncodeEnumValue(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
    EncodePNextStructIfValid(&encoder, nullptr);
    encoder.EncodeEnumValue(pBuildInfo.type);
    encoder.EncodeFlagsValue(pBuildInfo.flags);
    encoder.EncodeEnumValue(pBuildInfo.mode);
    encoder.EncodeHandleIdValue(format::kNullHandleId);
    encoder.EncodeHandleIdValue(format::kNullHandleId);
    encoder.EncodeUInt32Value(pBuildInfo.geometryCount);

    encoder.EncodeStructArrayPreamble(pBuildInfo.pGeometries, pBuildInfo.geometryCount, false, false);

    for (uint32_t g = 0; g < pBuildInfo.geometryCount; ++g)
    {
        encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].sType);
        EncodePNextStruct(&encoder, pBuildInfo.pGeometries[g].pNext);
        encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].geometryType);
        switch (pBuildInfo.pGeometries[g].geometryType)
        {
            case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].geometry.triangles.sType);
                if (!build_info.omm_infos.empty())
                {
                    encoder.EncodeStructPtrPreamble(&(build_info.omm_infos[g]), false, false);

                    encoder.EncodeEnumValue(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT);
                    EncodePNextStruct(&encoder, build_info.omm_infos[g].pNext);
                    encoder.EncodeEnumValue(build_info.omm_infos[g].indexType);
                    EncodeStruct(&encoder, build_info.omm_infos[g].indexBuffer);
                    encoder.EncodeUInt64Value(build_info.omm_infos[g].indexStride);
                    encoder.EncodeUInt32Value(build_info.omm_infos[g].baseTriangle);
                    encoder.EncodeUInt32Value(build_info.omm_infos[g].usageCountsCount);
                    EncodeStructArray(
                        &encoder, build_info.omm_infos[g].pUsageCounts, build_info.omm_infos[g].usageCountsCount);
                    EncodeStructArray2D(
                        &encoder, build_info.omm_infos[g].ppUsageCounts, build_info.omm_infos[g].usageCountsCount, 1);

                    // todo: All this monstrocity is done for this line
                    encoder.EncodeHandleIdValue(build_info.geometry_omm_id_map[g]);
                }
                else
                {
                    EncodePNextStruct(&encoder, nullptr);
                }
                encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].geometry.triangles.vertexFormat);
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.triangles.vertexData);
                encoder.EncodeUInt64Value(pBuildInfo.pGeometries[g].geometry.triangles.vertexStride);
                encoder.EncodeUInt32Value(pBuildInfo.pGeometries[g].geometry.triangles.maxVertex);
                encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].geometry.triangles.indexType);
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.triangles.indexData);
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.triangles.transformData);
                break;
            case VK_GEOMETRY_TYPE_AABBS_KHR:
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.aabbs);
                break;
            case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.instances);
                break;
            default:
                break;
        }
        encoder.EncodeFlagsValue(pBuildInfo.pGeometries[g].flags);
    }
    EncodeStructArray2D(&encoder, pBuildInfo.ppGeometries, pBuildInfo.geometryCount, 1);
    EncodeStruct(&encoder, pBuildInfo.scratchData);

    encoder.EncodeUInt32Array(max_primitive_counts, pBuildInfo.geometryCount);
    encode::EncodeStructPtr(&encoder, &pSizeInfo);
}

void VulkanRayTracingModifier::Process_vkCreateRayTracingPipelinesNV(const ApiCallInfo&                 call_info,
                                                                     args::CreateRayTracingPipelinesNV& args)
{
    if (IsModificationPass())
    {
        return;
    }

    auto in_pCreateInfos = args.pCreateInfos.GetPointer();
    for (uint32_t i = 0; i < args.createInfoCount; i++)
    {
        for (uint32_t j = 0; j < in_pCreateInfos[i].stageCount; j++)
        {
            auto adress_offset_arm =
                graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(&(in_pCreateInfos[i].pStages[j]));
            const VkSpecializationInfo* specialization_info = in_pCreateInfos[i].pStages[j].pSpecializationInfo;
            if ((adress_offset_arm != nullptr) && (specialization_info != nullptr))
            {
                skip_address_replacement = true;
            }
        }
        format::HandleId handle = args.pPipelines.GetPointer()[i];
        pipelines_[handle]      = PipelineInfo{ handle, call_info.index, std::numeric_limits<uint64_t>::max() };
    }
}

void VulkanRayTracingModifier::Process_vkCreateRayTracingPipelinesKHR(const ApiCallInfo&                  call_info,
                                                                      args::CreateRayTracingPipelinesKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }

    auto in_pCreateInfos = args.pCreateInfos.GetPointer();
    for (uint32_t i = 0; i < args.createInfoCount; i++)
    {
        for (uint32_t j = 0; j < in_pCreateInfos[i].stageCount; j++)
        {
            auto adress_offset_arm =
                graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(&(in_pCreateInfos[i].pStages[j]));
            const VkSpecializationInfo* specialization_info = in_pCreateInfos[i].pStages[j].pSpecializationInfo;
            if ((adress_offset_arm != nullptr) && (specialization_info != nullptr))
            {
                skip_address_replacement = true;
            }
        }
        format::HandleId handle = args.pPipelines.GetPointer()[i];
        pipelines_[handle]      = PipelineInfo{ handle, call_info.index, std::numeric_limits<uint64_t>::max() };
    }
}

void VulkanRayTracingModifier::Process_vkCreateGraphicsPipelines(const ApiCallInfo&             call_info,
                                                                 args::CreateGraphicsPipelines& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkGraphicsPipelineCreateInfo* in_p_create_infos = args.pCreateInfos.GetPointer();
    for (uint32_t i = 0; i < args.createInfoCount; i++)
    {
        for (uint32_t j = 0; j < in_p_create_infos[i].stageCount; j++)
        {
            auto adress_offset_arm =
                graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(&(in_p_create_infos[i].pStages[j]));
            const VkSpecializationInfo* specialization_info = in_p_create_infos[i].pStages[j].pSpecializationInfo;
            if ((adress_offset_arm != nullptr) && (specialization_info != nullptr))
            {
                skip_address_replacement = true;
            }
        }

        format::HandleId handle = args.pPipelines.GetPointer()[i];
        pipelines_[handle]      = PipelineInfo{ handle, call_info.index, std::numeric_limits<uint64_t>::max() };
    }
}

void VulkanRayTracingModifier::Process_vkCmdUpdateBuffer2ARM(const ApiCallInfo&         call_info,
                                                             args::CmdUpdateBuffer2ARM& args)
{
    if (IsModificationPass())
    {
        return;
    }

    VkUpdateBufferInfoARM* in_pInfo = args.pInfo.GetPointer();
    if (auto address_offset_arm = gfxrecon::graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(in_pInfo);
        address_offset_arm != nullptr)
    {
        skip_address_replacement = true;
    }
}
void VulkanRayTracingModifier::Process_vkFlushMappedMemoryRanges(const ApiCallInfo&             call_info,
                                                                 args::FlushMappedMemoryRanges& args)
{
    if (IsModificationPass())
    {
        return;
    }

    auto in_pMemoryRanges = args.pMemoryRanges.GetPointer();
    for (uint32_t i = 0; i < args.memoryRangeCount; i++)
    {
        if (auto address_offset_arm =
                gfxrecon::graphics::vulkan_struct_get_pnext<VkMarkedOffsetsARM>(&(in_pMemoryRanges[i]));
            address_offset_arm != nullptr)
        {
            skip_address_replacement = true;
        }
    }
}

void VulkanRayTracingModifier::Process_vkDestroyPipeline(const ApiCallInfo& call_info, args::DestroyPipeline& args)
{
    if (IsModificationPass())
    {
        return;
    }

    pipelines_[args.pipeline].destruction_index = call_info.index;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
