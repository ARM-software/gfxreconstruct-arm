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
#include "vulkan_acceleration_structure_builder.h"

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
    acceleration_structures_.emplace_back(std::make_unique<AccelerationStructureEntry>(
        device_address, 0, handle, VkAccelerationStructureBuildSizesInfoKHR()));
}

void VulkanAccelerationStructureBuilder::UntrackAccelerationStructure(
    const AccelerationStructureKHRInfo* acceleration_structure_info)
{
    auto ac_begin = acceleration_structures_.begin();
    auto ac_end   = acceleration_structures_.end();
    auto result   = std::find_if(
        ac_begin, ac_end, [&acceleration_structure_info](const std::unique_ptr<AccelerationStructureEntry>& entry) {
            return acceleration_structure_info->handle == entry->handle_;
        });

    if (result != acceleration_structures_.end() && result->get()->replacement_acceleration_struct_)
    {

        const auto& real_as = result->get()->replacement_acceleration_struct_;
        functions_.destroy_buffer(device_, real_as->scratch_, nullptr);
        functions_.destroy_buffer(device_, real_as->storage_, nullptr);
        functions_.destroy_acceleration_structure(device_, real_as->handle_, nullptr);
    }
}

void VulkanAccelerationStructureBuilder::SetBufferInfo(BufferInfo*     buffer_info,
                                                       VkDeviceAddress original_address,
                                                       VkDeviceAddress new_address)
{
    if (new_address == 0)
    {
        new_address = GetBufferDeviceAddress(buffer_info->handle);
    }
    auto existing_buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
        return entry->buffer_info_->handle == buffer_info->handle;
    });
    if (existing_buffer != buffers_.end())
    {
        if ((*existing_buffer)->original_address_ == 0)
        {
            (*existing_buffer)->original_address_ = original_address;
        }
        if ((*existing_buffer)->new_address_ == 0)
        {
            (*existing_buffer)->new_address_ = new_address;
        }
        if ((*existing_buffer)->buffer_info_ == nullptr)
        {
            (*existing_buffer)->buffer_info_ = buffer_info;
        }
    }
    else
    {
        buffers_.push_back(std::make_unique<BufferEntry>(original_address, new_address, buffer_info));
    }
}

void VulkanAccelerationStructureBuilder::UntrackBufferInfo(const BufferInfo* buffer_info)
{
    auto result =
        std::find_if(buffers_.begin(), buffers_.end(), [&buffer_info](const std::unique_ptr<BufferEntry>& entry) {
            return entry->buffer_info_->capture_id == buffer_info->capture_id;
        });
    if (result != buffers_.end())
    {
        buffers_.erase(result);
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

    VkMemoryRequirements requirements{};
    functions_.get_buffer_memory_requirements(device_, buffer, &requirements);

    VkMemoryAllocateInfo allocate_info{ .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                        .allocationSize  = requirements.size,
                                        .memoryTypeIndex = 1 };

    VkDeviceMemory                      memory{};
    VulkanResourceAllocator::MemoryData memory_allocator_data{};
    allocator_->AllocateMemory(&allocate_info, nullptr, format::kNullHandleId, &memory, &memory_allocator_data);

    VkMemoryPropertyFlags bind_flags{};
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
    // If we have already updates this buffer, the acceleration structure address would not be found by above code
    else if (std::find_if(acceleration_structures_.begin(), acceleration_structures_.end(), [&](const auto& entry) {
                 return entry->new_address_ == address;
             }) == acceleration_structures_.end())
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
        buffer              = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
            auto buffer_size = allocator_->GetBufferSize(entry->buffer_info_->allocator_data);
            return entry->original_address_ < address && (entry->original_address_ + buffer_size) > address;
        });
        GFXRECON_ASSERT(buffer != buffers_.end());
        offset  = address - (*buffer)->original_address_;
        address = (*buffer)->new_address_ + offset;
    }
}

VulkanAccelerationStructureBuilder::BufferEntry*
VulkanAccelerationStructureBuilder::GetBufferByDeviceAddress(VkDeviceAddress runtime_address)
{
    auto buffer = std::find_if(
        buffers_.begin(), buffers_.end(), [&](const auto& entry) { return entry->new_address_ == runtime_address; });
    if (buffer == buffers_.end())
    {
        buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
            auto buffer_size = allocator_->GetBufferSize(entry->buffer_info_->allocator_data);
            return entry->new_address_ < runtime_address && (entry->new_address_ + buffer_size) > runtime_address;
        });
    }
    GFXRECON_ASSERT(buffer != buffers_.end());
    return buffer->get();
}

void VulkanAccelerationStructureBuilder::UpdateDescriptorSets(VkWriteDescriptorSetAccelerationStructureKHR* ac_write)
{
    for (uint32_t i = 0; i < ac_write->accelerationStructureCount; ++i)
    {
        AccelerationStructureEntry* entry = GetAccelerationStructureEntry(ac_write->pAccelerationStructures[i]);
        if (entry && entry->replacement_acceleration_struct_)
        {
            const_cast<VkAccelerationStructureKHR*>(ac_write->pAccelerationStructures)[i] =
                entry->replacement_acceleration_struct_->handle_;
        }
    }
}

// Map accel struct HandleId to AccelerationStructureKHR handle
void VulkanAccelerationStructureBuilder::UpdateDescriptorSetWithTemplateKHR(
    gfxrecon::decode::DescriptorUpdateTemplateDecoder* descriptor)
{
    const size_t accel_struct_count = descriptor->GetAccelerationStructureKHRCount();
    for (size_t i = 0; i < accel_struct_count; ++i)
    {
        format::HandleId           accel_struct_id = descriptor->GetAccelerationStructureKHRHandleIdsPointer()[i];
        VkAccelerationStructureKHR accel_struct    = descriptor->GetAccelerationStructureKHRPointer()[i];

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

void VulkanAccelerationStructureBuilder::UpdateInstanceBuffer(
    VkAccelerationStructureGeometryInstancesDataKHR& instances,
    const VkAccelerationStructureBuildRangeInfoKHR&  build_range)
{
    if (instances.arrayOfPointers)
    {
        throw "Unsupported";
    }
    // update device address of the instance buffer
    UpdateBufferDeviceAddress(instances.data.deviceAddress);
    // find buffer by device address
    BufferInfo* instance_buffer = GetBufferByDeviceAddress(instances.data.deviceAddress)->buffer_info_;

    VkAccelerationStructureInstanceKHR* data;
    // TODO Handle possible primitive_offset
    allocator_->MapResourceMemoryDirect(sizeof(VkAccelerationStructureInstanceKHR) * build_range.primitiveCount,
                                        0,
                                        (void**)&data,
                                        instance_buffer->allocator_data + build_range.primitiveOffset);

    for (uint32_t instance_index = 0; instance_index < build_range.primitiveCount; ++instance_index)
    {
        UpdateAccelerationStructDeviceAddress(data[instance_index].accelerationStructureReference);
    }

    allocator_->UnmapResourceMemoryDirect(instance_buffer->allocator_data);
}

void VulkanAccelerationStructureBuilder::UpdateDeviceAddress(
    VkAccelerationStructureBuildGeometryInfoKHR& build_geometry, VkAccelerationStructureBuildRangeInfoKHR* range_infos)
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
                UpdateInstanceBuffer(instances, range_infos[geometry_index]);
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

VulkanAccelerationStructureBuilder::AccelerationStructureEntry*
VulkanAccelerationStructureBuilder::GetAccelerationStructureEntry(VkAccelerationStructureKHR acceleration_struct)
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
        const auto& mode = geometry_infos[i].mode;
        if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR)
        {
            // Create new acceleration structure and scratch of required size
            VkAccelerationStructureBuildSizesInfoKHR size_info =
                GetAccelerationStructureSizeInfo(&geometry_infos[i], range_infos[i]);
            VkBuffer storage    = CreateBuffer(size_info.accelerationStructureSize,
                                            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
            auto replacement_as = CreateAccelerationStructure(geometry_infos[i], range_infos[i], size_info, storage);
            auto scratch        = CreateBuffer(size_info.buildScratchSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

            // Update all device addresses in geometries
            UpdateDeviceAddress(geometry_infos[i], range_infos[i]);
            auto original_as                            = geometry_infos[i].dstAccelerationStructure;
            geometry_infos[i].dstAccelerationStructure  = replacement_as;
            geometry_infos[i].scratchData.deviceAddress = GetBufferDeviceAddress(scratch);

            // Store acceleration structure data
            auto original_as_entry      = GetAccelerationStructureEntry(original_as);
            auto replacement_as_address = GetDeviceAddress(replacement_as);

            original_as_entry->replacement_acceleration_struct_ = std::make_unique<AccelerationStructureEntry>(
                0, replacement_as_address, replacement_as, size_info, storage, scratch);
            original_as_entry->new_address_ = GetDeviceAddress(original_as);
        }
        else if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR)
        {
            // create new scratch for update
            VkAccelerationStructureBuildSizesInfoKHR size_info =
                GetAccelerationStructureSizeInfo(&geometry_infos[i], range_infos[i]);
            auto scratch                                = CreateBuffer(size_info.updateScratchSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            geometry_infos[i].scratchData.deviceAddress = GetBufferDeviceAddress(scratch);

            // update srcAccelerationStructure handle
            auto original_src_as = GetAccelerationStructureEntry(geometry_infos[i].srcAccelerationStructure);
            GFXRECON_ASSERT(original_src_as);
            GFXRECON_ASSERT(original_src_as->replacement_acceleration_struct_.get() != nullptr);
            GFXRECON_ASSERT(original_src_as->replacement_acceleration_struct_->handle_ != 0);
            auto replacement_src_as                    = original_src_as->replacement_acceleration_struct_->handle_;
            geometry_infos[i].srcAccelerationStructure = replacement_src_as;

            // update dstAccelerationStructure handle
            // dst AS was created but not built, record new device address
            auto original_dst_as = GetAccelerationStructureEntry(geometry_infos[i].dstAccelerationStructure);
            GFXRECON_ASSERT(original_dst_as);
            original_dst_as->new_address_ = GetDeviceAddress(geometry_infos[i].dstAccelerationStructure);

            // update geometry buffers
            UpdateDeviceAddress(geometry_infos[i], range_infos[i]);
        }
    }
    functions_.cmd_build_acceleration_structures(commandBuffer, info_count, geometry_infos, range_infos);
}

void VulkanAccelerationStructureBuilder::CmdCopyAccelerationStructure(VkCommandBuffer                     commandBuffer,
                                                                      VkCopyAccelerationStructureInfoKHR* copy_info)
{
    // In the typical compaction scenario, we copy the built acceleration structure to a smaller storage,
    // which is only created but not built
    VkCopyAccelerationStructureInfoKHR modified_info = *copy_info;

    if (VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR == modified_info.mode)
    {
        // First, get the replay device address of the destination structure and store it
        auto compacted_entry = GetAccelerationStructureEntry(modified_info.dst);

        const VkAccelerationStructureDeviceAddressInfoKHR dst_device_address_info{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, nullptr, modified_info.dst
        };

        compacted_entry->new_address_ =
            functions_.get_acceleration_structure_device_address(device_, &dst_device_address_info);

        // Replace the handle to be of the real built structure
        auto original_entry = GetAccelerationStructureEntry(modified_info.src);
        modified_info.src   = original_entry->replacement_acceleration_struct_->handle_;
    }
    functions_.cmd_copy_acceleration_structure(commandBuffer, &modified_info);
}

void VulkanAccelerationStructureBuilder::CmdWriteAccelerationStructuresProperties(
    VkCommandBuffer             command_buffer,
    uint32_t                    count,
    VkAccelerationStructureKHR* acceleration_structures,
    VkQueryType                 query_type,
    VkQueryPool                 pool,
    uint32_t                    first_query)
{
    for (uint32_t index = 0; index < count; ++index)
    {
        VkAccelerationStructureKHR capture_handle = acceleration_structures[index];
        auto                       entry          = GetAccelerationStructureEntry(capture_handle);
        if (entry->replacement_acceleration_struct_)
        {
            acceleration_structures[index] = entry->replacement_acceleration_struct_->handle_;
        }
        else
        {
            acceleration_structures[index] = entry->handle_;
        }
    }

    functions_.cmd_write_acceleration_structures_properties(
        command_buffer, count, acceleration_structures, query_type, pool, first_query);
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
    const VkAccelerationStructureBuildSizesInfoKHR& size_info,
    VkBuffer                                        storage)
{
    VkAccelerationStructureCreateInfoKHR create_info = {
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = storage,
        .size   = size_info.accelerationStructureSize,
        .type   = geometry_info.type,
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
        primitive_counts[i] = range_info->primitiveCount;
    }
    VkAccelerationStructureBuildSizesInfoKHR size_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
                                                        nullptr };
    functions_.get_acceleration_structure_build_sizes(
        device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, geometry_info, primitive_counts.data(), &size_info);
    return size_info;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
