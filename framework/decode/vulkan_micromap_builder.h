/*
** Copyright (c) 2024 LunarG, Inc.
** Copyright (c) 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#ifndef GFXRECON_DECODE_VULKAN_MICROMAP_BUILDER_H
#define GFXRECON_DECODE_VULKAN_MICROMAP_BUILDER_H

#include "decode/vulkan_resource_allocator.h"
#include "decode/descriptor_update_template_decoder.h"
#include "decode/vulkan_object_info_table.h"
#include "decode/vulkan_buffer_tracker.h"
#include "decode/vulkan_internal_buffer_manager.h"
#include "util/defines.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

// Important: The flow of the VulkanMicromapBuilder assumes that the trace has been postprocessed. It may still work if
// that's not the case but there's no guarantee

// Note: scratches handling is a 1 to 1 copy of the handling from the AccelerationStructureBuilder. A way to reuse the
// code would be preferred

class VulkanMicromapBuilder
{
  public:
    VulkanMicromapBuilder(const encode::VulkanDeviceTable*        device_table,
                          const PhysicalDeviceInfo*               physical_device_info,
                          VkDevice                                device,
                          VulkanResourceAllocator*                allocator,
                          const VkPhysicalDeviceMemoryProperties& properties,
                          VulkanBufferTracker*                    buffer_tracker);

    void OnGetMicromapBuildSizes(const DeviceInfo*                   device_info,
                                 VkAccelerationStructureBuildTypeKHR buildType,
                                 VkMicromapBuildInfoEXT*             info,
                                 VkMicromapBuildSizesInfoEXT*        size_info);

    VkResult OnCreateMicromap(const DeviceInfo*            device_info,
                              VkMicromapCreateInfoEXT*     info,
                              const VkAllocationCallbacks* pAllocator,
                              VkMicromapEXT*               handle);

    void OnCmdBuildMicromaps(VkCommandBuffer command_buffer, uint32_t info_count, VkMicromapBuildInfoEXT* build_infos);

    static void OnCmdBuildAccStrHandling(VulkanBufferTracker*                         buffer_tracker,
                                         uint32_t                                     info_count,
                                         VkAccelerationStructureBuildGeometryInfoKHR* infos);

    void OnDestroyBuffer(const BufferInfo* buffer_info);

    void OnDestroyMicromap(const MicromapEXTInfo* micromap_info);

  private:
    void UpdateDeviceAddress(VkMicromapBuildInfoEXT& build_info);
    void UpdateScratchDeviceAddress(VkMicromapBuildInfoEXT& build_infos, VkDeviceSize scratch_size);

    void InitializeFunctionPointers(const encode::VulkanDeviceTable* device_table);
    struct Functions
    {
        PFN_vkGetMicromapBuildSizesEXT get_micromap_build_sizes{ nullptr };
        PFN_vkCreateMicromapEXT        create_micromap{ nullptr };
        PFN_vkCmdBuildMicromapsEXT     cmd_build_micromaps{ nullptr };
    };

    Functions                   functions_;
    VulkanBufferTracker*        buffer_tracker_;
    VulkanInternalBufferManager internal_buffer_manager_;

    struct MicromapData
    {
        VkMicromapBuildSizesInfoEXT                                     info;
        std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> storage;
    };

    VkMicromapBuildSizesInfoEXT                     last_build_sizes_;
    std::unordered_map<VkMicromapEXT, MicromapData> micromaps_;

    struct DoubleBufferScratch
    {
        std::unordered_map<format::HandleId,
                           std::vector<std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>>>
            scratches_previous;
        std::unordered_map<format::HandleId,
                           std::vector<std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>>>
            scratches_current;
    } scratch_double_buffer_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_MICROMAP_BUILDER_H
