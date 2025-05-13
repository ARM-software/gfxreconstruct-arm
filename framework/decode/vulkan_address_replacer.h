/*
** Copyright (c) 2024 LunarG, Inc.
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

#ifndef GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_H
#define GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_H

#include "util/linear_hashmap.h"
#include "decode/common_object_info_table.h"
#include "decode/vulkan_device_address_tracker.h"
#include "graphics/vulkan_shader_group_handle.h"
#include "format/platform_types.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

/**
 * @brief   VulkanAddressReplacer can be used to check and potentially sanitize input-parameters for various cases.
 *
 * Important note: all internal Vulkan-API calls performed by this class are expected to be wrapped by calls to:
 * - decode::BeginInjectedCommands() / decode::EndInjectedCommands()
 */
class VulkanAddressReplacer
{
  public:
    VulkanAddressReplacer() = default;

    VulkanAddressReplacer(const VulkanDeviceInfo*              device_info,
                          const encode::VulkanDeviceTable*     device_table,
                          const decode::CommonObjectInfoTable& object_table);

    //! prevent copying
    VulkanAddressReplacer(const VulkanAddressReplacer&) = delete;

    //! allow moving
    VulkanAddressReplacer(VulkanAddressReplacer&& other) noexcept;

    ~VulkanAddressReplacer();

    /**
     * @brief   ProcessCmdTraceRays will check and potentially correct input-parameters to 'VkCmdTraceRays',
     *          like buffer-device-addresses and shader-group-handles.
     *
     * Depending on capture- and replay-device-properties one of the following strategies will be used:
     *
     * if the shader-binding-table (SBT) layout is compatible and group-handles are also valid:
     * - happy day, nothing to do!
     *
     * if the shader-binding-table (SBT) layout is compatible, but group-handles are invalid:
     * - Apply in-place correction of group-handles contained in SBT
     *
     * if the shader-binding-table (SBT) layout is not compatible:
     * - Create a shadow-SBT matching the replay-device's layout, map/copy group-handles to that, adjust input-addresses
     *
     * @param command_buffer_info   a provided VulkanCommandBufferInfo
     * @param raygen_sbt            ray-generation sbt
     * @param miss_sbt              ray-miss sbt
     * @param hit_sbt               ray-hit sbt
     * @param callable_sbt          ray-callable sbt
     * @param address_tracker       const reference to a VulkanDeviceAddressTracker, used for mapping device-addresses
     * @param group_handle_map      a map from capture- to replay-time group-handles
     */
    void ProcessCmdTraceRays(
        const VulkanCommandBufferInfo*                                                              command_buffer_info,
        VkStridedDeviceAddressRegionKHR*                                                            raygen_sbt,
        VkStridedDeviceAddressRegionKHR*                                                            miss_sbt,
        VkStridedDeviceAddressRegionKHR*                                                            hit_sbt,
        VkStridedDeviceAddressRegionKHR*                                                            callable_sbt,
        const decode::VulkanDeviceAddressTracker&                                                   address_tracker,
        const std::unordered_map<graphics::shader_group_handle_t, graphics::shader_group_handle_t>& group_handle_map);

    /**
     * @brief   ProcessCmdBuildAccelerationStructuresKHR will check
     *          and potentially correct input-parameters to 'VkCmdBuildAccelerationStructuresKHR'
     *
     * @param command_buffer_info   a provided VulkanCommandBufferInfo
     * @param info_count            number of elements in 'build_geometry_infos'
     * @param build_geometry_infos  provided array of VkAccelerationStructureBuildGeometryInfoKHR
     * @param build_range_infos     provided array of VkAccelerationStructureBuildRangeInfoKHR*
     * @param address_tracker       const reference to a VulkanDeviceAddressTracker, used for mapping device-addresses
     */
    void ProcessCmdBuildAccelerationStructuresKHR(const VulkanCommandBufferInfo*               command_buffer_info,
                                                  uint32_t                                     info_count,
                                                  VkAccelerationStructureBuildGeometryInfoKHR* build_geometry_infos,
                                                  VkAccelerationStructureBuildRangeInfoKHR**   build_range_infos,
                                                  const decode::VulkanDeviceAddressTracker&    address_tracker,
                                                  bool                                         process_scratch_buffers);

    friend void swap(VulkanAddressReplacer& lhs, VulkanAddressReplacer& rhs) noexcept;

  private:
    struct buffer_context_t
    {
        decode::VulkanResourceAllocator*              resource_allocator = nullptr;
        uint32_t                                      num_bytes          = 0;
        VkDeviceMemory                                device_memory      = VK_NULL_HANDLE;
        VkBuffer                                      buffer             = VK_NULL_HANDLE;
        decode::VulkanResourceAllocator::ResourceData allocator_data{};
        decode::VulkanResourceAllocator::MemoryData   memory_data{};
        VkDeviceAddress                               device_address = 0;
        void*                                         mapped_data    = nullptr;
        ~buffer_context_t();
    };

    struct pipeline_context_t
    {
        buffer_context_t input_handle_buffer  = {};
        buffer_context_t output_handle_buffer = {};
        buffer_context_t hashmap_storage      = {};
    };

    struct acceleration_structure_asset_t
    {
        VkAccelerationStructureKHR handle  = VK_NULL_HANDLE;
        VkDeviceAddress            address = 0;
        buffer_context_t           storage = {};
        buffer_context_t           scratch = {};

        VkDevice                              device     = VK_NULL_HANDLE;
        PFN_vkDestroyAccelerationStructureKHR destroy_fn = nullptr;
        ~acceleration_structure_asset_t();
    };

    [[nodiscard]] bool init_pipeline();

    [[nodiscard]] bool create_buffer(size_t num_bytes, buffer_context_t& buffer_context, uint32_t usage_flags = 0);

    void barrier(VkCommandBuffer      command_buffer,
                 VkBuffer             buffer,
                 VkPipelineStageFlags src_stage,
                 VkAccessFlags        src_access,
                 VkPipelineStageFlags dst_stage,
                 VkAccessFlags        dst_access);

    const encode::VulkanDeviceTable*                               device_table_      = nullptr;
    const VulkanDeviceInfo*                                        device_info_       = nullptr;
    const decode::CommonObjectInfoTable*                           object_table_      = nullptr;
    VkPhysicalDeviceMemoryProperties                               memory_properties_ = {};
    std::optional<VkPhysicalDeviceRayTracingPipelinePropertiesKHR> capture_ray_properties_{}, replay_ray_properties_{};
    std::optional<VkPhysicalDeviceAccelerationStructurePropertiesKHR> replay_acceleration_structure_properties_{};
    bool                                                              valid_sbt_alignment_ = true;

    const decode::VulkanPhysicalDeviceInfo* physical_device_info_ = nullptr;
    VkDevice                                device_               = VK_NULL_HANDLE;
    decode::VulkanResourceAllocator*        resource_allocator_   = nullptr;

    // common layout used for all pipelines
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

    // pipeline dealing with shader-binding-table (SBT), replacing group-handles
    VkPipeline pipeline_sbt_ = VK_NULL_HANDLE;

    // pipeline dealing with buffer-device-addresses (BDA), replacing addresses
    VkPipeline pipeline_bda_ = VK_NULL_HANDLE;

    pipeline_context_t pipeline_context_sbt_;
    pipeline_context_t pipeline_context_bda_;

    // required assets for submitting meta-commands
    VkCommandPool   command_pool_   = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
    VkFence         fence_          = VK_NULL_HANDLE;
    VkQueue         queue_          = VK_NULL_HANDLE;
    VkQueryPool     query_pool_     = VK_NULL_HANDLE;

    util::linear_hashmap<graphics::shader_group_handle_t, graphics::shader_group_handle_t> hashmap_sbt_;
    util::linear_hashmap<VkDeviceAddress, VkDeviceAddress>                                 hashmap_bda_;
    std::unordered_map<VkCommandBuffer, buffer_context_t>                                  shadow_sbt_map_;

    // pipeline-contexts dealing with shader-binding-tables, per command-buffer
    std::unordered_map<VkCommandBuffer, pipeline_context_t> pipeline_sbt_context_map_;

    // resources related to acceleration-structures
    std::unordered_map<VkAccelerationStructureKHR, acceleration_structure_asset_t> shadow_as_map_;

    // pipeline-contexts dealing with acceleration-structure builds, per command-buffer
    std::unordered_map<VkCommandBuffer, pipeline_context_t> build_as_context_map_;

    // currently running compaction queries. pool -> AS -> query-pool-index
    std::unordered_map<VkQueryPool, std::unordered_map<VkAccelerationStructureKHR, uint32_t>> as_compact_queries_;
    std::unordered_map<VkAccelerationStructureKHR, VkDeviceSize>                              as_compact_sizes_;

    // required function pointers
    PFN_vkGetBufferDeviceAddress       get_device_address_fn_             = nullptr;
    PFN_vkGetPhysicalDeviceProperties2 get_physical_device_properties_fn_ = nullptr;
};
GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_H
