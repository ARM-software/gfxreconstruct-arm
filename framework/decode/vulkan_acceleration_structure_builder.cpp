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

    for (auto& [handle, info] : buffer_infos_)
    {
        allocator_->DestroyBuffer(handle, nullptr, info.allocator_data);
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

    if (result != acceleration_structures_.end() && result->get()->replacement_acceleration_struct_)
    {

        const auto& real_as = result->get()->replacement_acceleration_struct_;
        functions_.destroy_acceleration_structure(device_, real_as->handle_, nullptr);
    }

    acceleration_structures_.erase(result);
}

void VulkanAccelerationStructureBuilder::ProcessInitVulkanAccelerationStructuresCommand(
    VkCommandBuffer                                               command_buffer,
    uint32_t                                                      info_count,
    VkAccelerationStructureBuildGeometryInfoKHR*                  geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**                    range_infos,
    std::vector<std::vector<VkAccelerationStructureInstanceKHR>>& instance_buffers_data)
{
    // Retrieve / initialize the command executable structures or reuse the capture command buffer
    if (command_buffer == VK_NULL_HANDLE)
    {
        if (!m_cmd_execute_obj)
        {
            InitializeInternalExecObjects();
        }
        command_buffer = m_cmd_execute_obj->m_command_buffer;
    }

    BeginCommandBuffer();

    for (uint32_t i = 0; i < info_count; ++i)
    {
        for (uint32_t g = 0; g < geometry_infos[i].geometryCount; ++g)
        {
            if (geometry_infos[i].pGeometries[g].geometryType != VK_GEOMETRY_TYPE_INSTANCES_KHR)
            {
                continue;
            }
            auto buffer_entry =
                GetBufferByCaptureDeviceAddress(geometry_infos[i].pGeometries[g].geometry.instances.data.deviceAddress);
            if (buffer_entry)
            {
                continue;
            }
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            VkDeviceSize buffer_size = range_infos[i][g].primitiveCount * sizeof(VkAccelerationStructureInstanceKHR);

            BufferEntry* entry       = CreateBuffer(buffer_size, usage, instance_buffers_data[i].data());
            entry->original_address_ = geometry_infos[i].pGeometries[g].geometry.instances.data.deviceAddress;
            entry->new_address_      = GetBufferDeviceAddress(entry->buffer_info_->handle);
        }
    }

    CmdBuildAccelerationStructures(command_buffer, info_count, geometry_infos, range_infos);

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

VulkanAccelerationStructureBuilder::BufferEntry*
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
    allocator_->CreateBufferDirect(&create_info, nullptr, &buffer, &buffer_allocator_data);

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
    allocator_->AllocateMemoryDirect(&allocate_info, nullptr, &memory, &memory_allocator_data);

    allocator_->BindBufferMemoryDirect(buffer, memory, 0, buffer_allocator_data, memory_allocator_data, &found_flags);

    if (initial_data)
    {
        void* mapped;
        allocator_->MapResourceMemoryDirect(size, 0, &mapped, buffer_allocator_data);
        util::platform::MemoryCopy(mapped, size, initial_data, size);
        allocator_->UnmapResourceMemoryDirect(buffer_allocator_data);
    }

    auto [it, inserted] = buffer_infos_.insert(std::make_pair(buffer, BufferInfo{}));

    it->second.allocator_data        = buffer_allocator_data;
    it->second.capture_id            = format::kNullHandleId;
    it->second.parent_id             = format::kNullHandleId;
    it->second.handle                = buffer;
    it->second.memory_property_flags = found_flags;
    it->second.queue_family_index    = 0;
    it->second.usage                 = create_info.usage;

    buffers_.push_back(std::make_unique<BufferEntry>(0, 0, &it->second));

    return buffers_.back().get();
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
            auto buffer_size = allocator_->GetBufferSize(entry->buffer_info_->allocator_data);
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
            auto buffer_size = allocator_->GetBufferSize(entry->buffer_info_->allocator_data);
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
    // The idea here is that some buffers may not survive when trimming
    if (buffer == buffers_.end())
    {
        return nullptr;
    }
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
    BufferInfo*  instance_buffer = GetBufferByRuntimeDeviceAddress(instances.data.deviceAddress)->buffer_info_;
    VkDeviceSize offset          = instances.data.deviceAddress - GetBufferDeviceAddress(instance_buffer->handle);

    // store information on instance buffer content to be updated before queuesubmit
    instance_buffer_updates_[command_buffer].push_back(std::make_tuple(instance_buffer, offset, build_range));
}

// Map provided instance buffer, update acceleration structure references inside
void VulkanAccelerationStructureBuilder::UpdateInstanceBufferContent(
    BufferInfo* instance_buffer, VkDeviceSize offset, VkAccelerationStructureBuildRangeInfoKHR build_range)
{
    uint8_t* data;
    allocator_->MapResourceMemoryDirect(sizeof(VkAccelerationStructureInstanceKHR) * build_range.primitiveCount,
                                        0,
                                        (void**)&data,
                                        instance_buffer->allocator_data);
    data += offset + build_range.primitiveOffset;

    VkAccelerationStructureInstanceKHR* instance_data = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(data);
    for (uint32_t instance_index = 0; instance_index < build_range.primitiveCount; ++instance_index)
    {
        UpdateAccelerationStructDeviceAddress(instance_data[instance_index].accelerationStructureReference);
    }
    allocator_->UnmapResourceMemoryDirect(instance_buffer->allocator_data);
}

void VulkanAccelerationStructureBuilder::InitializeInternalExecObjects()
{
    // Just initialize without any check - the caller checks if the objects are already created
    VkResult result;

    m_cmd_execute_obj = std::make_unique<CommandExecuteObjects>(device_, functions_.destroy_command_pool);

    // Create the command pool
    VkCommandPoolCreateInfo create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    create_info.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex        = 0;

    result = functions_.create_command_pool(device_, &create_info, nullptr, &m_cmd_execute_obj->m_pool);
    GFXRECON_ASSERT(result == VK_SUCCESS);

    VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc_info.pNext                       = nullptr;
    alloc_info.commandPool                 = m_cmd_execute_obj->m_pool;
    alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount          = 1;

    result = functions_.allocate_command_buffers(device_, &alloc_info, &m_cmd_execute_obj->m_command_buffer);
    GFXRECON_ASSERT(result == VK_SUCCESS);

    functions_.get_device_queue(device_, 0, 0, &m_cmd_execute_obj->m_queue);
}

void VulkanAccelerationStructureBuilder::BeginCommandBuffer()
{
    functions_.reset_command_buffer(m_cmd_execute_obj->m_command_buffer, 0);

    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.pNext                    = nullptr;
    begin_info.flags                    = 0;
    begin_info.pInheritanceInfo         = nullptr;

    VkResult result = functions_.begin_command_buffer(m_cmd_execute_obj->m_command_buffer, &begin_info);
    GFXRECON_ASSERT(result == VK_SUCCESS);
}

void VulkanAccelerationStructureBuilder::ExecuteCommandBuffer()
{
    functions_.end_command_buffer(m_cmd_execute_obj->m_command_buffer);

    VkSubmitInfo submit_info         = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.pNext                = nullptr;
    submit_info.waitSemaphoreCount   = 0;
    submit_info.pWaitSemaphores      = nullptr;
    submit_info.pWaitDstStageMask    = nullptr;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &m_cmd_execute_obj->m_command_buffer;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores    = nullptr;

    OnQueueSubmit(1, &submit_info);
    VkResult result = functions_.queue_submit(m_cmd_execute_obj->m_queue, 1, &submit_info, VK_NULL_HANDLE);

    if (result == VK_SUCCESS)
    {
        result = functions_.queue_wait_idle(m_cmd_execute_obj->m_queue);
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
                UpdateInstanceBuffer(command_buffer, instances, range_infos[geometry_index]);
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
    VkCommandBuffer                              command_buffer,
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
            VkAccelerationStructureKHR               original_as       = geometry_infos[i].dstAccelerationStructure;
            AccelerationStructureEntry*              original_as_entry = GetAccelerationStructureEntry(original_as);
            VkAccelerationStructureBuildSizesInfoKHR size_info =
                GetAccelerationStructureSizeInfo(&geometry_infos[i], range_infos[i]);
            if (!original_as_entry->replacement_acceleration_struct_)
            {
                BufferEntry*               storage        = CreateBuffer(size_info.accelerationStructureSize,
                                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
                VkAccelerationStructureKHR replacement_as = CreateAccelerationStructure(
                    geometry_infos[i], range_infos[i], size_info, storage->buffer_info_->handle);

                // Store acceleration structure data
                auto replacement_as_address = GetAccelerationStructureDeviceAddress(replacement_as);
                original_as_entry->replacement_acceleration_struct_ =
                    std::make_unique<AccelerationStructureEntry>(0, replacement_as_address, replacement_as, size_info);

                original_as_entry->new_address_ = replacement_as_address;
            }
            // Update all device addresses in geometries
            BufferEntry* scratch =
                CreateBuffer(size_info.buildScratchSize,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            geometry_infos[i].dstAccelerationStructure  = original_as_entry->replacement_acceleration_struct_->handle_;
            geometry_infos[i].scratchData.deviceAddress = GetBufferDeviceAddress(scratch->buffer_info_->handle);
            UpdateDeviceAddress(command_buffer, geometry_infos[i], range_infos[i]);
        }
        else if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR)
        {
            // create new scratch for update
            VkAccelerationStructureBuildSizesInfoKHR size_info =
                GetAccelerationStructureSizeInfo(&geometry_infos[i], range_infos[i]);
            BufferEntry* scratch =
                CreateBuffer(size_info.updateScratchSize,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            geometry_infos[i].scratchData.deviceAddress = GetBufferDeviceAddress(scratch->buffer_info_->handle);

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
            original_dst_as->new_address_ =
                GetAccelerationStructureDeviceAddress(geometry_infos[i].dstAccelerationStructure);

            // update geometry buffers
            UpdateDeviceAddress(command_buffer, geometry_infos[i], range_infos[i]);
        }
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

        compacted_entry->new_address_ = GetAccelerationStructureDeviceAddress(modified_info.dst);

        // Replace the handle to be of the real built structure
        auto original_entry = GetAccelerationStructureEntry(modified_info.src);
        modified_info.src   = original_entry->replacement_acceleration_struct_->handle_;
    }
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
    VkResult status = functions_.create_acceleration_structure(device_, &create_info, nullptr, &acceleration_structure);
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
        for (int cmdbuffer_index = 0; cmdbuffer_index < submission.commandBufferCount; ++cmdbuffer_index)
        {
            auto submitted_buffer = submission.pCommandBuffers[cmdbuffer_index];

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
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
