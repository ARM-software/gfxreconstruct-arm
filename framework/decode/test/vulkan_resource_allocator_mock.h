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
    virtual VkResult Initialize(uint32_t                                api_version,
                                VkInstance                              instance,
                                VkPhysicalDevice                        physical_device,
                                VkDevice                                device,
                                const std::vector<std::string>&         enabled_device_extensions,
                                VkPhysicalDeviceType                    capture_device_type,
                                const VkPhysicalDeviceMemoryProperties& capture_memory_properties,
                                const VkPhysicalDeviceMemoryProperties& replay_memory_properties,
                                const Functions&                        functions)
    {
        return VK_SUCCESS;
    }

    virtual void Destroy() {}

    virtual VkResult CreateBuffer(const VkBufferCreateInfo*    create_info,
                                  const VkAllocationCallbacks* allocation_callbacks,
                                  format::HandleId             capture_id,
                                  VkBuffer*                    buffer,
                                  ResourceData*                allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual void
    DestroyBuffer(VkBuffer buffer, const VkAllocationCallbacks* allocation_callbacks, ResourceData allocator_data)
    {}

    virtual VkResult CreateImage(const VkImageCreateInfo*     create_info,
                                 const VkAllocationCallbacks* allocation_callbacks,
                                 format::HandleId             capture_id,
                                 VkImage*                     image,
                                 ResourceData*                allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual void
    DestroyImage(VkImage image, const VkAllocationCallbacks* allocation_callbacks, ResourceData allocator_data)
    {}

    virtual VkResult CreateVideoSession(const VkVideoSessionCreateInfoKHR* create_info,
                                        const VkAllocationCallbacks*       allocation_callbacks,
                                        format::HandleId                   capture_id,
                                        VkVideoSessionKHR*                 session,
                                        std::vector<ResourceData>*         allocator_datas)
    {
        return VK_SUCCESS;
    }

    virtual void DestroyVideoSession(VkVideoSessionKHR            session,
                                     const VkAllocationCallbacks* allocation_callbacks,
                                     std::vector<ResourceData>    allocator_datas)
    {}

    virtual void
    GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements* memory_requirements, ResourceData allocator_data)
    {}

    virtual void GetBufferMemoryRequirements2(const VkBufferMemoryRequirementsInfo2* info,
                                              VkMemoryRequirements2*                 memory_requirements,
                                              ResourceData                           allocator_data)
    {}

    virtual void GetImageSubresourceLayout(VkImage                    image,
                                           const VkImageSubresource*  subresource,
                                           VkSubresourceLayout*       layout,
                                           const VkSubresourceLayout* original_layout,
                                           ResourceData               allocator_data)
    {}

    virtual void
    GetImageMemoryRequirements(VkImage image, VkMemoryRequirements* memory_requirements, ResourceData allocator_data)
    {}

    virtual void GetImageMemoryRequirements2(const VkImageMemoryRequirementsInfo2* info,
                                             VkMemoryRequirements2*                memory_requirements,
                                             ResourceData                          allocator_data)
    {}

    virtual VkResult GetVideoSessionMemoryRequirementsKHR(VkVideoSessionKHR video_session,
                                                          uint32_t*         memory_requirements_count,
                                                          VkVideoSessionMemoryRequirementsKHR* memory_requirements,
                                                          std::vector<ResourceData>            allocator_datas)
    {
        return VK_SUCCESS;
    }

    virtual VkResult AllocateMemory(const VkMemoryAllocateInfo*  allocate_info,
                                    const VkAllocationCallbacks* allocation_callbacks,
                                    format::HandleId             capture_id,
                                    VkDeviceMemory*              memory,
                                    MemoryData*                  allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual void
    FreeMemory(VkDeviceMemory memory, const VkAllocationCallbacks* allocation_callbacks, MemoryData allocator_data)
    {}

    virtual void
    GetDeviceMemoryCommitment(VkDeviceMemory memory, VkDeviceSize* committed_memory_in_bytes, MemoryData allocator_data)
    {}

    virtual VkResult BindBufferMemory(VkBuffer               buffer,
                                      VkDeviceMemory         memory,
                                      VkDeviceSize           memory_offset,
                                      ResourceData           allocator_buffer_data,
                                      MemoryData             allocator_memory_data,
                                      VkMemoryPropertyFlags* bind_memory_properties)
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindBufferMemory2(uint32_t                      bind_info_count,
                                       const VkBindBufferMemoryInfo* bind_infos,
                                       const ResourceData*           allocator_buffer_datas,
                                       const MemoryData*             allocator_memory_datas,
                                       VkMemoryPropertyFlags*        bind_memory_properties)
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindImageMemory(VkImage                image,
                                     VkDeviceMemory         memory,
                                     VkDeviceSize           memory_offset,
                                     ResourceData           allocator_image_data,
                                     MemoryData             allocator_memory_data,
                                     VkMemoryPropertyFlags* bind_memory_properties)
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindImageMemory2(uint32_t                     bind_info_count,
                                      const VkBindImageMemoryInfo* bind_infos,
                                      const ResourceData*          allocator_image_datas,
                                      const MemoryData*            allocator_memory_datas,
                                      VkMemoryPropertyFlags*       bind_memory_properties)
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindVideoSessionMemory(VkVideoSessionKHR                      video_session,
                                            uint32_t                               bind_info_count,
                                            const VkBindVideoSessionMemoryInfoKHR* bind_infos,
                                            const ResourceData*                    allocator_session_datas,
                                            const MemoryData*                      allocator_memory_datas,
                                            VkMemoryPropertyFlags*                 bind_memory_properties)
    {
        return VK_SUCCESS;
    }

    virtual VkResult MapMemory(VkDeviceMemory   memory,
                               VkDeviceSize     offset,
                               VkDeviceSize     size,
                               VkMemoryMapFlags flags,
                               void**           data,
                               MemoryData       allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual void UnmapMemory(VkDeviceMemory memory, MemoryData allocator_data) {}

    virtual VkResult FlushMappedMemoryRanges(uint32_t                   memory_range_count,
                                             const VkMappedMemoryRange* memory_ranges,
                                             const MemoryData*          allocator_datas)
    {
        return VK_SUCCESS;
    }

    virtual VkResult InvalidateMappedMemoryRanges(uint32_t                   memory_range_count,
                                                  const VkMappedMemoryRange* memory_ranges,
                                                  const MemoryData*          allocator_datas)
    {
        return VK_SUCCESS;
    }

    virtual VkResult
    SetDebugUtilsObjectNameEXT(VkDevice device, VkDebugUtilsObjectNameInfoEXT* name_info, uintptr_t allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual VkResult
    SetDebugUtilsObjectTagEXT(VkDevice device, VkDebugUtilsObjectTagInfoEXT* tag_info, uintptr_t allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual VkResult
    WriteMappedMemoryRange(MemoryData allocator_data, uint64_t offset, uint64_t size, const uint8_t* data)
    {
        return VK_SUCCESS;
    }

    virtual void ReportAllocateMemoryIncompatibility(const VkMemoryAllocateInfo* allocate_info) {}

    virtual void ReportBindBufferIncompatibility(VkBuffer     buffer,
                                                 ResourceData allocator_resource_data,
                                                 MemoryData   allocator_memory_data)
    {}

    virtual void ReportBindBuffer2Incompatibility(uint32_t                      bind_info_count,
                                                  const VkBindBufferMemoryInfo* bind_infos,
                                                  const ResourceData*           allocator_resource_datas,
                                                  const MemoryData*             allocator_memory_datas)
    {}

    virtual void ReportBindImageIncompatibility(VkImage      image,
                                                ResourceData allocator_resource_data,
                                                MemoryData   allocator_memory_data)
    {}

    virtual void ReportBindImage2Incompatibility(uint32_t                     bind_info_count,
                                                 const VkBindImageMemoryInfo* bind_infos,
                                                 const ResourceData*          allocator_resource_datas,
                                                 const MemoryData*            allocator_memory_datas)
    {}

    virtual void ReportBindVideoSessionIncompatibility(VkVideoSessionKHR                      video_session,
                                                       uint32_t                               bind_info_count,
                                                       const VkBindVideoSessionMemoryInfoKHR* bind_infos,
                                                       const ResourceData*                    allocator_resource_datas,
                                                       const MemoryData*                      allocator_memory_datas)
    {}

    std::function<void(const VkBufferCreateInfo*)> OnCreateBufferDirect;

    virtual VkResult CreateBufferDirect(const VkBufferCreateInfo*    create_info,
                                        const VkAllocationCallbacks* allocation_callbacks,
                                        VkBuffer*                    buffer,
                                        ResourceData*                allocator_data)
    {
        OnCreateBufferDirect(create_info);
        return VK_SUCCESS;
    }

    virtual void
    DestroyBufferDirect(VkBuffer buffer, const VkAllocationCallbacks* allocation_callbacks, ResourceData allocator_data)
    {}

    virtual VkResult CreateImageDirect(const VkImageCreateInfo*     create_info,
                                       const VkAllocationCallbacks* allocation_callbacks,
                                       VkImage*                     image,
                                       ResourceData*                allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual void
    DestroyImageDirect(VkImage image, const VkAllocationCallbacks* allocation_callbacks, ResourceData allocator_data)
    {}

    virtual VkResult AllocateMemoryDirect(const VkMemoryAllocateInfo*  allocate_info,
                                          const VkAllocationCallbacks* allocation_callbacks,
                                          VkDeviceMemory*              memory,
                                          MemoryData*                  allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual void FreeMemoryDirect(VkDeviceMemory               memory,
                                  const VkAllocationCallbacks* allocation_callbacks,
                                  MemoryData                   allocator_data)
    {}

    virtual VkResult BindBufferMemoryDirect(VkBuffer               buffer,
                                            VkDeviceMemory         memory,
                                            VkDeviceSize           memory_offset,
                                            ResourceData           allocator_buffer_data,
                                            MemoryData             allocator_memory_data,
                                            VkMemoryPropertyFlags* bind_memory_properties)
    {
        return VK_SUCCESS;
    }

    virtual VkResult BindImageMemoryDirect(VkImage                image,
                                           VkDeviceMemory         memory,
                                           VkDeviceSize           memory_offset,
                                           ResourceData           allocator_image_data,
                                           MemoryData             allocator_memory_data,
                                           VkMemoryPropertyFlags* bind_memory_properties)
    {
        return VK_SUCCESS;
    }

    virtual void
    BindMemoryImageAHardwareBuffer(MemoryData* allocator_memory_data, VkImage image, void* ahardwarebuffer_info)
    {}

    virtual VkResult
    MapResourceMemoryDirect(VkDeviceSize size, VkMemoryMapFlags flags, void** data, ResourceData allocator_data)
    {
        return VK_SUCCESS;
    }

    virtual void UnmapResourceMemoryDirect(ResourceData allocator_data) {}

    virtual VkResult FlushMappedMemoryRangesDirect(uint32_t                   memory_range_count,
                                                   const VkMappedMemoryRange* memory_ranges,
                                                   const MemoryData*          allocator_datas)
    {
        return VK_SUCCESS;
    }

    virtual VkResult InvalidateMappedMemoryRangesDirect(uint32_t                   memory_range_count,
                                                        const VkMappedMemoryRange* memory_ranges,
                                                        const MemoryData*          allocator_datas)
    {
        return VK_SUCCESS;
    }

    virtual bool SupportsOpaqueDeviceAddresses() { return false; }

    virtual bool SupportsExternalMemory() { return true; }

    std::function<size_t(VulkanResourceAllocator::ResourceData)> OnGetBufferSize;
    virtual size_t GetBufferSize(VulkanResourceAllocator::ResourceData alloc_data)
    {
        return OnGetBufferSize(alloc_data);
    }

    virtual bool SupportBindVideoSessionMemory() { return true; }
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_RESOURCE_ALLOCATOR_MOCK_H