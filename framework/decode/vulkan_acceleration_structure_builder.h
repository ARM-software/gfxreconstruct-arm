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
        PFN_vkGetAccelerationStructureBuildSizesKHR       get_acceleration_structure_build_sizes{ nullptr };
        PFN_vkCreateAccelerationStructureKHR              create_acceleration_structure{ nullptr };
        PFN_vkGetBufferDeviceAddressKHR                   get_buffer_device_address{ nullptr };
        PFN_vkCmdBuildAccelerationStructuresKHR           cmd_build_acceleration_structures{ nullptr };
        PFN_vkGetAccelerationStructureDeviceAddressKHR    get_acceleration_structure_device_address{ nullptr };
        PFN_vkGetBufferMemoryRequirements                 get_buffer_memory_requirements{ nullptr };
        PFN_vkCmdCopyAccelerationStructureKHR             cmd_copy_acceleration_structure{ nullptr };
        PFN_vkCmdWriteAccelerationStructuresPropertiesKHR cmd_write_acceleration_structures_properties{ nullptr };
        PFN_vkDestroyAccelerationStructureKHR             destroy_acceleration_structure{ nullptr };
        PFN_vkDestroyBuffer                               destroy_buffer{ nullptr };
        PFN_vkCreateCommandPool                           create_command_pool{ nullptr };
        PFN_vkDestroyCommandPool                          destroy_command_pool{ nullptr };
        PFN_vkAllocateCommandBuffers                      allocate_command_buffers{ nullptr };
        PFN_vkGetDeviceQueue                              get_device_queue{ nullptr };
        PFN_vkBeginCommandBuffer                          begin_command_buffer{ nullptr };
        PFN_vkEndCommandBuffer                            end_command_buffer{ nullptr };
        PFN_vkResetCommandBuffer                          reset_command_buffer{ nullptr };
        PFN_vkQueueSubmit                                 queue_submit{ nullptr };
        PFN_vkQueueWaitIdle                               queue_wait_idle{ nullptr };
    };

    VulkanAccelerationStructureBuilder(Functions                               functions,
                                       VkDevice                                device,
                                       VulkanResourceAllocator*                allocator,
                                       const VkPhysicalDeviceMemoryProperties& properties);

    ~VulkanAccelerationStructureBuilder();
    void UpdateDescriptorSets(VkWriteDescriptorSetAccelerationStructureKHR* ac_write);
    void UpdateDescriptorSetWithTemplateKHR(gfxrecon::decode::DescriptorUpdateTemplateDecoder* pData);

    void CmdBuildAccelerationStructures(VkCommandBuffer                              command_buffer,
                                        uint32_t                                     info_count,
                                        VkAccelerationStructureBuildGeometryInfoKHR* geometry_infos,
                                        VkAccelerationStructureBuildRangeInfoKHR**   range_infos);

    void CmdCopyAccelerationStructure(VkCommandBuffer command_buffer, VkCopyAccelerationStructureInfoKHR* copy_info);
    void CmdWriteAccelerationStructuresProperties(VkCommandBuffer             command_buffer,
                                                  uint32_t                    count,
                                                  VkAccelerationStructureKHR* acceleration_structures,
                                                  VkQueryType                 query_type,
                                                  VkQueryPool                 pool,
                                                  uint32_t                    first_query);

    void SetBufferInfo(BufferInfo* buffer_info, VkDeviceAddress original_address, VkDeviceAddress new_address);
    void UntrackBufferInfo(const BufferInfo* buffer_info);
    void SetAccelerationStructureEntry(VkAccelerationStructureKHR acceleration_struct,
                                       VkDeviceAddress            original_address,
                                       VkDeviceAddress            new_address);
    void RegisterAccelerationStructure(VkAccelerationStructureKHR handle, VkDeviceAddress device_address);
    void UntrackAccelerationStructure(const AccelerationStructureKHRInfo* acceleration_structure_info);

    // Used when building acc structures in state recreation, naming to be changed
    void ProcessInitVulkanAccelerationStructuresCommand(
        VkCommandBuffer                                               command_buffer,
        uint32_t                                                      info_count,
        VkAccelerationStructureBuildGeometryInfoKHR*                  geometry_infos,
        VkAccelerationStructureBuildRangeInfoKHR**                    range_infos,
        std::vector<std::vector<VkAccelerationStructureInstanceKHR>>& instance_buffers_data);

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
    std::unordered_map<VkBuffer, BufferInfo>                 buffer_infos_;

    Functions                functions_;
    VkDevice                 device_;
    VulkanResourceAllocator* allocator_;

    BufferEntry* CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, void* initial_data = nullptr);

    AccelerationStructureEntry* GetAccelerationStructureEntry(VkAccelerationStructureKHR acceleration_struct);
    void                        UpdateAccelerationStructDeviceAddress(VkDeviceAddress& address);
    void                        UpdateBufferDeviceAddress(VkDeviceAddress& address);
    void                        UpdateDeviceAddress(VkAccelerationStructureBuildGeometryInfoKHR& build_geometry,
                                                    VkAccelerationStructureBuildRangeInfoKHR*    range_infos);
    BufferEntry*                GetBufferByRuntimeDeviceAddress(VkDeviceAddress runtime_address);
    BufferEntry*                GetBufferByCaptureDeviceAddress(VkDeviceAddress original_address);
    VkDeviceAddress             GetBufferDeviceAddress(VkBuffer buffer);
    VkDeviceAddress GetAccelerationStructureDeviceAddress(VkAccelerationStructureKHR acceleration_structure);

    VkAccelerationStructureKHR CreateAccelerationStructure(VkAccelerationStructureBuildGeometryInfoKHR& geometry_info,
                                                           VkAccelerationStructureBuildRangeInfoKHR*    range_info,
                                                           const VkAccelerationStructureBuildSizesInfoKHR& size_info,
                                                           VkBuffer                                        storage);

    VkAccelerationStructureBuildSizesInfoKHR
    GetAccelerationStructureSizeInfo(VkAccelerationStructureBuildGeometryInfoKHR* geometry_info,
                                     VkAccelerationStructureBuildRangeInfoKHR*    range_info);

    void UpdateInstanceBuffer(VkAccelerationStructureGeometryInstancesDataKHR& instances,
                              const VkAccelerationStructureBuildRangeInfoKHR&  build_range);

    VkPhysicalDeviceMemoryProperties physical_device_memory_properties_;
    void                             InitializeInternalExecObjects();
    void                             BeginCommandBuffer();
    void                             ExecuteCommandBuffer();
    struct CommandExecuteObjects
    {
        CommandExecuteObjects(VkDevice device, PFN_vkDestroyCommandPool destroy_func) :
            m_device(device), destroy_command_pool(destroy_func)
        {}
        ~CommandExecuteObjects() { destroy_command_pool(m_device, m_pool, nullptr); }
        PFN_vkDestroyCommandPool destroy_command_pool{ nullptr };

        VkDevice        m_device{ VK_NULL_HANDLE };
        VkCommandPool   m_pool{ VK_NULL_HANDLE };
        VkCommandBuffer m_command_buffer{ VK_NULL_HANDLE };
        VkQueue         m_queue{ VK_NULL_HANDLE };
    };

    std::unique_ptr<CommandExecuteObjects> m_cmd_execute_obj;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_ACCELERATION_STRUCTURE_BUILDER_H
