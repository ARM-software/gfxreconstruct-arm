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

#ifndef GFXRECON_DECODE_VULKAN_MICROMAP_EXT_BUILDER_H
#define GFXRECON_DECODE_VULKAN_MICROMAP_EXT_BUILDER_H

#include "decode/vulkan_resource_allocator.h"
#include "decode/descriptor_update_template_decoder.h"
#include "decode/vulkan_object_info_table.h"
#include "decode/vulkan_device_address_tracker.h"
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

// Important: The flow of the VulkanMicromapEXTBuilder assumes that the trace has been postprocessed. It may still work
// if that's not the case but there's no guarantee

// Note: scratches handling is a 1 to 1 copy of the handling from the AccelerationStructureBuilder. A way to reuse the
// code would be preferred

class VulkanMicromapEXTBuilder
{
    friend class VulkanMicromapEXTBuilderTestAccess;

  public:
    VulkanMicromapEXTBuilder(const graphics::VulkanDeviceTable*      device_table,
                             VkDevice                                device,
                             VulkanResourceAllocator*                allocator,
                             const VkPhysicalDeviceMemoryProperties& properties,
                             VulkanDeviceAddressTracker&             device_address_tracker);

    void OnGetMicromapEXTBuildSizes(const VulkanDeviceInfo*             device_info,
                                    VkAccelerationStructureBuildTypeKHR buildType,
                                    VkMicromapBuildInfoEXT*             info,
                                    VkMicromapBuildSizesInfoEXT*        size_info);

    VkResult OnCreateMicromapEXT(const VulkanDeviceInfo*      device_info,
                                 VkMicromapCreateInfoEXT*     info,
                                 const VkAllocationCallbacks* pAllocator,
                                 format::HandleId             capture_id,
                                 VkMicromapEXT*               handle);

    void
    OnCmdBuildMicromapsEXT(VkCommandBuffer command_buffer, uint32_t info_count, VkMicromapBuildInfoEXT* build_infos);

    void OnMicromapEXTCompactionDependencyCommand(VkMicromapEXT parent, const std::vector<format::HandleId>& children);

    void OnCmdWriteMicromapsEXTProperties(VkCommandBuffer command_buffer,
                                          uint32_t        count,
                                          VkMicromapEXT*  micromapsEXT,
                                          VkQueryType     query_type,
                                          VkQueryPool     pool,
                                          uint32_t        first_query);

    // called before command gets executed
    // the query pool results contain the EXT micromap compacted sizes
    // inject duplicate of this command that puts the results in internal buffer
    void OnCmdCopyQueryPoolResults(const VulkanCommandBufferInfo* command_buffer_info,
                                   const VulkanQueryPoolInfo*     query_pool_info);

    // called before command gets executed
    // inject duplicate of this command to retrieve compact sizes
    void OnGetQueryPoolResults(const VulkanDeviceInfo* device_info, const VulkanQueryPoolInfo* query_pool_info);

    void ProcessCapturedQueryPoolResults(const VulkanQueryPoolInfo* query_pool_info,
                                         uint32_t                   first_query,
                                         uint32_t                   query_count,
                                         const uint8_t*             data,
                                         size_t                     data_size,
                                         VkDeviceSize               stride,
                                         VkQueryResultFlags         flags);

    void OnDestroyBuffer(const VulkanBufferInfo* buffer_info);

    void OnDestroyMicromapEXT(const VulkanMicromapEXTInfo* micromap_ext_info);

  private:
    void UpdateDeviceAddress(VkMicromapBuildInfoEXT& build_info);
    void UpdateScratchDeviceAddress(VkMicromapBuildInfoEXT& build_infos, VkDeviceSize scratch_size);

    void InitializeFunctionPointers(const graphics::VulkanDeviceTable* device_table);
    struct Functions
    {
        PFN_vkCreateMicromapEXT        create_micromap_ext{ nullptr };
        PFN_vkGetMicromapBuildSizesEXT get_micromap_ext_build_sizes{ nullptr };
        PFN_vkGetQueryPoolResults      get_query_pool_results{ nullptr };
        PFN_vkCmdBuildMicromapsEXT     cmd_build_micromaps_ext{ nullptr };
        PFN_vkCmdCopyQueryPoolResults  cmd_copy_query_pool_results{ nullptr };
        PFN_vkCmdPipelineBarrier       cmd_pipeline_barrier{ nullptr };
    };

    VkDevice                    device_;
    VulkanResourceAllocator*    allocator_;
    Functions                   functions_;
    VulkanDeviceAddressTracker& device_address_tracker_;
    VulkanInternalBufferManager internal_buffer_manager_;

    struct MicromapEXTData
    {
        format::HandleId                                                capture_id;
        VkMicromapBuildSizesInfoEXT                                     info;
        std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> storage;
    };

    struct PreProcessingCompactionInfo
    {
        uint32_t                                                        first_query;
        std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> buffer_info_wrapper;
        std::vector<VkMicromapEXT>                                      parents;
    };

    // holds information gathered during vkCmdCopyQueryPoolResults that needs to be processed before
    // vkCreateMicromapEXT in order to know EXT micromap compressed sizes
    std::unordered_map<VkQueryPool, std::vector<PreProcessingCompactionInfo>> compacted_sizes_unprocessed_;
    // map containing relation between uncompacted EXT micromap capture id and the compacted EXT micromap size
    std::unordered_map<VkMicromapEXT, VkDeviceSize> compacted_sizes_processed_;

    std::unordered_map<format::HandleId, VkMicromapEXT> compaction_child_to_parent_dependency_;

    VkMicromapBuildSizesInfoEXT                        last_build_sizes_{};
    std::unordered_map<VkMicromapEXT, MicromapEXTData> micromaps_ext_;

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

#endif // GFXRECON_DECODE_VULKAN_MICROMAP_EXT_BUILDER_H
