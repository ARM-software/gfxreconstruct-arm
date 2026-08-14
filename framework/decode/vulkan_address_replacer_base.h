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

#ifndef GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_BASE_H
#define GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_BASE_H

#include <span>

#include "util/linear_hashmap.h"
#include "decode/common_object_info_table.h"
#include "decode/vulkan_device_address_tracker.h"
#include "graphics/vulkan_semaphore_util.h"
#include "graphics/vulkan_shader_group_handle.h"
#include "util/vulkan_device_table_dispatcher.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanAddressReplacerBase
{
  public:
    VulkanAddressReplacerBase() = default;

    VulkanAddressReplacerBase(const VulkanDeviceInfo*              device_info,
                              const graphics::VulkanDeviceTable*   device_table,
                              const graphics::VulkanInstanceTable* instance_table,
                              const decode::CommonObjectInfoTable& object_table);

    VulkanAddressReplacerBase(const VulkanAddressReplacerBase&) = delete;

    VulkanAddressReplacerBase(VulkanAddressReplacerBase&& other) noexcept;

    virtual ~VulkanAddressReplacerBase() {}

    virtual void SetRaytracingProperties(const decode::VulkanPhysicalDeviceInfo* physical_device_info) {}

    virtual graphics::VulkanSemaphore
    UpdateBufferAddresses(const VulkanCommandBufferInfo*             command_buffer_info,
                          const std::span<VkDeviceAddress>           addresses_to_replace,
                          const decode::VulkanDeviceAddressTracker&  address_tracker,
                          const std::span<graphics::VulkanSemaphore> wait_semaphores = {})
    {
        return {};
    }

    virtual std::pair<std::vector<VkDeviceAddress>, const VulkanCommandBufferInfo*>
    ResolveBufferAddresses(Decoded_VkSubmitInfo& submit_info, const VulkanDeviceAddressTracker& address_tracker)
    {
        return { {}, nullptr };
    }

    virtual std::pair<std::vector<VkDeviceAddress>, const VulkanCommandBufferInfo*>
    ResolveBufferAddresses(Decoded_VkSubmitInfo2& submit_info2, const VulkanDeviceAddressTracker& address_tracker)
    {
        return { {}, nullptr };
    }

    virtual void ProcessCmdPushConstants(const VulkanCommandBufferInfo*            command_buffer_info,
                                         VkShaderStageFlags                        stage_flags,
                                         uint32_t                                  offset,
                                         uint32_t                                  size,
                                         void*                                     data,
                                         const decode::VulkanDeviceAddressTracker& address_tracker)
    {}

    virtual void ProcessCmdBindPipeline(VulkanCommandBufferInfo*                  command_buffer_info,
                                        const decode::VulkanDeviceAddressTracker& address_tracker)
    {}

    virtual bool ProcessMarkedOffsetARM(VkDeviceSize                              offset,
                                        const void*                               pData,
                                        const decode::VulkanDeviceAddressTracker& address_tracker)
    {
        return true;
    }

    virtual void ProcessCmdBindDescriptorSets(VulkanCommandBufferInfo*               command_buffer_info,
                                              VkPipelineBindPoint                    pipelineBindPoint,
                                              uint32_t                               firstSet,
                                              uint32_t                               descriptorSetCount,
                                              HandlePointerDecoder<VkDescriptorSet>* pDescriptorSets,
                                              decode::VulkanDeviceAddressTracker&    address_tracker)
    {}

    virtual void ProcessCmdTraceRays(
        const VulkanCommandBufferInfo*                                                              command_buffer_info,
        VkStridedDeviceAddressRegionKHR*                                                            raygen_sbt,
        VkStridedDeviceAddressRegionKHR*                                                            miss_sbt,
        VkStridedDeviceAddressRegionKHR*                                                            hit_sbt,
        VkStridedDeviceAddressRegionKHR*                                                            callable_sbt,
        const decode::VulkanDeviceAddressTracker&                                                   address_tracker,
        const std::unordered_map<graphics::shader_group_handle_t, graphics::shader_group_handle_t>& group_handle_map)
    {}

    virtual void
    ProcessCmdBuildAccelerationStructuresKHR(const VulkanCommandBufferInfo*               command_buffer_info,
                                             uint32_t                                     info_count,
                                             VkAccelerationStructureBuildGeometryInfoKHR* build_geometry_infos,
                                             VkAccelerationStructureBuildRangeInfoKHR**   build_range_infos,
                                             const decode::VulkanDeviceAddressTracker&    address_tracker,
                                             bool                                         process_scratch_buffers)
    {}

    virtual void ProcessGetDescriptorEXT(const VulkanDeviceInfo*           device_info,
                                         VkDescriptorGetInfoEXT*           descriptorInfo,
                                         const VulkanDeviceAddressTracker& address_tracker)
    {}

    virtual void ProcessCmdBindDescriptorBuffersEXT(const VulkanCommandBufferInfo*    commandBuffer_info,
                                                    uint32_t                          bufferCount,
                                                    VkDescriptorBufferBindingInfoEXT* bindingInfos,
                                                    const VulkanDeviceAddressTracker& address_tracker)
    {}

    virtual void DestroyShadowResources(const VulkanAccelerationStructureKHRInfo* acceleration_structure_info) {}

    virtual void DestroyShadowResources(const VulkanBufferInfo*           buffer_info,
                                        const VulkanDeviceAddressTracker& address_tracker)
    {}

    virtual void DestroyShadowResources(VkCommandBuffer handle) {}

    virtual void ProcessUpdateDescriptorSets(uint32_t                                  descriptor_write_count,
                                             VkWriteDescriptorSet*                     descriptor_writes,
                                             uint32_t                                  descriptor_copy_count,
                                             VkCopyDescriptorSet*                      descriptor_copies,
                                             const decode::VulkanDeviceAddressTracker& address_tracker)
    {}

    virtual void
    ProcessCmdWriteAccelerationStructuresPropertiesKHR(uint32_t                    count,
                                                       VkAccelerationStructureKHR* acceleration_structures,
                                                       VkQueryType                 query_type,
                                                       VkQueryPool                 pool,
                                                       uint32_t                    first_query,
                                                       const decode::VulkanDeviceAddressTracker& address_tracker)
    {}

    virtual void ProcessGeneratedCommandsInfoEXT(VkGeneratedCommandsInfoEXT*               pGeneratedCommandsInfo,
                                                 const decode::VulkanDeviceAddressTracker& address_tracker)
    {}

    virtual void ProcessSpecializationInfo(VkSpecializationInfo*             info,
                                           const VulkanDeviceAddressTracker& address_tracker)
    {}
};
GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_ADDRESS_REPLACER_BASE_H
