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

#ifndef GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_ARM_H
#define GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_ARM_H

#include "util/linear_hashmap.h"
#include "decode/common_object_info_table.h"
#include "decode/vulkan_device_address_tracker.h"
#include "graphics/vulkan_shader_group_handle.h"
#include "decode/vulkan_address_replacer_base.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

/**
 * @brief   VulkanAddressReplacerARM can be used to check and potentially sanitize input-parameters for various cases.
 *
 * Important note: all internal Vulkan-API calls performed by this class are expected to be wrapped by calls to:
 * - decode::BeginInjectedCommands() / decode::EndInjectedCommands()
 */
class VulkanAddressReplacerARM : public VulkanAddressReplacerBase
{
  public:
    VulkanAddressReplacerARM()  = default;
    ~VulkanAddressReplacerARM() = default;

    VulkanAddressReplacerARM(const VulkanDeviceInfo*              device_info,
                             const decode::CommonObjectInfoTable& object_table);

    //! prevent copying
    VulkanAddressReplacerARM(const VulkanAddressReplacerARM&) = delete;

    //! prevent moving
    VulkanAddressReplacerARM(VulkanAddressReplacerARM&&) = delete;

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
    void ProcessCmdTraceRays(const VulkanCommandBufferInfo*            command_buffer_info,
                             VkStridedDeviceAddressRegionKHR*          raygen_sbt,
                             VkStridedDeviceAddressRegionKHR*          miss_sbt,
                             VkStridedDeviceAddressRegionKHR*          hit_sbt,
                             VkStridedDeviceAddressRegionKHR*          callable_sbt,
                             const decode::VulkanDeviceAddressTracker& address_tracker,
                             const std::unordered_map<graphics::shader_group_handle_t, graphics::shader_group_handle_t>&
                                 group_handle_map) override;

    bool ProcessMarkedOffsetARM(VkDeviceSize                              offset,
                                const void*                               pData,
                                const decode::VulkanDeviceAddressTracker& address_tracker) override;

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
                                                  bool process_scratch_buffers) override;

    void ProcessGetDescriptorEXT(const VulkanDeviceInfo*           device_info,
                                 VkDescriptorGetInfoEXT*           descriptorInfo,
                                 const VulkanDeviceAddressTracker& address_tracker) override;

    void ProcessCmdBindDescriptorBuffersEXT(const VulkanCommandBufferInfo*    commandBuffer_info,
                                            uint32_t                          bufferCount,
                                            VkDescriptorBufferBindingInfoEXT* bindingInfos,
                                            const VulkanDeviceAddressTracker& address_tracker) override;

    void ProcessGeneratedCommandsInfoEXT(VkGeneratedCommandsInfoEXT*               pGeneratedCommandsInfo,
                                         const decode::VulkanDeviceAddressTracker& address_tracker) override;

    void ProcessSpecializationInfo(VkSpecializationInfo*             info,
                                   const VulkanDeviceAddressTracker& address_tracker) override;

  private:
    const VulkanDeviceInfo*              device_info_       = nullptr;
    const decode::CommonObjectInfoTable* object_table_      = nullptr;
    VkPhysicalDeviceMemoryProperties     memory_properties_ = {};

    const decode::VulkanPhysicalDeviceInfo* physical_device_info_ = nullptr;
    VkDevice                                device_               = VK_NULL_HANDLE;
    decode::VulkanResourceAllocator*        resource_allocator_   = nullptr;

  private:
    bool address_remap(VkDeviceAddress& capture_address, const VulkanDeviceAddressTracker& address_tracker);
};
GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_ARM_H
