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
#include "decode/vulkan_object_info.h"
#include "decode/vulkan_query_util.h"
#include "format/format.h"
#include "decode/vulkan_acceleration_structure_builder.h"
#include "decode/vulkan_micromap_builder.h"
#include "util/callbacks.h"
#include "util/logging.h"

#include <algorithm>
#include <cstdint>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanAccelerationStructureBuilder::VulkanAccelerationStructureBuilder(
    const graphics::VulkanDeviceTable*      device_table,
    VkDevice                                device,
    VulkanResourceAllocator*                allocator,
    const VkPhysicalDeviceMemoryProperties& memory_properties,
    VulkanDeviceAddressTracker&             device_address_tracker) :
    device_(device),
    allocator_(allocator), physical_device_memory_properties_(memory_properties),
    device_address_tracker_(device_address_tracker),
    internal_buffer_manager_(device_table, device_, allocator_, physical_device_memory_properties_)
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
    VulkanAccelerationStructureKHRInfo*         acceleration_structure_info,
    VkAccelerationStructureKHR*                 handle)
{
    // Create new storage buffer for AccelerationStructure based on previously recorded
    // GetAccelerationStructureBuildSize (this call must be inserted by gfxrecon-optimize)
    VulkanResourceAllocator* allocator = device_info->allocator.get();
    assert(allocator != nullptr);
    assert(buffer_info != nullptr);

    VkAccelerationStructureBuildSizesInfoKHR build_sizes = max_build_sizes_;
    max_build_sizes_                                     = {};

    VkAccelerationStructureCreateInfoKHR modified_create_info = *create_info;

    bool reallocate = true;

    // Points to storage that will be used in the creation call
    auto* target_storage_buffer = const_cast<VulkanBufferInfo*>(buffer_info);

    if (auto compaction_record = compaction_child_to_parent_dependency_.find(acceleration_structure_info->capture_id);
        compaction_record != compaction_child_to_parent_dependency_.end())
    {
        const auto& [child, parent] = *compaction_record;
        build_sizes                 = {};

        if (auto ready_record = compacted_sizes_processed_.find(parent);
            ready_record == compacted_sizes_processed_.end())
        {
            // Handling for results (only in case of vkCmdCopyQueryPoolResults)
            for (const auto& [query_pool, compaction_infos] : compacted_sizes_unprocessed_)
            {
                for (const auto& [first_query, buffer, sources] : compaction_infos)
                {
                    std::vector<uint64_t> vector_of_acc_str_sizes(sources.size(), 0);
                    uint64_t              buffer_size = vector_of_acc_str_sizes.size() * sizeof(uint64_t);

                    util::MarkingLayersUtil::BeginInjected(device_info->handle);
                    void*    mapped;
                    VkResult mapping_result =
                        allocator_->MapResourceMemoryDirect(buffer_size, 0, &mapped, buffer->info_.allocator_data);
                    GFXRECON_ASSERT(mapping_result == VK_SUCCESS);

                    util::platform::MemoryCopy(vector_of_acc_str_sizes.data(), buffer_size, mapped, buffer_size);

                    allocator_->UnmapResourceMemoryDirect(buffer->info_.allocator_data);
                    util::MarkingLayersUtil::EndInjected(device_info->handle);

                    // add results to compacted_sizes_processed map
                    for (uint64_t j = 0; j < sources.size(); j++)
                    {
                        compacted_sizes_processed_.try_emplace(sources[j], vector_of_acc_str_sizes[j]);
                    }
                }
            }
        }

        auto ready_record = compacted_sizes_processed_.find(parent);
        GFXRECON_ASSERT(ready_record != compacted_sizes_processed_.end());

        // Compacted size data may be invalid - fallback to the uncompressed size of parent object
        if (ready_record->second == 0)
        {
            GFXRECON_LOG_WARNING_ONCE("Driver did not provide valid size data for acceleration structure compaction "
                                      "process. Replayer will use non-compacted sizes.");
            build_sizes.accelerationStructureSize =
                device_address_tracker_.GetAccelerationStructureByHandle(parent)->size;
        }
        else
        {
            build_sizes.accelerationStructureSize = ready_record->second;
        }

        reallocate                  = true;
        modified_create_info.size   = build_sizes.accelerationStructureSize;
        modified_create_info.offset = 0;
    }
    else if (build_sizes.accelerationStructureSize != 0)
    {
        modified_create_info.size   = build_sizes.accelerationStructureSize;
        modified_create_info.offset = 0;

        if (auto buffer_bucket = replaced_buffers_.find(buffer_info->capture_id);
            buffer_bucket != replaced_buffers_.end())
        {
            for (const auto& replacement : buffer_bucket->second)
            {
                if (replacement->info_.capture_address == acceleration_structure_info->capture_address &&
                    replacement->info_.replay_size >= build_sizes.accelerationStructureSize)
                {
                    modified_create_info.buffer = replacement->info_.handle;
                    target_storage_buffer       = &replacement->info_;
                    reallocate                  = false;
                    break;
                }
            }
        }
    }
    else
    {
        // no info, cant make a good decision, reuse
        reallocate           = false;
        modified_create_info = *create_info;
    }

    if (reallocate)
    {
        auto& replacements    = replaced_buffers_[buffer_info->capture_id];
        auto& new_replacement = replacements.emplace_back(internal_buffer_manager_.CreateBuffer(
            build_sizes.accelerationStructureSize,
            buffer_info->usage | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            buffer_info->memory_property_flags));

        new_replacement->info_.capture_address = acceleration_structure_info->capture_address;
        new_replacement->info_.capture_size    = create_info->size;

        modified_create_info.buffer = new_replacement->info_.handle;
        device_address_tracker_.TrackBuffer(&new_replacement->info_);
        target_storage_buffer = &new_replacement->info_;
    }

    // Last minute validation: storage buffer should be bigger than or equal to the acceleration structure size + offset
    // Note: We operate on 2 kinds of sizes - the size provided as an input in Create* calls and the actual size
    // retrieved from allocator/GetASBuildSizes query. Assume all these sizes should satisfy the above condition.
    size_t target_storage_buffer_allocated_size = allocator->GetBufferSize(target_storage_buffer->allocator_data);
    GFXRECON_ASSERT(target_storage_buffer_allocated_size >= modified_create_info.size + modified_create_info.offset);
    GFXRECON_ASSERT(target_storage_buffer->replay_size >= modified_create_info.size + modified_create_info.offset);

    acceleration_structure_info->size   = modified_create_info.size;
    acceleration_structure_info->offset = modified_create_info.offset;
    acceleration_structure_info->buffer = modified_create_info.buffer;

    VkResult result =
        functions_.create_acceleration_structure(device_info->handle, &modified_create_info, pAllocator, handle);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkAccelerationStructureDeviceAddressInfoKHR address_info{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, nullptr, *handle
    };
    acceleration_structure_info->replay_address =
        functions_.get_acceleration_structure_device_address(device_info->handle, &address_info);

    return result;
}

void VulkanAccelerationStructureBuilder::ProcessVulkanBuildAccelerationStructuresCommand(
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

void VulkanAccelerationStructureBuilder::ProcessVulkanCopyAccelerationStructuresCommand(
    uint32_t info_count, VkCopyAccelerationStructureInfoKHR* copy_infos)
{
    BeginCommandBuffer();
    for (uint32_t i = 0; i < info_count; ++i)
    {
        OnCmdCopyAccelerationStructure(cmd_execute_obj_.command_buffer_, &copy_infos[i]);
    }
    ExecuteCommandBuffer();
}

void VulkanAccelerationStructureBuilder::ProcessVulkanWriteAccelerationStructuresPropertiesCommand(
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
    auto buffer_bucket = replaced_buffers_.find(buffer_info->capture_id);
    if (buffer_bucket != replaced_buffers_.end())
    {
        for (auto& replacement : buffer_bucket->second)
        {
            device_address_tracker_.RemoveBuffer(&replacement->info_);
        }
    }
    replaced_buffers_.erase(buffer_info->capture_id);
}

void VulkanAccelerationStructureBuilder::InitializeFunctionPointers(const graphics::VulkanDeviceTable* device_table)
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
    util::MarkingLayersUtil::BeginInjected(device_);
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
    util::MarkingLayersUtil::EndInjected(device_);
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
    if (size_info->accelerationStructureSize > max_build_sizes_.accelerationStructureSize)
    {
        max_build_sizes_ = *size_info;
    }
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
        VkAccelerationStructureBuildSizesInfoKHR queried{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
        };

        std::vector<uint32_t> max_primitive_counts(geometry_infos[i].geometryCount);
        for (uint32_t g = 0; g < geometry_infos[i].geometryCount; ++g)
        {
            max_primitive_counts[g] = range_infos[i][g].primitiveCount;
        }

        functions_.get_acceleration_structure_build_sizes(device_,
                                                          VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                          &geometry_infos[i],
                                                          max_primitive_counts.data(),
                                                          &queried);
        auto cached_info =
            device_address_tracker_.GetAccelerationStructureByHandle(geometry_infos[i].dstAccelerationStructure);
        if (cached_info->size < queried.accelerationStructureSize)
        {
            GFXRECON_LOG_WARNING("Wrong expected size of acceleration structure %" PRIu64, cached_info->capture_id);
            GFXRECON_LOG_WARNING("\t expected size: %" PRIu64, cached_info->size);
            GFXRECON_LOG_WARNING("\t size queried from build geometry: %" PRIu64, queried.accelerationStructureSize);
            GFXRECON_ASSERT(false);
        }

        VkDeviceSize scratch_size = queried.buildScratchSize;
        if (geometry_infos[i].mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR)
        {
            scratch_size = queried.updateScratchSize;
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

    util::MarkingLayersUtil::BeginInjected(command_buffer_info->handle);
    for (uint64_t i = 0; i < unprocessed.size(); i++)
    {
        PreProcessingCompactionInfo& pre_processed{ unprocessed[i] };

        functions_.cmd_copy_query_pool_results(command_buffer_info->handle,
                                               query_pool_info->handle,
                                               pre_processed.first_query,
                                               pre_processed.sources.size(),
                                               pre_processed.buffer_info_wrapper->info_.handle,
                                               0,
                                               sizeof(uint64_t),
                                               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        VkBufferMemoryBarrier buffer_memory_barrier{};
        buffer_memory_barrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buffer_memory_barrier.pNext         = nullptr;
        buffer_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        buffer_memory_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        buffer_memory_barrier.buffer        = pre_processed.buffer_info_wrapper->info_.handle;
        buffer_memory_barrier.offset        = 0;
        buffer_memory_barrier.size          = pre_processed.sources.size() * sizeof(uint64_t);

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
    util::MarkingLayersUtil::EndInjected(command_buffer_info->handle);
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
        std::vector<VkAccelerationStructureKHR>& vector_of_acc_str{ pre_processed.sources };

        std::vector<uint64_t> vector_of_acc_str_sizes(pre_processed.sources.size(), 0);

        util::MarkingLayersUtil::BeginInjected(device_info->handle);

        functions_.get_query_pool_results(device_info->handle,
                                          query_pool_info->handle,
                                          pre_processed.first_query,
                                          vector_of_acc_str.size(),
                                          vector_of_acc_str_sizes.size() * sizeof(uint64_t),
                                          vector_of_acc_str_sizes.data(),
                                          8,
                                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        util::MarkingLayersUtil::EndInjected(device_info->handle);

        // add results to compacted_sizes_processed map
        for (uint64_t j = 0; j < vector_of_acc_str.size(); j++)
        {
            compacted_sizes_processed_.try_emplace(vector_of_acc_str[j], vector_of_acc_str_sizes[j]);
        }
    }
    compacted_sizes_unprocessed_.erase(query_pool_info->handle);
}

void VulkanAccelerationStructureBuilder::ProcessCapturedQueryPoolResults(const VulkanQueryPoolInfo* query_pool_info,
                                                                         uint32_t                   first_query,
                                                                         uint32_t                   query_count,
                                                                         const uint8_t*             data,
                                                                         size_t                     data_size,
                                                                         VkDeviceSize               stride,
                                                                         VkQueryResultFlags         flags)
{
    if ((query_pool_info == nullptr) || (data == nullptr) ||
        !compacted_sizes_unprocessed_.count(query_pool_info->handle))
    {
        return;
    }

    const size_t value_size  = ((flags & VK_QUERY_RESULT_64_BIT) != 0) ? sizeof(uint64_t) : sizeof(uint32_t);
    auto&        unprocessed = compacted_sizes_unprocessed_[query_pool_info->handle];

    auto it = unprocessed.begin();
    while (it != unprocessed.end())
    {
        const uint32_t range_begin = it->first_query;
        const uint32_t range_end   = it->first_query + static_cast<uint32_t>(it->sources.size());
        const uint32_t query_end   = first_query + query_count;

        if ((range_begin < first_query) || (range_end > query_end))
        {
            ++it;
            continue;
        }

        const uint32_t source_offset = range_begin - first_query;
        for (size_t i = 0; i < it->sources.size(); ++i)
        {
            compacted_sizes_processed_[it->sources[i]] =
                ReadCapturedQueryResult(data, data_size, stride, source_offset + static_cast<uint32_t>(i), value_size);
        }

        it = unprocessed.erase(it);
    }

    if (unprocessed.empty())
    {
        compacted_sizes_unprocessed_.erase(query_pool_info->handle);
    }
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
