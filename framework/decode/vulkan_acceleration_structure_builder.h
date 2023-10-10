/*
** Copyright (c) 2023 LunarG, Inc.
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

#ifndef GFXRECON_DECODE_VULKAN_ACCELERATION_STRUCTURE_BUILDER_H
#define GFXRECON_DECODE_VULKAN_ACCELERATION_STRUCTURE_BUILDER_H

#include "decode/vulkan_resource_allocator.h"
#include "util/defines.h"

#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanAccelerationStructureBuilder
{
  public:
    struct Functions
    {
        PFN_vkGetAccelerationStructureBuildSizesKHR    get_acceleration_structure_build_sizes{ nullptr };
        PFN_vkCreateAccelerationStructureKHR           create_acceleration_structure{ nullptr };
        PFN_vkGetBufferDeviceAddress                   get_buffer_device_address{ nullptr };
        PFN_vkCmdBuildAccelerationStructuresKHR        cmd_build_acceleration_structures{ nullptr };
        PFN_vkGetAccelerationStructureDeviceAddressKHR get_acceleration_structure_device_address{ nullptr };
    };

    VulkanAccelerationStructureBuilder(Functions functions, VkDevice device, VulkanResourceAllocator* allocator);

    void CmdBuildAccelerationStructures(VkCommandBuffer                              commandBuffer,
                                        uint32_t                                     info_count,
                                        VkAccelerationStructureBuildGeometryInfoKHR* geometry_infos,
                                        VkAccelerationStructureBuildRangeInfoKHR**   range_infos);

    void AddDeviceAddressPair(VkDeviceAddress capture, VkDeviceAddress replay);

  private:
    struct AccelerationStructureData
    {
        VkAccelerationStructureKHR handle;
        VkDeviceAddress            device_address;
    };
    std::vector<AccelerationStructureData>               acceleration_structures_;
    std::unordered_map<VkDeviceAddress, VkDeviceAddress> capture_replay_address_map_;

    Functions                functions_;
    VkDevice                 device_;
    VulkanResourceAllocator* allocator_;

    VkBuffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage);

    VkDeviceOrHostAddressKHR GetBufferAddress(VkDeviceOrHostAddressKHR captured_address);

    void UpdateDeviceAddress(VkDeviceAddress& address);
    void UpdateDeviceAddress(VkAccelerationStructureBuildGeometryInfoKHR& build_geometry);

    VkDeviceAddress GetDeviceAddress(VkBuffer buffer);
    VkDeviceAddress GetDeviceAddress(VkAccelerationStructureKHR acceleration_structure);

    VkAccelerationStructureKHR CreateAccelerationStructure(VkAccelerationStructureBuildGeometryInfoKHR& geometry_info,
                                                           VkAccelerationStructureBuildRangeInfoKHR*    range_info,
                                                           const VkAccelerationStructureBuildSizesInfoKHR& size_info);

    VkAccelerationStructureBuildSizesInfoKHR
    GetAccelerationStructureSizeInfo(VkAccelerationStructureBuildGeometryInfoKHR* geometry_info,
                                     VkAccelerationStructureBuildRangeInfoKHR*    range_info);
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_ACCELERATION_STRUCTURE_BUILDER_H
