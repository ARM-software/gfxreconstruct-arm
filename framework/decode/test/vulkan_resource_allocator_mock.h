/*
** Copyright (c) 2025 LunarG, Inc.
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

#ifndef GFXRECON_DECODE_VULKAN_RESOURCE_ALLOCATOR_MOCK_H
#define GFXRECON_DECODE_VULKAN_RESOURCE_ALLOCATOR_MOCK_H

#include "decode/vulkan_resource_allocator.h"
#include "util/defines.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanResourceAllocatorMock : public gfxrecon::decode::VulkanResourceAllocator
{
  public:
    virtual VkResult Initialize(const VulkanPhysicalDeviceInfo*      physical_device_info,
                                VkDevice                             device,
                                const VkDeviceCreateInfo&            device_create_info,
                                const std::vector<std::string>&      enabled_device_extensions,
                                const graphics::VulkanInstanceTable& instance_table,
                                const graphics::VulkanDeviceTable*   device_table) override
    {
        device_table_ = graphics::VulkanInjectedDeviceCalls(device_table, device);
        return VK_SUCCESS;
    }

    virtual void Destroy() {}

    virtual VkResult CreateBuffer(const VkBufferCreateInfo*    create_info,
                                  const VkAllocationCallbacks* allocation_callbacks,
                                  format::HandleId             capture_id,
                                  VkBuffer*                    buffer,
                                  ResourceData*                allocator_data) override
    {
        OnCreateBuffer(create_info);
        return VK_SUCCESS;
    }

    virtual void DestroyBuffer(VkBuffer                     buffer,
                               const VkAllocationCallbacks* allocation_callbacks,
                               ResourceData                 allocator_data) override
    {}

    virtual VkResult CreateImage(const VkImageCreateInfo*     create_info,
                                 const VkAllocationCallbacks* allocation_callbacks,
                                 format::HandleId             capture_id,
                                 VkImage*                     image,
                                 ResourceData*                allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual void
    DestroyImage(VkImage image, const VkAllocationCallbacks* allocation_callbacks, ResourceData allocator_data) override
    {}

    virtual VkResult CreateVideoSession(const VkVideoSessionCreateInfoKHR* create_info,
                                        const VkAllocationCallbacks*       allocation_callbacks,
                                        format::HandleId                   capture_id,
                                        VkVideoSessionKHR*                 session,
                                        ResourceData*                      allocator_datas) override
    {
        return VK_SUCCESS;
    }

    virtual void DestroyVideoSession(VkVideoSessionKHR            session,
                                     const VkAllocationCallbacks* allocation_callbacks,
                                     ResourceData                 allocator_datas) override
    {}

    virtual void GetBufferMemoryRequirements(VkBuffer              buffer,
                                             VkMemoryRequirements* memory_requirements,
                                             ResourceData          allocator_data) override
    {}

    virtual void GetBufferMemoryRequirements2(const VkBufferMemoryRequirementsInfo2* info,
                                              VkMemoryRequirements2*                 memory_requirements,
                                              ResourceData                           allocator_data) override
    {}

    virtual void GetImageSubresourceLayout(VkImage                    image,
                                           const VkImageSubresource*  subresource,
                                           VkSubresourceLayout*       layout,
                                           const VkSubresourceLayout* original_layout,
                                           ResourceData               allocator_data) override
    {}

    virtual void GetImageMemoryRequirements(VkImage               image,
                                            VkMemoryRequirements* memory_requirements,
                                            ResourceData          allocator_data) override
    {}

    virtual void GetImageMemoryRequirements2(const VkImageMemoryRequirementsInfo2* info,
                                             VkMemoryRequirements2*                memory_requirements,
                                             ResourceData                          allocator_data) override
    {}

    virtual VkResult GetVideoSessionMemoryRequirementsKHR(VkVideoSessionKHR video_session,
                                                          uint32_t*         memory_requirements_count,
                                                          VkVideoSessionMemoryRequirementsKHR* memory_requirements,
                                                          ResourceData                         allocator_datas) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult AllocateMemory(const VkMemoryAllocateInfo*  allocate_info,
                                    const VkAllocationCallbacks* allocation_callbacks,
                                    format::HandleId             capture_id,
                                    VkDeviceMemory*              memory,
                                    MemoryData*                  allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual void FreeMemory(VkDeviceMemory               memory,
                            const VkAllocationCallbacks* allocation_callbacks,
                            MemoryData                   allocator_data) override
    {}

    virtual void GetDeviceMemoryCommitment(VkDeviceMemory memory,
                                           VkDeviceSize*  committed_memory_in_bytes,
                                           MemoryData     allocator_data) override
    {}

    virtual VkResult BindBufferMemory(VkBuffer               buffer,
                                      VkDeviceMemory         memory,
                                      VkDeviceSize           memory_offset,
                                      ResourceData           allocator_buffer_data,
                                      MemoryData             allocator_memory_data,
                                      VkMemoryPropertyFlags* bind_memory_properties) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindBufferMemory2(uint32_t                      bind_info_count,
                                       const VkBindBufferMemoryInfo* bind_infos,
                                       const ResourceData*           allocator_buffer_datas,
                                       const MemoryData*             allocator_memory_datas,
                                       VkMemoryPropertyFlags*        bind_memory_properties) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindImageMemory(VkImage                image,
                                     VkDeviceMemory         memory,
                                     VkDeviceSize           memory_offset,
                                     ResourceData           allocator_image_data,
                                     MemoryData             allocator_memory_data,
                                     VkMemoryPropertyFlags* bind_memory_properties) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindImageMemory2(uint32_t                     bind_info_count,
                                      const VkBindImageMemoryInfo* bind_infos,
                                      const ResourceData*          allocator_image_datas,
                                      const MemoryData*            allocator_memory_datas,
                                      VkMemoryPropertyFlags*       bind_memory_properties) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindVideoSessionMemory(VkVideoSessionKHR                      video_session,
                                            uint32_t                               bind_info_count,
                                            const VkBindVideoSessionMemoryInfoKHR* bind_infos,
                                            const ResourceData                     allocator_session_datas,
                                            const MemoryData*                      allocator_memory_datas,
                                            VkMemoryPropertyFlags*                 bind_memory_properties) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult MapMemory(VkDeviceMemory   memory,
                               VkDeviceSize     offset,
                               VkDeviceSize     size,
                               VkMemoryMapFlags flags,
                               void**           data,
                               MemoryData       allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult MapMemory2(const VkMemoryMapInfo* memory_map_info, void** data, MemoryData allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual void UnmapMemory(VkDeviceMemory memory, MemoryData allocator_data) override {}

    virtual VkResult UnmapMemory2(const VkMemoryUnmapInfo* memory_unmap_info, MemoryData allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult FlushMappedMemoryRanges(uint32_t                   memory_range_count,
                                             const VkMappedMemoryRange* memory_ranges,
                                             const MemoryData*          allocator_datas) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult InvalidateMappedMemoryRanges(uint32_t                   memory_range_count,
                                                  const VkMappedMemoryRange* memory_ranges,
                                                  const MemoryData*          allocator_datas) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult SetDebugUtilsObjectNameEXT(VkDevice                       device,
                                                VkDebugUtilsObjectNameInfoEXT* name_info,
                                                uintptr_t                      allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult SetDebugUtilsObjectTagEXT(VkDevice                      device,
                                               VkDebugUtilsObjectTagInfoEXT* tag_info,
                                               uintptr_t                     allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult
    WriteMappedMemoryRange(MemoryData allocator_data, uint64_t offset, uint64_t size, const uint8_t* data) override
    {
        return VK_SUCCESS;
    }

    virtual void ReportAllocateMemoryIncompatibility(const VkMemoryAllocateInfo* allocate_info) override {}

    virtual void ReportBindBufferIncompatibility(VkBuffer     buffer,
                                                 ResourceData allocator_resource_data,
                                                 MemoryData   allocator_memory_data) override
    {}

    virtual void ReportBindBuffer2Incompatibility(uint32_t                      bind_info_count,
                                                  const VkBindBufferMemoryInfo* bind_infos,
                                                  const ResourceData*           allocator_resource_datas,
                                                  const MemoryData*             allocator_memory_datas) override
    {}

    virtual void ReportBindImageIncompatibility(VkImage      image,
                                                ResourceData allocator_resource_data,
                                                MemoryData   allocator_memory_data) override
    {}

    virtual void ReportBindImage2Incompatibility(uint32_t                     bind_info_count,
                                                 const VkBindImageMemoryInfo* bind_infos,
                                                 const ResourceData*          allocator_resource_datas,
                                                 const MemoryData*            allocator_memory_datas) override
    {}

    virtual void ReportBindVideoSessionIncompatibility(VkVideoSessionKHR                      video_session,
                                                       uint32_t                               bind_info_count,
                                                       const VkBindVideoSessionMemoryInfoKHR* bind_infos,
                                                       const ResourceData                     allocator_resource_datas,
                                                       const MemoryData* allocator_memory_datas) override
    {}

    virtual void
    ReportBindAccelerationStructureMemoryNVIncompatibility(uint32_t bind_info_count,
                                                           const VkBindAccelerationStructureMemoryInfoNV* bind_infos,
                                                           const ResourceData* allocator_acc_datas,
                                                           const MemoryData*   allocator_memory_datas) override
    {}

    virtual void ReportQueueBindSparseIncompatibility(VkQueue                 queue,
                                                      uint32_t                bind_info_count,
                                                      const VkBindSparseInfo* bind_infos,
                                                      VkFence                 fence,
                                                      const ResourceData*     allocator_buf_datas,
                                                      const MemoryData*       allocator_buf_mem_datas,
                                                      const ResourceData*     allocator_img_op_datas,
                                                      const MemoryData*       allocator_img_op_mem_datas,
                                                      const ResourceData*     allocator_img_datas,
                                                      const MemoryData*       allocator_img_mem_datas) override
    {}

    virtual void SetDeviceMemoryPriority(VkDeviceMemory memory, float priority, MemoryData allocator_data) override {}

    virtual VkResult GetMemoryRemoteAddressNV(const VkMemoryGetRemoteAddressInfoNV* memory_get_remote_address_info,
                                              VkRemoteAddressNV*                    address,
                                              MemoryData                            allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult CreateAccelerationStructureNV(const VkAccelerationStructureCreateInfoNV* create_info,
                                                   const VkAllocationCallbacks*               allocation_callbacks,
                                                   format::HandleId                           capture_id,
                                                   VkAccelerationStructureNV*                 acc_str,
                                                   ResourceData*                              allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual void DestroyAccelerationStructureNV(VkAccelerationStructureNV    acc_str,
                                                const VkAllocationCallbacks* allocation_callbacks,
                                                ResourceData                 allocator_data) override
    {}

    virtual void
    GetAccelerationStructureMemoryRequirementsNV(const VkAccelerationStructureMemoryRequirementsInfoNV* info,
                                                 VkMemoryRequirements2KHR* memory_requirements,
                                                 ResourceData              allocator_data) override
    {}

    virtual VkResult BindAccelerationStructureMemoryNV(uint32_t                                       bind_info_count,
                                                       const VkBindAccelerationStructureMemoryInfoNV* bind_infos,
                                                       const ResourceData*    allocator_acc_datas,
                                                       const MemoryData*      allocator_memory_datas,
                                                       VkMemoryPropertyFlags* bind_memory_properties) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult GetMemoryFd(const VkMemoryGetFdInfoKHR* get_fd_info, int* pFd, MemoryData allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult QueueBindSparse(VkQueue                 queue,
                                     uint32_t                bind_info_count,
                                     const VkBindSparseInfo* bind_infos,
                                     VkFence                 fence,
                                     ResourceData*           allocator_buf_datas,
                                     const MemoryData*       allocator_buf_mem_datas,
                                     VkMemoryPropertyFlags*  bind_buf_mem_properties,
                                     ResourceData*           allocator_img_op_datas,
                                     const MemoryData*       allocator_img_op_mem_datas,
                                     VkMemoryPropertyFlags*  bind_img_op_mem_properties,
                                     ResourceData*           allocator_img_datas,
                                     const MemoryData*       allocator_img_mem_datas,
                                     VkMemoryPropertyFlags*  bind_img_mem_properties) override
    {
        return VK_SUCCESS;
    }

    virtual uint64_t GetDeviceMemoryOpaqueCaptureAddress(const VkDeviceMemoryOpaqueCaptureAddressInfo* info,
                                                         MemoryData allocator_data) override
    {
        return 0;
    }

    virtual void BindMemoryImageAHardwareBuffer(MemoryData* allocator_memory_data,
                                                VkImage     image,
                                                void*       ahardwarebuffer_info) override
    {}

    virtual bool SupportsOpaqueDeviceAddresses() override { return false; }

    virtual bool SupportsExternalMemory() override { return true; }

    virtual size_t GetBufferSize(VulkanResourceAllocator::ResourceData alloc_data) const override
    {
        return OnGetBufferSize(alloc_data);
    }

    virtual bool SupportBindVideoSessionMemory() override { return true; }

    virtual VkResult CreateDataGraphPipelineSession(const VkDataGraphPipelineSessionCreateInfoARM* create_info,
                                                    const VkAllocationCallbacks*                   allocation_callbacks,
                                                    format::HandleId                               capture_id,
                                                    VkDataGraphPipelineSessionARM* data_graph_pipeline_session,
                                                    ResourceData*                  allocator_data) override
    {
        return VK_SUCCESS;
    }
    virtual void DestroyDataGraphPipelineSession(VkDataGraphPipelineSessionARM data_graph_pipeline_session,
                                                 const VkAllocationCallbacks*  allocation_callbacks,
                                                 ResourceData                  allocator_data) override
    {}
    virtual VkResult CreateTensor(const VkTensorCreateInfoARM* create_info,
                                  const VkAllocationCallbacks* allocation_callbacks,
                                  format::HandleId             capture_id,
                                  VkTensorARM*                 tensor,
                                  ResourceData*                allocator_data) override
    {
        return VK_SUCCESS;
    }
    virtual void DestroyTensor(VkTensorARM                  tensor,
                               const VkAllocationCallbacks* allocation_callbacks,
                               ResourceData                 allocator_data) override
    {}
    virtual VkResult BindTensorMemory(uint32_t                         bindInfoCount,
                                      const VkBindTensorMemoryInfoARM* pBindInfos,
                                      const ResourceData*              allocator_buffer_data,
                                      const MemoryData*                allocator_memory_data,
                                      VkMemoryPropertyFlags*           bind_memory_properties) override
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindDataGraphPipelineSessionMemory(uint32_t bind_info_count,
                                                        const VkBindDataGraphPipelineSessionMemoryInfoARM* bind_infos,
                                                        const ResourceData*    allocator_session_datas,
                                                        const MemoryData*      allocator_memory_datas,
                                                        VkMemoryPropertyFlags* bind_memory_properties) override
    {
        return VK_SUCCESS;
    }

    virtual void GetTensorMemoryRequirementsARM(VkTensorMemoryRequirementsInfoARM* tensor_memory_requirements,
                                                VkMemoryRequirements2*             memory_requirements,
                                                ResourceData                       allocator_data) override
    {}

    virtual VkResult MapResourceMemoryDirectImpl(VkDeviceSize     size,
                                                 VkMemoryMapFlags flags,
                                                 void**           data,
                                                 ResourceData     allocator_data) override
    {
        return VK_SUCCESS;
    }

    virtual void UnmapResourceMemoryDirectImpl(ResourceData allocator_data) override {}

    std::function<void(const VkBufferCreateInfo*)>               OnCreateBuffer;
    std::function<size_t(VulkanResourceAllocator::ResourceData)> OnGetBufferSize;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_RESOURCE_ALLOCATOR_MOCK_H