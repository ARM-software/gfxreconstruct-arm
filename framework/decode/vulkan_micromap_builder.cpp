/*
** Copyright (c) 2024 LunarG, Inc.
** Copyright (c) 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
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
#include "decode/vulkan_micromap_builder.h"
#include "decode/vulkan_query_util.h"
#include "util/callbacks.h"
#include <algorithm>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanMicromapBuilder::VulkanMicromapBuilder(const graphics::VulkanDeviceTable*      device_table,
                                             VkDevice                                device,
                                             VulkanResourceAllocator*                allocator,
                                             const VkPhysicalDeviceMemoryProperties& properties,
                                             VulkanDeviceAddressTracker&             device_address_tracker) :
    device_(device),
    allocator_(allocator), device_address_tracker_(device_address_tracker),
    internal_buffer_manager_(device_table, device, allocator, properties)

{
    InitializeFunctionPointers(device_table);
}

void VulkanMicromapBuilder::OnGetMicromapBuildSizes(const VulkanDeviceInfo*             device_info,
                                                    VkAccelerationStructureBuildTypeKHR buildType,
                                                    VkMicromapBuildInfoEXT*             info,
                                                    VkMicromapBuildSizesInfoEXT*        size_info)
{
    auto address_remap = [this](VkDeviceAddress& capture_address) {
        auto buffer_info = device_address_tracker_.GetBufferByCaptureDeviceAddress(capture_address);
        // TODO: we 'should' find that buffer here, check what's missing
        if (buffer_info != nullptr && buffer_info->replay_address != 0)
        {
            uint64_t offset = capture_address - buffer_info->capture_address;
            // in-place address-remap via const-cast
            capture_address = buffer_info->replay_address + offset;
        }
    };

    address_remap(info->data.deviceAddress);
    address_remap(info->triangleArray.deviceAddress);
    functions_.get_micromap_build_sizes(device_info->handle, buildType, info, size_info);
    last_build_sizes_ = *size_info;
}

void VulkanMicromapBuilder::OnMicromapCompactionDependencyCommand(VkMicromapEXT                        parent,
                                                                  const std::vector<format::HandleId>& children)
{
    for (uint64_t i = 0; i < children.size(); i++)
    {
        compaction_child_to_parent_dependency_[children[i]] = parent;
    }
}

VkResult VulkanMicromapBuilder::OnCreateMicromap(const VulkanDeviceInfo*      device_info,
                                                 VkMicromapCreateInfoEXT*     info,
                                                 const VkAllocationCallbacks* pAllocator,
                                                 format::HandleId             capture_id,
                                                 VkMicromapEXT*               handle)
{
    // Create new storage buffer for Micromap based on previously recorded vkGetMicromapBuildSizesEXT (this call must be
    // inserted by gfxrecon-optimize)
    auto allocator = device_info->allocator.get();
    assert(allocator != nullptr);

    VkMicromapBuildSizesInfoEXT sizes = last_build_sizes_;
    last_build_sizes_                 = {};

    if (compaction_child_to_parent_dependency_.count(capture_id) != 0)
    {
        VkMicromapEXT parent = compaction_child_to_parent_dependency_[capture_id];
        sizes                = { VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT };
        // compaction flow
        if (compacted_sizes_processed_.count(parent) != 0)
        {
            sizes.micromapSize = compacted_sizes_processed_[parent];
        }
        else
        {
            // Handling for results (only in case of vkCmdCopyQueryPoolResults)
            for (auto it = compacted_sizes_unprocessed_.begin(); it != compacted_sizes_unprocessed_.end(); ++it)
            {
                std::vector<PreProcessingCompactionInfo>& vector = it->second;
                for (uint64_t i = 0; i < vector.size(); i++)
                {
                    auto&                       buffer       = vector[i].buffer_info_wrapper;
                    auto                        first_query  = vector[i].first_query;
                    std::vector<VkMicromapEXT>& vector_of_mm = vector[i].parents;

                    std::vector<uint64_t> vector_of_mm_sizes(vector_of_mm.size(), 0);
                    uint64_t              buffer_size = vector_of_mm_sizes.size() * sizeof(uint64_t);

                    util::MarkingLayersUtil::BeginInjected(device_);
                    void*    mapped;
                    VkResult mapping_result =
                        allocator_->MapResourceMemoryDirect(buffer_size, 0, &mapped, buffer->info_.allocator_data);
                    GFXRECON_ASSERT(mapping_result == VK_SUCCESS);

                    util::platform::MemoryCopy(vector_of_mm_sizes.data(), buffer_size, mapped, buffer_size);

                    allocator_->UnmapResourceMemoryDirect(buffer->info_.allocator_data);
                    util::MarkingLayersUtil::EndInjected(device_);

                    // This assert may get hit if the opacity micromaps were not build and the optimized sizes are
                    // not known. This situation can happen if gpu is mocked and the assert can be triggered in debug
                    // mode only
                    GFXRECON_ASSERT(vector_of_mm_sizes != std::vector<uint64_t>(vector_of_mm.size(), 0));

                    // add results to compacted_sizes_processed map
                    for (uint64_t j = 0; j < vector_of_mm.size(); j++)
                    {
                        compacted_sizes_processed_.try_emplace(vector_of_mm[j], vector_of_mm_sizes[j]);
                    }
                }
            }
            assert(compacted_sizes_processed_.count(parent) != 0);
            sizes.micromapSize = compacted_sizes_processed_[parent];
        }
    }

    assert(sizes.micromapSize != 0);

    std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> bufferInfoWrapper =
        internal_buffer_manager_.CreateBuffer(
            sizes.micromapSize, VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    info->size   = allocator->GetBufferSize(bufferInfoWrapper->info_.allocator_data);
    info->buffer = bufferInfoWrapper->info_.handle;
    info->offset = 0;

    VkResult result = functions_.create_micromap(device_info->handle, info, pAllocator, handle);
    assert(result == VK_SUCCESS);

    micromaps_[*handle] = { capture_id, sizes, std::move(bufferInfoWrapper) };
    return result;
}

void VulkanMicromapBuilder::OnCmdBuildMicromaps(VkCommandBuffer         command_buffer,
                                                uint32_t                info_count,
                                                VkMicromapBuildInfoEXT* build_infos)
{
    for (uint32_t i = 0; i < info_count; ++i)
    {
        assert(build_infos[i].type == VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT);

        if (micromaps_.count(build_infos[i].dstMicromap) == 0)
        {
            GFXRECON_LOG_FATAL("Handle %ul for vkCmdBuildMicromapsEXT not found", build_infos[i].dstMicromap);
            GFXRECON_ASSERT(false);
        }

        VkDeviceSize scratch_size = micromaps_[build_infos[i].dstMicromap].info.buildScratchSize;

        UpdateScratchDeviceAddress(build_infos[i], scratch_size);
        UpdateDeviceAddress(build_infos[i]);
    }
    functions_.cmd_build_micromaps(command_buffer, info_count, build_infos);
}

void VulkanMicromapBuilder::UpdateScratchDeviceAddress(VkMicromapBuildInfoEXT& build_infos, VkDeviceSize scratch_size)
{
    VkDeviceAddress capture_scratch_address = build_infos.scratchData.deviceAddress;

    format::HandleId capture_id = format::kNullHandleId;
    // When fastforwarding, the scratch buffers could be destroyed and not be recreated in state recreation
    const VulkanBufferInfo* original_scratch_entry =
        device_address_tracker_.GetBufferByCaptureDeviceAddress(capture_scratch_address);
    if (original_scratch_entry)
    {
        capture_id = original_scratch_entry->capture_id;
    }

    auto scratch_entries = scratch_double_buffer_.scratches_current.find(capture_id);

    if (scratch_entries != scratch_double_buffer_.scratches_current.end())
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
            build_infos.scratchData.deviceAddress = (*scratch_entry)->info_.replay_address;
        }
        else
        {
            const auto& new_scratch = scratch_entries->second.emplace_back(internal_buffer_manager_.CreateBuffer(
                scratch_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT));
            new_scratch->info_.capture_address    = build_infos.scratchData.deviceAddress;
            build_infos.scratchData.deviceAddress = new_scratch->info_.replay_address;
        }
    }
    else
    {
        auto [it, inserted] = scratch_double_buffer_.scratches_current.emplace(
            capture_id, std::vector<std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper>>());
        auto& new_scratch                     = it->second.emplace_back(internal_buffer_manager_.CreateBuffer(
            scratch_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT));
        new_scratch->info_.capture_address    = build_infos.scratchData.deviceAddress;
        build_infos.scratchData.deviceAddress = new_scratch->info_.replay_address;
    }
}

void VulkanMicromapBuilder::UpdateDeviceAddress(VkMicromapBuildInfoEXT& build_info)
{

    auto address_remap = [this](VkDeviceAddress& capture_address) {
        auto buffer_info = device_address_tracker_.GetBufferByCaptureDeviceAddress(capture_address);
        // TODO: we 'should' find that buffer here, check what's missing
        if (buffer_info != nullptr && buffer_info->replay_address != 0)
        {
            uint64_t offset = capture_address - buffer_info->capture_address;
            // in-place address-remap via const-cast
            capture_address = buffer_info->replay_address + offset;
        }
    };

    auto& data          = build_info.data.deviceAddress;
    auto& triangleArray = build_info.triangleArray.deviceAddress;
    address_remap(data);
    address_remap(triangleArray);
}

void VulkanMicromapBuilder::OnDestroyBuffer(const VulkanBufferInfo* buffer_info)
{
    scratch_double_buffer_.scratches_previous.erase(buffer_info->capture_id);
    scratch_double_buffer_.scratches_current.erase(buffer_info->capture_id);
}

void VulkanMicromapBuilder::OnCmdBuildAccStrHandling(VulkanDeviceAddressTracker& device_address_tracker,
                                                     uint32_t                    info_count,
                                                     VkAccelerationStructureBuildGeometryInfoKHR* infos)
{
    auto address_remap = [&device_address_tracker](VkDeviceAddress& capture_address) {
        auto buffer_info = device_address_tracker.GetBufferByCaptureDeviceAddress(capture_address);
        // TODO: we 'should' find that buffer here, check what's missing
        if (buffer_info != nullptr && buffer_info->replay_address != 0)
        {
            uint64_t offset = capture_address - buffer_info->capture_address;
            // in-place address-remap via const-cast
            capture_address = buffer_info->replay_address + offset;
        }
    };

    // replace indexBuffer address
    for (uint64_t i = 0; i < info_count; i++)
    {
        uint32_t geometry_count = infos[i].geometryCount;

        VkAccelerationStructureGeometryKHR*  pGeometries  = (VkAccelerationStructureGeometryKHR*)infos[i].pGeometries;
        VkAccelerationStructureGeometryKHR** ppGeometries = (VkAccelerationStructureGeometryKHR**)infos[i].ppGeometries;

        if ((infos[i].type != VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR) || (geometry_count == 0))
        {
            continue;
        }

        // If type is VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR then the geometryType member of each geometry in
        // either pGeometries or ppGeometries must be the same
        if ((pGeometries != nullptr && pGeometries->geometryType != VK_GEOMETRY_TYPE_TRIANGLES_KHR) ||
            (ppGeometries != nullptr && (*ppGeometries)->geometryType != VK_GEOMETRY_TYPE_TRIANGLES_KHR))
        {
            continue;
        }

        for (uint64_t j = 0; j < geometry_count; j++)
        {
            VkBaseOutStructure* pNextStruct = nullptr;
            if (pGeometries != nullptr)
            {
                pNextStruct = (VkBaseOutStructure*)(pGeometries[j].geometry.triangles.pNext);
            }
            else // ppGeometries != nullptr
            {
                pNextStruct = (VkBaseOutStructure*)(ppGeometries[j]->geometry.triangles.pNext);
            }

            while (pNextStruct != nullptr)
            {
                if (pNextStruct->sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT)
                {
                    VkAccelerationStructureTrianglesOpacityMicromapEXT* micromap_struct =
                        (VkAccelerationStructureTrianglesOpacityMicromapEXT*)pNextStruct;
                    address_remap(micromap_struct->indexBuffer.deviceAddress);
                }
                pNextStruct = pNextStruct->pNext;
            }
        }
    }
}

void VulkanMicromapBuilder::OnDestroyMicromap(const VulkanMicromapEXTInfo* micromap_info)
{
    micromaps_.erase(micromap_info->handle);
}

void VulkanMicromapBuilder::OnCmdWriteMicromapsProperties(VkCommandBuffer command_buffer,
                                                          uint32_t        count,
                                                          VkMicromapEXT*  micromaps,
                                                          VkQueryType     query_type,
                                                          VkQueryPool     pool,
                                                          uint32_t        first_query)
{
    if (query_type != VK_QUERY_TYPE_MICROMAP_COMPACTED_SIZE_EXT)
    {
        return;
    }

    std::vector<VkMicromapEXT> handles_to_process(micromaps, micromaps + count);

    std::unique_ptr<VulkanInternalBufferManager::BufferInfoWrapper> staging_buffer_entry =
        internal_buffer_manager_.CreateBuffer(
            sizeof(uint64_t) * count,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    // keep track of relation between MM to be compacted and the query pool that handles the results (the order of
    // MM is also important)

    auto [it, inserted] = compacted_sizes_unprocessed_.emplace(pool, std::vector<PreProcessingCompactionInfo>());
    it->second.push_back({ first_query, std::move(staging_buffer_entry), handles_to_process });
}

// inject vkCmdCopyQueryPoolResults command that copies the results to internal buffer in the expected format
// processing of the results happens in OnCreateMicromap
void VulkanMicromapBuilder::OnCmdCopyQueryPoolResults(const VulkanCommandBufferInfo* command_buffer_info,
                                                      const VulkanQueryPoolInfo*     query_pool_info)
{
    if (!compacted_sizes_unprocessed_.count(query_pool_info->handle))
    {
        return;
    }

    std::vector<PreProcessingCompactionInfo>& unprocessed = compacted_sizes_unprocessed_[query_pool_info->handle];

    util::MarkingLayersUtil::BeginInjected(device_);
    for (uint64_t i = 0; i < unprocessed.size(); i++)
    {
        PreProcessingCompactionInfo& pre_processed{ unprocessed[i] };

        functions_.cmd_copy_query_pool_results(command_buffer_info->handle,
                                               query_pool_info->handle,
                                               pre_processed.first_query,
                                               pre_processed.parents.size(),
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
        buffer_memory_barrier.size          = pre_processed.parents.size() * sizeof(uint64_t);

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
    util::MarkingLayersUtil::EndInjected(device_);
}

// inject vkGetQueryPoolResults command to retrieve data in the desired format and write results in correlation to AS in
// processed map
void VulkanMicromapBuilder::OnGetQueryPoolResults(const VulkanDeviceInfo*    device_info,
                                                  const VulkanQueryPoolInfo* query_pool_info)
{
    if (!compacted_sizes_unprocessed_.count(query_pool_info->handle))
    {
        return;
    }

    auto& unprocessed = compacted_sizes_unprocessed_[query_pool_info->handle];

    for (uint64_t i = 0; i < unprocessed.size(); i++)
    {
        PreProcessingCompactionInfo& pre_processed{ unprocessed[i] };
        std::vector<VkMicromapEXT>&  vector_of_mm{ pre_processed.parents };

        std::vector<uint64_t> vector_of_mm_sizes(pre_processed.parents.size(), 0);

        util::MarkingLayersUtil::BeginInjected(device_);

        functions_.get_query_pool_results(device_info->handle,
                                          query_pool_info->handle,
                                          pre_processed.first_query,
                                          vector_of_mm.size(),
                                          vector_of_mm_sizes.size() * sizeof(uint64_t),
                                          vector_of_mm_sizes.data(),
                                          8,
                                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        util::MarkingLayersUtil::EndInjected(device_);

        GFXRECON_ASSERT(vector_of_mm_sizes != std::vector<uint64_t>(vector_of_mm.size(), 0));

        // add results to compacted_sizes_processed map
        for (uint64_t j = 0; j < vector_of_mm.size(); j++)
        {
            compacted_sizes_processed_.try_emplace(vector_of_mm[j], vector_of_mm_sizes[j]);
        }
    }
    compacted_sizes_unprocessed_.erase(query_pool_info->handle);
}

void VulkanMicromapBuilder::ProcessCapturedQueryPoolResults(const VulkanQueryPoolInfo* query_pool_info,
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
        const uint32_t range_end   = it->first_query + static_cast<uint32_t>(it->parents.size());
        const uint32_t query_end   = first_query + query_count;

        if ((range_begin < first_query) || (range_end > query_end))
        {
            ++it;
            continue;
        }

        const uint32_t source_offset = range_begin - first_query;
        for (size_t i = 0; i < it->parents.size(); ++i)
        {
            compacted_sizes_processed_[it->parents[i]] =
                ReadCapturedQueryResult(data, data_size, stride, source_offset + static_cast<uint32_t>(i), value_size);
        }

        it = unprocessed.erase(it);
    }

    if (unprocessed.empty())
    {
        compacted_sizes_unprocessed_.erase(query_pool_info->handle);
    }
}

void VulkanMicromapBuilder::InitializeFunctionPointers(const graphics::VulkanDeviceTable* device_table)
{
    functions_.get_micromap_build_sizes    = device_table->GetMicromapBuildSizesEXT;
    functions_.create_micromap             = device_table->CreateMicromapEXT;
    functions_.cmd_build_micromaps         = device_table->CmdBuildMicromapsEXT;
    functions_.cmd_copy_query_pool_results = device_table->CmdCopyQueryPoolResults;
    functions_.cmd_pipeline_barrier        = device_table->CmdPipelineBarrier;
    functions_.get_query_pool_results      = device_table->GetQueryPoolResults;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
