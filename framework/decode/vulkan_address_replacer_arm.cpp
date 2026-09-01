/*
** Copyright (c) 2024 LunarG, Inc.
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

#include "decode/vulkan_address_replacer_arm.h"
#include "decode/vulkan_object_info.h"
#include "util/logging.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanAddressReplacerARM::VulkanAddressReplacerARM(const VulkanDeviceInfo*              device_info,
                                                   const decode::CommonObjectInfoTable& object_table) :
    device_info_(device_info), object_table_(&object_table)
{
    GFXRECON_ASSERT(device_info != nullptr)

    const VulkanPhysicalDeviceInfo* physical_device_info =
        object_table.GetVkPhysicalDeviceInfo(device_info_->parent_id);

    if (physical_device_info != nullptr)
    {
        GFXRECON_ASSERT(physical_device_info->replay_device_info != nullptr);
        GFXRECON_ASSERT(physical_device_info->replay_device_info->memory_properties.has_value());
        memory_properties_ = *physical_device_info->replay_device_info->memory_properties;
    }
}

void VulkanAddressReplacerARM::ProcessCmdTraceRays(
    const VulkanCommandBufferInfo*                                                              command_buffer_info,
    VkStridedDeviceAddressRegionKHR*                                                            raygen_sbt,
    VkStridedDeviceAddressRegionKHR*                                                            miss_sbt,
    VkStridedDeviceAddressRegionKHR*                                                            hit_sbt,
    VkStridedDeviceAddressRegionKHR*                                                            callable_sbt,
    const decode::VulkanDeviceAddressTracker&                                                   address_tracker,
    const std::unordered_map<graphics::shader_group_handle_t, graphics::shader_group_handle_t>& group_handle_map)
{
    // NOTE: we expect this map to be populated here, but not for older captures (before #1844) using trimming.
    if (group_handle_map.empty())
    {
        // the capture appears to be older and is missing information we require here -> bail out
        return;
    }

    // in-place remap: capture-addresses -> replay-addresses
    bool success = address_remap(raygen_sbt->deviceAddress, address_tracker);
    success      = success && address_remap(miss_sbt->deviceAddress, address_tracker);
    success      = success && address_remap(hit_sbt->deviceAddress, address_tracker);
    success      = success && address_remap(callable_sbt->deviceAddress, address_tracker);
    if (!success)
    {
        GFXRECON_LOG_WARNING_ONCE("VulkanAddressReplacerARM::ProcessCmdTraceRays: remap failed")
    }
}

bool VulkanAddressReplacerARM::ProcessMarkedOffsetARM(VkDeviceSize                              offset,
                                                      const void*                               pData,
                                                      const decode::VulkanDeviceAddressTracker& address_tracker)

{
    VkDeviceAddress& address = *((uint64_t*)((((uint8_t*)(pData)) + offset)));

    if (address == 0)
    {
        // This signals something going wrong
        GFXRECON_LOG_FATAL("Marked Offset points to an address that is 0")
        GFXRECON_ASSERT(false);
    }

    return address_remap(address, address_tracker);
}

void VulkanAddressReplacerARM::ProcessCmdBuildAccelerationStructuresKHR(
    const VulkanCommandBufferInfo*               command_buffer_info,
    uint32_t                                     info_count,
    VkAccelerationStructureBuildGeometryInfoKHR* build_geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**   build_range_infos,
    const VulkanDeviceAddressTracker&            address_tracker,
    bool                                         process_scratch_buffers)
{
    // TODO: testing only -> remove when closing issue #1526
    constexpr bool force_replace = false;

    for (uint32_t i = 0; i < info_count; ++i)
    {
        auto& build_geometry_info = build_geometry_infos[i];
        auto  range_info          = build_range_infos[i];

        // check/correct scratch-address
        if (process_scratch_buffers)
        {
            address_remap(build_geometry_info.scratchData.deviceAddress, address_tracker);
        }

        for (uint32_t j = 0; j < build_geometry_info.geometryCount; ++j)
        {
            auto geometry = const_cast<VkAccelerationStructureGeometryKHR*>(build_geometry_info.pGeometries != nullptr
                                                                                ? build_geometry_info.pGeometries + j
                                                                                : build_geometry_info.ppGeometries[j]);
            switch (geometry->geometryType)
            {
                case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                {
                    auto& triangles = geometry->geometry.triangles;
                    bool  success   = true;
                    success         = success && address_remap(triangles.vertexData.deviceAddress, address_tracker);
                    success         = success && address_remap(triangles.indexData.deviceAddress, address_tracker);
                    success         = success && address_remap(triangles.transformData.deviceAddress, address_tracker);
                    if (!success)
                    {
                        GFXRECON_LOG_WARNING_ONCE(
                            "ProcessCmdBuildAccelerationStructuresKHR: Address remap for TRIANGLES failed");
                    }
                    break;
                }
                case VK_GEOMETRY_TYPE_AABBS_KHR:
                {
                    auto& aabbs = geometry->geometry.aabbs;
                    if (!address_remap(aabbs.data.deviceAddress, address_tracker))
                    {
                        GFXRECON_LOG_WARNING_ONCE(
                            "ProcessCmdBuildAccelerationStructuresKHR: Address remap for AABBS failed");
                    }
                    break;
                }
                case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                {
                    auto& instances = geometry->geometry.instances;
                    // TODO: Support array of pointers
                    GFXRECON_ASSERT(instances.arrayOfPointers == false);
                    if (!address_remap(instances.data.deviceAddress, address_tracker))
                    {
                        GFXRECON_LOG_WARNING_ONCE(
                            "ProcessCmdBuildAccelerationStructuresKHR: Address remap for INSTANCES failed");
                    }
                    break;
                }
                default:
                    GFXRECON_LOG_ERROR(
                        "OverrideCmdBuildAccelerationStructuresKHR: unhandled case in switch-statement: %d",
                        geometry->geometryType);
                    break;
            }
        }
    }
}

void VulkanAddressReplacerARM::ProcessGetDescriptorEXT(const VulkanDeviceInfo*           device_info,
                                                       VkDescriptorGetInfoEXT*           descriptorInfo,
                                                       const VulkanDeviceAddressTracker& address_tracker)
{
    auto&                       descriptorData = descriptorInfo->data;
    VkDescriptorAddressInfoEXT* addressInfo;

    switch (descriptorInfo->type)
    {
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        {
            if (descriptorData.pUniformTexelBuffer == nullptr)
            {
                break;
            }
            addressInfo = const_cast<VkDescriptorAddressInfoEXT*>(descriptorData.pUniformTexelBuffer);
            if (!address_remap(addressInfo->address, address_tracker))
            {
                GFXRECON_LOG_WARNING_ONCE(
                    "VulkanAddressReplacer::ProcessGetDescriptorEXT: UNIFOR_TEXEL_BUFFER address remap failed");
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        {
            if (descriptorData.pStorageTexelBuffer == nullptr)
            {
                break;
            }
            addressInfo = const_cast<VkDescriptorAddressInfoEXT*>(descriptorData.pStorageTexelBuffer);
            if (!address_remap(addressInfo->address, address_tracker))
            {
                GFXRECON_LOG_WARNING_ONCE(
                    "VulkanAddressReplacer::ProcessGetDescriptorEXT: STORAGE_TEXEL_BUFFER remap failed");
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        {
            if (descriptorData.pUniformBuffer == nullptr)
            {
                break;
            }
            addressInfo = const_cast<VkDescriptorAddressInfoEXT*>(descriptorData.pUniformBuffer);
            if (!address_remap(addressInfo->address, address_tracker))
            {
                GFXRECON_LOG_WARNING_ONCE(
                    "VulkanAddressReplacer::ProcessGetDescriptorEXT: UNIFORM_BUFFER remap failed");
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        {
            if (descriptorData.pStorageBuffer == nullptr)
            {
                break;
            }
            addressInfo = const_cast<VkDescriptorAddressInfoEXT*>(descriptorData.pStorageBuffer);
            if (!address_remap(addressInfo->address, address_tracker))
            {
                GFXRECON_LOG_WARNING_ONCE(
                    "VulkanAddressReplacer::ProcessGetDescriptorEXT: STORAGE_BUFFER remap failed");
            }
            break;
        }
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        {
            if (!address_remap(descriptorData.accelerationStructure, address_tracker))
            {
                GFXRECON_LOG_WARNING_ONCE(
                    "VulkanAddressReplacer::ProcessGetDescriptorEXT: ACCELERATION_STRUCTURE remap failed");
            }
            break;
        }
        default:
            break;
    }
}

void VulkanAddressReplacerARM::ProcessCmdBindDescriptorBuffersEXT(const VulkanCommandBufferInfo*    commandBuffer_info,
                                                                  uint32_t                          bufferCount,
                                                                  VkDescriptorBufferBindingInfoEXT* bindingInfos,
                                                                  const VulkanDeviceAddressTracker& address_tracker)
{
    for (uint32_t i = 0; i < bufferCount; i++)
    {
        auto& bindingInfo = bindingInfos[i];
        if (!address_remap(bindingInfo.address, address_tracker))
        {
            GFXRECON_LOG_WARNING_ONCE("VulkanAddressReplacer::CmdBindDescriptorBuffersEXT: address remap failed");
        }
    }
}

void VulkanAddressReplacerARM::ProcessGeneratedCommandsInfoEXT(
    VkGeneratedCommandsInfoEXT* pGeneratedCommandsInfo, const decode::VulkanDeviceAddressTracker& address_tracker)
{
    GFXRECON_ASSERT(pGeneratedCommandsInfo != nullptr);

    if (!address_remap(pGeneratedCommandsInfo->indirectAddress, address_tracker))
    {
        GFXRECON_LOG_WARNING_ONCE(
            "VulkanAddressReplacer::ProcessGeneratedCommandsInfoEXT: indirectAddress remap failed");
    }

    if (!address_remap(pGeneratedCommandsInfo->preprocessAddress, address_tracker))
    {
        GFXRECON_LOG_WARNING_ONCE(
            "VulkanAddressReplacer::ProcessGeneratedCommandsInfoEXT: preprocessAddress remap failed");
    }

    if (!address_remap(pGeneratedCommandsInfo->sequenceCountAddress, address_tracker))
    {
        GFXRECON_LOG_WARNING_ONCE(
            "VulkanAddressReplacer::ProcessGeneratedCommandsInfoEXT: sequenceCountAddress remap failed");
    }
}

void VulkanAddressReplacerARM::ProcessSpecializationInfo(VkSpecializationInfo*             info,
                                                         const VulkanDeviceAddressTracker& address_tracker)
{
    if (info == nullptr || info->pData == nullptr || info->dataSize < sizeof(VkDeviceAddress))
    {
        return;
    }

    uint8_t* data = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(info->pData));
    for (size_t offset = 0; (offset + sizeof(VkDeviceAddress)) <= info->dataSize; ++offset)
    {
        address_remap(*reinterpret_cast<VkDeviceAddress*>(data + offset), address_tracker);
    }
}

bool VulkanAddressReplacerARM::address_remap(VkDeviceAddress&                  capture_address,
                                             const VulkanDeviceAddressTracker& address_tracker)
{
    if (capture_address == 0)
    {
        return true;
    }

    const VulkanBufferInfo* buffer_info = address_tracker.GetBufferByCaptureDeviceAddress(capture_address);

    if (buffer_info != nullptr && buffer_info->replay_address != 0)
    {
        uint64_t offset = capture_address - buffer_info->capture_address;
        // in-place address-remap via const-cast
        capture_address = buffer_info->replay_address + offset;
        return true;
    }

    return false;
}

GFXRECON_END_NAMESPACE(decode) GFXRECON_END_NAMESPACE(gfxrecon)
