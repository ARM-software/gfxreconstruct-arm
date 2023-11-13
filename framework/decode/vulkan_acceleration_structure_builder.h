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
#include "decode/vulkan_object_info.h"
#include "decode/descriptor_update_template_decoder.h"

#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

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

    void UpdateDescriptorSetWithTemplateKHR(gfxrecon::decode::DescriptorUpdateTemplateDecoder *pData);

    void CmdBuildAccelerationStructures(VkCommandBuffer                              commandBuffer,
                                        uint32_t                                     info_count,
                                        VkAccelerationStructureBuildGeometryInfoKHR* geometry_infos,
                                        VkAccelerationStructureBuildRangeInfoKHR**   range_infos);

    void SetBufferInfo(BufferInfo* buffer_info, VkDeviceAddress original_address, VkDeviceAddress new_address);
    void SetAccelerationStructureEntry(VkAccelerationStructureKHR acceleration_struct,
                                       VkDeviceAddress            original_address,
                                       VkDeviceAddress            new_address);
    void RegisterAccelerationStructure(VkAccelerationStructureKHR handle, VkDeviceAddress device_address);

  private:
    class AccelerationStructureEntry
    {
      public:
        VkDeviceAddress                             original_address_;
        VkDeviceAddress                             new_address_;
        VkAccelerationStructureKHR                  handle_;
        VkAccelerationStructureBuildSizesInfoKHR    size_info_;
        std::unique_ptr<AccelerationStructureEntry> replacement_acceleration_struct_;

        AccelerationStructureEntry(VkDeviceAddress                          original_address,
                                   VkDeviceAddress                          new_address,
                                   VkAccelerationStructureKHR               handle,
                                   VkAccelerationStructureBuildSizesInfoKHR size_info) :
            original_address_(original_address),
            new_address_(new_address), handle_(handle), size_info_(size_info)
        {}
    };

    struct BufferEntry
    {
        VkDeviceAddress original_address_;
        VkDeviceAddress new_address_;
        BufferInfo*     buffer_info_;

        BufferEntry(VkDeviceAddress original_address, VkDeviceAddress new_address, BufferInfo* buffer_info) :
            original_address_(original_address), new_address_(new_address), buffer_info_(buffer_info)
        {}
    };

    std::vector<std::unique_ptr<AccelerationStructureEntry>> acceleration_structures_;
    std::vector<std::unique_ptr<BufferEntry>>                buffers_;

    Functions                functions_;
    VkDevice                 device_;
    VulkanResourceAllocator* allocator_;

    VkBuffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage);

    VkDeviceOrHostAddressKHR GetBufferAddress(VkDeviceOrHostAddressKHR captured_address);
    AccelerationStructureEntry* GetAccelerationStructureEntry(VkAccelerationStructureKHR acceleration_struct);
    void UpdateAccelerationStructDeviceAddress(VkDeviceAddress& address);
    void UpdateBufferDeviceAddress(VkDeviceAddress& address);
    void UpdateDeviceAddress(VkAccelerationStructureBuildGeometryInfoKHR& build_geometry);
    BufferEntry* GetBufferByDeviceAddress(VkDeviceAddress runtime_address);
    VkDeviceAddress GetBufferDeviceAddress(VkBuffer buffer);
    VkDeviceAddress GetDeviceAddress(VkAccelerationStructureKHR acceleration_structure);

    VkAccelerationStructureKHR CreateAccelerationStructure(VkAccelerationStructureBuildGeometryInfoKHR& geometry_info,
                                                           VkAccelerationStructureBuildRangeInfoKHR*    range_info,
                                                           const VkAccelerationStructureBuildSizesInfoKHR& size_info);

    VkAccelerationStructureBuildSizesInfoKHR
    GetAccelerationStructureSizeInfo(VkAccelerationStructureBuildGeometryInfoKHR* geometry_info,
                                     VkAccelerationStructureBuildRangeInfoKHR*    range_info);


  void UpdateInstanceBuffer(VkAccelerationStructureGeometryInstancesDataKHR &instances);
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_ACCELERATION_STRUCTURE_BUILDER_H
