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

#include "graphics/vulkan_resources_util.h"
#include "decode/vulkan_internal_buffer_manager.h"
#include <algorithm>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanInternalBufferManager::VulkanInternalBufferManager(const graphics::VulkanDeviceTable*      device_table,
                                                         VkDevice                                device,
                                                         VulkanResourceAllocator*                allocator,
                                                         const VkPhysicalDeviceMemoryProperties& memory_properties) :
    device_(device), allocator_(allocator), physical_device_memory_properties_(memory_properties),
    dispatcher_(device_table)
{}

VulkanInternalBufferManager::~VulkanInternalBufferManager()
{
    buffers_.clear();
}

void VulkanInternalBufferManager::AddEntry(
    std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>& buffer_entry)
{
    auto existing_buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
        return entry->info_.handle == buffer_entry->info_.handle;
    });

    // Handle reuse, update the data in the entry
    if (existing_buffer != buffers_.end())
    {
        *existing_buffer = std::move(buffer_entry);
    }
    else
    {
        buffers_.emplace_back(std::move(buffer_entry));
    }
}

// TODO: this is exactly the same method as in VulkanInternalBufferManager. Try to remove this duplication
VkDeviceAddress VulkanInternalBufferManager::GetBufferDeviceAddress(VkBuffer buffer)
{
    VkBufferDeviceAddressInfo info;
    info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.pNext  = nullptr;
    info.buffer = buffer;

    VkDeviceAddress result = 0;

    util::MarkingLayersUtil::BeginInjected(device_);
    result = dispatcher_.GetBufferDeviceAddress(device_, &info);
    util::MarkingLayersUtil::EndInjected(device_);

    return result;
}

std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> VulkanInternalBufferManager::CreateBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_prop_flags)
{
    VkBuffer buffer;

    VkBufferCreateInfo create_info    = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    create_info.pNext                 = nullptr;
    create_info.flags                 = 0;
    create_info.size                  = size;
    create_info.usage                 = usage;
    create_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices   = nullptr;

    VulkanResourceAllocator::ResourceData buffer_allocator_data;
    util::MarkingLayersUtil::BeginInjected(device_);

    allocator_->CreateBufferDirect(&create_info, nullptr, &buffer, &buffer_allocator_data);

    VkMemoryRequirements requirements{};
    allocator_->GetBufferMemoryRequirements(buffer, &requirements, buffer_allocator_data);

    uint32_t              mem_type_index = 1;
    VkMemoryPropertyFlags desired_flags{ mem_prop_flags };
    VkMemoryPropertyFlags found_flags{};

    graphics::FindMemoryTypeIndex(
        physical_device_memory_properties_, requirements.memoryTypeBits, desired_flags, &mem_type_index, &found_flags);

    VkMemoryAllocateInfo allocate_info;
    allocate_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext           = nullptr;
    allocate_info.allocationSize  = requirements.size;
    allocate_info.memoryTypeIndex = mem_type_index;

    VkDeviceMemory                      memory{};
    VulkanResourceAllocator::MemoryData memory_allocator_data{};
    allocator_->AllocateMemoryDirect(&allocate_info, nullptr, &memory, &memory_allocator_data);

    allocator_->BindBufferMemoryDirect(buffer, memory, 0, buffer_allocator_data, memory_allocator_data, &found_flags);
    util::MarkingLayersUtil::EndInjected(device_);

    std::unique_ptr<BufferInfoWrapper> entry =
        std::make_unique<BufferInfoWrapper>(VulkanBufferInfo(), VulkanDeviceMemoryInfo(), allocator_, device_);
    entry->info_.allocator_data        = buffer_allocator_data;
    entry->info_.handle                = buffer;
    entry->info_.replay_size           = size;
    entry->memory_info_.handle         = memory;
    entry->memory_info_.allocator_data = memory_allocator_data;

    if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        entry->info_.replay_address = GetBufferDeviceAddress(buffer);
    }

    return entry;
}
GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)