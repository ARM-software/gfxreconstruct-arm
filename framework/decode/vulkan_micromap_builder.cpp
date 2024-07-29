#include "graphics/vulkan_resources_util.h"
#include "decode/vulkan_micromap_builder.h"
#include "util/marking_layers.h"
#include <algorithm>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanMicromapBuilder::VulkanMicromapBuilder(const encode::VulkanDeviceTable*        device_table,
                                             const PhysicalDeviceInfo*               physical_device_info,
                                             VkDevice                                device,
                                             VulkanResourceAllocator*                allocator,
                                             const VkPhysicalDeviceMemoryProperties& properties,
                                             VulkanBufferTracker*                    buffer_tracker) :
    buffer_tracker_(buffer_tracker),
    internal_buffer_manager_(device_table, physical_device_info, device, allocator, properties)
{
    InitializeFunctionPointers(device_table);
}

void VulkanMicromapBuilder::OnGetMicromapBuildSizes(const DeviceInfo*                   device_info,
                                                    VkAccelerationStructureBuildTypeKHR buildType,
                                                    VkMicromapBuildInfoEXT*             info,
                                                    VkMicromapBuildSizesInfoEXT*        size_info)
{
    buffer_tracker_->UpdateBufferDeviceAddress(info->data.deviceAddress);
    buffer_tracker_->UpdateBufferDeviceAddress(info->triangleArray.deviceAddress);
    functions_.get_micromap_build_sizes(device_info->handle, buildType, info, size_info);
    last_build_sizes_ = *size_info;
}

VkResult VulkanMicromapBuilder::OnCreateMicromap(const DeviceInfo*            device_info,
                                                 VkMicromapCreateInfoEXT*     info,
                                                 const VkAllocationCallbacks* pAllocator,
                                                 VkMicromapEXT*               handle)
{
    // Create new storage buffer for Micromap based on previously recorded vkGetMicromapBuildSizesEXT (this call must be
    // inserted by gfxrecon-optimize)
    auto allocator = device_info->allocator.get();
    assert(allocator != nullptr);

    std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> bufferInfoWrapper =
        internal_buffer_manager_.CreateBuffer(last_build_sizes_.micromapSize, 0x01020000);

    info->size   = allocator->GetBufferSize(bufferInfoWrapper->info_.allocator_data);
    info->buffer = bufferInfoWrapper->info_.handle;
    info->offset = 0;

    VkResult result = functions_.create_micromap(device_info->handle, info, pAllocator, handle);
    assert(result == VK_SUCCESS);

    micromaps_[*handle] = { last_build_sizes_, std::move(bufferInfoWrapper) };
    return result;
}

void VulkanMicromapBuilder::OnCmdBuildMicromaps(VkCommandBuffer         command_buffer,
                                                uint32_t                info_count,
                                                VkMicromapBuildInfoEXT* build_infos)
{
    for (uint32_t i = 0; i < info_count; ++i)
    {
        assert(build_infos[i].type == VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT);

        if (micromaps_.count(build_infos[i].dstMicromap) == 0)
        {
            GFXRECON_LOG_FATAL("Handle %ul for vkCmdBuildMicromapsEXT not found", build_infos[i].dstMicromap);
            GFXRECON_ASSERT(false);
        }

        VkDeviceSize scratch_size = micromaps_[build_infos[i].dstMicromap].info.buildScratchSize;

        UpdateScratchDeviceAddress(build_infos[i], scratch_size);
        UpdateDeviceAddress(build_infos[i]);
    }
    functions_.cmd_build_micromaps(command_buffer, info_count, build_infos);
}

void VulkanMicromapBuilder::UpdateScratchDeviceAddress(VkMicromapBuildInfoEXT& build_infos, VkDeviceSize scratch_size)
{
    VkDeviceAddress capture_scratch_address = build_infos.scratchData.deviceAddress;

    format::HandleId capture_id = format::kNullHandleId;
    // When fastforwarding, the scratch buffers could be destroyed and not be recreated in state recreation
    BufferInfo* original_scratch_entry = buffer_tracker_->GetBufferByCaptureDeviceAddress(capture_scratch_address);
    if (original_scratch_entry)
    {
        capture_id = original_scratch_entry->capture_id;
    }

    auto scratch_entries = scratch_double_buffer_.scratches_current.find(capture_id);

    if (scratch_entries != scratch_double_buffer_.scratches_current.end())
    {
        auto scratch_entry =
            std::find_if(scratch_entries->second.begin(),
                         scratch_entries->second.end(),
                         [scratch_size, capture_scratch_address](
                             const std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>& entry) {
                             return entry->allocator_->GetBufferSize(entry->info_.allocator_data) >= scratch_size &&
                                    entry->info_.capture_address == capture_scratch_address;
                         });
        if (scratch_entry != scratch_entries->second.end())
        {
            build_infos.scratchData.deviceAddress = (*scratch_entry)->info_.replay_address;
        }
        else
        {
            const auto& new_scratch = scratch_entries->second.emplace_back(internal_buffer_manager_.CreateBuffer(
                scratch_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT));
            new_scratch->info_.capture_address    = build_infos.scratchData.deviceAddress;
            build_infos.scratchData.deviceAddress = new_scratch->info_.replay_address;
        }
    }
    else
    {
        auto [it, inserted] = scratch_double_buffer_.scratches_current.emplace(
            capture_id, std::vector<std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>>());
        auto& new_scratch                     = it->second.emplace_back(internal_buffer_manager_.CreateBuffer(
            scratch_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT));
        new_scratch->info_.capture_address    = build_infos.scratchData.deviceAddress;
        build_infos.scratchData.deviceAddress = new_scratch->info_.replay_address;
    }
}

void VulkanMicromapBuilder::UpdateDeviceAddress(VkMicromapBuildInfoEXT& build_info)
{
    auto& data          = build_info.data.deviceAddress;
    auto& triangleArray = build_info.triangleArray.deviceAddress;
    buffer_tracker_->UpdateBufferDeviceAddress(data);
    buffer_tracker_->UpdateBufferDeviceAddress(triangleArray);
}

void VulkanMicromapBuilder::OnDestroyBuffer(const BufferInfo* buffer_info)
{
    scratch_double_buffer_.scratches_previous.erase(buffer_info->capture_id);
    scratch_double_buffer_.scratches_current.erase(buffer_info->capture_id);
}

void VulkanMicromapBuilder::OnCmdBuildAccStrHandling(VulkanBufferTracker*                         buffer_tracker,
                                                     uint32_t                                     info_count,
                                                     VkAccelerationStructureBuildGeometryInfoKHR* infos)
{
    // replace indexBuffer address
    for (uint64_t i = 0; i < info_count; i++)
    {
        uint32_t geometry_count = infos[i].geometryCount;

        VkAccelerationStructureGeometryKHR*  pGeometries  = (VkAccelerationStructureGeometryKHR*)infos[i].pGeometries;
        VkAccelerationStructureGeometryKHR** ppGeometries = (VkAccelerationStructureGeometryKHR**)infos[i].ppGeometries;

        if ((infos[i].type != VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR) || (geometry_count == 0))
        {
            continue;
        }

        // If type is VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR then the geometryType member of each geometry in
        // either pGeometries or ppGeometries must be the same
        if ((pGeometries != nullptr && pGeometries->geometryType != VK_GEOMETRY_TYPE_TRIANGLES_KHR) ||
            (ppGeometries != nullptr && (*ppGeometries)->geometryType != VK_GEOMETRY_TYPE_TRIANGLES_KHR))
        {
            continue;
        }

        for (uint64_t j = 0; j < geometry_count; j++)
        {
            VkBaseOutStructure* pNextStruct = nullptr;
            if (pGeometries != nullptr)
            {
                pNextStruct = (VkBaseOutStructure*)(pGeometries[j].geometry.triangles.pNext);
            }
            else // ppGeometries != nullptr
            {
                pNextStruct = (VkBaseOutStructure*)(ppGeometries[j]->geometry.triangles.pNext);
            }

            while (pNextStruct != nullptr)
            {
                if (pNextStruct->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT)
                {
                    VkAccelerationStructureTrianglesOpacityMicromapEXT* micromap_struct =
                        (VkAccelerationStructureTrianglesOpacityMicromapEXT*)pNextStruct;
                    buffer_tracker->UpdateBufferDeviceAddress(micromap_struct->indexBuffer.deviceAddress);
                }
                pNextStruct = pNextStruct->pNext;
            }
        }
    }
}

void VulkanMicromapBuilder::OnDestroyMicromap(const MicromapEXTInfo* micromap_info)
{
    micromaps_.erase(micromap_info->handle);
}

void VulkanMicromapBuilder::InitializeFunctionPointers(const encode::VulkanDeviceTable* device_table)
{
    functions_.get_micromap_build_sizes = device_table->GetMicromapBuildSizesEXT;
    functions_.create_micromap          = device_table->CreateMicromapEXT;
    functions_.cmd_build_micromaps      = device_table->CmdBuildMicromapsEXT;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)