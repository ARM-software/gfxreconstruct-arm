/*
** Copyright (c) 2024-2025 LunarG, Inc.
** Copyright (c) 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "decode/vulkan_object_info.h"
#include "decode/vulkan_resource_allocator.h"
#include "decode/vulkan_device_address_tracker.h"
#include "decode/vulkan_internal_buffer_manager.h"
#include "format/format.h"
#include "util/defines.h"

#include <memory>
#include <vector>
#include <unordered_map>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

// TODO: Consider support of indirect calls
// TODO: Consider support of CmdPushDescriptorSet/CmdPushDescriptorSetWithTemplate
class VulkanAccelerationStructureBuilder
{
    friend class VulkanAccelerationStructureBuilderTestAccess;

  public:
    VulkanAccelerationStructureBuilder(const graphics::VulkanDeviceTable*      device_table,
                                       VkDevice                                device,
                                       VulkanResourceAllocator*                allocator,
                                       const VkPhysicalDeviceMemoryProperties& properties,
                                       VulkanDeviceAddressTracker&             buffer_tracker);

    ~VulkanAccelerationStructureBuilder();

    void OnAccelerationStructureCompactionDependencyCommand(VkAccelerationStructureKHR           parent,
                                                            const std::vector<format::HandleId>& children);

    void OnCmdBuildAccelerationStructures(VkCommandBuffer                              command_buffer,
                                          uint32_t                                     info_count,
                                          VkAccelerationStructureBuildGeometryInfoKHR* geometry_infos,
                                          VkAccelerationStructureBuildRangeInfoKHR**   range_infos);

    void OnCmdCopyAccelerationStructure(VkCommandBuffer command_buffer, VkCopyAccelerationStructureInfoKHR* copy_info);
    void OnCmdWriteAccelerationStructuresProperties(VkCommandBuffer             command_buffer,
                                                    uint32_t                    count,
                                                    VkAccelerationStructureKHR* acceleration_structures,
                                                    VkQueryType                 query_type,
                                                    VkQueryPool                 pool,
                                                    uint32_t                    first_query);

    void OnGetAccelerationStructureBuildSizes(const VulkanDeviceInfo*                            device_info,
                                              VkAccelerationStructureBuildTypeKHR                type,
                                              const VkAccelerationStructureBuildGeometryInfoKHR* build_Info,
                                              const uint32_t*                                    max_primitive_counts,
                                              VkAccelerationStructureBuildSizesInfoKHR*          size_info);

    VkResult OnCreateAccelerationStructure(const VulkanDeviceInfo*                     device_info,
                                           const VkAccelerationStructureCreateInfoKHR* create_info,
                                           const VkAllocationCallbacks*                pAllocator,
                                           const VulkanBufferInfo*                     buffer_info,
                                           VulkanAccelerationStructureKHRInfo*         acceleration_structure_info,
                                           VkAccelerationStructureKHR*                 handle);

    void OnDestroyBuffer(const VulkanBufferInfo* buffer_info);

    void ProcessVulkanBuildAccelerationStructuresCommand(
        uint32_t                                                      info_count,
        VkAccelerationStructureBuildGeometryInfoKHR*                  geometry_infos,
        VkAccelerationStructureBuildRangeInfoKHR**                    range_infos,
        std::vector<std::vector<VkAccelerationStructureInstanceKHR>>& instance_buffers_data);

    void ProcessVulkanCopyAccelerationStructuresCommand(uint32_t                            info_count,
                                                        VkCopyAccelerationStructureInfoKHR* copy_infos);

    void ProcessVulkanWriteAccelerationStructuresPropertiesCommand(VkQueryType                query_type,
                                                                   VkAccelerationStructureKHR acceleration_structure);

    void OnQueueSubmit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
    void OnQueueSubmit2(uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence);

    void OnWaitForFences(VkResult result, uint32_t fenceCount, const VkFence* pFences);

    void OnGetFenceStatus(VkResult result, VkFence fence);

    // called before command gets executed
    // the query pool results contain the AS compacted sizes
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

  private:
    void InitializeFunctionPointers(const graphics::VulkanDeviceTable* device_table);
    struct Functions
    {
        PFN_vkGetAccelerationStructureBuildSizesKHR       get_acceleration_structure_build_sizes{ nullptr };
        PFN_vkCreateAccelerationStructureKHR              create_acceleration_structure{ nullptr };
        PFN_vkCmdBuildAccelerationStructuresKHR           cmd_build_acceleration_structures{ nullptr };
        PFN_vkGetAccelerationStructureDeviceAddressKHR    get_acceleration_structure_device_address{ nullptr };
        PFN_vkCmdCopyAccelerationStructureKHR             cmd_copy_acceleration_structure{ nullptr };
        PFN_vkCmdWriteAccelerationStructuresPropertiesKHR cmd_write_acceleration_structures_properties{ nullptr };
        PFN_vkDestroyAccelerationStructureKHR             destroy_acceleration_structure{ nullptr };
        PFN_vkCreateCommandPool                           create_command_pool{ nullptr };
        PFN_vkDestroyCommandPool                          destroy_command_pool{ nullptr };
        PFN_vkAllocateCommandBuffers                      allocate_command_buffers{ nullptr };
        PFN_vkFreeCommandBuffers                          free_command_buffers{ nullptr };
        PFN_vkGetDeviceQueue                              get_device_queue{ nullptr };
        PFN_vkBeginCommandBuffer                          begin_command_buffer{ nullptr };
        PFN_vkEndCommandBuffer                            end_command_buffer{ nullptr };
        PFN_vkResetCommandBuffer                          reset_command_buffer{ nullptr };
        PFN_vkQueueSubmit                                 queue_submit{ nullptr };
        PFN_vkQueueWaitIdle                               queue_wait_idle{ nullptr };
        PFN_vkUpdateDescriptorSets                        update_descriptor_sets{ nullptr };
        PFN_vkGetQueryPoolResults                         get_query_pool_results{ nullptr };
        PFN_vkCmdCopyQueryPoolResults                     cmd_copy_query_pool_results{ nullptr };
        PFN_vkCmdPipelineBarrier                          cmd_pipeline_barrier{ nullptr };
        PFN_vkCreateQueryPool                             create_query_pool{ nullptr };
        PFN_vkCmdResetQueryPool                           cmd_reset_query_pool{ nullptr };
        PFN_vkDestroyQueryPool                            destroy_query_pool{ nullptr };
        PFN_vkGetFenceStatus                              get_fence_status{ nullptr };
    };

    // This objects are internal and responsible for executing the state recreation meta commands
    struct CommandExecuteObjects
    {
        CommandExecuteObjects() = default;
        ~CommandExecuteObjects()
        {
            if (initialized_)
            {
                free_command_buffers_(device_, pool_, 1, &command_buffer_);
                destroy_command_pool_(device_, pool_, nullptr);
                destroy_query_pool_(device_, query_pool_, nullptr);
            }
        }
        PFN_vkFreeCommandBuffers free_command_buffers_{ nullptr };
        PFN_vkDestroyCommandPool destroy_command_pool_{ nullptr };
        PFN_vkDestroyQueryPool   destroy_query_pool_{ nullptr };

        VkDevice        device_{ VK_NULL_HANDLE };
        VkCommandPool   pool_{ VK_NULL_HANDLE };
        VkCommandBuffer command_buffer_{ VK_NULL_HANDLE };
        VkQueue         queue_{ VK_NULL_HANDLE };
        VkQueryPool     query_pool_{ VK_NULL_HANDLE };
        bool            initialized_{ false };
    };

  private:
    Functions                        functions_;
    VkDevice                         device_;
    VulkanResourceAllocator*         allocator_;
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties_;

    VulkanDeviceAddressTracker& device_address_tracker_;
    VulkanInternalBufferManager internal_buffer_manager_;

    std::unordered_map<VkFence, std::vector<std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>>>
        submitted_scratches;
    std::unordered_map<VkCommandBuffer, std::vector<std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>>>
        recorded_scratches;

    struct PreProcessingCompactionInfo
    {
        uint32_t                                                        first_query;
        std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> buffer_info_wrapper;
        std::vector<VkAccelerationStructureKHR>                         sources;
    };

    // holds information gathered during vkCmdCopyQueryPoolResults that needs to be processed before
    // vkCmdCopyAccelerationStructureKHR in order to know replacement AS compressed sizes
    std::unordered_map<VkQueryPool, std::vector<PreProcessingCompactionInfo>> compacted_sizes_unprocessed_;
    // map containing relation between uncompacted AS capture id and the size of compacted AS
    std::unordered_map<VkAccelerationStructureKHR, VkDeviceSize> compacted_sizes_processed_;

    std::unordered_map<format::HandleId, VkAccelerationStructureKHR> compaction_child_to_parent_dependency_;

    std::unordered_map<format::HandleId, std::vector<std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>>>
        replaced_buffers_;

    VkAccelerationStructureBuildSizesInfoKHR max_build_sizes_{};

    CommandExecuteObjects cmd_execute_obj_;

  private:
    void InitializeInternalExecObjects();
    void BeginCommandBuffer();
    void ExecuteCommandBuffer();

    void UpdateScratchDeviceAddress(VkCommandBuffer                              command_buffer,
                                    VkAccelerationStructureBuildGeometryInfoKHR& geometry_infos,
                                    VkDeviceSize                                 scratch_size);
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_ACCELERATION_STRUCTURE_BUILDER_H
