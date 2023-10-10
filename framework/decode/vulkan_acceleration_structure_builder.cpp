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

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanAccelerationStructureBuilder::VulkanAccelerationStructureBuilder(Functions                functions,
                                                                       VkDevice                 device,
                                                                       VulkanResourceAllocator* allocator) :
    functions_(functions),
    device_(device), allocator_(allocator)
{}

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

VkDeviceAddress VulkanAccelerationStructureBuilder::GetDeviceAddress(VkBuffer b)
{
    VkBufferDeviceAddressInfo info;
    info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.pNext  = nullptr;
    info.buffer = b;
    return functions_.get_buffer_device_address(device_, &info);
}

void VulkanAccelerationStructureBuilder::UpdateDeviceAddress(VkDeviceAddress& address)
{
    address = capture_replay_address_map_[address];
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
                UpdateDeviceAddress(triangles.indexData.deviceAddress);
                UpdateDeviceAddress(triangles.transformData.deviceAddress);
                UpdateDeviceAddress(triangles.vertexData.deviceAddress);
            }
        }
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
        // Create new acceleration structure and scratch of required size
        VkAccelerationStructureBuildSizesInfoKHR size_info =
            GetAccelerationStructureSizeInfo(&geometry_infos[i], range_infos[i]);
        auto replacement_as = CreateAccelerationStructure(geometry_infos[i], range_infos[i], size_info);
        auto scratch        = CreateBuffer(size_info.buildScratchSize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

        // Update all device addresses in geometries
        UpdateDeviceAddress(geometry_infos[i]);
        geometry_infos[i].dstAccelerationStructure  = replacement_as;
        geometry_infos[i].scratchData.deviceAddress = GetDeviceAddress(scratch);

        // Store acceleration structure data
        acceleration_structures_.push_back({ replacement_as, GetDeviceAddress(replacement_as) });
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

void VulkanAccelerationStructureBuilder::AddDeviceAddressPair(VkDeviceAddress capture, VkDeviceAddress replay)
{
    capture_replay_address_map_[capture] = replay;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
