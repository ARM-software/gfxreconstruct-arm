#include "vulkan_raytracing_modifier.h"

#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "format/format.h"
#include "tools/optimize/vulkan_optimize_options.h"
#include "util/defines.h"
#include "util/memory_output_stream.h"
#include "encode/parameter_buffer.h"
#include "encode/struct_pointer_encoder.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanRayTracingModifier::VulkanRayTracingModifier(const VulkanOptimizationOptions& options) : options_(options){};

bool VulkanRayTracingModifier::CanOptimize()
{
    return true;
}

void VulkanRayTracingModifier::Process_vkGetBufferDeviceAddress(
    const ApiCallInfo&                                       call_info,
    VkDeviceAddress                                          returnValue,
    format::HandleId                                         device,
    StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                 = pInfo->GetMetaStructPointer()->buffer;
    buffer_device_addresses_[returnValue] = buffer_id;
}

void VulkanRayTracingModifier::Process_vkGetBufferDeviceAddressKHR(
    const ApiCallInfo&                                       call_info,
    VkDeviceAddress                                          returnValue,
    format::HandleId                                         device,
    StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                 = pInfo->GetMetaStructPointer()->buffer;
    buffer_device_addresses_[returnValue] = buffer_id;
}

void VulkanRayTracingModifier::Process_vkGetBufferDeviceAddressEXT(
    const ApiCallInfo&                                       call_info,
    VkDeviceAddress                                          returnValue,
    format::HandleId                                         device,
    StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                 = pInfo->GetMetaStructPointer()->buffer;
    buffer_device_addresses_[returnValue] = buffer_id;
}

void VulkanRayTracingModifier::Process_vkGetAccelerationStructureDeviceAddressKHR(
    const ApiCallInfo&                                                         call_info,
    VkDeviceAddress                                                            returnValue,
    format::HandleId                                                           device,
    StructPointerDecoder<Decoded_VkAccelerationStructureDeviceAddressInfoKHR>* pInfo)
{
    const auto& as_id = pInfo->GetMetaStructPointer()->accelerationStructure;
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
            // it will be inserted new AS function before ProcessCopyVulkanAccelerationStructuresMetaCommand
            SetDeleteCurrentCall();

            gfxrecon::encode::ParameterEncoder encoder(
                &acceleration_structure_build_infos_[as_id].get_address_parameter_buffer);
            encoder.EncodeHandleIdValue(device);
            encoder.EncodeStructPtrPreamble(pInfo->GetPointer());
            encoder.EncodeEnumValue(pInfo->GetPointer()->sType);
            encode::EncodePNextStruct(&encoder, pInfo->GetPointer()->pNext);
            encoder.EncodeHandleIdValue(as_id);
            encoder.EncodeHandleIdValue(returnValue);
        }
        return;
    }

    auto [it, inserted] =
        acceleration_structure_device_addresses_.try_emplace(returnValue, std::unordered_set<format::HandleId>{});
    it->second.insert(as_id);

    acceleration_structure_entries_[as_id].device_address = returnValue;

    if (acceleration_structure_entries_.find(as_id) != acceleration_structure_entries_.end())
    {
        if (buffer_device_addresses_.find(returnValue) != buffer_device_addresses_.end())
        {
            buffer_device_addresses_.erase(returnValue);
        }
    }
}

void VulkanRayTracingModifier::Process_vkGetRayTracingShaderGroupHandlesKHR(const ApiCallInfo&       call_info,
                                                                            VkResult                 returnValue,
                                                                            format::HandleId         device,
                                                                            format::HandleId         pipeline,
                                                                            uint32_t                 firstGroup,
                                                                            uint32_t                 groupCount,
                                                                            size_t                   dataSize,
                                                                            PointerDecoder<uint8_t>* pData)
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

    const auto single_entry_size = dataSize / groupCount;
    assert(single_entry_size == format::kMaxShaderGroupHandleSize);
    assert(pData != nullptr);

    for (int group = 0; group < groupCount; group++)
    {
        uint8_t* ptr = (uint8_t*)pData->GetPointer() + group * single_entry_size;

        std::vector<uint8_t> data_zero_handle(single_entry_size, 0);
        if (0 != memcmp(ptr, data_zero_handle.data(), single_entry_size))
        {
            format::ShaderHandleLocationInfo loc{};
            loc.id         = pipeline;
            loc.group      = firstGroup + group;
            loc.group_size = single_entry_size;
            memcpy(loc.original_handles, ptr, single_entry_size);
            shader_group_handle_entries_[pipeline][firstGroup + group] = loc;
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
            if (entry->second.creation_index > block_index_)
            {
                continue;
            }
            if (entry->second.destruction_index < block_index_)
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

    std::vector<format::ShaderHandleLocationInfo> locations;
    std::vector<uint8_t>                          data_zero_handle(single_shader_group_size, 0);
    uint8_t*                                      start = (uint8_t*)data;
    for (int i = 0; i < size / single_shader_group_size; i++)
    {
        uint8_t* ptr = start + i * single_shader_group_size;
        if (0 == memcmp(ptr, data_zero_handle.data(), single_shader_group_size))
        {
            continue;
        }

        for (auto& object : shader_group_handle_entries_)
        {
            for (auto& location : object.second)
            {
                if (0 == memcmp(ptr, location.second.original_handles, single_shader_group_size))
                {
                    location.second.offset_in_memory = (uint64_t)ptr - (uint64_t)start;
                    locations.push_back(location.second);
                    break;
                }
            }
        }
    }
    return locations;
}

void VulkanRayTracingModifier::WriteFixShaderGroupHandleCmd(format::HandleId                  relation_id,
                                                            uint64_t                          num_of_locations,
                                                            format::ShaderHandleLocationInfo* locations)
{
    auto new_call       = CreatePreCall();
    new_call->type      = NewCallDataType::MetaDataCall;
    new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
    new_call->thread_id = 1;

    format::FixShaderGroupHandleCommandHeader fix_cmd;
    fix_cmd.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
    fix_cmd.meta_header.block_header.size =
        format::GetMetaDataBlockBaseSize(fix_cmd) + (num_of_locations * sizeof(format::ShaderHandleLocationInfo));
    fix_cmd.meta_header.meta_data_id = format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan,
                                                              format::MetaDataType::kFixShaderGroupHandleCommand);
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

void VulkanRayTracingModifier::ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader& header,
                                                              const format::AddressLocationInfo*           infos)
{
    if (IsModificationPass())
    {
        // delete the old fixed meta command, it will be inserted new fixed meta command
        SetDeleteCurrentCall();
        return;
    }
}

void VulkanRayTracingModifier::ProcessFixShaderGroupHandleCommand(
    const format::FixShaderGroupHandleCommandHeader& header, const format::ShaderHandleLocationInfo* infos)
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

void VulkanRayTracingModifier::Process_vkCreateBuffer(const ApiCallInfo&                                   call_info,
                                                      VkResult                                             returnValue,
                                                      format::HandleId                                     device,
                                                      StructPointerDecoder<Decoded_VkBufferCreateInfo>*    pCreateInfo,
                                                      StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                                      HandlePointerDecoder<VkBuffer>*                      pBuffer)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle                   = *pBuffer->GetPointer();
    buffer_entries_[handle].handle            = handle;
    buffer_entries_[handle].size              = pCreateInfo->GetPointer()->size;
    buffer_entries_[handle].usage             = pCreateInfo->GetPointer()->usage;
    buffer_entries_[handle].flags             = pCreateInfo->GetPointer()->flags;
    buffer_entries_[handle].creation_index    = call_info.index;
    buffer_entries_[handle].destruction_index = UINT64_MAX;
}

void VulkanRayTracingModifier::Process_vkDestroyBuffer(const ApiCallInfo&                                   call_info,
                                                       format::HandleId                                     device,
                                                       format::HandleId                                     buffer,
                                                       StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator)
{
    if (IsModificationPass())
    {
        return;
    }

    if (buffer_entries_.find(buffer) != buffer_entries_.end())
    {
        buffer_entries_[buffer].destruction_index = call_info.index;
    }
}

void VulkanRayTracingModifier::Process_vkAllocateMemory(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    format::HandleId                                     device,
    StructPointerDecoder<Decoded_VkMemoryAllocateInfo>*  pAllocateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkDeviceMemory>*                pMemory)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle                           = *pMemory->GetPointer();
    memory_binding_entries_[handle].handle            = handle;
    memory_binding_entries_[handle].size              = pAllocateInfo->GetPointer()->allocationSize;
    memory_binding_entries_[handle].type_index        = pAllocateInfo->GetPointer()->memoryTypeIndex;
    memory_binding_entries_[handle].creation_index    = call_info.index;
    memory_binding_entries_[handle].destruction_index = UINT64_MAX;
    if (pAllocateInfo->GetPointer()->pNext)
    {
        VkMemoryAllocateFlagsInfo* info = (VkMemoryAllocateFlagsInfo*)(pAllocateInfo->GetPointer()->pNext);
        if (info->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO)
        {
            memory_binding_entries_[handle].flags = info->flags;
        }
    }
}

void VulkanRayTracingModifier::Process_vkFreeMemory(const ApiCallInfo&                                   call_info,
                                                    format::HandleId                                     device,
                                                    format::HandleId                                     memory,
                                                    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[memory].destruction_index = call_info.index;
    }
}

void VulkanRayTracingModifier::Process_vkBindBufferMemory(const ApiCallInfo& call_info,
                                                          VkResult           returnValue,
                                                          format::HandleId   device,
                                                          format::HandleId   buffer,
                                                          format::HandleId   memory,
                                                          VkDeviceSize       memory_offset)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[memory].memory_binding_records_.push_back({ buffer, true, memory_offset });
    }
}

void VulkanRayTracingModifier::Process_vkBindImageMemory(const ApiCallInfo& call_info,
                                                         VkResult           returnValue,
                                                         format::HandleId   device,
                                                         format::HandleId   image,
                                                         format::HandleId   memory,
                                                         VkDeviceSize       memory_offset)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[memory].memory_binding_records_.push_back({ image, false, memory_offset });
    }
}

void VulkanRayTracingModifier::Process_vkBindBufferMemory2(
    const ApiCallInfo&                                    call_info,
    VkResult                                              returnValue,
    format::HandleId                                      device,
    uint32_t                                              bindInfoCount,
    StructPointerDecoder<Decoded_VkBindBufferMemoryInfo>* pBindInfos)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkBindBufferMemoryInfo*         bind_infos      = pBindInfos->GetPointer();
    const Decoded_VkBindBufferMemoryInfo* bind_meta_infos = pBindInfos->GetMetaStructPointer();

    for (uint32_t i = 0; i < bindInfoCount; ++i)
    {
        if (memory_binding_entries_.find(bind_meta_infos[i].memory) != memory_binding_entries_.end())
        {
            memory_binding_entries_[bind_meta_infos[i].memory].memory_binding_records_.push_back(
                { bind_meta_infos[i].buffer, true, bind_infos[i].memoryOffset });
        }
    }
}

void VulkanRayTracingModifier::Process_vkBindImageMemory2(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    format::HandleId                                     device,
    uint32_t                                             bindInfoCount,
    StructPointerDecoder<Decoded_VkBindImageMemoryInfo>* pBindInfos)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkBindImageMemoryInfo*         bind_infos      = pBindInfos->GetPointer();
    const Decoded_VkBindImageMemoryInfo* bind_meta_infos = pBindInfos->GetMetaStructPointer();

    for (uint32_t i = 0; i < bindInfoCount; ++i)
    {
        if (memory_binding_entries_.find(bind_meta_infos[i].memory) != memory_binding_entries_.end())
        {
            memory_binding_entries_[bind_meta_infos[i].memory].memory_binding_records_.push_back(
                { bind_meta_infos[i].image, false, bind_infos[i].memoryOffset });
        }
    }
}

void VulkanRayTracingModifier::Process_vkCreateAccelerationStructureKHR(
    const ApiCallInfo&                                                  call_info,
    VkResult                                                            returnValue,
    format::HandleId                                                    device,
    StructPointerDecoder<Decoded_VkAccelerationStructureCreateInfoKHR>* pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>*                pAllocator,
    HandlePointerDecoder<VkAccelerationStructureKHR>*                   pAccelerationStructure)
{
    format::HandleId handle = *pAccelerationStructure->GetPointer();
    if (acceleration_structure_entries_.find(handle) == acceleration_structure_entries_.end())
    {
        acceleration_structure_entries_[handle].as_handle         = handle;
        acceleration_structure_entries_[handle].buf_handle        = pCreateInfo->GetMetaStructPointer()->buffer;
        acceleration_structure_entries_[handle].size              = pCreateInfo->GetPointer()->size;
        acceleration_structure_entries_[handle].offset            = pCreateInfo->GetPointer()->offset;
        acceleration_structure_entries_[handle].type              = pCreateInfo->GetPointer()->type;
        acceleration_structure_entries_[handle].bind_descriptor   = false;
        acceleration_structure_entries_[handle].creation_index    = call_info.index;
        acceleration_structure_entries_[handle].destruction_index = UINT64_MAX;
    }

    if (!IsModificationPass())
    {
        return;
    }

    if (options_.remove_rt)
    {
        SetDeleteCurrentCall();
        return;
    }

    assert(acceleration_structure_entries_.count(handle) > 0);
    if (acceleration_structure_build_infos_.find(handle) == acceleration_structure_build_infos_.end())
    {
        auto object = acceleration_structure_entries_[handle];
        if (object.bind_descriptor && object.type != VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
        {
            auto entry = std::find_if(
                acceleration_structure_entries_.begin(), acceleration_structure_entries_.end(), [&object](auto& entry) {
                    const auto& object_entry = entry.second;
                    return (object.buf_handle == object_entry.buf_handle) && (object.offset == object_entry.offset) &&
                           (object.size == object_entry.size) && (!object_entry.bind_descriptor) &&
                           (object_entry.type != VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
                });

            if (entry != acceleration_structure_entries_.end())
            {
                handle = entry->first;
            }
            else
            {
                return;
            }
        }
        else
        {
            return;
        }
    }

    assert(acceleration_structure_build_infos_.count(handle) > 0);
    if (acceleration_structure_build_infos_[handle].is_meta_copy &&
        block_index_ < acceleration_structure_build_infos_[handle].process_compacted_as_index)
    {
        // delete the compacted AS function,
        // it will be inserted new AS function before ProcessCopyVulkanAccelerationStructuresMetaCommand
        SetDeleteCurrentCall();

        gfxrecon::encode::ParameterEncoder encoder(
            &acceleration_structure_build_infos_[handle].create_parameter_buffer);
        encoder.EncodeHandleIdValue(device);
        encoder.EncodeStructPtrPreamble(pCreateInfo->GetPointer());
        encoder.EncodeEnumValue(pCreateInfo->GetPointer()->sType);
        encode::EncodePNextStruct(&encoder, pCreateInfo->GetPointer()->pNext);
        encoder.EncodeFlagsValue(pCreateInfo->GetPointer()->createFlags);
        encoder.EncodeHandleIdValue(pCreateInfo->GetMetaStructPointer()->buffer);
        encoder.EncodeUInt64Value(pCreateInfo->GetPointer()->offset);
        encoder.EncodeUInt64Value(pCreateInfo->GetPointer()->size);
        encoder.EncodeEnumValue(pCreateInfo->GetPointer()->type);
        encoder.EncodeUInt64Value(pCreateInfo->GetPointer()->deviceAddress);
        encode::EncodeStructPtr(&encoder, pAllocator->GetPointer());
        encoder.EncodeHandleIdPtr(pAccelerationStructure->GetPointer());
        encoder.EncodeEnumValue(returnValue);
        return;
    }

    if (acceleration_structure_build_infos_[handle].is_first_built)
    {
        VkAccelerationStructureBuildGeometryInfoKHR pBuildInfo =
            acceleration_structure_build_infos_[handle].build_infos;
        pBuildInfo.pGeometries = acceleration_structure_build_infos_[handle].geometry_infos.data();
        if (acceleration_structure_build_infos_[handle].omm_infos.size())
        {
            auto omm_info = acceleration_structure_build_infos_[handle].omm_infos.begin();
            for (; omm_info != acceleration_structure_build_infos_[handle].omm_infos.end(); omm_info++)
            {
                if (omm_info->second.usageCountsCount)
                {
                    omm_info->second.pUsageCounts =
                        acceleration_structure_build_infos_[handle].usage_infos[omm_info->first].data();
                }
                const_cast<VkAccelerationStructureGeometryKHR*>(pBuildInfo.pGeometries)[omm_info->first]
                    .geometry.triangles.pNext = (void*)&omm_info->second;
            }
        }

        uint32_t* max_primitive_counts = acceleration_structure_build_infos_[handle].primitive_counts.data();
        VkAccelerationStructureBuildSizesInfoKHR pSizeInfo{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0
        };

        auto new_call       = CreatePreCall();
        new_call->type      = NewCallDataType::ApiCall;
        new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_vkGetAccelerationStructureBuildSizesKHR;
        new_call->thread_id = 1;
        gfxrecon::encode::ParameterEncoder encoder(&new_call->parameter_buffer);
        encoder.EncodeHandleIdValue(device);
        encoder.EncodeEnumValue(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);
        encode::EncodeStructPtr(&encoder, &pBuildInfo);
        encoder.EncodeUInt32Array(max_primitive_counts, pBuildInfo.geometryCount);
        encode::EncodeStructPtr(&encoder, &pSizeInfo);
    }

    if (acceleration_structure_build_infos_[handle].source_of_compaction != format::kNullHandleId)
    {
        format::ParentToChildDependencyHeader header;

        header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
        header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(header) + sizeof(format::HandleId);
        header.meta_header.meta_data_id      = format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan,
                                                                 format::MetaDataType::kParentToChildDependency);
        header.thread_id                     = 1;
        header.dependency_type = format::ParentToChildDependencyType::kAccelerationStructureCompactionDependency;
        header.parent_id       = acceleration_structure_build_infos_[handle].source_of_compaction;
        header.child_count     = 1;

        auto new_call     = CreatePreCall();
        new_call->type    = util::CallModifierBase::NewCallDataType::MetaDataCall;
        new_call->call_id = gfxrecon::format::ApiCallId::ApiCall_Unknown;
        new_call->parameter_buffer.Write(&header, sizeof(header));
        new_call->parameter_buffer.Write(&handle, sizeof(format::HandleId));
    }
}

void VulkanRayTracingModifier::Process_vkGetAccelerationStructureBuildSizesKHR(
    const ApiCallInfo&                                                         call_info,
    format::HandleId                                                           device,
    VkAccelerationStructureBuildTypeKHR                                        buildType,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pBuildInfo,
    PointerDecoder<uint32_t>*                                                  pMaxPrimitiveCounts,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildSizesInfoKHR>*    pSizeInfo)
{
    if (IsModificationPass())
    {
        SetDeleteCurrentCall();
    }
}

void VulkanRayTracingModifier::Process_vkDestroyAccelerationStructureKHR(
    const ApiCallInfo&                                   call_info,
    format::HandleId                                     device,
    format::HandleId                                     accelerationStructure,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator)
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

    if (acceleration_structure_entries_.find(accelerationStructure) != acceleration_structure_entries_.end())
    {
        acceleration_structure_entries_[accelerationStructure].destruction_index = call_info.index;
    }
}

void VulkanRayTracingModifier::Process_vkCmdBuildAccelerationStructuresKHR(
    const ApiCallInfo&                                                         call_info,
    format::HandleId                                                           commandBuffer,
    uint32_t                                                                   infoCount,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   ppBuildRangeInfos)
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

    for (uint32_t info_index = 0; info_index < infoCount; ++info_index)
    {
        const auto& geometry_info      = pInfos->GetPointer()[info_index];
        const auto& build_range_info   = ppBuildRangeInfos->GetPointer()[info_index];
        const auto& geometry_meta_info = pInfos->GetMetaStructPointer()[info_index];
        const auto& mode               = geometry_info.mode;
        const auto& dst_as_id          = geometry_meta_info.dstAccelerationStructure;

        if (acceleration_structure_build_infos_.find(dst_as_id) != acceleration_structure_build_infos_.end())
        {
            bool is_replaced = false;
            for (uint32_t geometry_index = 0; geometry_index < geometry_info.geometryCount; ++geometry_index)
            {
                auto& geometry_data =
                    const_cast<VkAccelerationStructureGeometryKHR*>(geometry_info.pGeometries)[geometry_index];
                if (geometry_data.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
                {
                    continue;
                }

                if (geometry_info.geometryCount ==
                    acceleration_structure_build_infos_[dst_as_id].primitive_counts.size())
                {
                    uint32_t primitive_count =
                        acceleration_structure_build_infos_[dst_as_id].primitive_counts[geometry_index];
                    if (build_range_info[geometry_index].primitiveCount > primitive_count)
                    {
                        is_replaced = true;
                    }
                }
            }
            if (!is_replaced)
            {
                continue;
            }
        }

        acceleration_structure_build_infos_[dst_as_id].is_first_built             = true;
        acceleration_structure_build_infos_[dst_as_id].is_meta_copy               = false;
        acceleration_structure_build_infos_[dst_as_id].process_compacted_as_index = 0;
        acceleration_structure_build_infos_[dst_as_id].source_of_compaction       = format::kNullHandleId;
        acceleration_structure_build_infos_[dst_as_id].build_infos                = geometry_info;
        for (uint32_t geometry_index = 0; geometry_index < geometry_info.geometryCount; ++geometry_index)
        {
            auto& geometry_data =
                const_cast<VkAccelerationStructureGeometryKHR*>(geometry_info.pGeometries)[geometry_index];
            if (geometry_data.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
            {
                continue;
            }

            acceleration_structure_build_infos_[dst_as_id].geometry_infos.push_back(geometry_data);
            acceleration_structure_build_infos_[dst_as_id].primitive_counts.push_back(
                build_range_info[geometry_index].primitiveCount);
            switch (geometry_data.geometryType)
            {
                case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                {
                    const auto& triangles = geometry_data.geometry.triangles;
                    if (triangles.pNext)
                    {
                        VkAccelerationStructureTrianglesOpacityMicromapEXT* omm =
                            (VkAccelerationStructureTrianglesOpacityMicromapEXT*)(triangles.pNext);
                        if (omm->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT)
                        {
                            acceleration_structure_build_infos_[dst_as_id].omm_infos[geometry_index] = *omm;
                            for (uint32_t usage_index = 0; usage_index < omm->usageCountsCount; ++usage_index)
                            {
                                acceleration_structure_build_infos_[dst_as_id].usage_infos[geometry_index].push_back(
                                    omm->pUsageCounts[usage_index]);
                            }
                        }
                    }
                    break;
                }
                case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                {
                    auto result = buffer_device_addresses_.find(geometry_data.geometry.instances.data.deviceAddress);
                    if (result != buffer_device_addresses_.end())
                    {
                        instance_buffers_.insert(result->second);
                    }
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

void VulkanRayTracingModifier::Process_vkCmdCopyAccelerationStructureKHR(
    const ApiCallInfo&                                                call_info,
    format::HandleId                                                  commandBuffer,
    StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* pInfo)
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

    const auto& info = pInfo->GetMetaStructPointer();
    if (acceleration_structure_build_infos_.count(info->src) != 0)
    {
        if (acceleration_structure_build_infos_.count(info->dst) == 0)
        {
            if (info->decoded_value->mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR)
            {
                acceleration_structure_build_infos_[info->dst].build_infos                = {};
                acceleration_structure_build_infos_[info->dst].geometry_infos             = {};
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

void VulkanRayTracingModifier::ProcessBuildVulkanAccelerationStructuresMetaCommand(
    format::HandleId                                                           device_id,
    uint32_t                                                                   info_count,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* geometry_infos,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   range_infos,
    std::vector<std::vector<VkAccelerationStructureInstanceKHR>>&              instance_buffers_data)
{
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

    for (uint32_t info_index = 0; info_index < info_count; ++info_index)
    {
        const auto& geometry_info      = geometry_infos->GetPointer()[info_index];
        const auto& build_range_info   = range_infos->GetPointer()[info_index];
        const auto& geometry_meta_info = geometry_infos->GetMetaStructPointer()[info_index];
        const auto& mode               = geometry_info.mode;
        const auto& dst_as_id          = geometry_meta_info.dstAccelerationStructure;

        if (acceleration_structure_build_infos_.find(dst_as_id) != acceleration_structure_build_infos_.end())
        {
            bool is_replaced = false;
            for (uint32_t geometry_index = 0; geometry_index < geometry_info.geometryCount; ++geometry_index)
            {
                auto& geometry_data =
                    const_cast<VkAccelerationStructureGeometryKHR*>(geometry_info.pGeometries)[geometry_index];
                if (geometry_data.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
                {
                    continue;
                }

                if (geometry_info.geometryCount ==
                    acceleration_structure_build_infos_[dst_as_id].primitive_counts.size())
                {
                    uint32_t primitive_count =
                        acceleration_structure_build_infos_[dst_as_id].primitive_counts[geometry_index];
                    if (build_range_info[geometry_index].primitiveCount > primitive_count)
                    {
                        is_replaced = true;
                    }
                }
            }
            if (!is_replaced)
            {
                continue;
            }
        }

        acceleration_structure_build_infos_[dst_as_id].is_first_built             = true;
        acceleration_structure_build_infos_[dst_as_id].is_meta_copy               = false;
        acceleration_structure_build_infos_[dst_as_id].process_compacted_as_index = 0;
        acceleration_structure_build_infos_[dst_as_id].source_of_compaction       = format::kNullHandleId;
        acceleration_structure_build_infos_[dst_as_id].build_infos                = geometry_info;
        for (uint32_t geometry_index = 0; geometry_index < geometry_info.geometryCount; ++geometry_index)
        {
            auto& geometry_data =
                const_cast<VkAccelerationStructureGeometryKHR*>(geometry_info.pGeometries)[geometry_index];
            if (geometry_data.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
            {
                continue;
            }

            acceleration_structure_build_infos_[dst_as_id].geometry_infos.push_back(geometry_data);
            acceleration_structure_build_infos_[dst_as_id].primitive_counts.push_back(
                build_range_info[geometry_index].primitiveCount);
            switch (geometry_data.geometryType)
            {
                case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                {
                    const auto& triangles = geometry_data.geometry.triangles;
                    if (triangles.pNext)
                    {
                        VkAccelerationStructureTrianglesOpacityMicromapEXT* omm =
                            (VkAccelerationStructureTrianglesOpacityMicromapEXT*)(triangles.pNext);
                        if (omm->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT)
                        {
                            acceleration_structure_build_infos_[dst_as_id].omm_infos[geometry_index] = *omm;
                            for (uint32_t usage_index = 0; usage_index < omm->usageCountsCount; ++usage_index)
                            {
                                acceleration_structure_build_infos_[dst_as_id].usage_infos[geometry_index].push_back(
                                    omm->pUsageCounts[usage_index]);
                            }
                        }
                    }
                    break;
                }
                case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                {
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

void VulkanRayTracingModifier::ProcessCopyVulkanAccelerationStructuresMetaCommand(
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
                    acceleration_structure_build_infos_[dst_id].build_infos                = {};
                    acceleration_structure_build_infos_[dst_id].geometry_infos             = {};
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

void VulkanRayTracingModifier::Process_vkCreateComputePipelines(
    const ApiCallInfo&                                         call_info,
    VkResult                                                   returnValue,
    format::HandleId                                           device,
    format::HandleId                                           pipelineCache,
    uint32_t                                                   createInfoCount,
    StructPointerDecoder<Decoded_VkComputePipelineCreateInfo>* pCreateInfos,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>*       pAllocator,
    HandlePointerDecoder<VkPipeline>*                          pPipelines)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t i = 0; i < createInfoCount; i++)
    {
        format::HandleId handle = pPipelines->GetPointer()[i];
    }
}

void VulkanRayTracingModifier::Process_vkDestroyPipeline(
    const ApiCallInfo&                                   call_info,
    format::HandleId                                     device,
    format::HandleId                                     pipeline,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator)
{
    if (IsModificationPass())
    {
        return;
    }
}

void VulkanRayTracingModifier::Process_vkAllocateCommandBuffers(
    const ApiCallInfo&                                         call_info,
    VkResult                                                   returnValue,
    format::HandleId                                           device,
    StructPointerDecoder<Decoded_VkCommandBufferAllocateInfo>* pAllocateInfo,
    HandlePointerDecoder<VkCommandBuffer>*                     pCommandBuffers)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkCommandBufferAllocateInfo* allocate_info = pAllocateInfo->GetPointer();
    for (uint32_t i = 0; i < allocate_info->commandBufferCount; i++)
    {
        format::HandleId handle = pCommandBuffers->GetPointer()[i];
        command_buffer_entries_.try_emplace(handle, handle, device, allocate_info->level, 0, call_info.index);
    }
}

void VulkanRayTracingModifier::Process_vkBeginCommandBuffer(
    const ApiCallInfo&                                      call_info,
    VkResult                                                returnValue,
    format::HandleId                                        commandBuffer,
    StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkCommandBufferBeginInfo* begin_info       = pBeginInfo->GetPointer();
    command_buffer_entries_.at(commandBuffer).usage_ = begin_info->flags;
}

void VulkanRayTracingModifier::Process_vkCmdBindPipeline(const ApiCallInfo&  call_info,
                                                         format::HandleId    commandBuffer,
                                                         VkPipelineBindPoint pipelineBindPoint,
                                                         format::HandleId    pipeline)
{
    if (IsModificationPass())
    {
        return;
    }

    if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        command_buffers_with_compute_.insert(commandBuffer);
    }
}

void VulkanRayTracingModifier::Process_vkUpdateDescriptorSets(
    const ApiCallInfo&                                  call_info,
    format::HandleId                                    device,
    uint32_t                                            descriptorWriteCount,
    StructPointerDecoder<Decoded_VkWriteDescriptorSet>* pDescriptorWrites,
    uint32_t                                            descriptorCopyCount,
    StructPointerDecoder<Decoded_VkCopyDescriptorSet>*  pDescriptorCopies)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkWriteDescriptorSet* writes = pDescriptorWrites->GetPointer();
    for (uint32_t i = 0; i < descriptorWriteCount; i++)
    {
        const auto& write      = pDescriptorWrites->GetPointer()[i];
        const auto& meta_write = pDescriptorWrites->GetMetaStructPointer()[i];

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
        }
    }
}

void VulkanRayTracingModifier::Process_vkCmdBindDescriptorSets(const ApiCallInfo&  call_info,
                                                               format::HandleId    commandBuffer,
                                                               VkPipelineBindPoint pipelineBindPoint,
                                                               format::HandleId    layout,
                                                               uint32_t            firstSet,
                                                               uint32_t            descriptorSetCount,
                                                               HandlePointerDecoder<VkDescriptorSet>* pDescriptorSets,
                                                               uint32_t                  dynamicOffsetCount,
                                                               PointerDecoder<uint32_t>* pDynamicOffsets)
{
    if (IsModificationPass())
    {
        return;
    }

    if (pipelineBindPoint != VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        return;
    }

    for (uint32_t i = 0; i < descriptorSetCount; i++)
    {
        format::HandleId handle = pDescriptorSets->GetPointer()[i];
    }
}

void VulkanRayTracingModifier::Process_vkCmdCopyBuffer(const ApiCallInfo&                          call_info,
                                                       format::HandleId                            commandBuffer,
                                                       format::HandleId                            srcBuffer,
                                                       format::HandleId                            dstBuffer,
                                                       uint32_t                                    regionCount,
                                                       StructPointerDecoder<Decoded_VkBufferCopy>* pRegions)
{}

void VulkanRayTracingModifier::Process_vkCmdCopyBuffer2(
    const ApiCallInfo&                               call_info,
    format::HandleId                                 commandBuffer,
    StructPointerDecoder<Decoded_VkCopyBufferInfo2>* pCopyBufferInfo)
{}

void VulkanRayTracingModifier::Process_vkCmdCopyBuffer2KHR(
    const ApiCallInfo&                               call_info,
    format::HandleId                                 commandBuffer,
    StructPointerDecoder<Decoded_VkCopyBufferInfo2>* pCopyBufferInfo)
{}

void VulkanRayTracingModifier::Process_vkCmdExecuteCommands(const ApiCallInfo&                     call_info,
                                                            format::HandleId                       commandBuffer,
                                                            uint32_t                               commandBufferCount,
                                                            HandlePointerDecoder<VkCommandBuffer>* pCommandBuffers)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t i = 0; i < commandBufferCount; i++)
    {
        format::HandleId handle = pCommandBuffers->GetPointer()[i];
    }
}

void VulkanRayTracingModifier::Process_vkFreeCommandBuffers(const ApiCallInfo&                     call_info,
                                                            format::HandleId                       device,
                                                            format::HandleId                       commandPool,
                                                            uint32_t                               commandBufferCount,
                                                            HandlePointerDecoder<VkCommandBuffer>* pCommandBuffers)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t i = 0; i < commandBufferCount; i++)
    {
        format::HandleId handle                               = pCommandBuffers->GetPointer()[i];
        command_buffer_entries_.at(handle).destruction_index_ = call_info.index;
        command_buffers_with_compute_.erase(handle);
    }
}

void VulkanRayTracingModifier::Process_vkCmdUpdateBuffer(const ApiCallInfo&       call_info,
                                                         format::HandleId         commandBuffer,
                                                         format::HandleId         dstBuffer,
                                                         VkDeviceSize             dstOffset,
                                                         VkDeviceSize             dataSize,
                                                         PointerDecoder<uint8_t>* pData)
{
    if (!IsModificationPass())
    {
        return;
    }

    const CommandBufferInfo& command_buffer_info = command_buffer_entries_.at(commandBuffer);

    format::HandleId device_id = command_buffer_info.device_id_;
    auto             data      = pData->GetPointer();
    // Find shader group handle values inside data parameter, and generate FixShaderGroupHandleCommand meta block
    auto shader_handle_locations = GetShaderGroupHandlesInFillMemory(data, dataSize);
    if (shader_handle_locations.size())
    {
        WriteFixShaderGroupHandleCmd(device_id, shader_handle_locations.size(), shader_handle_locations.data());
    }

    // Find as and buffer device address values inside data parameter, and generate FixDeviceAddressCommand meta block
    std::vector<format::AddressLocationInfo> as_address_locations =
        GetAccelerationStructureDeviceAddressesInFillMemory(data, dataSize);
    std::vector<format::AddressLocationInfo> buffer_address_locations =
        GetBufferDeviceAddressesInFillMemory(as_address_locations, data, dataSize);
    if (as_address_locations.size() || buffer_address_locations.size())
    {
        WriteFixDeviceAddressCmd(device_id,
                                 as_address_locations.size(),
                                 as_address_locations.data(),
                                 buffer_address_locations.size(),
                                 buffer_address_locations.data());
    }
}

void VulkanRayTracingModifier::Process_vkCmdPushConstants(const ApiCallInfo&       call_info,
                                                          format::HandleId         commandBuffer,
                                                          format::HandleId         layout,
                                                          VkShaderStageFlags       stageFlags,
                                                          uint32_t                 offset,
                                                          uint32_t                 size,
                                                          PointerDecoder<uint8_t>* pValues)
{
    if (!IsModificationPass())
    {
        return;
    }
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

void VulkanRayTracingModifier::Process_vkQueueSubmit(const ApiCallInfo&                          call_info,
                                                     VkResult                                    returnValue,
                                                     format::HandleId                            queue,
                                                     uint32_t                                    submitCount,
                                                     StructPointerDecoder<Decoded_VkSubmitInfo>* pSubmits,
                                                     format::HandleId                            fence)
{
    if (IsModificationPass())
    {
        for (uint32_t info_index = 0; info_index < submitCount; info_index++)
        {
            const auto& submit_info      = pSubmits->GetPointer()[info_index];
            const auto& submit_meta_info = pSubmits->GetMetaStructPointer()[info_index];

            for (uint32_t cmd_buffer_index = 0; cmd_buffer_index < submit_info.commandBufferCount; cmd_buffer_index++)
            {

                const format::HandleId command_buffer = submit_meta_info.pCommandBuffers.GetPointer()[cmd_buffer_index];
                command_buffers_with_compute_.erase(command_buffer);
            }
        }
        return;
    }

    for (uint32_t info_index = 0; info_index < submitCount; info_index++)
    {
        const auto& submit_info      = pSubmits->GetPointer()[info_index];
        const auto& submit_meta_info = pSubmits->GetMetaStructPointer()[info_index];

        bool should_inspect = false;
        for (uint32_t cmd_buffer_index = 0; cmd_buffer_index < submit_info.commandBufferCount; cmd_buffer_index++)
        {

            const format::HandleId command_buffer = submit_meta_info.pCommandBuffers.GetPointer()[cmd_buffer_index];
            should_inspect                        = should_inspect || heuristic_check_compute(command_buffer);
            command_buffers_with_compute_.erase(command_buffer);
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

void VulkanRayTracingModifier::Process_vkQueueSubmit2(const ApiCallInfo&                           call_info,
                                                      VkResult                                     returnValue,
                                                      format::HandleId                             queue,
                                                      uint32_t                                     submitCount,
                                                      StructPointerDecoder<Decoded_VkSubmitInfo2>* pSubmits,
                                                      format::HandleId                             fence)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t info_index = 0; info_index < submitCount; info_index++)
    {
        auto& submit_info      = pSubmits->GetPointer()[info_index];
        auto& submit_meta_info = pSubmits->GetMetaStructPointer()[info_index];

        bool should_inspect = false;
        for (uint32_t cmd_buffer_index = 0; cmd_buffer_index < submit_info.commandBufferInfoCount; cmd_buffer_index++)
        {

            const format::HandleId command_buffer =
                submit_meta_info.pCommandBufferInfos->GetMetaStructPointer()->commandBuffer;
            should_inspect = should_inspect || heuristic_check_compute(command_buffer);
            command_buffers_with_compute_.erase(command_buffer);
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

void VulkanRayTracingModifier::Process_vkQueueSubmit2KHR(const ApiCallInfo&                           call_info,
                                                         VkResult                                     returnValue,
                                                         format::HandleId                             queue,
                                                         uint32_t                                     submitCount,
                                                         StructPointerDecoder<Decoded_VkSubmitInfo2>* pSubmits,
                                                         format::HandleId                             fence)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t info_index = 0; info_index < submitCount; info_index++)
    {
        auto& submit_info      = pSubmits->GetPointer()[info_index];
        auto& submit_meta_info = pSubmits->GetMetaStructPointer()[info_index];

        bool should_inspect = false;
        for (uint32_t cmd_buffer_index = 0; cmd_buffer_index < submit_info.commandBufferInfoCount; cmd_buffer_index++)
        {

            const format::HandleId command_buffer =
                submit_meta_info.pCommandBufferInfos->GetMetaStructPointer()->commandBuffer;
            should_inspect = should_inspect || heuristic_check_compute(command_buffer);
            command_buffers_with_compute_.erase(command_buffer);
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
    const ApiCallInfo&                                call_info,
    format::HandleId                                  commandBuffer,
    uint32_t                                          accelerationStructureCount,
    HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructures,
    VkQueryType                                       queryType,
    format::HandleId                                  queryPool,
    uint32_t                                          firstQuery)
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

bool VulkanRayTracingModifier::heuristic_check_compute(format::HandleId command_buffer)
{
    if (command_buffers_with_compute_.count(command_buffer) > 0)
    {
        return true;
    }
    return false;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
