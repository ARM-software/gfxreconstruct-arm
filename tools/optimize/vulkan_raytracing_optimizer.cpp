#include "vulkan_raytracing_optimizer.h"

#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "decode/referenced_resource_table.h"
#include "generated/generated_vulkan_consumer.h"
#include "util/defines.h"
#include "util/memory_output_stream.h"
#include "encode/parameter_buffer.h"
#include "vulkan/vulkan.h"
#include "decode/vulkan_object_info.h"
#include "util/vulkan_modifier_base.h"
#include "encode/struct_pointer_encoder.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

// Traverses modification chain to identify the original modification source call - e.g. FillMemoryCommand
VulkanRaytracingOptimizer::Call* VulkanRaytracingOptimizer::TrackSourceModification(Call*    start_call,
                                                                                    Buffer*  first_modified_buffer,
                                                                                    uint64_t offset,
                                                                                    uint64_t size)
{
    MemoryRangeReference start_referece{ start_call, 0, 0, 0 };

    // buffer that will be scanned for modifications of identified SBT offset
    Buffer* modified_buffer = first_modified_buffer;

    // get all modifications to memory this buffer was bound to
    auto* memory_modification_data = &modified_buffer->memory_binding->memory->modified_memory_ranges;

    // offset in the buffer with SBT + buffer binding offset
    uint64_t modified_memory_offset = modified_buffer->memory_binding->offset + offset;

    // result - found source modification
    Call* result = nullptr;

    // attempt to find the original modification containing SBT data in the chain of modificaitons applied to the
    // buffer starting with start_call_index then moving backwards to previous modifications
    // TODO: handle synchronization better - submit call might wait for another submit called later in the trace
    while (!result)
    {
        // find last command relative to start_call modifying identified offset in the buffer
        for (auto modification =
                 std::upper_bound(memory_modification_data->begin(), memory_modification_data->end(), start_referece);
             modification != memory_modification_data->begin();
             --modification)
        {
            // check if this modification affects target memory area
            const uint64_t input_range_end = offset + size;
            if (offset <= modification->end_offset_ && modification->start_offset_ <= input_range_end)
            {
                if (modification->source_modification == nullptr)
                {
                    // Reached the original modification, e.g. FillMemoryCommand
                    result = modification->call_;
                }
                else
                {
                    // This call modifies target memory with data retrieved from another source - continue lookup using
                    // source call e.g. vkCmdCopyBuffer
                    if (modification->call_->call_type == Call::CallType::CmdCopyBuffer)
                    {
                        // vkCmdCopyBuffer: repeat modification identification process on the source buffer of this copy
                        // command
                        CmdCopyBuffer* cmd_copy_buffer = reinterpret_cast<CmdCopyBuffer*>(modification->call_);

                        // find source copy region
                        for (const auto& modification_region : cmd_copy_buffer->regions)
                        {
                            if (offset == std::clamp(offset,
                                                     modification_region.dstOffset,
                                                     modification_region.dstOffset + modification_region.size))
                            {
                                // update local variables, analyze the modifications affecting the source buffer
                                offset          = modification_region.srcOffset;
                                modified_buffer = cmd_copy_buffer->source;
                                memory_modification_data =
                                    &modified_buffer->memory_binding->memory->modified_memory_ranges;
                                start_referece = { modification->call_, 0, 0, 0 };
                                modification   = std::upper_bound(
                                    memory_modification_data->begin(), memory_modification_data->end(), start_referece);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}

// Prepares gathered data for analysis
// Fills out missing information in collected data based on full trace data
void VulkanRaytracingOptimizer::ProcessIntermediateData()
{
    // Go over all registered vulkan objects and let them prepare internal data used later in analysis
    for (const auto& [_, vulkan_object] : objects_)
    {
        vulkan_object->ProcessIntermediateData();
    }
}

void VulkanRaytracingOptimizer::ValidateBuildAccelerationStructureCalls()
{
    // For each CmdBuildAccelerationStructure command
    // Check if instance buffer was bound to a compute pipeline, issue a warning if so
    // TODO: In this case needs a solution based on spirv parsing
    // for (const auto& instance_buffer_id : tlas_build_commands_[command_buffer])
    // {
    //     // was this buffer bound to compute?
    //     // Get descriptor sets from vkCmdBindDescriptorSets in this cmdbuffer
    //     for (const auto& descriptor_set : bound_descriptor_sets_[command_buffer])
    //     {
    //         // for each descriptor set, get buffers from vkUpdateDescriptorSets up to this point
    //         for (const auto& buffer_binding : descriptor_data_[descriptor_set.descriptor_handle])
    //         {}
    //     }
    // }
}

// Every FillMemoryCommand was scanned for known values of shader group handles, buffer and acceleration structure
// device addresses. Matching instances were tracked, now they need to be verified
void VulkanRaytracingOptimizer::ProcessRuntimeVariables()
{
    // TODO
}

bool VulkanRaytracingOptimizer::CanOptimize()
{
    // First, fill out any missing data in previous calls based on full trace database
    ProcessIntermediateData();

    // Proceed with validation
    ValidateBuildAccelerationStructureCalls();

    // Verify buffer areas affected by FixDeviceAddress actually did contain device addresses
    ValidateFixDeviceAddressCommands();

    // Every FillMemoryCommand was scanned for known values of shader group handles, buffer and acceleration structure
    // device addresses Matching instances were tracked, now they need to be verified
    ProcessRuntimeVariables();

    // Each trace rays call contains a set of device addresses of shader binding tables
    // Attempt to track these addresses to the original call inserting shader grup handles at those addresses
    ProcessCmdTraceRays();

    return true;
}

void VulkanRaytracingOptimizer::ProcessCmdTraceRays()
{
    // Verify memory modifications with suspected shader group handle values by tracking back vkCmdTraceRays calls
    for (auto* cmd_trace_rays : cmd_trace_rays_commands_)
    {
        // Buffer containing the shader binding table
        Buffer* sbt_buffer = cmd_trace_rays->raygen.sbt_buffer;

        assert(sbt_buffer != nullptr);
        assert(sbt_buffer->memory_binding != nullptr);

        // shader binding table may be created at some offset
        const auto sbt_offset_in_buffer = cmd_trace_rays->raygen.deviceAddress - sbt_buffer->device_address;

        // attempt to find the original modification containing SBT data in the chain of modificaitons applied to
        // the buffer starting with submit_call_index then moving backwards to previos modifications
        // TODO: handle synchronization better - submit call might wait for another submit called later in the trace
        Call* source_modification =
            TrackSourceModification(cmd_trace_rays, sbt_buffer, sbt_offset_in_buffer, cmd_trace_rays->raygen.size);

        // TODO:
        // Verify source modification against SGH values detected in FillMemoryCommands, etc.
    }
}

//  For each buffer, go over set bindings:
//      vkCmdBindIndexBuffer
//      vkCmdBindVertexBuffers
//      vkCmdBindDescriptorSets + vkUpdateDescriptorSets
//  For each FixDeviceAddres before submission, if any of these updates
//  buffers bound as index or vertex, that location update should be removed
void VulkanRaytracingOptimizer::ValidateFixDeviceAddressCommands()
{
    // for each fix device address command
    // validate buffers bound to modified memory offset
    // TODO: Currently checks bindings based on location offset only, should cover buffer memory range:
    // <offset,offset+size>
    for (const auto* fix_device_address_command : fix_device_address_commands_)
    {
        const auto* object = objects_[fix_device_address_command->memory_id].get();
        if (object->type == VK_OBJECT_TYPE_DEVICE_MEMORY)
        {
            const auto*                           memory      = reinterpret_cast<const MemoryAllocation*>(object);
            VkDeviceSize                          data_offset = fix_device_address_command->fill_memory_data_offset_;
            std::unordered_set<BindBufferMemory*> bound_buffers;

            // Loop over all marked offsets
            // TODO: This is outdated due to rearchitecturing, memory ranges should be marked with MemoryRangeReference already, no need to process buffer bindings here
            //for (auto location : fix_device_address_command->locations)
            //{
            //    // actual memory offset is a sum of FillMemoryCommand data offset and FixDeviceAddress location
            //    // offset
            //    VkDeviceSize marked_offset = data_offset + location.offset_in_memory;
            //    // Get buffers bound to memory at marked_offset
            //    // bound_buffers.merge(memory->GetBoundBuffersByOffset(marked_offset));
            //}
            //for (const auto* buffer_binding : bound_buffers)
            //{
            //    // For each bound buffer calculate offset inside that buffer based on binding offset and marked
            //    // offset
            //    // Check actual usage of these offsets in buffers - bound as index buffers, etc.
            //}
        }
    }
}

// Multiple buffers can be bound to the same or overlapping areas of memory, so there can be multiple buffers
// containing given device address This function returns all such buffers, except for those that never had its
// device address retrieved current_index can be set to filter out deleted buffers
std::vector<VulkanRaytracingOptimizer::Buffer*>
VulkanRaytracingOptimizer::GetBufferByDeviceAddress(VkDeviceAddress address, uint64_t current_index)
{
    std::vector<Buffer*> matching_buffers;
    // Traverse buffers that had its device address retrieved, starting with the one matching the input address
    // value closest
    for (auto it = buffer_device_addresses_.upper_bound(address); it != buffer_device_addresses_.begin(); it--)
    {
        auto  found_device_address = it->first;
        auto* buffer               = it->second;
        if (buffer->destruction_call < current_index)
        {
            continue;
        }
        if (address == std::clamp(address, found_device_address, found_device_address + buffer->size))
        {
            matching_buffers.push_back(buffer);
        }
    }
    return matching_buffers;
}

void VulkanRaytracingOptimizer::Process_vkGetRayTracingShaderGroupHandlesKHR(const ApiCallInfo&       call_info,
                                                                             VkResult                 returnValue,
                                                                             format::HandleId         device,
                                                                             format::HandleId         pipeline,
                                                                             uint32_t                 firstGroup,
                                                                             uint32_t                 groupCount,
                                                                             size_t                   dataSize,
                                                                             PointerDecoder<uint8_t>* pData)
{
    // Gather SGH values for each pipeline separately
    if (!IsModificationPass())
    {
        // TODO: dataSize can be different, check SGH size based on
        // VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupHandleSize
        // TODO: use firstGroup to identify exact groups
        const auto single_entry_size = dataSize / groupCount;
        assert(single_entry_size == 32);

        for (uint32_t group_index = 0; group_index < groupCount; ++group_index)
        {
            auto sgh_value_pointer = pData->GetPointer() + (single_entry_size * group_index);
            shader_group_handle_entries_[pipeline][group_index + firstGroup].assign(
                sgh_value_pointer, sgh_value_pointer + single_entry_size);
        }
    }
}

void VulkanRaytracingOptimizer::Process_vkCmdTraceRaysKHR(
    const ApiCallInfo&                                             call_info,
    format::HandleId                                               commandBuffer,
    StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pRaygenShaderBindingTable,
    StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pMissShaderBindingTable,
    StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pHitShaderBindingTable,
    StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pCallableShaderBindingTable,
    uint32_t                                                       width,
    uint32_t                                                       height,
    uint32_t                                                       depth)
{
    if (IsModificationPass())
    {
        return;
    }
    auto trace_rays = std::make_unique<CmdTraceRays>(block_index_,
                                                     commandBuffer,
                                                     pRaygenShaderBindingTable->GetPointer(),
                                                     pMissShaderBindingTable->GetPointer(),
                                                     pHitShaderBindingTable->GetPointer(),
                                                     pCallableShaderBindingTable->GetPointer());

    // TODO: There may be many buffers bound to area of memory with provided device address
    // picking first one is not ideal, SBT's need a better identification
    trace_rays->callable.sbt_buffer = GetBufferByDeviceAddress(trace_rays->callable.deviceAddress)[0];
    trace_rays->raygen.sbt_buffer   = GetBufferByDeviceAddress(trace_rays->raygen.deviceAddress)[0];
    trace_rays->hit.sbt_buffer      = GetBufferByDeviceAddress(trace_rays->hit.deviceAddress)[0];
    trace_rays->miss.sbt_buffer     = GetBufferByDeviceAddress(trace_rays->miss.deviceAddress)[0];

    command_buffers_[commandBuffer].calls.push_back(trace_rays.get());
    cmd_trace_rays_commands_.push_back(trace_rays.get());
    calls_[block_index_] = std::move(trace_rays);
}

void VulkanRaytracingOptimizer::ProcessFillMemoryCommand(uint64_t memory_id,
                                                         uint64_t offset,
                                                         uint64_t size,
                                                         uint8_t* data)
{
    if (!IsModificationPass())
    {
        // check if this fillmemorycommand has an associated Fix command - should be last call in calls_ map
        auto* last_call = calls_.rbegin()->second.get();
        if (last_call->call_type == Call::CallType::FixDeviceAddress)
        {
            // verify associated fix command, add information on FillMemory offset to properly track memory
            // modification
            FixDeviceAddress* fix_call = reinterpret_cast<FixDeviceAddress*>(last_call);
            assert(fix_call->memory_id == memory_id);
            fix_call->fill_memory_data_offset_ = offset;
        }

        auto fill_memory_command = std::make_unique<FillMemoryCommand>(block_index_);

        MemoryAllocation* modified_memory = reinterpret_cast<MemoryAllocation*>(objects_.at(memory_id).get());

        // register memory modification
        MemoryModification modification{ fill_memory_command.get(), modified_memory, offset, offset + size };

        // Find SGH values inside data parameter
        // For each pipeline-SGH groups pair
        for (const auto& [_, sgh_groups] : shader_group_handle_entries_)
        {
            // For each SGH index-value pair
            for (const auto& [_, sgh_value] : sgh_groups)
            {
                const uint64_t sgh_size = sgh_value.size();

                // TODO: aligment can change - use shaderGroupBaseAlignment from
                // VkPhysicalDeviceRayTracingPipelinePropertiesKHR
                const uint64_t sgh_alignment = 32;

                if (sgh_size > size)
                {
                    // early exit; modification size is smaller that SGH size, this can't be a valid (hopefully)
                    break;
                }

                // attempt to compare provided data with known SGH values
                for (uint64_t data_offset = 0; data_offset < size - sgh_size; data_offset += sgh_alignment)
                {
                    uint8_t* start = data + data_offset;
                    if (memcmp(sgh_value.data(), start, sgh_size) == 0)
                    {
                        // FillMemory contains matching SGH value - verify this memory at this offset
                        // is used as SGH in SBT in some vkCmdTraceRays
                        // SGH value offset is a sum of FillMemoryCommand offset and the offset within the modified
                        // range
                        VkDeviceAddress sgh_location = offset + data_offset;
                        modification.shader_group_handles_.push_back({sgh_location, sgh_location + sgh_size});
                    }
                }
            }
        }

        // Find device addresses inside data parameter
        for (const auto& [device_address, _] : buffer_device_addresses_)
        {
            // attempt to compare provided data with known device address values
            for (uint64_t data_offset = 0; data_offset < size - sizeof(VkDeviceAddress);
                 data_offset += sizeof(VkDeviceAddress))
            {
                uint8_t* possible_device_address_location = data + data_offset;
                if (memcmp(&device_address, possible_device_address_location, sizeof(VkDeviceAddress)) == 0)
                {
                    // FillMemory contains matching device address value
                    // Track this for later verification
                    // device address value offset is a sum of FillMemoryCommand offset and the offset within the
                    // modified range
                    VkDeviceAddress device_address_location = offset + data_offset;
                    modification.device_addresses_.push_back({device_address_location, device_address_location + sizeof(VkDeviceAddress)});
                }
            }
        }

        // Register memory modification for memory itself
        modified_memory->modified_memory_ranges.push_back(modification);
    }
}

void VulkanRaytracingOptimizer::Process_vkCmdUpdateBuffer(const ApiCallInfo&       call_info,
                                                          format::HandleId         commandBuffer,
                                                          format::HandleId         dstBuffer,
                                                          VkDeviceSize             dstOffset,
                                                          VkDeviceSize             dataSize,
                                                          PointerDecoder<uint8_t>* pData)
{
    // TODO: scan for BDA/SGH in pData
}

void VulkanRaytracingOptimizer::Process_vkQueueSubmit(const ApiCallInfo&                          call_info,
                                                      VkResult                                    returnValue,
                                                      format::HandleId                            queue,
                                                      uint32_t                                    submitCount,
                                                      StructPointerDecoder<Decoded_VkSubmitInfo>* pSubmits,
                                                      format::HandleId                            fence)
{
    if (IsModificationPass())
    {
        return;
    }
    auto queue_submit = std::make_unique<QueueSubmitCall>(block_index_);

    for (uint32_t submit_index = 0; submit_index < submitCount; ++submit_index)
    {
        const auto& submission = pSubmits[submit_index].GetMetaStructPointer();
        for (uint32_t command_buffer_index = 0; command_buffer_index < submission->pCommandBuffers.GetLength();
             ++command_buffer_index)
        {
            const auto& command_buffer = submission->pCommandBuffers.GetPointer()[command_buffer_index];

            // Copy command buffer data into queue submit calls internal vector
            queue_submit->command_buffers.push_back(command_buffers_[command_buffer]);

            for (auto* call : queue_submit->command_buffers.back().calls)
            {
                reinterpret_cast<CommandCall*>(call)->command_buffer_instance = &queue_submit->command_buffers.back();
            }
        }
    }
    calls_[block_index_] = std::move(queue_submit);
}

// Record FixDeviceAddressCommand for future reference
// Fix/FillMemory commands might happen at any time, so this data may be processed only on submission
void VulkanRaytracingOptimizer::ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader& header,
                                                              const format::AddressLocationInfo*           infos)
{
    if (IsModificationPass())
    {
        return;
    }
    auto fix_device_command = std::make_unique<FixDeviceAddress>(block_index_);
    VulkanObject* object = objects_[header.relation_id].get();
    if (object->type == VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY)
    {
        fix_device_command->memory_id = header.relation_id;
        fix_device_command->locations = std::vector<format::AddressLocationInfo>(infos, infos + header.num_of_locations);
    }
    fix_device_address_commands_.push_back(fix_device_command.get());
    calls_[block_index_] = std::move(fix_device_command);
}

void VulkanRaytracingOptimizer::ProcessFixShaderGroupHandleCommand(
    const format::FixShaderGroupHandleCommandHeader& header, const format::ShaderHandleLocationInfo* infos)
{}

void VulkanRaytracingOptimizer::Process_vkBeginCommandBuffer(
    const ApiCallInfo&                                      call_info,
    VkResult                                                returnValue,
    format::HandleId                                        commandBuffer,
    StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo)
{
    if (IsModificationPass())
    {
        return;
    }

    command_buffers_[commandBuffer].Reset();
}

// Record BindBufferMemory for future reference.
// Buffers may be bound to memory at any time and FillMemory might have happened before or after binding.
// Binding data needs to be recorded here for processing on QueueSubmit
void VulkanRaytracingOptimizer::Process_vkBindBufferMemory(const ApiCallInfo& call_info,
                                                           VkResult           returnValue,
                                                           format::HandleId   device,
                                                           format::HandleId   buffer,
                                                           format::HandleId   memory,
                                                           VkDeviceSize       memoryOffset)
{
    if (IsModificationPass())
    {
        return;
    }

    calls_[block_index_] = std::make_unique<BindBufferMemory>(block_index_,
                                                              dynamic_cast<Buffer*>(objects_[buffer].get()),
                                                              dynamic_cast<MemoryAllocation*>(objects_[memory].get()),
                                                              memoryOffset);
}

// Add new draw command data for this cmdbuffer
void VulkanRaytracingOptimizer::Process_vkCmdDrawIndexed(const ApiCallInfo& call_info,
                                                         format::HandleId   commandBuffer,
                                                         uint32_t           indexCount,
                                                         uint32_t           instanceCount,
                                                         uint32_t           firstIndex,
                                                         int32_t            vertexOffset,
                                                         uint32_t           firstInstance)
{
    if (IsModificationPass())
    {
        return;
    }
    auto draw_command = std::make_unique<DrawIndexedCall>(block_index_, commandBuffer);
    draw_command->index_count = indexCount;
    draw_command->first_index = firstIndex;
    uint32_t           index_type_size = 0;
    switch (draw_command->index_buffer.index_type)
    {
        case VK_INDEX_TYPE_UINT16:
            index_type_size = 2;
            break;
        case VK_INDEX_TYPE_UINT32:
            index_type_size = 4;
            break;
        default:
            index_type_size = 4;
    }
    draw_command->index_buffer.size = indexCount * index_type_size;
    command_buffers_[commandBuffer].draw_indexed_commands.push_back(draw_command.get());
    calls_[block_index_] = std::move(draw_command);
}

// Store index buffer binding data for the command buffer
// Complete range of index data is not known, size is not specified at this point - filled in draw command
void VulkanRaytracingOptimizer::Process_vkCmdBindIndexBuffer(const ApiCallInfo& call_info,
                                                             format::HandleId   commandBuffer,
                                                             format::HandleId   buffer,
                                                             VkDeviceSize       offset,
                                                             VkIndexType        indexType)
{
    if (IsModificationPass())
    {
        return;
    }
    auto& command_buffer = command_buffers_[commandBuffer];
    auto  bind_call = std::make_unique<CommandCall>(block_index_, Call::CallType::CmdBindIndexBuffer, commandBuffer);
    command_buffer.calls.push_back(bind_call.get());
    auto* buffer_data = objects_.at(buffer).get();
    // TODO: buffer could be bound after this call, move this to later stage, OnQueueSubmit/CanOptimize


    command_buffers_[commandBuffer].current_bound_index_buffer.index_type = indexType;
    command_buffers_[commandBuffer].current_bound_index_buffer.offset     = offset;
    command_buffers_[commandBuffer].current_bound_index_buffer.size       = 0;
    calls_[block_index_] = std::move(bind_call);
}

void VulkanRaytracingOptimizer::Process_vkGetBufferDeviceAddress(
    const ApiCallInfo&                                       call_info,
    VkDeviceAddress                                          returnValue,
    format::HandleId                                         device,
    StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                 = pInfo->GetMetaStructPointer()->buffer;
    buffer_device_addresses_[returnValue] = reinterpret_cast<Buffer*>(objects_[buffer_id].get());
}

void VulkanRaytracingOptimizer::Process_vkCreateBuffer(const ApiCallInfo&                                   call_info,
                                                       VkResult                                             returnValue,
                                                       format::HandleId                                     device,
                                                       StructPointerDecoder<Decoded_VkBufferCreateInfo>*    pCreateInfo,
                                                       StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                                       HandlePointerDecoder<VkBuffer>*                      pBuffer)
{
    // TODO: Add buffer to objects_
}

void VulkanRaytracingOptimizer::Process_vkAllocateMemory(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    format::HandleId                                     device,
    StructPointerDecoder<Decoded_VkMemoryAllocateInfo>*  pAllocateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkDeviceMemory>*                pMemory)
{
    if (IsModificationPass())
    {
        return;
    }
    format::HandleId handle = *pMemory->GetPointer();
    objects_[handle] = std::make_unique<MemoryAllocation>(handle, pAllocateInfo->GetPointer()->allocationSize);
}

void VulkanRaytracingOptimizer::Process_vkCmdCopyBuffer(const ApiCallInfo&                          call_info,
                                                        format::HandleId                            commandBuffer,
                                                        format::HandleId                            srcBuffer,
                                                        format::HandleId                            dstBuffer,
                                                        uint32_t                                    regionCount,
                                                        StructPointerDecoder<Decoded_VkBufferCopy>* pRegions)
{
    if (IsModificationPass())
    {
        return;
    }
    auto copy_command = std::make_unique<CmdCopyBuffer>(block_index_, commandBuffer);

    command_buffers_[commandBuffer].copy_commands.push_back(copy_command.get());
    command_buffers_[commandBuffer].calls.push_back(copy_command.get());

    copy_command->destination = reinterpret_cast<Buffer*>(&objects_.at(dstBuffer));
    copy_command->source      = reinterpret_cast<Buffer*>(&objects_.at(srcBuffer));
    copy_command->regions     = std::vector<VkBufferCopy>(pRegions->GetPointer(), pRegions->GetPointer() + regionCount);
    copy_command->destination->buffer_updates[block_index_].push_back(copy_command.get());

    calls_[block_index_] = std::move(copy_command);
}

void VulkanRaytracingOptimizer::Process_vkCmdBuildAccelerationStructuresKHR(
    const ApiCallInfo&                                                         call_info,
    format::HandleId                                                           commandBuffer,
    uint32_t                                                                   infoCount,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   ppBuildRangeInfos)
{
    if (IsModificationPass())
    {
        return;
    }
    calls_[block_index_] =
        std::make_unique<CommandCall>(block_index_, Call::CallType::CmdBuildAccelerationStructures, commandBuffer);
    // Keep track of instance device addresses
    for (uint32_t info_index = 0; info_index < infoCount; ++info_index)
    {
        const auto& geometry_info = pInfos->GetPointer()[info_index];
        const auto& mode          = geometry_info.mode;

        if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR)
        {
            for (uint32_t geometry_index = 0; geometry_index < geometry_info.geometryCount; ++geometry_index)
            {
                auto& geometry_data =
                    const_cast<VkAccelerationStructureGeometryKHR*>(geometry_info.pGeometries)[geometry_index];
                if (geometry_data.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
                {
                    continue;
                }
                switch (geometry_data.geometryType)
                {
                    case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                    {
                        const auto& triangles = geometry_data.geometry.triangles;
                        break;
                    }
                    case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                    {
                        const auto& instances = geometry_data.geometry.instances;
                        instance_addresses_.push_back(
                            { dynamic_cast<CommandCall*>(calls_[block_index_].get()), instances.data.deviceAddress });
                        break;
                    }
                    case VK_GEOMETRY_TYPE_AABBS_KHR:
                    {
                        const auto& aabbs = geometry_data.geometry.aabbs;
                        break;
                    }
                    default:
                    {
                        GFXRECON_LOG_ERROR("Unexpected geometry type");
                        break;
                    }
                }
            }
        }
    }
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
