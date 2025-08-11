#include "vulkan_descriptor_buffer_modifier.h"

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

VulkanDescriptorBufferModifier::VulkanDescriptorBufferModifier(const VulkanOptimizationOptions& options) :
    options_(options){};

bool VulkanDescriptorBufferModifier::CanOptimize()
{
    return true;
}

std::vector<format::DescriptorDataLocationInfo> VulkanDescriptorBufferModifier::GetDescriptorsInFillMemory(
    uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data)
{
    std::vector<format::DescriptorDataLocationInfo> locations;

    auto entry = device_memory_descriptor_locations.find(memory_id);
    if (entry != device_memory_descriptor_locations.end())
    {
        uint64_t end_offset = offset + size;

        for (auto& loc_map : entry->second)
        {
            auto loc_info = loc_map.second.first;
            if (loc_info.descriptor_offset_in_mapped_memory >= offset &&
                (loc_info.descriptor_offset_in_mapped_memory + loc_info.orig_size) <= end_offset)
            {
                uint64_t descriptor_offset_in_memory = loc_info.descriptor_offset_in_mapped_memory - offset;
                void*    dest                        = (uint8_t*)(data + descriptor_offset_in_memory);
                // found out the loc_info to be a descriptor in this filled-Memory range
                if (util::platform::MemoryCompare(loc_map.second.second.data(), dest, loc_info.orig_size) == 0)
                {
                    loc_info.descriptor_offset_in_memory = descriptor_offset_in_memory;
                    locations.emplace_back(loc_info);
                    GFXRECON_LOG_DEBUG("Found descriptor in Filled Memory(%" PRIu64
                                       ", pageoffset 0x%lx) at relative offset 0x%lx.",
                                       memory_id,
                                       offset,
                                       descriptor_offset_in_memory);
                }
            }
        }
    }
    return locations;
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
    fix_cmd_header.meta_header.meta_data_id =
        format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kFixDescriptorDataCommand);
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
        GFXRECON_LOG_DEBUG("    is descriptor buffer %d.", desc_locations[i].is_descriptor_buffer);
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
    const format::FixDescriptorDataCommandHeader& header, const format::DescriptorDataLocationInfo* infos)
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

void VulkanDescriptorBufferModifier::Process_vkCreateBuffer(
    const ApiCallInfo&                                   call_info,
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

void VulkanDescriptorBufferModifier::Process_vkDestroyBuffer(
    const ApiCallInfo&                                   call_info,
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

void VulkanDescriptorBufferModifier::Process_vkAllocateMemory(
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

void VulkanDescriptorBufferModifier::Process_vkFreeMemory(
    const ApiCallInfo&                                   call_info,
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

void VulkanDescriptorBufferModifier::Process_vkMapMemory(const ApiCallInfo&               call_info,
                                                         VkResult                         returnValue,
                                                         format::HandleId                 device,
                                                         format::HandleId                 memory,
                                                         VkDeviceSize                     offset,
                                                         VkDeviceSize                     size,
                                                         VkMemoryMapFlags                 flags,
                                                         PointerDecoder<uint64_t, void*>* ppData)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        if (size == VK_WHOLE_SIZE)
        {
            assert(offset <= memory_binding_entries_[memory].size);
            size = memory_binding_entries_[memory].size - offset;
        }

        memory_binding_entries_[memory].mapping.offset        = offset;
        memory_binding_entries_[memory].mapping.size          = size;
        memory_binding_entries_[memory].mapping.map_memory    = *ppData->GetPointer();
        memory_binding_entries_[memory].mapping.shadow_memory = 0;
    }
}

void VulkanDescriptorBufferModifier::Process_vkMapMemory2(const ApiCallInfo&                             call_info,
                                                          VkResult                                       returnValue,
                                                          format::HandleId                               device,
                                                          StructPointerDecoder<Decoded_VkMemoryMapInfo>* pMemoryMapInfo,
                                                          PointerDecoder<uint64_t, void*>*               ppData)
{
    if (IsModificationPass())
    {
        return;
    }

    VkMemoryMapInfo*         map_info      = pMemoryMapInfo->GetPointer();
    Decoded_VkMemoryMapInfo* map_meta_info = pMemoryMapInfo->GetMetaStructPointer();

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
        memory_binding_entries_[memory].mapping.map_memory    = *ppData->GetPointer();
        memory_binding_entries_[memory].mapping.shadow_memory = 0;
    }
}

void VulkanDescriptorBufferModifier::Process_vkUnmapMemory(const ApiCallInfo& call_info,
                                                           format::HandleId   device,
                                                           format::HandleId   memory)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[memory].mapping.offset        = 0;
        memory_binding_entries_[memory].mapping.size          = 0;
        memory_binding_entries_[memory].mapping.map_memory    = 0;
        memory_binding_entries_[memory].mapping.shadow_memory = 0;
    }
}

void VulkanDescriptorBufferModifier::Process_vkUnmapMemory2(
    const ApiCallInfo&                               call_info,
    VkResult                                         returnValue,
    format::HandleId                                 device,
    StructPointerDecoder<Decoded_VkMemoryUnmapInfo>* pMemoryUnmapInfo)
{
    if (IsModificationPass())
    {
        return;
    }

    Decoded_VkMemoryUnmapInfo* unmap_meta_info = pMemoryUnmapInfo->GetMetaStructPointer();

    format::HandleId memory = unmap_meta_info->memory;
    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[memory].mapping.offset        = 0;
        memory_binding_entries_[memory].mapping.size          = 0;
        memory_binding_entries_[memory].mapping.map_memory    = 0;
        memory_binding_entries_[memory].mapping.shadow_memory = 0;
    }
}

void VulkanDescriptorBufferModifier::Process_vkBindBufferMemory(const ApiCallInfo& call_info,
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

void VulkanDescriptorBufferModifier::Process_vkBindBufferMemory2(
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

void VulkanDescriptorBufferModifier::Process_vkAllocateCommandBuffers(
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

void VulkanDescriptorBufferModifier::Process_vkBeginCommandBuffer(
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

void VulkanDescriptorBufferModifier::Process_vkFreeCommandBuffers(
    const ApiCallInfo&                     call_info,
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
    }
}

void VulkanDescriptorBufferModifier::Process_vkCmdCopyBuffer(const ApiCallInfo&                          call_info,
                                                             format::HandleId                            commandBuffer,
                                                             format::HandleId                            srcBuffer,
                                                             format::HandleId                            dstBuffer,
                                                             uint32_t                                    regionCount,
                                                             StructPointerDecoder<Decoded_VkBufferCopy>* pRegions)
{}

void VulkanDescriptorBufferModifier::Process_vkCmdCopyBuffer2(
    const ApiCallInfo&                               call_info,
    format::HandleId                                 commandBuffer,
    StructPointerDecoder<Decoded_VkCopyBufferInfo2>* pCopyBufferInfo)
{}

void VulkanDescriptorBufferModifier::Process_vkCmdCopyBuffer2KHR(
    const ApiCallInfo&                               call_info,
    format::HandleId                                 commandBuffer,
    StructPointerDecoder<Decoded_VkCopyBufferInfo2>* pCopyBufferInfo)
{}

void VulkanDescriptorBufferModifier::Process_vkCmdUpdateBuffer(const ApiCallInfo&       call_info,
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
    // reserved for descriptor buffer to WriteFixDescriptorDataCmd
}

void VulkanDescriptorBufferModifier::Process_vkCmdPushConstants(const ApiCallInfo&       call_info,
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
    // reserved for descriptor buffer to WriteFixDescriptorDataCmd
}

void VulkanDescriptorBufferModifier::Process_vkGetDescriptorEXT(
    const ApiCallInfo&                                    call_info,
    format::HandleId                                      device,
    StructPointerDecoder<Decoded_VkDescriptorGetInfoEXT>* pDescriptorInfo,
    size_t                                                dataSize,
    PointerDecoder<uint8_t>*                              pDescriptor)
{
    if (IsModificationPass())
    {
        return;
    }

    uint64_t desc_addr = pDescriptor->GetAddress();
    bool     found     = false;

    format::DescriptorDataLocationInfo location{};
    location.orig_size       = dataSize;
    location.new_size        = 0;
    location.descriptor_addr = desc_addr;

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
                        location.is_descriptor_buffer = ((buffer_entries_[binding.handle].usage &
                                                          VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) ==
                                                         VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);

                        auto it = device_memory_descriptor_locations.find(mem_id);
                        if (it != device_memory_descriptor_locations.end())
                        {
                            auto it2 = it->second.find(desc_addr);
                            if (it2 != it->second.end())
                            {
                                GFXRECON_LOG_INFO("Get descriptor into the same pointer address 0x%lx, erase it first.",
                                                  desc_addr);
                                it->second.erase(desc_addr);
                            }
                        }

                        // fill map
                        device_memory_descriptor_locations[mem_id][desc_addr] =
                            std::make_pair(location, std::vector<uint8_t>(dataSize));
                        util::platform::MemoryCopy(device_memory_descriptor_locations[mem_id][desc_addr].second.data(),
                                                   dataSize,
                                                   pDescriptor->GetPointer(),
                                                   dataSize);

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
