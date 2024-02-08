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
#include "graphics/vulkan_resources_util.h"
#include "decode/vulkan_acceleration_structure_builder.h"
#include <algorithm>
#include <map>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanAccelerationStructureBuilder::VulkanAccelerationStructureBuilder(
    Functions                               functions,
    VkDevice                                device,
    VulkanResourceAllocator*                allocator,
    const VkPhysicalDeviceMemoryProperties& memory_properties) :
    functions_(functions),
    device_(device), allocator_(allocator), physical_device_memory_properties_(memory_properties)
{}

VulkanAccelerationStructureBuilder::~VulkanAccelerationStructureBuilder()
{
    for (auto& entry : acceleration_structures_)
    {
        if (entry->replacement_acceleration_struct_)
        {
            functions_.destroy_acceleration_structure(
                device_, entry->replacement_acceleration_struct_->handle_, nullptr);
        }
    }
}

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

    GFXRECON_ASSERT(result != acceleration_structures_.end());

    if (result->get()->replacement_acceleration_struct_)
    {
        const auto& real_as = result->get()->replacement_acceleration_struct_;
        functions_.destroy_acceleration_structure(device_, real_as->handle_, nullptr);
    }

    acceleration_structures_.erase(result);
}

void VulkanAccelerationStructureBuilder::ProcessBuildVulkanAccelerationStructuresMetaCommand(
    uint32_t                                                      info_count,
    VkAccelerationStructureBuildGeometryInfoKHR*                  geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**                    range_infos,
    std::vector<std::vector<VkAccelerationStructureInstanceKHR>>& instance_buffers_data)
{
    static const VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    // Retrieve / initialize the command executable structures or reuse the capture command buffer
    if (!cmd_execute_obj_)
    {
        InitializeInternalExecObjects();
    }
    VkCommandBuffer command_buffer = cmd_execute_obj_->command_buffer_;

    BeginCommandBuffer();

    std::vector<std::unique_ptr<BufferEntry>> state_recreation_buffer_entries;

    for (uint32_t i = 0; i < info_count; ++i)
    {
        for (uint32_t g = 0; g < geometry_infos[i].geometryCount; ++g)
        {
            if (geometry_infos[i].pGeometries[g].geometryType != VK_GEOMETRY_TYPE_INSTANCES_KHR)
            {
                continue;
            }

            VkDeviceSize buffer_size = range_infos[i][g].primitiveCount * sizeof(VkAccelerationStructureInstanceKHR);

            state_recreation_buffer_entries.push_back(
                CreateBuffer(buffer_size, usage, instance_buffers_data[i].data()));
            state_recreation_buffer_entries.back()->original_address_ =
                geometry_infos[i].pGeometries[g].geometry.instances.data.deviceAddress;
            state_recreation_buffer_entries.back()->new_address_ =
                GetBufferDeviceAddress(state_recreation_buffer_entries.back()->handle_);
        }
    }

    CmdBuildAccelerationStructures(command_buffer, info_count, geometry_infos, range_infos);

    ExecuteCommandBuffer();
}

void VulkanAccelerationStructureBuilder::ProcessCopyVulkanAccelerationStructuresMetaCommand(
    uint32_t info_count, VkCopyAccelerationStructureInfoKHR* copy_infos)
{
    if (!cmd_execute_obj_)
    {
        InitializeInternalExecObjects();
    }
    VkCommandBuffer command_buffer = cmd_execute_obj_->command_buffer_;
    BeginCommandBuffer();

    for (uint32_t i = 0; i < info_count; ++i)
    {
        CmdCopyAccelerationStructure(command_buffer, &copy_infos[i]);
    }

    ExecuteCommandBuffer();
}

void VulkanAccelerationStructureBuilder::SetBufferInfo(BufferInfo*     buffer_info,
                                                       VkDeviceAddress original_address,
                                                       VkDeviceAddress new_address)
{
    if (new_address == 0)
    {
        new_address = GetBufferDeviceAddress(buffer_info->handle);
    }
    auto existing_buffer = std::find_if(
        buffers_.begin(), buffers_.end(), [&](const auto& entry) { return entry->handle_ == buffer_info->handle; });
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
        if ((*existing_buffer)->capture_id_ == format::kNullHandleId)
        {
            (*existing_buffer)->capture_id_ = buffer_info->capture_id;
        }
        if ((*existing_buffer)->handle_ == VK_NULL_HANDLE)
        {
            (*existing_buffer)->handle_ = buffer_info->handle;
        }
        if ((*existing_buffer)->allocator_data_ == 0)
        {
            (*existing_buffer)->allocator_data_ = buffer_info->allocator_data;
        }
    }
    else
    {
        buffers_.push_back(std::make_unique<BufferEntry>(original_address, new_address, allocator_, buffer_info));
    }
}

void VulkanAccelerationStructureBuilder::UntrackBufferInfo(const BufferInfo* buffer_info)
{
    buffers_.erase(std::remove_if(buffers_.begin(),
                                  buffers_.end(),
                                  [&buffer_info](const std::unique_ptr<BufferEntry>& entry) {
                                      return entry->capture_id_ == buffer_info->capture_id;
                                  }),
                   buffers_.end());
}

std::unique_ptr<VulkanAccelerationStructureBuilder::BufferEntry>
VulkanAccelerationStructureBuilder::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, void* initial_data)
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

    uint32_t              mem_type_index = 1;
    VkMemoryPropertyFlags desired_flags{};
    VkMemoryPropertyFlags found_flags{};
    if (initial_data)
    {
        desired_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    graphics::FindMemoryTypeIndex(
        physical_device_memory_properties_, requirements.memoryTypeBits, desired_flags, &mem_type_index, &found_flags);

    VkMemoryAllocateInfo allocate_info{ .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                        .allocationSize  = requirements.size,
                                        .memoryTypeIndex = mem_type_index };

    VkDeviceMemory                      memory{};
    VulkanResourceAllocator::MemoryData memory_allocator_data{};
    allocator_->AllocateMemory(&allocate_info, nullptr, format::kNullHandleId, &memory, &memory_allocator_data);

    allocator_->BindBufferMemory(buffer, memory, 0, buffer_allocator_data, memory_allocator_data, &found_flags);

    if (initial_data)
    {
        void* mapped;
        allocator_->MapResourceMemoryDirect(size, 0, &mapped, buffer_allocator_data);
        util::platform::MemoryCopy(mapped, size, initial_data, size);
        allocator_->UnmapResourceMemoryDirect(buffer_allocator_data);
    }

    auto entry             = std::make_unique<BufferEntry>(0, GetBufferDeviceAddress(buffer), allocator_);
    entry->allocator_data_ = buffer_allocator_data;
    entry->handle_         = buffer;

    return entry;
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
            // This is the case when the acceleration structure was not built but copied into
            address = (*as)->new_address_;
        }
        return;
    }

    // When we have already updated instance buffer, we will be searching for the new address
    as = std::find_if(acceleration_structures_.begin(), acceleration_structures_.end(), [&](const auto& entry) {
        return entry->replacement_acceleration_struct_->new_address_ == address;
    });
    if (as == acceleration_structures_.end())
    {
        GFXRECON_LOG_DEBUG("Acceleration structure address not found: " PRIx64, address);
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
        return;
    }
    else
    {
        VkDeviceSize offset;
        buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
            auto buffer_size = allocator_->GetBufferSize(entry->allocator_data_);
            return entry->original_address_ < address && (entry->original_address_ + buffer_size) > address;
        });
        GFXRECON_ASSERT(buffer != buffers_.end());
        offset  = address - (*buffer)->original_address_;
        address = (*buffer)->new_address_ + offset;
    }
}

VulkanAccelerationStructureBuilder::BufferEntry*
VulkanAccelerationStructureBuilder::GetBufferByRuntimeDeviceAddress(VkDeviceAddress runtime_address)
{
    auto buffer = std::find_if(
        buffers_.begin(), buffers_.end(), [&](const auto& entry) { return entry->new_address_ == runtime_address; });
    if (buffer == buffers_.end())
    {
        buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const auto& entry) {
            auto buffer_size = allocator_->GetBufferSize(entry->allocator_data_);
            return entry->new_address_ < runtime_address && (entry->new_address_ + buffer_size) > runtime_address;
        });
    }
    GFXRECON_ASSERT(buffer != buffers_.end());
    return buffer->get();
}

VulkanAccelerationStructureBuilder::BufferEntry*
VulkanAccelerationStructureBuilder::GetBufferByCaptureDeviceAddress(VkDeviceAddress original_address)
{
    auto buffer = std::find_if(buffers_.begin(), buffers_.end(), [&](const std::unique_ptr<BufferEntry>& entry) {
        return entry->original_address_ == original_address;
    });
    GFXRECON_ASSERT(buffer != buffers_.end());
    return buffer->get();
}

void VulkanAccelerationStructureBuilder::UpdateDescriptorSets(uint32_t              descriptor_write_count,
                                                              VkWriteDescriptorSet* descriptor_writes,
                                                              uint32_t              descriptor_copy_count,
                                                              VkCopyDescriptorSet*  descriptor_copies)
{
    GFXRECON_UNREFERENCED_PARAMETER(descriptor_copy_count);
    GFXRECON_UNREFERENCED_PARAMETER(descriptor_copies);

    VkWriteDescriptorSetAccelerationStructureKHR* acceleration_structure_write = nullptr;
    uint32_t                                      index                        = 0;
    for (uint32_t i = 0; i < descriptor_write_count; ++i)
    {
        if (descriptor_writes[i].descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            continue;
        }
        // Find the relevant data in the pNext chain
        const VkBaseOutStructure* structure = reinterpret_cast<const VkBaseOutStructure*>(descriptor_writes[i].pNext);
        while (structure != nullptr)
        {
            if (structure->sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR)
            {
                acceleration_structure_write = const_cast<VkWriteDescriptorSetAccelerationStructureKHR*>(
                    reinterpret_cast<const VkWriteDescriptorSetAccelerationStructureKHR*>(structure));
                index = i;
                break;
            }
            else
            {
                structure = structure->pNext;
            }
        }
    }

    if (!acceleration_structure_write)
    {
        return;
    }

    for (uint32_t i = 0; i < acceleration_structure_write->accelerationStructureCount; ++i)
    {
        AccelerationStructureEntry* entry =
            GetAccelerationStructureEntry(acceleration_structure_write->pAccelerationStructures[i]);
        if (!entry)
        {
            GFXRECON_LOG_DEBUG("Trying to update the descriptor with untracked acceleration structure");
            throw "Trying to update the descriptor with untracked acceleration structure";
        }
        if (entry->replacement_acceleration_struct_)
        {
            const_cast<VkAccelerationStructureKHR*>(acceleration_structure_write->pAccelerationStructures)[i] =
                entry->replacement_acceleration_struct_->handle_;
        }
        else if (entry->new_address_ == 0)
        {
            // Cache this update and perform it later, on build / copy of the target structure,
            // when we are sure that the handle is valid
            cached_descriptor_write.emplace(entry->handle_, &descriptor_writes[index]);
        }
    }
}

void VulkanAccelerationStructureBuilder::UpdateDescriptorSetWithTemplateKHR(
    gfxrecon::decode::DescriptorUpdateTemplateDecoder* descriptor)
{
    const size_t accel_struct_count = descriptor->GetAccelerationStructureKHRCount();
    for (size_t i = 0; i < accel_struct_count; ++i)
    {
        VkAccelerationStructureKHR& accel_struct = descriptor->GetAccelerationStructureKHRPointer()[i];

        auto target_as = std::find_if(std::begin(acceleration_structures_),
                                      std::end(acceleration_structures_),
                                      [&](const auto& as) -> bool { return as->handle_ == accel_struct; });
        if (target_as != std::end(acceleration_structures_))
        {
            accel_struct = (*target_as)->replacement_acceleration_struct_->handle_;
        }
    }
}

void VulkanAccelerationStructureBuilder::UpdateInstanceBuffer(
    VkCommandBuffer                                  command_buffer,
    VkAccelerationStructureGeometryInstancesDataKHR& instances,
    const VkAccelerationStructureBuildRangeInfoKHR&  build_range)
{
    if (instances.arrayOfPointers)
    {
        throw std::runtime_error("Unsupported instances.arrayOfPointers");
    }
    // update device address of the instance buffer
    UpdateBufferDeviceAddress(instances.data.deviceAddress);
    // find buffer by device address
    auto         instance_buffer = GetBufferByRuntimeDeviceAddress(instances.data.deviceAddress);
    VkDeviceSize offset          = instances.data.deviceAddress - GetBufferDeviceAddress(instance_buffer->handle_);

    // store information on instance buffer content to be updated before VkQueueSubmit
    instance_buffer_updates_[command_buffer].push_back(
        std::make_tuple(instance_buffer->allocator_data_, offset, build_range));
}

// Map provided instance buffer, update acceleration structure references inside
void VulkanAccelerationStructureBuilder::UpdateInstanceBufferContent(
    VulkanResourceAllocator::ResourceData    instance_buffer_allocator_data,
    VkDeviceSize                             offset,
    VkAccelerationStructureBuildRangeInfoKHR build_range)
{
    uint8_t* data;
    allocator_->MapResourceMemoryDirect(sizeof(VkAccelerationStructureInstanceKHR) * build_range.primitiveCount,
                                        0,
                                        (void**)&data,
                                        instance_buffer_allocator_data);
    data += offset + build_range.primitiveOffset;

    VkAccelerationStructureInstanceKHR* instance_data = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(data);
    for (uint32_t instance_index = 0; instance_index < build_range.primitiveCount; ++instance_index)
    {
        UpdateAccelerationStructDeviceAddress(instance_data[instance_index].accelerationStructureReference);
    }
    allocator_->UnmapResourceMemoryDirect(instance_buffer_allocator_data);
}

void VulkanAccelerationStructureBuilder::InitializeInternalExecObjects()
{
    // Just initialize without any check - the caller checks if the objects are already created
    VkResult result;

    cmd_execute_obj_ = std::make_unique<CommandExecuteObjects>(device_, functions_.destroy_command_pool);

    VkCommandPoolCreateInfo create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    create_info.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex        = 0;

    result = functions_.create_command_pool(device_, &create_info, nullptr, &cmd_execute_obj_->pool_);
    GFXRECON_ASSERT(result == VK_SUCCESS);

    VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc_info.pNext                       = nullptr;
    alloc_info.commandPool                 = cmd_execute_obj_->pool_;
    alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount          = 1;

    result = functions_.allocate_command_buffers(device_, &alloc_info, &cmd_execute_obj_->command_buffer_);
    GFXRECON_ASSERT(result == VK_SUCCESS);

    functions_.get_device_queue(device_, 0, 0, &cmd_execute_obj_->queue_);
}

void VulkanAccelerationStructureBuilder::BeginCommandBuffer()
{
    functions_.reset_command_buffer(cmd_execute_obj_->command_buffer_, 0);

    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.pNext                    = nullptr;
    begin_info.flags                    = 0;
    begin_info.pInheritanceInfo         = nullptr;

    VkResult result = functions_.begin_command_buffer(cmd_execute_obj_->command_buffer_, &begin_info);
    GFXRECON_ASSERT(result == VK_SUCCESS);
}

void VulkanAccelerationStructureBuilder::ExecuteCommandBuffer()
{
    functions_.end_command_buffer(cmd_execute_obj_->command_buffer_);

    VkSubmitInfo submit_info         = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.pNext                = nullptr;
    submit_info.waitSemaphoreCount   = 0;
    submit_info.pWaitSemaphores      = nullptr;
    submit_info.pWaitDstStageMask    = nullptr;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &cmd_execute_obj_->command_buffer_;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores    = nullptr;

    OnQueueSubmit(1, &submit_info);
    VkResult result = functions_.queue_submit(cmd_execute_obj_->queue_, 1, &submit_info, VK_NULL_HANDLE);

    if (result == VK_SUCCESS)
    {
        result = functions_.queue_wait_idle(cmd_execute_obj_->queue_);
    }
    GFXRECON_ASSERT(result == VK_SUCCESS);
}

void VulkanAccelerationStructureBuilder::UpdateDeviceAddress(
    VkCommandBuffer                              command_buffer,
    VkAccelerationStructureBuildGeometryInfoKHR& build_geometry,
    VkAccelerationStructureBuildRangeInfoKHR*    range_infos)
{
    for (uint32_t geometry_index = 0; geometry_index < build_geometry.geometryCount; ++geometry_index)
    {
        auto& geometry_data =
            const_cast<VkAccelerationStructureGeometryKHR*>(build_geometry.pGeometries)[geometry_index];
        if (geometry_data.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
        {
            continue;
        }
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
            UpdateInstanceBuffer(command_buffer, instances, range_infos[geometry_index]);
        }
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
    VkCommandBuffer                              command_buffer,
    uint32_t                                     info_count,
    VkAccelerationStructureBuildGeometryInfoKHR* geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**   range_infos)
{
    for (uint32_t i = 0; i < info_count; ++i)
    {
        const auto& mode = geometry_infos[i].mode;

        AccelerationStructureEntry* dst_entry =
            GetAccelerationStructureEntry(geometry_infos[i].dstAccelerationStructure);
        GFXRECON_ASSERT(dst_entry);
        VkAccelerationStructureBuildSizesInfoKHR size_info =
            GetAccelerationStructureSizeInfo(&geometry_infos[i], range_infos[i]);
        VkDeviceSize scratch_size = 0;
        if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR)
        {
            // Create new acceleration structure and scratch of required size
            if (!dst_entry->replacement_acceleration_struct_)
            {
                std::unique_ptr<BufferEntry> storage = CreateBuffer(
                    size_info.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
                VkAccelerationStructureKHR replacement_as =
                    CreateAccelerationStructure(geometry_infos[i], range_infos[i], size_info, storage->handle_);

                // Store acceleration structure data
                auto replacement_as_address = GetAccelerationStructureDeviceAddress(replacement_as);
                dst_entry->replacement_acceleration_struct_ =
                    std::make_unique<AccelerationStructureEntry>(0, replacement_as_address, replacement_as, size_info);
                dst_entry->replacement_acceleration_struct_->storage_ = std::move(storage);

                dst_entry->new_address_ = replacement_as_address;
            }
            // Update all device addresses in geometries
            geometry_infos[i].dstAccelerationStructure = dst_entry->replacement_acceleration_struct_->handle_;
            scratch_size                               = size_info.buildScratchSize;
        }
        else if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR)
        {
            // update srcAccelerationStructure handle
            auto src_entry = GetAccelerationStructureEntry(geometry_infos[i].srcAccelerationStructure);
            GFXRECON_ASSERT(src_entry);
            GFXRECON_ASSERT(src_entry->replacement_acceleration_struct_.get() != nullptr);
            GFXRECON_ASSERT(src_entry->replacement_acceleration_struct_->handle_ != 0);
            geometry_infos[i].srcAccelerationStructure = src_entry->replacement_acceleration_struct_->handle_;

            // update dstAccelerationStructure handle
            if (dst_entry->replacement_acceleration_struct_)
            {
                // dst AS was built before
                GFXRECON_ASSERT(dst_entry->replacement_acceleration_struct_->handle_ != 0);
                geometry_infos[i].dstAccelerationStructure = dst_entry->replacement_acceleration_struct_->handle_;
                dst_entry->new_address_ =
                    GetAccelerationStructureDeviceAddress(dst_entry->replacement_acceleration_struct_->handle_);
            }
            else
            {
                // dst AS was created but not built, record new device address
                dst_entry->new_address_ =
                    GetAccelerationStructureDeviceAddress(geometry_infos[i].dstAccelerationStructure);
            }
            scratch_size = size_info.updateScratchSize;
        }

        std::unique_ptr<BufferEntry>* target_scratch;
        if (dst_entry->replacement_acceleration_struct_)
        {
            target_scratch = &dst_entry->replacement_acceleration_struct_->scratch_;
        }
        else
        {
            target_scratch = &dst_entry->scratch_;
        }

        if (!(*target_scratch))
        {
            (*target_scratch) = CreateBuffer(
                scratch_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        }

        else if (allocator_->GetBufferSize((*target_scratch)->allocator_data_) != scratch_size)
        {
            *target_scratch = CreateBuffer(
                scratch_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        }
        geometry_infos[i].scratchData.deviceAddress = (*target_scratch)->new_address_;
        UpdateDeviceAddress(command_buffer, geometry_infos[i], range_infos[i]);
    }
    functions_.cmd_build_acceleration_structures(command_buffer, info_count, geometry_infos, range_infos);
}

void VulkanAccelerationStructureBuilder::CmdCopyAccelerationStructure(VkCommandBuffer command_buffer,
                                                                      VkCopyAccelerationStructureInfoKHR* copy_info)
{
    // In the typical compaction scenario, we copy the built acceleration structure to a smaller storage,
    // which is only created but not built
    VkCopyAccelerationStructureInfoKHR modified_info = *copy_info;

    if (VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR == modified_info.mode)
    {
        // First, get the replay device address of the destination structure and store it
        auto compacted_entry = GetAccelerationStructureEntry(modified_info.dst);

        // clang-format off
        // TODO: Similarly to the build destination, copy destination should be recreated here to adjust
        // the AS size on replay. Compacting copy destination is typically created by commands:
        // vkCmdWriteAccelerationStructuresPropertiesKHR(VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR)
        // vkCreateAccelerationStructureKHR(...compacted-size...)
        // vkCmdCopyAccelerationStructureKHR(...dst: compacted-as...)
        // The compacted size can be different on each device, so the destination AS needs a
        // replacement of size calculated at runtime.
        // clang-format on

        compacted_entry->new_address_ = GetAccelerationStructureDeviceAddress(modified_info.dst);

        // Replace the handle to be of the real built structure
        auto original_entry = GetAccelerationStructureEntry(modified_info.src);
        modified_info.src   = original_entry->replacement_acceleration_struct_->handle_;
    }
    // TODO: non-compacting copy
    functions_.cmd_copy_acceleration_structure(command_buffer, &modified_info);
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

VkDeviceAddress VulkanAccelerationStructureBuilder::GetAccelerationStructureDeviceAddress(
    VkAccelerationStructureKHR acceleration_structure)
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
    functions_.create_acceleration_structure(device_, &create_info, nullptr, &acceleration_structure);
    return acceleration_structure;
}

VkAccelerationStructureBuildSizesInfoKHR VulkanAccelerationStructureBuilder::GetAccelerationStructureSizeInfo(
    VkAccelerationStructureBuildGeometryInfoKHR* geometry_info, VkAccelerationStructureBuildRangeInfoKHR* range_info)
{
    std::vector<uint32_t> primitive_counts(geometry_info->geometryCount);
    for (uint32_t i = 0; i < geometry_info->geometryCount; ++i)
    {
        primitive_counts[i] = range_info[i].primitiveCount;
    }
    VkAccelerationStructureBuildSizesInfoKHR size_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
                                                        nullptr };
    functions_.get_acceleration_structure_build_sizes(
        device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, geometry_info, primitive_counts.data(), &size_info);
    return size_info;
}

// For each submitted buffer, if it contains a TLAS build command, update its instance buffer
// with replacement BLAS address
void VulkanAccelerationStructureBuilder::OnQueueSubmit(uint32_t submitCount, const VkSubmitInfo* pSubmits)
{
    for (int i = 0; i < submitCount; ++i)
    {
        auto submission = pSubmits[i];
        for (int cmd_buffer_index = 0; cmd_buffer_index < submission.commandBufferCount; ++cmd_buffer_index)
        {
            auto submitted_buffer = submission.pCommandBuffers[cmd_buffer_index];

            auto instance_buffers_update_itr = instance_buffer_updates_.find(submitted_buffer);
            if (instance_buffers_update_itr != instance_buffer_updates_.end())
            {
                auto instance_buffers_update = instance_buffers_update_itr->second;
                for (auto [instance_buffer_info, offset, range_info] : instance_buffers_update)
                {
                    UpdateInstanceBufferContent(instance_buffer_info, offset, range_info);
                }
                instance_buffer_updates_.erase(instance_buffers_update_itr);
            }
        }
    }

    // There can be a case when the handle is put into update descriptor sets
    // before the replacement is built
    for (auto it = cached_descriptor_write.begin(); it != cached_descriptor_write.end();)
    {
        auto entry = GetAccelerationStructureEntry(it->first);
        if (entry->replacement_acceleration_struct_)
        {
            auto handle =
                std::find(it->second.acc_structs_data.begin(), it->second.acc_structs_data.end(), entry->handle_);
            *handle = entry->replacement_acceleration_struct_->handle_;
            functions_.update_descriptor_sets(device_, 1, it->second.write_.get(), 0, nullptr);
            it = cached_descriptor_write.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)