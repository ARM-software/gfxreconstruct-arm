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

#ifndef GFXRECON_DECODE_VULKAN_INTERNAL_BUFFER_MANAGER_H
#define GFXRECON_DECODE_VULKAN_INTERNAL_BUFFER_MANAGER_H

#include "decode/vulkan_resource_allocator.h"
#include "decode/descriptor_update_template_decoder.h"
#include "decode/vulkan_object_info_table.h"
#include "util/defines.h"
#include "util/marking_layers.h"

#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanInternalBufferManager
{
  public:
    struct BufferInfoWrapper
    {
        VulkanBufferInfo                info_;
        VulkanDeviceMemoryInfo          memory_info_;
        VulkanResourceAllocator*        allocator_;
        const VulkanPhysicalDeviceInfo* physical_device_info_;

        BufferInfoWrapper(VulkanBufferInfo                buffer_info,
                          VulkanDeviceMemoryInfo          memory_info,
                          VulkanResourceAllocator*        allocator,
                          const VulkanPhysicalDeviceInfo* physical_device_info) :
            info_(buffer_info),
            memory_info_(memory_info), allocator_(allocator), physical_device_info_(physical_device_info)
        {}
        ~BufferInfoWrapper()
        {
            util::MarkingLayersUtil::instance().BeginInjected(physical_device_info_);
            allocator_->DestroyBufferDirect(info_.handle, nullptr, info_.allocator_data);
            allocator_->FreeMemoryDirect(memory_info_.handle, nullptr, memory_info_.allocator_data);
            util::MarkingLayersUtil::instance().EndInjected(physical_device_info_);
        }
    };

    VulkanInternalBufferManager(const encode::VulkanDeviceTable*        device_table,
                                const VulkanPhysicalDeviceInfo*         physical_device_info,
                                VkDevice                                device,
                                VulkanResourceAllocator*                allocator,
                                const VkPhysicalDeviceMemoryProperties& properties);

    ~VulkanInternalBufferManager();

    void AddEntry(std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>& buffer_entry);

    std::unique_ptr<BufferInfoWrapper>
    CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_prop_flags = {});

    VkDeviceAddress GetBufferDeviceAddress(VkBuffer buffer);

  private:
    void InitializeFunctionPointers(const encode::VulkanDeviceTable* device_table);
    struct Functions
    {
        PFN_vkGetBufferDeviceAddress    get_buffer_device_address{ nullptr };
        PFN_vkGetBufferDeviceAddressKHR get_buffer_device_address_khr{ nullptr };
    };

  private:
    Functions                                       functions_;
    VkDevice                                        device_;
    VulkanResourceAllocator*                        allocator_;
    VkPhysicalDeviceMemoryProperties                physical_device_memory_properties_;
    const VulkanPhysicalDeviceInfo*                 physical_device_info_;
    std::vector<std::unique_ptr<BufferInfoWrapper>> buffers_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_INTERNAL_BUFFER_MANAGER_H
