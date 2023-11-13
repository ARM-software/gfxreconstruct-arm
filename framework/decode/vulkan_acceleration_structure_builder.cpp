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

#include "decode/vulkan_acceleration_structure_builder.h"
#include <algorithm>
#include <map>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanAccelerationStructureBuilder::VulkanAccelerationStructureBuilder(Functions                functions,
                                                                       VkDevice                 device,
                                                                       VulkanResourceAllocator* allocator) :
    functions_(functions),
    device_(device), allocator_(allocator)
{}

void VulkanAccelerationStructureBuilder::RegisterAccelerationStructure(VkAccelerationStructureKHR handle,
                                                                       VkDeviceAddress            device_address)
{
    acceleration_structures_.push_back(std::make_unique<AccelerationStructureEntry>(
        device_address, 0, handle, VkAccelerationStructureBuildSizesInfoKHR()));
}

void VulkanAccelerationStructureBuilder::SetBufferInfo(BufferInfo* buffer_info, VkDeviceAddress original_address, VkDeviceAddress new_address)
{
    auto existing_buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
        return entry->buffer_info_->handle == buffer_info->handle;
    });
    if (existing_buffer != buffers_.end())
    {
        (*existing_buffer)->original_address_ = original_address;
        (*existing_buffer)->new_address_      = new_address;
    } else {
        buffers_.push_back(std::make_unique<BufferEntry>(original_address, new_address, buffer_info));
    }
}

VkBuffer VulkanAccelerationStructureBuilder::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage)
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
    allocator_->CreateBuffer(&create_info, nullptr, format::kNullHandleId, &buffer, &buffer_allocator_data);

    VkMemoryAllocateInfo allocate_info{ .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                        .allocationSize  = size,
                                        .memoryTypeIndex = 1 };

    VkDeviceMemory                      memory{};
    VulkanResourceAllocator::MemoryData memory_allocator_data{};
    allocator_->AllocateMemory(&allocate_info, nullptr, format::kNullHandleId, &memory, &memory_allocator_data);

    VkMemoryPropertyFlags bind_flags;
    allocator_->BindBufferMemory(buffer, memory, 0, buffer_allocator_data, memory_allocator_data, &bind_flags);

    return buffer;
}

VkDeviceAddress VulkanAccelerationStructureBuilder::GetBufferDeviceAddress(VkBuffer buffer)
{
    VkBufferDeviceAddressInfo info;
    info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.pNext  = nullptr;
    info.buffer = buffer;
    return functions_.get_buffer_device_address(device_, &info);
}

// overwrites acceleration structure capture device address with runtime device address
void VulkanAccelerationStructureBuilder::UpdateAccelerationStructDeviceAddress(VkDeviceAddress& address)
{
    if (address == 0)
        return;
    auto as = std::find_if(acceleration_structures_.begin(), acceleration_structures_.end(), [&](const auto& entry) {
        return entry->original_address_ == address;
    });
    if (as != acceleration_structures_.end())
    {
        if ((*as)->replacement_acceleration_struct_)
        {
            address = (*as)->replacement_acceleration_struct_->new_address_;
        }
        else
        {
            address = (*as)->new_address_;
        }
    } 
    else 
    {
        throw "Acceleration structure address not found";
    }
}

// overwrites buffer capture device address with runtime device address
void VulkanAccelerationStructureBuilder::UpdateBufferDeviceAddress(VkDeviceAddress& address)
{
    if (address == 0)
        return;
    auto buffer = std::find_if(
        buffers_.begin(), buffers_.end(), [&](const auto& entry) { return entry->original_address_ == address; });
    if (buffer != buffers_.end())
    {
        address = (*buffer)->new_address_;
    } 
    else 
    {
        VkDeviceSize offset = 0;
        auto         buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
            auto buffer_size = allocator_->GetBufferSize(entry->buffer_info_->allocator_data);
            return entry->original_address_ > address && (entry->original_address_ + buffer_size) < address;
        });

        offset  = address - (*buffer)->original_address_;
        address = (*buffer)->new_address_ + offset;
    }
}

VulkanAccelerationStructureBuilder::BufferEntry* VulkanAccelerationStructureBuilder::GetBufferByDeviceAddress(VkDeviceAddress runtime_address)
{
    auto buffer = std::find_if(
        buffers_.begin(), buffers_.end(), [&](const auto& entry) { return entry->new_address_ == runtime_address; });
    if (buffer != buffers_.end())
    {
        return buffer->get();
    } 
    else 
    {
        VkDeviceSize offset = 0;
        auto         buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
            auto buffer_size = allocator_->GetBufferSize(entry->buffer_info_->allocator_data);
            return entry->new_address_ > runtime_address && (entry->new_address_ + buffer_size) < runtime_address;
        });
        return buffer->get();
    }
}

// Map accel struct HandleId to AccelerationStructureKHR handle
void VulkanAccelerationStructureBuilder::UpdateDescriptorSetWithTemplateKHR(gfxrecon::decode::DescriptorUpdateTemplateDecoder *descriptor)
{
    const size_t accel_struct_count = descriptor->GetAccelerationStructureKHRCount();
    for(size_t i = 0; i < accel_struct_count; ++i)
    {
        format::HandleId accel_struct_id = descriptor->GetAccelerationStructureKHRHandleIdsPointer()[i];
        VkAccelerationStructureKHR accel_struct = descriptor->GetAccelerationStructureKHRPointer()[i];

        auto target_as = std::find_if(std::begin(acceleration_structures_),
                                      std::end(acceleration_structures_),
                                      [&](const auto& as) -> bool { return as->handle_ == accel_struct; });
        if (target_as != std::end(acceleration_structures_))
        {
            descriptor->GetAccelerationStructureKHRPointer()[i] =
                (*target_as)->replacement_acceleration_struct_->handle_;
        }
    }
}

void VulkanAccelerationStructureBuilder::UpdateInstanceBuffer(VkAccelerationStructureGeometryInstancesDataKHR &instances)
{
    if(instances.arrayOfPointers)
    {
        throw "Unsupported";
    }
    // update device address of the instance buffer
    UpdateBufferDeviceAddress(instances.data.deviceAddress);
    // find buffer by device address
    BufferInfo* instance_buffer = GetBufferByDeviceAddress(instances.data.deviceAddress)->buffer_info_;
    VkAccelerationStructureInstanceKHR* data;
    allocator_->MapResourceMemoryDirect(sizeof(VkAccelerationStructureInstanceKHR), 0, (void**)&data, instance_buffer->allocator_data);
    UpdateAccelerationStructDeviceAddress(data->accelerationStructureReference);
    allocator_->UnmapResourceMemoryDirect(instance_buffer->allocator_data);
}

void VulkanAccelerationStructureBuilder::UpdateDeviceAddress(
    VkAccelerationStructureBuildGeometryInfoKHR& build_geometry)
{
    for (uint32_t geometry_index = 0; geometry_index < build_geometry.geometryCount; ++geometry_index)
    {
        auto& geometry_data =
            const_cast<VkAccelerationStructureGeometryKHR*>(build_geometry.pGeometries)[geometry_index];
        if (geometry_data.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
        {
            if (geometry_data.geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR)
            {
                auto& triangles = geometry_data.geometry.triangles;
                UpdateBufferDeviceAddress(triangles.indexData.deviceAddress);
                UpdateBufferDeviceAddress(triangles.transformData.deviceAddress);
                UpdateBufferDeviceAddress(triangles.vertexData.deviceAddress);
            }
            else if (geometry_data.geometryType == VK_GEOMETRY_TYPE_INSTANCES_KHR)
            {
                // instance data - find the instance buffer by device address, map it, update referenced bottom level AS
                // address
                auto& instances = geometry_data.geometry.instances;
                UpdateInstanceBuffer(instances);
            }
        }
    }
}

void VulkanAccelerationStructureBuilder::SetAccelerationStructureEntry(VkAccelerationStructureKHR acceleration_struct,
                                                                       VkDeviceAddress            original_address,
                                                                       VkDeviceAddress            new_address)
{
    auto entry = GetAccelerationStructureEntry(acceleration_struct);
    if (entry)
    {
        entry->original_address_ = original_address;
        entry->new_address_      = new_address;
    }
    else
    {
        acceleration_structures_.push_back(std::make_unique<AccelerationStructureEntry>(
            original_address, new_address, acceleration_struct, VkAccelerationStructureBuildSizesInfoKHR()));
    }
}

VulkanAccelerationStructureBuilder::AccelerationStructureEntry* VulkanAccelerationStructureBuilder::GetAccelerationStructureEntry(VkAccelerationStructureKHR acceleration_struct)
{
    auto entry = std::find_if(acceleration_structures_.begin(), acceleration_structures_.end(), [&](auto& entry) {
        return entry->handle_ == acceleration_struct;
    });
    if (entry != acceleration_structures_.end())
    {
        return entry->get();
    }
    else
    {
        return nullptr;
    }
}

void VulkanAccelerationStructureBuilder::CmdBuildAccelerationStructures(
    VkCommandBuffer                              commandBuffer,
    uint32_t                                     info_count,
    VkAccelerationStructureBuildGeometryInfoKHR* geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**   range_infos)
{
    for (uint32_t i = 0; i < info_count; ++i)
    {
        if (geometry_infos[i].mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR)
        {
            // Create new acceleration structure and scratch of required size
            VkAccelerationStructureBuildSizesInfoKHR size_info =
                GetAccelerationStructureSizeInfo(&geometry_infos[i], range_infos[i]);
            auto replacement_as = CreateAccelerationStructure(geometry_infos[i], range_infos[i], size_info);
            auto scratch        = CreateBuffer(size_info.buildScratchSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

            // Update all device addresses in geometries
            UpdateDeviceAddress(geometry_infos[i]);
            auto original_as                            = geometry_infos[i].dstAccelerationStructure;
            geometry_infos[i].dstAccelerationStructure  = replacement_as;
            geometry_infos[i].scratchData.deviceAddress = GetBufferDeviceAddress(scratch);

            // Store acceleration structure data
            auto original_as_entry      = GetAccelerationStructureEntry(original_as);
            auto replacement_as_address = GetDeviceAddress(replacement_as);

            original_as_entry->replacement_acceleration_struct_ =
                std::make_unique<AccelerationStructureEntry>(0, replacement_as_address, replacement_as, size_info);
            original_as_entry->new_address_ = GetDeviceAddress(original_as);
        }
    }
    functions_.cmd_build_acceleration_structures(commandBuffer, info_count, geometry_infos, range_infos);
}

VkDeviceAddress VulkanAccelerationStructureBuilder::GetDeviceAddress(VkAccelerationStructureKHR acceleration_structure)
{
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext                 = nullptr,
        .accelerationStructure = acceleration_structure
    };
    return functions_.get_acceleration_structure_device_address(device_, &info);
}

VkAccelerationStructureKHR VulkanAccelerationStructureBuilder::CreateAccelerationStructure(
    VkAccelerationStructureBuildGeometryInfoKHR&    geometry_info,
    VkAccelerationStructureBuildRangeInfoKHR*       range_info,
    const VkAccelerationStructureBuildSizesInfoKHR& size_info)
{
    VkBuffer buffer_as =
        CreateBuffer(size_info.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);

    VkAccelerationStructureCreateInfoKHR create_info = {
        .sType       = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .createFlags = geometry_info.flags,
        .buffer      = buffer_as,
        .size        = size_info.accelerationStructureSize,
        .type        = geometry_info.type,
    };
    VkAccelerationStructureKHR acceleration_structure;
    VkResult status = functions_.create_acceleration_structure(device_, &create_info, nullptr, &acceleration_structure);
    return acceleration_structure;
}

VkAccelerationStructureBuildSizesInfoKHR VulkanAccelerationStructureBuilder::GetAccelerationStructureSizeInfo(
    VkAccelerationStructureBuildGeometryInfoKHR* geometry_info, VkAccelerationStructureBuildRangeInfoKHR* range_info)
{
    std::vector<uint32_t> primitive_counts(geometry_info->geometryCount);
    for (uint32_t i = 0; i < geometry_info->geometryCount; ++i)
    {
        primitive_counts.push_back(range_info->primitiveCount);
    }
    VkAccelerationStructureBuildSizesInfoKHR size_info;
    functions_.get_acceleration_structure_build_sizes(
        device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, geometry_info, primitive_counts.data(), &size_info);
    return size_info;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
