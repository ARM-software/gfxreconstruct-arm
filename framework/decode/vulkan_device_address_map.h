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

#ifndef GFXRECON_DECODE_VULKAN_DEVICE_ADDRESS_MAP_H
#define GFXRECON_DECODE_VULKAN_DEVICE_ADDRESS_MAP_H

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

class VulkanDeviceAddressMap
{
  public:
    void RegisterBuffer(VkDeviceAddress original_address, VkDeviceSize size, BufferInfo* object_info);
    void RegisterAccelerationStructure(VkDeviceAddress original_address, VkDeviceSize size, AccelerationStructureKHRInfo* object_info);

    BufferInfo* GetBuffer(VkDeviceAddress original_address);
    AccelerationStructureKHRInfo* GetAccelerationStructure(VkDeviceAddress original_address);
 
  private:

   struct DeviceAddressEntry
    {
        VkDeviceAddress original_address;
        VkDeviceAddress new_address;
    };

    struct AccelerationStructureEntry : DeviceAddressEntry
    {
        VkAccelerationStructureKHR original_acceleration_struct;
        VkAccelerationStructureKHR new_acceleration_struct;
        VkAccelerationStructureBuildSizesInfoKHR size_info;
        AccelerationStructureKHRInfo* object_info;
    };

    struct BufferEntry : DeviceAddressEntry
    {
        BufferInfo* buffer_info;
    };

    std::vector<AccelerationStructureEntry> acceleration_structures_;
    std::vector<BufferEntry> buffers_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_ACCELERATION_STRUCTURE_BUILDER_H
