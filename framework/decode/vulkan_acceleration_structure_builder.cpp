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
#include "decode/vulkan_acceleration_structure_builder.h"
#include "decode/vulkan_micromap_builder.h"
#include "util/marking_layers.h"

#include <algorithm>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanAccelerationStructureBuilder::VulkanAccelerationStructureBuilder(
    const encode::VulkanDeviceTable*        device_table,
    const VulkanPhysicalDeviceInfo*         physical_device_info,
    VkDevice                                device,
    VulkanResourceAllocator*                allocator,
    const VkPhysicalDeviceMemoryProperties& memory_properties,
    VulkanDeviceAddressTracker&             device_address_tracker) :
    device_(device),
    physical_device_info_(physical_device_info), allocator_(allocator),
    physical_device_memory_properties_(memory_properties), device_address_tracker_(device_address_tracker),
    internal_buffer_manager_(
        device_table, physical_device_info, device_, allocator_, physical_device_memory_properties_)
{
    InitializeFunctionPointers(device_table);
    InitializeInternalExecObjects();
}

VulkanAccelerationStructureBuilder::~VulkanAccelerationStructureBuilder() {}

VkResult VulkanAccelerationStructureBuilder::OnCreateAccelerationStructure(
    const VulkanDeviceInfo*                     device_info,
    const VkAccelerationStructureCreateInfoKHR* create_info,
    const VkAllocationCallbacks*                pAllocator,
    const VulkanBufferInfo*                     buffer_info,
    format::HandleId                            capture_id,
    VkAccelerationStructureKHR*                 handle)
{
    // Create new storage buffer for AccelerationStructure based on previously recorded
    // GetAccelerationStructureBuildSize (this call must be inserted by gfxrecon-optimize)
    auto allocator = device_info->allocator.get();
    assert(allocator != nullptr);
    assert(buffer_info != nullptr);

    VkAccelerationStructureBuildSizesInfoKHR build_sizes = last_build_sizes_;
    last_build_sizes_ = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0 };

    AccelerationStructureData             acceleration_structure_data;
    VkAccelerationStructureCreateInfoKHR* info = const_cast<VkAccelerationStructureCreateInfoKHR*>(create_info);
    acceleration_structure_data.create_info    = *info;
    bool is_recreated                          = true;
    auto buffer_size                           = allocator->GetBufferSize(buffer_info->allocator_data);
    auto memory_property_flags                 = buffer_info->memory_property_flags;

    if (build_sizes.accelerationStructureSize)
    {
        info->size = build_sizes.accelerationStructureSize;
    }

    // Points to storage that will be used in the creation call
    VulkanBufferInfo* target_storage_buffer = const_cast<VulkanBufferInfo*>(buffer_info);

    if ((memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        memory_property_flags = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    }

    if (buffer_binding_acceleration_structures_.find(info->buffer) != buffer_binding_acceleration_structures_.end())
    {
        for (auto& acceleration_structure : buffer_binding_acceleration_structures_[info->buffer])
        {
            if (acceleration_structures_.find(acceleration_structure) != acceleration_structures_.end())
            {
                VkAccelerationStructureCreateInfoKHR saved_create_info =
                    acceleration_structures_[acceleration_structure].create_info;
                VulkanInternalBufferManager::BufferInfoWrapper* buffer_wrapper =
                    acceleration_structures_[acceleration_structure].new_storage.get();
                if (buffer_wrapper != nullptr &&
                    saved_create_info.buffer == acceleration_structure_data.create_info.buffer &&
                    saved_create_info.size == acceleration_structure_data.create_info.size &&
                    saved_create_info.offset == acceleration_structure_data.create_info.offset)
                {
                    info->size =
                        acceleration_structures_[acceleration_structure].new_build_sizes.accelerationStructureSize;
                    target_storage_buffer = &buffer_wrapper->info_;
                    info->offset          = 0;
                    is_recreated          = false;
                }
            }
        }
    }

    if (build_sizes.accelerationStructureSize == 0)
    {
        if (compaction_child_to_parent_dependency_.count(capture_id) != 0)
        {
            VkAccelerationStructureKHR parent = compaction_child_to_parent_dependency_[capture_id];
            build_sizes = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0 };
            // compaction flow
            if (compacted_sizes_processed_.count(parent) == 0)
            {
                // Handling for results (only in case of vkCmdCopyQueryPoolResults)
                for (auto it = compacted_sizes_unprocessed_.begin(); it != compacted_sizes_unprocessed_.end(); ++it)
                {
                    std::vector<PreProcessingCompactionInfo>& vector = it->second;
                    for (uint64_t i = 0; i < vector.size(); i++)
                    {
                        auto&                                    buffer            = vector[i].buffer_info_wrapper;
                        auto                                     first_query       = vector[i].first_query;
                        std::vector<VkAccelerationStructureKHR>& vector_of_acc_str = vector[i].parents;

                        std::vector<uint64_t> vector_of_acc_str_sizes(vector_of_acc_str.size(), 0);
                        uint64_t              buffer_size = vector_of_acc_str_sizes.size() * sizeof(uint64_t);

                        util::MarkingLayersUtil::instance().BeginInjected(physical_device_info_);
                        void*    mapped;
                        VkResult mapping_result =
                            allocator_->MapResourceMemoryDirect(buffer_size, 0, &mapped, buffer->info_.allocator_data);
                        GFXRECON_ASSERT(mapping_result == VK_SUCCESS);

                        util::platform::MemoryCopy(vector_of_acc_str_sizes.data(), buffer_size, mapped, buffer_size);

                        allocator_->UnmapResourceMemoryDirect(buffer->info_.allocator_data);
                        util::MarkingLayersUtil::instance().EndInjected(physical_device_info_);

                        // add results to compacted_sizes_processed map
                        for (uint64_t j = 0; j < vector_of_acc_str.size(); j++)
                        {
                            compacted_sizes_processed_.try_emplace(vector_of_acc_str[j], vector_of_acc_str_sizes[j]);
                        }
                    }
                }
            }

            GFXRECON_ASSERT(compacted_sizes_processed_.count(parent) != 0);

            // Compacted size data may be invalid - fallback to the uncompressed size of parent object
            if (compacted_sizes_processed_[parent] == 0)
            {
                GFXRECON_LOG_WARNING_ONCE(
                    "Driver did not provide valid size data for acceleration structure compaction "
                    "process. Replayer will use non-compacted sizes.");
                build_sizes.accelerationStructureSize =
                    acceleration_structures_[parent].new_build_sizes.accelerationStructureSize;
            }
            else
            {
                build_sizes.accelerationStructureSize = compacted_sizes_processed_[parent];
            }
            is_recreated = true;
        }
        else
        {
            is_recreated = false;
        }
    }
    else if (build_sizes.accelerationStructureSize < acceleration_structure_data.create_info.size &&
             build_sizes.accelerationStructureSize < buffer_size)
    {
        is_recreated = false;
        // TODO this case must be aligned offset with the last acceleration structure size
    }

    if (is_recreated)
    {
        // New storage size needs to be bigger than the size of acceleration structure
        // Exact extra size is implementation depedent - some won't need any, some will require extra padding, specs
        // don't state how much.
        // Therefore an arbitrary padding value is added for safety, can be increased if needed
        const VkDeviceSize kAccelerationStructureStorageSafetyPadding = 256;
        VkDeviceSize       new_storage_size =
            build_sizes.accelerationStructureSize + kAccelerationStructureStorageSafetyPadding;
        std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> new_storage_buffer =
            internal_buffer_manager_.CreateBuffer(new_storage_size,
                                                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                  memory_property_flags);

        info->size                              = build_sizes.accelerationStructureSize;
        target_storage_buffer                   = &new_storage_buffer->info_;
        info->offset                            = 0;
        acceleration_structure_data.new_storage = std::move(new_storage_buffer);
    }
    else
    {
        acceleration_structure_data.new_storage = nullptr;
    }

    // Last minute validation: storage buffer should be bigger than the acceleration structure size + offset
    // Note: We operate on 2 kinds of sizes - the size provided as an input in Create* calls and the actual size
    // retrieved from allocator/GetASBuildSizes query. Assume all these sizes should satisfy the above condition.
    auto target_storage_buffer_allocated_size = allocator->GetBufferSize(target_storage_buffer->allocator_data);
    GFXRECON_ASSERT(target_storage_buffer_allocated_size > info->size + info->offset);
    GFXRECON_ASSERT(target_storage_buffer->size > info->size + info->offset);
    GFXRECON_ASSERT(target_storage_buffer_allocated_size > build_sizes.accelerationStructureSize + info->offset);
    GFXRECON_ASSERT(target_storage_buffer->size > build_sizes.accelerationStructureSize + info->offset);

    info->buffer    = target_storage_buffer->handle;
    VkResult result = functions_.create_acceleration_structure(device_info->handle, info, pAllocator, handle);
    assert(result == VK_SUCCESS);

    acceleration_structure_data.new_build_sizes = build_sizes;
    buffer_binding_acceleration_structures_[acceleration_structure_data.create_info.buffer].push_back(*handle);
    acceleration_structures_[*handle] = std::move(acceleration_structure_data);

    return result;
}

// On destroy acceleration structure, clean up the internal replacement structure
void VulkanAccelerationStructureBuilder::OnDestroyAccelerationStructure(
    const VulkanAccelerationStructureKHRInfo* acceleration_structure_info)
{
    auto it = acceleration_structures_.find(acceleration_structure_info->handle);
    if (it != acceleration_structures_.end())
    {
        if (it->second.new_storage != nullptr)
        {
            // TODO: handle the case of VkDestroyBuffer before VkDestroyAccelerationStructure
            storage_buffers_to_be_destroyed_[it->second.create_info.buffer].emplace_back(
                std::move(it->second.new_storage));
        }
        acceleration_structures_.erase(acceleration_structure_info->handle);
    }
}

void VulkanAccelerationStructureBuilder::ProcessBuildVulkanAccelerationStructuresMetaCommand(
    uint32_t                                                      info_count,
    VkAccelerationStructureBuildGeometryInfoKHR*                  geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**                    range_infos,
    std::vector<std::vector<VkAccelerationStructureInstanceKHR>>& instance_buffers_data)
{
    GFXRECON_UNREFERENCED_PARAMETER(instance_buffers_data);

    BeginCommandBuffer();
    OnCmdBuildAccelerationStructures(cmd_execute_obj_.command_buffer_, info_count, geometry_infos, range_infos);
    functions_.cmd_build_acceleration_structures(
        cmd_execute_obj_.command_buffer_, info_count, geometry_infos, range_infos);
    ExecuteCommandBuffer();
}

void VulkanAccelerationStructureBuilder::ProcessCopyVulkanAccelerationStructuresMetaCommand(
    uint32_t info_count, VkCopyAccelerationStructureInfoKHR* copy_infos)
{
    BeginCommandBuffer();
    for (uint32_t i = 0; i < info_count; ++i)
    {
        OnCmdCopyAccelerationStructure(cmd_execute_obj_.command_buffer_, &copy_infos[i]);
    }
    ExecuteCommandBuffer();
}

void VulkanAccelerationStructureBuilder::ProcessVulkanAccelerationStructuresWritePropertiesMetaCommand(
    VkQueryType query_type, VkAccelerationStructureKHR acceleration_structure)
{
    BeginCommandBuffer();

    functions_.cmd_reset_query_pool(cmd_execute_obj_.command_buffer_, cmd_execute_obj_.query_pool_, 0, 1);

    OnCmdWriteAccelerationStructuresProperties(
        cmd_execute_obj_.command_buffer_, 1, &acceleration_structure, query_type, cmd_execute_obj_.query_pool_, 0);

    ExecuteCommandBuffer();

    VulkanDeviceInfo device_info;
    device_info.handle = cmd_execute_obj_.device_;

    VulkanQueryPoolInfo query_pool_info;
    query_pool_info.handle = cmd_execute_obj_.query_pool_;

    OnGetQueryPoolResults(&device_info, &query_pool_info);
}

void VulkanAccelerationStructureBuilder::OnDestroyBuffer(const VulkanBufferInfo* buffer_info)
{
    auto it = storage_buffers_to_be_destroyed_.find(buffer_info->handle);
    if (it != storage_buffers_to_be_destroyed_.end())
    {
        storage_buffers_to_be_destroyed_.erase(it);
    }
}

void VulkanAccelerationStructureBuilder::InitializeFunctionPointers(const encode::VulkanDeviceTable* device_table)
{
    functions_.get_acceleration_structure_build_sizes       = device_table->GetAccelerationStructureBuildSizesKHR,
    functions_.create_acceleration_structure                = device_table->CreateAccelerationStructureKHR,
    functions_.cmd_build_acceleration_structures            = device_table->CmdBuildAccelerationStructuresKHR;
    functions_.get_acceleration_structure_device_address    = device_table->GetAccelerationStructureDeviceAddressKHR;
    functions_.cmd_copy_acceleration_structure              = device_table->CmdCopyAccelerationStructureKHR;
    functions_.cmd_write_acceleration_structures_properties = device_table->CmdWriteAccelerationStructuresPropertiesKHR;
    functions_.destroy_acceleration_structure               = device_table->DestroyAccelerationStructureKHR;
    functions_.create_command_pool                          = device_table->CreateCommandPool;
    functions_.destroy_command_pool                         = device_table->DestroyCommandPool;
    functions_.allocate_command_buffers                     = device_table->AllocateCommandBuffers;
    functions_.free_command_buffers                         = device_table->FreeCommandBuffers;
    functions_.get_device_queue                             = device_table->GetDeviceQueue;
    functions_.begin_command_buffer                         = device_table->BeginCommandBuffer;
    functions_.end_command_buffer                           = device_table->EndCommandBuffer;
    functions_.reset_command_buffer                         = device_table->ResetCommandBuffer;
    functions_.queue_submit                                 = device_table->QueueSubmit;
    functions_.queue_wait_idle                              = device_table->QueueWaitIdle;
    functions_.update_descriptor_sets                       = device_table->UpdateDescriptorSets;
    functions_.get_query_pool_results                       = device_table->GetQueryPoolResults;
    functions_.cmd_copy_query_pool_results                  = device_table->CmdCopyQueryPoolResults;
    functions_.cmd_pipeline_barrier                         = device_table->CmdPipelineBarrier;
    functions_.create_query_pool                            = device_table->CreateQueryPool;
    functions_.cmd_reset_query_pool                         = device_table->CmdResetQueryPool;
    functions_.destroy_query_pool                           = device_table->DestroyQueryPool;
    functions_.get_fence_status                             = device_table->GetFenceStatus;
}

void VulkanAccelerationStructureBuilder::InitializeInternalExecObjects()
{
    util::MarkingLayersUtil::instance().BeginInjected(physical_device_info_);
    VkResult result;
    cmd_execute_obj_.device_               = device_;
    cmd_execute_obj_.free_command_buffers_ = functions_.free_command_buffers;
    cmd_execute_obj_.destroy_command_pool_ = functions_.destroy_command_pool;
    cmd_execute_obj_.destroy_query_pool_   = functions_.destroy_query_pool;

    VkCommandPoolCreateInfo create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    create_info.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex        = 0;

    result = functions_.create_command_pool(device_, &create_info, nullptr, &cmd_execute_obj_.pool_);
    GFXRECON_ASSERT(result == VK_SUCCESS);

    VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc_info.pNext                       = nullptr;
    alloc_info.commandPool                 = cmd_execute_obj_.pool_;
    alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount          = 1;

    result = functions_.allocate_command_buffers(device_, &alloc_info, &cmd_execute_obj_.command_buffer_);
    GFXRECON_ASSERT(result == VK_SUCCESS);

    VkQueryPoolCreateInfo pool_info{};
    pool_info.sType              = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    pool_info.flags              = 0;
    pool_info.queryType          = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
    pool_info.queryCount         = 1;
    pool_info.pipelineStatistics = 0;
    pool_info.pNext              = nullptr;

    result = functions_.create_query_pool(device_, &pool_info, nullptr, &cmd_execute_obj_.query_pool_);
    GFXRECON_ASSERT(result == VK_SUCCESS);

    functions_.get_device_queue(device_, 0, 0, &cmd_execute_obj_.queue_);
    cmd_execute_obj_.initialized_ = true;
    util::MarkingLayersUtil::instance().EndInjected(physical_device_info_);
}

void VulkanAccelerationStructureBuilder::BeginCommandBuffer()
{
    functions_.reset_command_buffer(cmd_execute_obj_.command_buffer_, 0);

    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.pNext                    = nullptr;
    begin_info.flags                    = 0;
    begin_info.pInheritanceInfo         = nullptr;

    VkResult result = functions_.begin_command_buffer(cmd_execute_obj_.command_buffer_, &begin_info);
    GFXRECON_ASSERT(result == VK_SUCCESS);
}

void VulkanAccelerationStructureBuilder::ExecuteCommandBuffer()
{
    functions_.end_command_buffer(cmd_execute_obj_.command_buffer_);

    VkSubmitInfo submit_info         = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.pNext                = nullptr;
    submit_info.waitSemaphoreCount   = 0;
    submit_info.pWaitSemaphores      = nullptr;
    submit_info.pWaitDstStageMask    = nullptr;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &cmd_execute_obj_.command_buffer_;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores    = nullptr;

    VkResult result = functions_.queue_submit(cmd_execute_obj_.queue_, 1, &submit_info, VK_NULL_HANDLE);
    GFXRECON_ASSERT(result == VK_SUCCESS);
    result = functions_.queue_wait_idle(cmd_execute_obj_.queue_);
    GFXRECON_ASSERT(result == VK_SUCCESS);
}

void VulkanAccelerationStructureBuilder::OnGetAccelerationStructureBuildSizes(
    const VulkanDeviceInfo*                            device_info,
    VkAccelerationStructureBuildTypeKHR                type,
    const VkAccelerationStructureBuildGeometryInfoKHR* build_Info,
    const uint32_t*                                    max_primitive_counts,
    VkAccelerationStructureBuildSizesInfoKHR*          size_info)
{
    functions_.get_acceleration_structure_build_sizes(
        device_info->handle, type, build_Info, max_primitive_counts, size_info);
    last_build_sizes_ = *size_info;
}

void VulkanAccelerationStructureBuilder::OnAccelerationStructureCompactionDependencyCommand(
    VkAccelerationStructureKHR parent, const std::vector<format::HandleId>& children)
{
    for (uint64_t i = 0; i < children.size(); i++)
    {
        compaction_child_to_parent_dependency_[children[i]] = parent;
    }
}

void VulkanAccelerationStructureBuilder::OnCmdBuildAccelerationStructures(
    VkCommandBuffer                              command_buffer,
    uint32_t                                     info_count,
    VkAccelerationStructureBuildGeometryInfoKHR* geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**   range_infos)
{
    for (uint32_t i = 0; i < info_count; ++i)
    {
        const auto& mode                  = geometry_infos[i].mode;
        auto acceleration_structure_entry = acceleration_structures_.find(geometry_infos[i].dstAccelerationStructure);
        if (acceleration_structure_entry == acceleration_structures_.end())
        {
            GFXRECON_LOG_FATAL("Handle %ul for vkCmdBuildAccelerationStructuresKHR not found",
                               geometry_infos[i].dstAccelerationStructure);
            GFXRECON_ASSERT(false);
        }

        VkDeviceSize scratch_size = acceleration_structure_entry->second.new_build_sizes.buildScratchSize;
        if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR)
        {
            scratch_size = acceleration_structure_entry->second.new_build_sizes.updateScratchSize;
        }

        UpdateScratchDeviceAddress(command_buffer, geometry_infos[i], scratch_size);
    }

    VulkanMicromapBuilder::OnCmdBuildAccStrHandling(device_address_tracker_, info_count, geometry_infos);
}

void VulkanAccelerationStructureBuilder::UpdateScratchDeviceAddress(
    VkCommandBuffer                              command_buffer,
    VkAccelerationStructureBuildGeometryInfoKHR& geometry_infos,
    VkDeviceSize                                 scratch_size)
{
    // Check whether the scratch with this original device address is already allocated and fits the size
    VkDeviceAddress capture_scratch_address = geometry_infos.scratchData.deviceAddress;

    format::HandleId      capture_id            = format::kNullHandleId;
    VkMemoryPropertyFlags memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    // When fastforwarding, the scratch buffers could be destroyed and not be recreated in state recreation
    const VulkanBufferInfo* original_scratch_entry =
        device_address_tracker_.GetBufferByCaptureDeviceAddress(capture_scratch_address);
    if (original_scratch_entry)
    {
        capture_id            = original_scratch_entry->capture_id;
        memory_property_flags = original_scratch_entry->memory_property_flags;
    }

    if ((memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    auto scratch_entries = recorded_scratches.find(command_buffer);

    if (scratch_entries != recorded_scratches.end())
    {
        auto scratch_entry =
            std::find_if(scratch_entries->second.begin(),
                         scratch_entries->second.end(),
                         [scratch_size, capture_scratch_address](
                             const std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>& entry) {
                             return entry->allocator_->GetBufferSize(entry->info_.allocator_data) >= scratch_size &&
                                    entry->info_.capture_address == capture_scratch_address;
                         });
        if (scratch_entry != scratch_entries->second.end())
        {
            geometry_infos.scratchData.deviceAddress = (*scratch_entry)->info_.replay_address;
        }
        else
        {
            const auto& new_scratch = scratch_entries->second.emplace_back(internal_buffer_manager_.CreateBuffer(
                scratch_size,
                (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
                memory_property_flags));
            new_scratch->info_.capture_address       = geometry_infos.scratchData.deviceAddress;
            geometry_infos.scratchData.deviceAddress = new_scratch->info_.replay_address;
        }
    }
    else
    {
        auto [it, inserted] = recorded_scratches.emplace(
            command_buffer, std::vector<std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>>());

        auto& new_scratch                        = it->second.emplace_back(internal_buffer_manager_.CreateBuffer(
            scratch_size,
            (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
            memory_property_flags));
        new_scratch->info_.capture_address       = geometry_infos.scratchData.deviceAddress;
        geometry_infos.scratchData.deviceAddress = new_scratch->info_.replay_address;
    }
}

void VulkanAccelerationStructureBuilder::OnCmdCopyAccelerationStructure(VkCommandBuffer command_buffer,
                                                                        VkCopyAccelerationStructureInfoKHR* copy_info)
{
    functions_.cmd_copy_acceleration_structure(command_buffer, copy_info);
}

void VulkanAccelerationStructureBuilder::OnCmdWriteAccelerationStructuresProperties(
    VkCommandBuffer             command_buffer,
    uint32_t                    count,
    VkAccelerationStructureKHR* acceleration_structures,
    VkQueryType                 query_type,
    VkQueryPool                 pool,
    uint32_t                    first_query)
{
    functions_.cmd_write_acceleration_structures_properties(
        command_buffer, count, acceleration_structures, query_type, pool, first_query);

    if (query_type != VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR)
    {
        return;
    }

    std::vector<VkAccelerationStructureKHR> acc_str_to_process(acceleration_structures,
                                                               acceleration_structures + count);

    std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> staging_buffer_entry =
        internal_buffer_manager_.CreateBuffer(
            sizeof(uint64_t) * count,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    // keep track of relation between AS to be compacted and the query pool that handles the results (the order of
    // AS is also important)

    auto [it, inserted] = compacted_sizes_unprocessed_.emplace(pool, std::vector<PreProcessingCompactionInfo>());
    it->second.push_back({ first_query, std::move(staging_buffer_entry), acc_str_to_process });
}

VkDeviceAddress VulkanAccelerationStructureBuilder::GetActualDeviceAddress(VkAccelerationStructureKHR handle)
{
    return GetAccelerationStructureDeviceAddress(handle);
}
VkDeviceAddress VulkanAccelerationStructureBuilder::GetAccelerationStructureDeviceAddress(
    VkAccelerationStructureKHR acceleration_structure)
{
    VkAccelerationStructureDeviceAddressInfoKHR info;
    info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    info.pNext                 = nullptr;
    info.accelerationStructure = acceleration_structure;

    util::MarkingLayersUtil::instance().BeginInjected(physical_device_info_);
    VkDeviceAddress address = functions_.get_acceleration_structure_device_address(device_, &info);
    util::MarkingLayersUtil::instance().EndInjected(physical_device_info_);
    return address;
}

VkAccelerationStructureKHR VulkanAccelerationStructureBuilder::CreateAccelerationStructure(
    VkAccelerationStructureBuildGeometryInfoKHR&    geometry_info,
    VkAccelerationStructureBuildRangeInfoKHR*       range_info,
    const VkAccelerationStructureBuildSizesInfoKHR& size_info,
    VkBuffer                                        storage)
{
    VkAccelerationStructureCreateInfoKHR create_info;
    create_info.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.pNext         = nullptr;
    create_info.createFlags   = 0;
    create_info.buffer        = storage;
    create_info.offset        = 0;
    create_info.size          = size_info.accelerationStructureSize;
    create_info.type          = geometry_info.type;
    create_info.deviceAddress = 0;

    VkAccelerationStructureKHR acceleration_structure;
    util::MarkingLayersUtil::instance().BeginInjected(physical_device_info_);
    functions_.create_acceleration_structure(device_, &create_info, nullptr, &acceleration_structure);
    util::MarkingLayersUtil::instance().EndInjected(physical_device_info_);
    return acceleration_structure;
}

// inject vkCmdCopyQueryPoolResults command that copies the results to internal buffer in the expected format
// processing of the results happens in OnCmdCopyAccelerationStructure
void VulkanAccelerationStructureBuilder::OnCmdCopyQueryPoolResults(const VulkanCommandBufferInfo* command_buffer_info,
                                                                   const VulkanQueryPoolInfo*     query_pool_info)
{
    if (!compacted_sizes_unprocessed_.count(query_pool_info->handle))
    {
        return;
    }

    std::vector<PreProcessingCompactionInfo>& unprocessed = compacted_sizes_unprocessed_[query_pool_info->handle];

    util::MarkingLayersUtil::instance().BeginInjected(physical_device_info_);
    for (uint64_t i = 0; i < unprocessed.size(); i++)
    {
        PreProcessingCompactionInfo& pre_processed{ unprocessed[i] };

        functions_.cmd_copy_query_pool_results(command_buffer_info->handle,
                                               query_pool_info->handle,
                                               pre_processed.first_query,
                                               pre_processed.parents.size(),
                                               pre_processed.buffer_info_wrapper->info_.handle,
                                               0,
                                               8,
                                               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        VkBufferMemoryBarrier buffer_memory_barrier{};
        buffer_memory_barrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buffer_memory_barrier.pNext         = nullptr;
        buffer_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        buffer_memory_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        buffer_memory_barrier.buffer        = pre_processed.buffer_info_wrapper->info_.handle,
        buffer_memory_barrier.offset        = 0;
        buffer_memory_barrier.size          = pre_processed.parents.size();

        functions_.cmd_pipeline_barrier(command_buffer_info->handle,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_HOST_BIT,
                                        0,
                                        0,
                                        nullptr,
                                        1,
                                        &buffer_memory_barrier,
                                        0,
                                        nullptr);
    }
    util::MarkingLayersUtil::instance().EndInjected(physical_device_info_);
}

// inject vkGetQueryPoolResults command to retrieve data in the desired format and write results in correlation to AS in
// processed map
void VulkanAccelerationStructureBuilder::OnGetQueryPoolResults(const VulkanDeviceInfo*    device_info,
                                                               const VulkanQueryPoolInfo* query_pool_info)
{
    if (!compacted_sizes_unprocessed_.count(query_pool_info->handle))
    {
        return;
    }

    auto& unprocessed = compacted_sizes_unprocessed_[query_pool_info->handle];

    for (uint64_t i = 0; i < unprocessed.size(); i++)
    {
        PreProcessingCompactionInfo&             pre_processed{ unprocessed[i] };
        std::vector<VkAccelerationStructureKHR>& vector_of_acc_str{ pre_processed.parents };

        std::vector<uint64_t> vector_of_acc_str_sizes(pre_processed.parents.size(), 0);

        util::MarkingLayersUtil::instance().BeginInjected(physical_device_info_);

        functions_.get_query_pool_results(device_info->handle,
                                          query_pool_info->handle,
                                          pre_processed.first_query,
                                          vector_of_acc_str.size(),
                                          vector_of_acc_str_sizes.size() * sizeof(uint64_t),
                                          vector_of_acc_str_sizes.data(),
                                          8,
                                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        util::MarkingLayersUtil::instance().EndInjected(physical_device_info_);

        // add results to compacted_sizes_processed map
        for (uint64_t j = 0; j < vector_of_acc_str.size(); j++)
        {
            compacted_sizes_processed_.try_emplace(vector_of_acc_str[j], vector_of_acc_str_sizes[j]);
        }
    }
    compacted_sizes_unprocessed_.erase(query_pool_info->handle);
}

void VulkanAccelerationStructureBuilder::OnQueueSubmit(uint32_t            submit_count,
                                                       const VkSubmitInfo* pSubmits,
                                                       VkFence             fence)
{
    for (uint32_t submit = 0; submit < submit_count; ++submit)
    {
        const VkSubmitInfo& info = pSubmits[submit];
        for (uint32_t command_buffer = 0; command_buffer < info.commandBufferCount; ++command_buffer)
        {
            auto recorded_scratches_entry = recorded_scratches.find(info.pCommandBuffers[command_buffer]);
            if (recorded_scratches_entry == recorded_scratches.end())
            {
                continue;
            }
            auto [iterator, inserted] = submitted_scratches.emplace(fence, std::move(recorded_scratches_entry->second));
            recorded_scratches.erase(recorded_scratches_entry);
        }
    }
}
void VulkanAccelerationStructureBuilder::OnQueueSubmit2(uint32_t             submit_count,
                                                        const VkSubmitInfo2* pSubmits,
                                                        VkFence              fence)
{
    for (uint32_t submit = 0; submit < submit_count; ++submit)
    {
        const VkSubmitInfo2& info = pSubmits[submit];
        for (uint32_t command_buffer = 0; command_buffer < info.commandBufferInfoCount; ++command_buffer)
        {
            auto recorded_scratches_entry =
                recorded_scratches.find(info.pCommandBufferInfos[command_buffer].commandBuffer);
            if (recorded_scratches_entry == recorded_scratches.end())
            {
                continue;
            }
            auto [iterator, inserted] = submitted_scratches.emplace(fence, std::move(recorded_scratches_entry->second));
            recorded_scratches.erase(recorded_scratches_entry);
        }
    }
}

void VulkanAccelerationStructureBuilder::OnWaitForFences(VkResult result, uint32_t fenceCount, const VkFence* pFences)
{
    for (uint32_t fence = 0; fence < fenceCount; ++fence)
    {
        VkResult get_status_result = functions_.get_fence_status(device_, pFences[fence]);
        if (get_status_result == VK_SUCCESS)
        {
            submitted_scratches.erase(pFences[fence]);
        }
    }
}

void VulkanAccelerationStructureBuilder::OnGetFenceStatus(VkResult result, VkFence fence)
{
    if (result == VK_SUCCESS)
    {
        submitted_scratches.erase(fence);
    }
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)