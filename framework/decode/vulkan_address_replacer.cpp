/*
** Copyright (c) 2024 LunarG, Inc.
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

#include "decode/vulkan_address_replacer.h"
#include "decode/vulkan_address_replacer_shaders.h"
#include "util/marking_layers.h"
#include "util/logging.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

inline uint32_t aligned_size(uint32_t size, uint32_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

inline uint32_t div_up(uint32_t nom, uint32_t denom)
{
    GFXRECON_ASSERT(denom > 0)
    return (nom + denom - 1) / denom;
}

uint32_t get_memory_type_index(const VkPhysicalDeviceMemoryProperties& memory_properties,
                               uint32_t                                type_bits,
                               VkMemoryPropertyFlags                   property_flags)
{
    uint32_t memory_type_index = std::numeric_limits<uint32_t>::max();

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        if ((type_bits & (1 << i)) &&
            ((memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags))
        {
            memory_type_index = i;
            break;
        }
    }

    return memory_type_index;
}

struct hashmap_t
{
    VkDeviceAddress storage;
    uint32_t        size;
    uint32_t        capacity;
};

struct replacer_params_t
{
    hashmap_t hashmap;

    // input-/output-arrays can be identical when sbt-alignments/strides match
    VkDeviceAddress input_handles, output_handles;
    uint32_t        num_handles;
};

VulkanAddressReplacer::buffer_context_t::~buffer_context_t()
{
    if (resource_allocator != nullptr)
    {
        if (buffer != VK_NULL_HANDLE)
        {
            resource_allocator->DestroyBufferDirect(buffer, nullptr, allocator_data);
        }
        if (device_memory != VK_NULL_HANDLE)
        {
            resource_allocator->FreeMemoryDirect(device_memory, nullptr, memory_data);
        }
    }
}

VulkanAddressReplacer::acceleration_structure_asset_t::~acceleration_structure_asset_t()
{
    if (handle != VK_NULL_HANDLE && destroy_fn != nullptr && device != VK_NULL_HANDLE)
    {
        destroy_fn(device, handle, nullptr);
    }
}

VulkanAddressReplacer::VulkanAddressReplacer(const VulkanDeviceInfo*              device_info,
                                             const encode::VulkanDeviceTable*     device_table,
                                             const decode::CommonObjectInfoTable& object_table) :
    device_table_(device_table),
    device_info_(device_info), object_table_(&object_table), dispatcher_(device_table)
{
    GFXRECON_ASSERT(device_info != nullptr && device_table != nullptr)

    const VulkanPhysicalDeviceInfo* physical_device_info =
        object_table.GetVkPhysicalDeviceInfo(device_info_->parent_id);

    if (physical_device_info != nullptr && physical_device_info->capture_raytracing_properties &&
        physical_device_info->replay_device_info->raytracing_properties)
    {
        capture_ray_properties_ = *physical_device_info->capture_raytracing_properties;
        replay_ray_properties_  = *physical_device_info->replay_device_info->raytracing_properties;

        if (capture_ray_properties_->shaderGroupHandleSize != replay_ray_properties_->shaderGroupHandleSize ||
            capture_ray_properties_->shaderGroupHandleAlignment != replay_ray_properties_->shaderGroupHandleAlignment ||
            capture_ray_properties_->shaderGroupBaseAlignment != replay_ray_properties_->shaderGroupBaseAlignment)
        {
            valid_sbt_alignment_ = false;
        }

        GFXRECON_ASSERT(physical_device_info->replay_device_info != nullptr);
        GFXRECON_ASSERT(physical_device_info->replay_device_info->memory_properties.has_value());
        memory_properties_ = *physical_device_info->replay_device_info->memory_properties;
    }
}

VulkanAddressReplacer::VulkanAddressReplacer(VulkanAddressReplacer&& other) noexcept : VulkanAddressReplacer()
{
    swap(*this, other);
}

VulkanAddressReplacer::~VulkanAddressReplacer()
{
    if (device_info_ != nullptr)
    {
        util::MarkingLayersUtil::instance().BeginInjected(device_info_);

        // explicitly free resources here, in order to mark destruction API-calls as injected
        pipeline_context_sbt_ = {};
        pipeline_context_bda_ = {};
        shadow_sbt_map_       = {};

        if (pipeline_bda_ != VK_NULL_HANDLE)
        {
            device_table_->DestroyPipeline(device_info_->handle, pipeline_bda_, nullptr);
        }
        if (pipeline_sbt_ != VK_NULL_HANDLE)
        {
            device_table_->DestroyPipeline(device_info_->handle, pipeline_sbt_, nullptr);
        }
        if (pipeline_layout_ != VK_NULL_HANDLE)
        {
            device_table_->DestroyPipelineLayout(device_info_->handle, pipeline_layout_, nullptr);
        }

        util::MarkingLayersUtil::instance().EndInjected(device_info_);
    }
}

void VulkanAddressReplacer::ProcessCmdTraceRays(
    const VulkanCommandBufferInfo*                                                              command_buffer_info,
    VkStridedDeviceAddressRegionKHR*                                                            raygen_sbt,
    VkStridedDeviceAddressRegionKHR*                                                            miss_sbt,
    VkStridedDeviceAddressRegionKHR*                                                            hit_sbt,
    VkStridedDeviceAddressRegionKHR*                                                            callable_sbt,
    const decode::VulkanDeviceAddressTracker&                                                   address_tracker,
    const std::unordered_map<graphics::shader_group_handle_t, graphics::shader_group_handle_t>& group_handle_map)
{
    GFXRECON_ASSERT(device_table_ != nullptr);

    // NOTE: we expect this map to be populated here, but not for older captures (before #1844) using trimming.
    if (group_handle_map.empty())
    {
        // the capture appears to be older and is missing information we require here -> bail out
        return;
    }

    // figure out if the captured group-handles are valid for replay
    bool valid_group_handles = true;

    for (const auto& [lhs, rhs] : group_handle_map)
    {
        if (lhs != rhs)
        {
            valid_group_handles = false;
            break;
        }
    }

    // TODO: testing only -> remove when closing issue #1526
    //    valid_sbt_alignment_ = false;
    //    valid_group_handles = false;

    std::unordered_set<VkBuffer> buffer_set;

    auto address_remap = [&address_tracker, &buffer_set](VkStridedDeviceAddressRegionKHR* address_region) {
        if (address_region->size > 0)
        {
            auto buffer_info = address_tracker.GetBufferByCaptureDeviceAddress(address_region->deviceAddress);

            if (buffer_info != nullptr && buffer_info->replay_address != 0)
            {
                // keep track of used handles
                buffer_set.insert(buffer_info->handle);

                uint64_t offset = address_region->deviceAddress - buffer_info->capture_address;

                // in-place address-remap
                address_region->deviceAddress = buffer_info->replay_address + offset;
            }
            else
            {
                GFXRECON_LOG_INFO_ONCE(
                    "VulkanAddressReplacer::ProcessCmdTraceRays: missing buffer_info->replay_address, remap failed")
            }
        }
    };

    // in-place remap: capture-addresses -> replay-addresses
    address_remap(raygen_sbt);
    address_remap(miss_sbt);
    address_remap(hit_sbt);
    address_remap(callable_sbt);

    /******************** UPSTREAM CODE ********************

    if (!valid_sbt_alignment_ || !valid_group_handles)
    {
        // mark injected commands
        MarkInjectedCommandsHelper mark_injected_commands_helper;

        if (pipeline_sbt_ == VK_NULL_HANDLE)
        {
            if (!init_pipeline())
            {
                GFXRECON_LOG_WARNING_ONCE("ProcessCmdTraceRays: internal pipeline-creation failed")
                return;
            }
        }

        // prepare linear hashmap
        hashmap_sbt_.clear();

        for (const auto& [lhs, rhs] : group_handle_map)
        {
            hashmap_sbt_.put(lhs, rhs);
        }

        // get a context for this command-buffer
        auto& pipeline_context_sbt = pipeline_sbt_context_map_[command_buffer_info->handle];

        if (!create_buffer(pipeline_context_sbt.hashmap_storage, hashmap_sbt_.get_storage(nullptr)))
        {
            GFXRECON_LOG_ERROR("VulkanAddressReplacer: hashmap-storage-buffer creation failed");
        }
        hashmap_sbt_.get_storage(pipeline_context_sbt.hashmap_storage.mapped_data);

        // input-handles
        constexpr uint32_t max_num_handles = 512;
        if (!create_buffer(pipeline_context_sbt.input_handle_buffer, max_num_handles * sizeof(VkDeviceAddress)))
        {
            GFXRECON_LOG_ERROR("VulkanAddressReplacer: input-handle-buffer creation failed");
        }
        auto input_addresses = reinterpret_cast<VkDeviceAddress*>(pipeline_context_sbt.input_handle_buffer.mapped_data);

        std::unordered_map<const VkStridedDeviceAddressRegionKHR*, uint32_t> num_addresses_map;
        uint32_t                                                             num_addresses = 0;

        {
            const auto handle_size_aligned = static_cast<uint32_t>(util::aligned_value(
                capture_ray_properties_->shaderGroupHandleSize, capture_ray_properties_->shaderGroupHandleAlignment));

            for (const auto& region : { raygen_sbt, miss_sbt, hit_sbt, callable_sbt })
            {
                if (region != nullptr && region->size != 0 && region->stride != 0)
                {
                    // NOTE: if region->stride > capture_handle_size, the excess-data is considered a shaderRecord
                    num_addresses_map[region] = region->size / handle_size_aligned;

                    // populate input-addresses, which are handles to replace and shaderRecord data to pass-through
                    for (uint32_t offset = 0; offset < region->size; offset += handle_size_aligned)
                    {
                        input_addresses[num_addresses++] = region->deviceAddress + offset;
                    }
                }
            }
        }

        replacer_params_t replacer_params = {};
        replacer_params.hashmap.storage   = pipeline_context_sbt.hashmap_storage.device_address;
        replacer_params.hashmap.size      = hashmap_sbt_.size();
        replacer_params.hashmap.capacity  = hashmap_sbt_.capacity();

        replacer_params.input_handles = pipeline_context_sbt.input_handle_buffer.device_address;
        replacer_params.num_handles   = num_addresses;

        if (valid_sbt_alignment_)
        {
            GFXRECON_LOG_INFO_ONCE("VulkanAddressReplacer::ProcessCmdTraceRays: Replay is adjusting mismatching "
                                   "raytracing shader-group-handles");

            // rewrite group-handles in-place
            replacer_params.output_handles = replacer_params.input_handles;

            // pre memory-barrier
            for (const auto& buf : buffer_set)
            {
                barrier(command_buffer_info->handle,
                        buf,
                        VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT);
            }
        }
        else
        {
            GFXRECON_LOG_INFO_ONCE("VulkanAddressReplacer::ProcessCmdTraceRays: Replay is adjusting mismatching "
                                   "raytracing shader-binding-tables using shadow-buffers");

            // output-handles
            if (!create_buffer(pipeline_context_sbt.output_handle_buffer, max_num_handles * sizeof(VkDeviceAddress)))
            {
                GFXRECON_LOG_ERROR("VulkanAddressReplacer: input-handle-buffer creation failed");
                return;
            }
            // output to shadow-sbt-buffer
            replacer_params.output_handles = pipeline_context_sbt.output_handle_buffer.device_address;

            // find/create shadow-SBT-buffer
            uint32_t sbt_offset         = 0;
            auto&    shadow_buf_context = shadow_sbt_map_[command_buffer_info->handle];

            const auto handle_size_aligned = static_cast<uint32_t>(util::aligned_value(
                replay_ray_properties_->shaderGroupHandleSize, replay_ray_properties_->shaderGroupHandleAlignment));

            for (auto& region : { raygen_sbt, miss_sbt, hit_sbt, callable_sbt })
            {
                if (region != nullptr && region->size != 0 && region->stride != 0)
                {
                    uint32_t num_handles = num_addresses_map[region];
                    auto     group_size  = static_cast<uint32_t>(util::aligned_value(
                        num_handles * handle_size_aligned, replay_ray_properties_->shaderGroupBaseAlignment));

                    // increase group-size/stride, if required
                    region->size   = std::max<VkDeviceSize>(group_size, region->size);
                    region->stride = std::max<VkDeviceSize>(handle_size_aligned, region->stride);

                    sbt_offset += region->size;
                }
            }
            // raygen: stride == size
            raygen_sbt->size = raygen_sbt->stride =
                util::aligned_value(raygen_sbt->stride, replay_ray_properties_->shaderGroupBaseAlignment);

            if (!create_buffer(shadow_buf_context,
                               sbt_offset,
                               VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                               replay_ray_properties_->shaderGroupBaseAlignment))
            {
                GFXRECON_LOG_ERROR("VulkanAddressReplacer: shadow shader-binding-table creation failed");
                return;
            }

            auto output_addresses =
                reinterpret_cast<VkDeviceAddress*>(pipeline_context_sbt.output_handle_buffer.mapped_data);
            uint32_t out_index = 0;
            sbt_offset         = 0;
            for (auto& region : { raygen_sbt, miss_sbt, hit_sbt, callable_sbt })
            {
                if (region != nullptr && region->size != 0 && region->stride != 0)
                {
                    uint32_t num_handles = num_addresses_map[region];

                    // assign shadow-sbt-address
                    region->deviceAddress = shadow_buf_context.device_address + sbt_offset;

                    for (uint32_t i = 0; i < num_handles; ++i)
                    {
                        output_addresses[out_index++] = region->deviceAddress + i * handle_size_aligned;
                    }
                    sbt_offset += region->size;
                }
            }
            GFXRECON_ASSERT(out_index == num_addresses);
        }

        device_table_->CmdBindPipeline(command_buffer_info->handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_sbt_);

        // NOTE: using push-constants here requires us to re-establish the previous data, if any
        device_table_->CmdPushConstants(command_buffer_info->handle,
                                        pipeline_layout_,
                                        VK_SHADER_STAGE_COMPUTE_BIT,
                                        0,
                                        sizeof(replacer_params_t),
                                        &replacer_params);
        // run a single workgroup
        constexpr uint32_t wg_size = 32;
        device_table_->CmdDispatch(
            command_buffer_info->handle, util::div_up(replacer_params.num_handles, wg_size), 1, 1);

        // post memory-barrier
        for (const auto& buf : buffer_set)
        {
            barrier(command_buffer_info->handle,
                    buf,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    VK_ACCESS_SHADER_READ_BIT);
        }

        // set previous compute-pipeline, if any
        if (command_buffer_info->bound_pipelines.count(VK_PIPELINE_BIND_POINT_COMPUTE))
        {
            auto* previous_pipeline = object_table_->GetVkPipelineInfo(
                command_buffer_info->bound_pipelines.at(VK_PIPELINE_BIND_POINT_COMPUTE));
            GFXRECON_ASSERT(previous_pipeline);
            if (previous_pipeline != nullptr)
            {
                GFXRECON_LOG_INFO_ONCE(
                    "VulkanAddressReplacer::ProcessCmdTraceRays: Replay is injecting compute-dispatches, "
                    "originally bound compute-pipelines are restored.");
                device_table_->CmdBindPipeline(
                    command_buffer_info->handle, VK_PIPELINE_BIND_POINT_COMPUTE, previous_pipeline->handle);
            }
        }

        // set previous push-constant data, if any
        if (!command_buffer_info->push_constant_data.empty())
        {
            device_table_->CmdPushConstants(command_buffer_info->handle,
                                            command_buffer_info->push_constant_pipeline_layout,
                                            command_buffer_info->push_constant_stage_flags,
                                            0,
                                            command_buffer_info->push_constant_data.size(),
                                            command_buffer_info->push_constant_data.data());
        }
    } // !valid_sbt_alignment_ || !valid_group_handles

    ******************** UPSTREAM CODE ********************/
}

void VulkanAddressReplacer::ProcessCmdBuildAccelerationStructuresKHR(
    const VulkanCommandBufferInfo*               command_buffer_info,
    uint32_t                                     info_count,
    VkAccelerationStructureBuildGeometryInfoKHR* build_geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**   build_range_infos,
    const VulkanDeviceAddressTracker&            address_tracker,
    bool                                         process_scratch_buffers)
{
    GFXRECON_ASSERT(device_table_ != nullptr);

    // TODO: testing only -> remove when closing issue #1526
    constexpr bool force_replace = false;

    std::unordered_set<VkBuffer> buffer_set;
    auto                         address_remap = [&address_tracker, &buffer_set](VkDeviceAddress& capture_address) {
        if (capture_address == 0)
        {
            return;
        }
        auto buffer_info = address_tracker.GetBufferByCaptureDeviceAddress(capture_address);

        if (buffer_info != nullptr && buffer_info->replay_address != 0)
        {
            // keep track of used handles
            buffer_set.insert(buffer_info->handle);

            uint64_t offset = capture_address - buffer_info->capture_address;

            // in-place address-remap via const-cast
            capture_address = buffer_info->replay_address + offset;
        }
        else
        {
            GFXRECON_LOG_WARNING(
                "ProcessCmdBuildAccelerationStructuresKHR: missing buffer_info->replay_address, remap failed");
        }
    };

    std::vector<VkDeviceAddress> addresses_to_replace;

    for (uint32_t i = 0; i < info_count; ++i)
    {
        auto& build_geometry_info = build_geometry_infos[i];
        auto  range_info          = build_range_infos[i];

        // check/correct scratch-address
        if (process_scratch_buffers)
        {
            address_remap(build_geometry_info.scratchData.deviceAddress);
        }

        /******************** UPSTREAM CODE ********************

        // check capture/replay acceleration-structure buffer-sizes
        {
            VkAccelerationStructureBuildSizesInfoKHR build_size_info = {};
            build_size_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

            {
                std::vector<uint32_t> primitive_counts(build_geometry_info.geometryCount);
                for (uint32_t j = 0; j < build_geometry_info.geometryCount; ++j)
                {
                    primitive_counts[j] = range_info->primitiveCount;
                }

                MarkInjectedCommandsHelper mark_injected_commands_helper;
                device_table_->GetAccelerationStructureBuildSizesKHR(device_,
                                                                     VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                                     &build_geometry_info,
                                                                     primitive_counts.data(),
                                                                     &build_size_info);
            }

            bool as_buffer_usable = false;

            // retrieve VkAccelerationStructureKHR -> VkBuffer -> check/correct size
            auto* acceleration_structure_info =
                address_tracker.GetAccelerationStructureByHandle(build_geometry_info.dstAccelerationStructure);
            if (acceleration_structure_info != nullptr)
            {
                auto* buffer_info = address_tracker.GetBufferByHandle(acceleration_structure_info->buffer);
                as_buffer_usable =
                    buffer_info != nullptr && buffer_info->size >= build_size_info.accelerationStructureSize;
            }

            // determine required size of scratch-buffer
            uint32_t scratch_size      = build_geometry_info.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR
                                             ? build_size_info.buildScratchSize
                                             : build_size_info.updateScratchSize;
            bool scratch_buffer_usable = scratch_buffer_info != nullptr && scratch_buffer_info->size >= scratch_size;

            if (!as_buffer_usable || !scratch_buffer_usable)
            {
                MarkInjectedCommandsHelper mark_injected_commands_helper;
                GFXRECON_LOG_INFO_ONCE(
                    "VulkanAddressReplacer::ProcessCmdBuildAccelerationStructuresKHR: Replay is adjusting mismatching "
                    "acceleration-structures using shadow-structures and -buffers");

                // now definitely requiring address-replacement
                force_replace = true;

                auto& replacement_as = shadow_as_map_[build_geometry_info.dstAccelerationStructure];

                if (replacement_as.handle == VK_NULL_HANDLE)
                {
                    if (as_buffer_usable)
                    {
                        replacement_as.handle = build_geometry_info.dstAccelerationStructure;
                        auto accel_info       = address_tracker.GetAccelerationStructureByHandle(
                            build_geometry_info.dstAccelerationStructure);
                        GFXRECON_ASSERT(accel_info != nullptr && accel_info->replay_address != 0);
                        replacement_as.address = accel_info->replay_address;
                    }
                    else
                    {
                        auto ret = create_acceleration_asset(replacement_as,
                                                             build_geometry_info.type,
                                                             build_size_info.accelerationStructureSize,
                                                             scratch_size);
                        if (!ret)
                        {
                            GFXRECON_LOG_ERROR("ProcessCmdBuildAccelerationStructuresKHR: creation of shadow "
                                               "acceleration-structure failed");
                        }
                    }
                }

                // check/correct source acceleration-structure
                swap_acceleration_structure_handle(build_geometry_info.srcAccelerationStructure);

                // hot swap acceleration-structure handle
                build_geometry_info.dstAccelerationStructure = replacement_as.handle;

                // acceleration-structure properties not populated yet, check if we missed it
                if (!replay_acceleration_structure_properties_)
                {
                    SetRaytracingProperties(physical_device_info_);

                    // capture did not contain the call, inject
                    if (!replay_acceleration_structure_properties_)
                    {
                        VkPhysicalDeviceAccelerationStructurePropertiesKHR as_properties = {};
                        as_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
                        as_properties.pNext = nullptr;

                        VkPhysicalDeviceProperties2 physical_device_properties = {};
                        physical_device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                        physical_device_properties.pNext = &as_properties;
                        get_physical_device_properties_fn_(physical_device_info_->handle, &physical_device_properties);
                        replay_acceleration_structure_properties_ = as_properties;
                    }
                }

                // create a replacement scratch-buffer
                if (!create_buffer(
                        replacement_as.scratch,
                        scratch_size,
                        0,
                        replay_acceleration_structure_properties_->minAccelerationStructureScratchOffsetAlignment,
                        false))
                {
                    GFXRECON_LOG_ERROR("ProcessCmdBuildAccelerationStructuresKHR: scratch-buffer creation failed");
                    return;
                }

                // hot swap scratch-buffer
                build_geometry_info.scratchData.deviceAddress = replacement_as.scratch.device_address;
            }
        }

        ******************** UPSTREAM CODE ********************/

        for (uint32_t j = 0; j < build_geometry_info.geometryCount; ++j)
        {
            auto geometry = const_cast<VkAccelerationStructureGeometryKHR*>(build_geometry_info.pGeometries != nullptr
                                                                                ? build_geometry_info.pGeometries + j
                                                                                : build_geometry_info.ppGeometries[j]);
            switch (geometry->geometryType)
            {
                case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                {
                    auto& triangles = geometry->geometry.triangles;
                    address_remap(triangles.vertexData.deviceAddress);
                    address_remap(triangles.indexData.deviceAddress);
                    address_remap(triangles.transformData.deviceAddress);
                    break;
                }
                case VK_GEOMETRY_TYPE_AABBS_KHR:
                {
                    auto& aabbs = geometry->geometry.aabbs;
                    address_remap(aabbs.data.deviceAddress);
                    break;
                }
                case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                {
                    auto& instances = geometry->geometry.instances;
                    // TODO: Support array of pointers
                    GFXRECON_ASSERT(instances.arrayOfPointers == false);
                    address_remap(instances.data.deviceAddress);

                    // replace VkAccelerationStructureInstanceKHR::accelerationStructureReference inside buffer
                    for (uint32_t k = 0; k < range_info->primitiveCount; ++k)
                    {
                        VkDeviceAddress accel_structure_reference =
                            instances.data.deviceAddress + k * sizeof(VkAccelerationStructureInstanceKHR) +
                            offsetof(VkAccelerationStructureInstanceKHR, accelerationStructureReference);
                        addresses_to_replace.push_back(accel_structure_reference);
                    }
                    break;
                }
                default:
                    GFXRECON_LOG_ERROR(
                        "OverrideCmdBuildAccelerationStructuresKHR: unhandled case in switch-statement: %d",
                        geometry->geometryType);
                    break;
            }
        }
    }

    /******************** UPSTREAM CODE ********************

    if (!addresses_to_replace.empty())
    {
        // prepare linear hashmap
        hashmap_bda_.clear();
        auto acceleration_structure_map = address_tracker.GetAccelerationStructureDeviceAddressMap();
        for (const auto& [capture_address, replay_address] : acceleration_structure_map)
        {
            auto* accel_info = address_tracker.GetAccelerationStructureByCaptureDeviceAddress(capture_address);
            GFXRECON_ASSERT(accel_info != nullptr);

            if (force_replace || capture_address != replay_address)
            {
                auto new_address = replay_address;

                // extra look-up required for potentially replaced AS
                if (accel_info != nullptr)
                {
                    auto shadow_as_it = shadow_as_map_.find(accel_info->handle);
                    if (shadow_as_it != shadow_as_map_.end())
                    {
                        new_address = shadow_as_it->second.address;
                    }
                }

                // store addresses we will need to replace
                GFXRECON_ASSERT(new_address != 0);
                hashmap_bda_.put(capture_address, new_address);
            }
        }

        if (!hashmap_bda_.empty())
        {
            // mark injected commands
            MarkInjectedCommandsHelper mark_injected_commands_helper;

            if (pipeline_bda_ == VK_NULL_HANDLE && !init_pipeline())
            {
                GFXRECON_LOG_WARNING_ONCE("ProcessCmdBuildAccelerationStructuresKHR: internal pipeline-creation failed")
                return;
            }

            auto& pipeline_context_bda = build_as_context_map_[command_buffer_info->handle];

            if (!create_buffer(pipeline_context_bda.hashmap_storage, hashmap_bda_.get_storage(nullptr)))
            {
                GFXRECON_LOG_ERROR("VulkanAddressReplacer: hashmap-storage-buffer creation failed");
                return;
            }
            hashmap_bda_.get_storage(pipeline_context_bda.hashmap_storage.mapped_data);

            uint32_t num_bytes = addresses_to_replace.size() * sizeof(VkDeviceAddress);

            if (!create_buffer(pipeline_context_bda.input_handle_buffer, num_bytes))
            {
                GFXRECON_LOG_ERROR("VulkanAddressReplacer: input-handle-buffer creation failed");
                return;
            }
            memcpy(pipeline_context_bda.input_handle_buffer.mapped_data, addresses_to_replace.data(), num_bytes);

            replacer_params_t replacer_params = {};
            replacer_params.hashmap.storage   = pipeline_context_bda.hashmap_storage.device_address;
            replacer_params.hashmap.size      = hashmap_bda_.size();
            replacer_params.hashmap.capacity  = hashmap_bda_.capacity();

            // in-place
            replacer_params.input_handles  = pipeline_context_bda.input_handle_buffer.device_address;
            replacer_params.output_handles = pipeline_context_bda.input_handle_buffer.device_address;

            replacer_params.num_handles = addresses_to_replace.size();

            device_table_->CmdBindPipeline(command_buffer_info->handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_bda_);

            // NOTE: using push-constants here requires us to re-establish the previous data, if any
            device_table_->CmdPushConstants(command_buffer_info->handle,
                                            pipeline_layout_,
                                            VK_SHADER_STAGE_COMPUTE_BIT,
                                            0,
                                            sizeof(replacer_params_t),
                                            &replacer_params);
            // dispatch workgroups
            constexpr uint32_t wg_size = 32;
            device_table_->CmdDispatch(
                command_buffer_info->handle, util::div_up(replacer_params.num_handles, wg_size), 1, 1);

            // post memory-barrier
            for (const auto& buf : buffer_set)
            {
                barrier(command_buffer_info->handle,
                        buf,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                        VK_ACCESS_SHADER_READ_BIT);
            }

            // set previous compute-pipeline, if any
            if (command_buffer_info->bound_pipelines.count(VK_PIPELINE_BIND_POINT_COMPUTE))
            {
                auto* previous_pipeline = object_table_->GetVkPipelineInfo(
                    command_buffer_info->bound_pipelines.at(VK_PIPELINE_BIND_POINT_COMPUTE));
                GFXRECON_ASSERT(previous_pipeline);

                if (previous_pipeline != nullptr)
                {
                    GFXRECON_LOG_INFO_ONCE("VulkanAddressReplacer::ProcessCmdBuildAccelerationStructuresKHR: Replay is "
                                           "injecting compute-dispatches, "
                                           "originally bound compute-pipelines are restored.");
                    device_table_->CmdBindPipeline(
                        command_buffer_info->handle, VK_PIPELINE_BIND_POINT_COMPUTE, previous_pipeline->handle);
                }
            }

            // set previous push-constant data, if any
            if (!command_buffer_info->push_constant_data.empty())
            {
                device_table_->CmdPushConstants(command_buffer_info->handle,
                                                command_buffer_info->push_constant_pipeline_layout,
                                                command_buffer_info->push_constant_stage_flags,
                                                0,
                                                command_buffer_info->push_constant_data.size(),
                                                command_buffer_info->push_constant_data.data());
            }
        } // !hashmap_bda_.empty()
    }

    ******************** UPSTREAM CODE ********************/
}

/******************** UPSTREAM CODE ********************

void VulkanAddressReplacer::ProcessCmdCopyAccelerationStructuresKHR(
    VkCopyAccelerationStructureInfoKHR* info, const decode::VulkanDeviceAddressTracker& address_tracker)
{
    if (info != nullptr)
    {
        VkDeviceSize compact_size    = 0;
        auto         compact_size_it = as_compact_sizes_.find(info->src);
        if (compact_size_it != as_compact_sizes_.end())
        {
            compact_size = compact_size_it->second;
            as_compact_sizes_.erase(info->src);

            auto* as_info = address_tracker.GetAccelerationStructureByHandle(info->dst);
            GFXRECON_ASSERT(as_info != nullptr);
            if (as_info != nullptr)
            {
                auto* buffer_info = address_tracker.GetBufferByHandle(as_info->buffer);
                GFXRECON_ASSERT(buffer_info != nullptr);
                if (buffer_info != nullptr)
                {
                    if (buffer_info->size < compact_size)
                    {
                        // TODO: need replacement AS
                    }
                }
            }
        }

        // correct in-place
        swap_acceleration_structure_handle(info->src);
        swap_acceleration_structure_handle(info->dst);
    }
}

void VulkanAddressReplacer::ProcessCmdWriteAccelerationStructuresPropertiesKHR(
    uint32_t                    count,
    VkAccelerationStructureKHR* acceleration_structures,
    VkQueryType                 query_type,
    VkQueryPool                 pool,
    uint32_t                    first_query)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto shadow_as_it = shadow_as_map_.find(acceleration_structures[i]);
        if (shadow_as_it != shadow_as_map_.end())
        {
            acceleration_structures[i] = shadow_as_it->second.handle;
        }

        if (query_type == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR)
        {
            // read back compacted size later
            as_compact_queries_[pool][acceleration_structures[i]] = first_query + i;
        }
    }
}

void VulkanAddressReplacer::ProcessUpdateDescriptorSets(uint32_t              descriptor_write_count,
                                                        VkWriteDescriptorSet* descriptor_writes,
                                                        uint32_t              descriptor_copy_count,
                                                        VkCopyDescriptorSet*  descriptor_copies)
{
    GFXRECON_UNREFERENCED_PARAMETER(descriptor_copy_count);
    GFXRECON_UNREFERENCED_PARAMETER(descriptor_copies);

    // bail out if we're not tracking any shadow acceleration-structures
    if (shadow_as_map_.empty())
    {
        return;
    }

    for (uint32_t i = 0; i < descriptor_write_count; ++i)
    {
        VkWriteDescriptorSet& write = descriptor_writes[i];

        if (write.descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            continue;
        }

        if (auto* write_as = graphics::vulkan_struct_get_pnext<VkWriteDescriptorSetAccelerationStructureKHR>(&write))
        {
            for (uint32_t j = 0; j < write_as->accelerationStructureCount; ++j)
            {
                auto acceleration_structure_it = shadow_as_map_.find(write_as->pAccelerationStructures[j]);
                if (acceleration_structure_it != shadow_as_map_.end())
                {
                    // we found an existing replacement-structure -> swap
                    auto* out_array = const_cast<VkAccelerationStructureKHR*>(write_as->pAccelerationStructures);
                    out_array[j]    = acceleration_structure_it->second.handle;

                    GFXRECON_LOG_INFO_ONCE("VulkanAddressReplacer::ProcessUpdateDescriptorSets: Replay adjusted "
                                           "AccelerationStructure handles")
                }
            }
        }
    }
}

void VulkanAddressReplacer::ProcessGetQueryPoolResults(VkDevice           device,
                                                       VkQueryPool        query_pool,
                                                       uint32_t           firstQuery,
                                                       uint32_t           queryCount,
                                                       size_t             dataSize,
                                                       void*              pData,
                                                       VkDeviceSize       stride,
                                                       VkQueryResultFlags flags)
{
    GFXRECON_UNREFERENCED_PARAMETER(device);
    GFXRECON_UNREFERENCED_PARAMETER(firstQuery);
    GFXRECON_UNREFERENCED_PARAMETER(queryCount);
    GFXRECON_UNREFERENCED_PARAMETER(dataSize);

    // intercept queries containing acceleration-structure compaction-sizes
    if (!as_compact_queries_.empty() && stride == sizeof(VkDeviceSize))
    {
        bool is_synced = flags & VK_QUERY_RESULT_WAIT_BIT;

        auto it = as_compact_queries_.find(query_pool);
        if (is_synced && it != as_compact_queries_.end())
        {
            // assuming post-processing here, pData was already written
            auto* result_array = reinterpret_cast<const VkDeviceSize*>(pData);

            for (const auto& [as, query_index] : it->second)
            {
                as_compact_sizes_[as] = result_array[query_index];
            }
        }
        as_compact_queries_.erase(query_pool);
    }
}

void VulkanAddressReplacer::ProcessBuildVulkanAccelerationStructuresMetaCommand(
    uint32_t                                     info_count,
    VkAccelerationStructureBuildGeometryInfoKHR* geometry_infos,
    VkAccelerationStructureBuildRangeInfoKHR**   range_infos,
    const decode::VulkanDeviceAddressTracker&    address_tracker)
{
    if (info_count > 0 && init_queue_assets())
    {
        // reset/submit/sync command-buffer
        QueueSubmitHelper queue_submit_helper(device_table_, device_, command_buffer_, queue_, fence_);

        // dummy-wrapper
        VulkanCommandBufferInfo command_buffer_info = {};
        command_buffer_info.handle                  = command_buffer_;
        ProcessCmdBuildAccelerationStructuresKHR(
            &command_buffer_info, info_count, geometry_infos, range_infos, address_tracker);

        // issue build-command
        MarkInjectedCommandsHelper mark_injected_commands_helper;
        device_table_->CmdBuildAccelerationStructuresKHR(command_buffer_, info_count, geometry_infos, range_infos);
    }
}

void VulkanAddressReplacer::ProcessCopyVulkanAccelerationStructuresMetaCommand(
    uint32_t                                  info_count,
    VkCopyAccelerationStructureInfoKHR*       copy_infos,
    const decode::VulkanDeviceAddressTracker& address_tracker)
{
    if (copy_infos != nullptr && info_count > 0 && init_queue_assets())
    {
        // reset/submit/sync command-buffer
        QueueSubmitHelper queue_submit_helper(device_table_, device_, command_buffer_, queue_, fence_);

        for (uint32_t i = 0; i < info_count; ++i)
        {
            auto* copy_info = copy_infos + i;

            if (copy_info->src != VK_NULL_HANDLE && copy_info->dst != VK_NULL_HANDLE)
            {
                ProcessCmdCopyAccelerationStructuresKHR(copy_info, address_tracker);

                // issue copy command
                MarkInjectedCommandsHelper mark_injected_commands_helper;
                device_table_->CmdCopyAccelerationStructureKHR(command_buffer_, copy_info);
            }
            else
            {
                GFXRECON_LOG_ERROR("ProcessCopyVulkanAccelerationStructuresMetaCommand: missing handles");
            }
        }
    }
}

void VulkanAddressReplacer::ProcessVulkanAccelerationStructuresWritePropertiesMetaCommand(
    VkQueryType query_type, VkAccelerationStructureKHR acceleration_structure)
{
    if (init_queue_assets())
    {
        // reset/submit/sync command-buffer
        QueueSubmitHelper queue_submit_helper(device_table_, device_, command_buffer_, queue_, fence_);

        ProcessCmdWriteAccelerationStructuresPropertiesKHR(1, &acceleration_structure, query_type, query_pool_, 0);

        // issue vkCmdResetQueryPool and vkCmdWriteAccelerationStructuresPropertiesKHR
        MarkInjectedCommandsHelper mark_injected_commands_helper;
        device_table_->CmdResetQueryPool(command_buffer_, query_pool_, 0, 1);
        device_table_->CmdWriteAccelerationStructuresPropertiesKHR(
            command_buffer_, 1, &acceleration_structure, query_type, query_pool_, 0);
    }

    VkDeviceSize compact_size = 0;

    // the above command-buffer is already synced here, retrieve result using vkGetQueryPoolResults
    MarkInjectedCommandsHelper mark_injected_commands_helper;
    device_table_->GetQueryPoolResults(device_,
                                       query_pool_,
                                       0,
                                       1,
                                       sizeof(VkDeviceSize),
                                       &compact_size,
                                       sizeof(VkDeviceSize),
                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    // apply usual post-processing of queries
    ProcessGetQueryPoolResults(device_,
                               query_pool_,
                               0,
                               1,
                               sizeof(VkDeviceSize),
                               &compact_size,
                               sizeof(VkDeviceSize),
                               VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}

******************** UPSTREAM CODE ********************/

bool VulkanAddressReplacer::init_pipeline()
{
    if (pipeline_sbt_ != VK_NULL_HANDLE)
    {
        // assume already initialized
        return true;
    }
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags          = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant_range.offset              = 0;
    push_constant_range.size                = sizeof(replacer_params_t);

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pNext                      = nullptr;
    pipeline_layout_info.flags                      = 0;
    pipeline_layout_info.setLayoutCount             = 0;
    pipeline_layout_info.pSetLayouts                = nullptr;
    pipeline_layout_info.pushConstantRangeCount     = 1;
    pipeline_layout_info.pPushConstantRanges        = &push_constant_range;

    VkResult result =
        device_table_->CreatePipelineLayout(device_info_->handle, &pipeline_layout_info, nullptr, &pipeline_layout_);

    if (result != VK_SUCCESS)
    {
        GFXRECON_LOG_FATAL("VulkanAddressReplacer: failed in vkCreatePipelineLayout");
    }

    auto create_pipeline = [this](VkPipelineLayout layout, const auto& spirv, VkPipeline& out_pipeline) -> VkResult {
        VkShaderModule           compute_module            = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo shader_module_create_info = {};
        shader_module_create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_create_info.pNext                    = VK_NULL_HANDLE;
        shader_module_create_info.flags                    = 0;
        shader_module_create_info.codeSize                 = spirv.size();
        shader_module_create_info.pCode                    = reinterpret_cast<const uint32_t*>(spirv.data());

        VkResult result = device_table_->CreateShaderModule(
            device_info_->handle, &shader_module_create_info, nullptr, &compute_module);

        if (result != VK_SUCCESS)
        {
            GFXRECON_LOG_FATAL("VulkanAddressReplacer: failed in vkCreateShaderModule");
            return result;
        }
        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.pNext                           = nullptr;
        stage_info.flags                           = 0;
        stage_info.stage                           = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module                          = compute_module;
        stage_info.pName                           = "main";
        stage_info.pSpecializationInfo             = nullptr;

        VkComputePipelineCreateInfo pipeline_create_info = {};
        pipeline_create_info.sType                       = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_create_info.layout                      = layout;
        pipeline_create_info.stage                       = stage_info;

        result = device_table_->CreateComputePipelines(
            device_info_->handle, VK_NULL_HANDLE, 1, &pipeline_create_info, VK_NULL_HANDLE, &out_pipeline);

        if (result != VK_SUCCESS)
        {
            GFXRECON_LOG_ERROR("VulkanAddressReplacer: pipeline creation failed");
        }

        if (compute_module != VK_NULL_HANDLE)
        {
            device_table_->DestroyShaderModule(device_info_->handle, compute_module, nullptr);
        }
        return result;
    };

    // create SBT pipeline
    if (create_pipeline(pipeline_layout_, g_replacer_sbt_comp, pipeline_sbt_) != VK_SUCCESS)
    {
        return false;
    }

    // create BDA pipeline
    if (create_pipeline(pipeline_layout_, g_replacer_bda_comp, pipeline_bda_) != VK_SUCCESS)
    {
        return false;
    }
    return true;
}

bool VulkanAddressReplacer::create_buffer(size_t                                   num_bytes,
                                          VulkanAddressReplacer::buffer_context_t& buffer_context,
                                          uint32_t                                 usage_flags)
{
    // nothing to do
    if (num_bytes <= buffer_context.num_bytes)
    {
        return true;
    }

    // free previous resources
    buffer_context                    = {};
    buffer_context.resource_allocator = device_info_->allocator.get();
    buffer_context.num_bytes          = num_bytes;

    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | usage_flags;
    buffer_create_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.size                  = num_bytes;

    VkResult result = buffer_context.resource_allocator->CreateBufferDirect(
        &buffer_create_info, nullptr, &buffer_context.buffer, &buffer_context.allocator_data);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memory_requirements;
    device_table_->GetBufferMemoryRequirements(device_info_->handle, buffer_context.buffer, &memory_requirements);

    uint32_t memory_type_index =
        get_memory_type_index(memory_properties_,
                              memory_requirements.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    if (memory_type_index == std::numeric_limits<uint32_t>::max())
    {
        /* fallback to coherent */
        memory_type_index =
            get_memory_type_index(memory_properties_,
                                  memory_requirements.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    GFXRECON_ASSERT(memory_type_index != std::numeric_limits<uint32_t>::max());

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize       = memory_requirements.size;
    alloc_info.memoryTypeIndex      = memory_type_index;

    VkMemoryAllocateFlagsInfo alloc_flags_info = {};
    alloc_flags_info.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    alloc_flags_info.flags                     = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    alloc_info.pNext                           = &alloc_flags_info;

    result = buffer_context.resource_allocator->AllocateMemoryDirect(
        &alloc_info, nullptr, &buffer_context.device_memory, &buffer_context.memory_data);

    if (result != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryPropertyFlags memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    result                             = buffer_context.resource_allocator->BindBufferMemory(buffer_context.buffer,
                                                                 buffer_context.device_memory,
                                                                 0,
                                                                 buffer_context.allocator_data,
                                                                 buffer_context.memory_data,
                                                                 &memory_flags);
    if (result != VK_SUCCESS)
    {
        return false;
    }

    // get device-address
    VkBufferDeviceAddressInfo address_info = {};
    address_info.sType                     = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer                    = buffer_context.buffer;
    buffer_context.device_address          = dispatcher_.GetBufferDeviceAddress(device_info_->handle, &address_info);

    // map buffer
    result = buffer_context.resource_allocator->MapResourceMemoryDirect(
        VK_WHOLE_SIZE, 0, &buffer_context.mapped_data, buffer_context.allocator_data);
    return result == VK_SUCCESS;
}

void VulkanAddressReplacer::barrier(VkCommandBuffer      command_buffer,
                                    VkBuffer             buffer,
                                    VkPipelineStageFlags src_stage,
                                    VkAccessFlags        src_access,
                                    VkPipelineStageFlags dst_stage,
                                    VkAccessFlags        dst_access)
{
    VkBufferMemoryBarrier barrier = {};
    barrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer                = buffer;
    barrier.offset                = 0;
    barrier.size                  = VK_WHOLE_SIZE;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask                                     = src_access;
    barrier.dstAccessMask                                     = dst_access;

    device_table_->CmdPipelineBarrier(
        command_buffer, src_stage, dst_stage, VkDependencyFlags(0), 0, nullptr, 1, &barrier, 0, nullptr);
}

void swap(VulkanAddressReplacer& lhs, VulkanAddressReplacer& rhs) noexcept
{
    std::swap(lhs.device_table_, rhs.device_table_);
    std::swap(lhs.memory_properties_, rhs.memory_properties_);
    std::swap(lhs.capture_ray_properties_, rhs.capture_ray_properties_);
    std::swap(lhs.replay_ray_properties_, rhs.replay_ray_properties_);
    std::swap(lhs.valid_sbt_alignment_, rhs.valid_sbt_alignment_);
    std::swap(lhs.device_info_, rhs.device_info_);
    std::swap(lhs.pipeline_layout_, rhs.pipeline_layout_);
    std::swap(lhs.pipeline_sbt_, rhs.pipeline_sbt_);
    std::swap(lhs.pipeline_bda_, rhs.pipeline_bda_);
    std::swap(lhs.pipeline_context_sbt_, rhs.pipeline_context_sbt_);
    std::swap(lhs.pipeline_context_bda_, rhs.pipeline_context_bda_);
    std::swap(lhs.hashmap_sbt_, rhs.hashmap_sbt_);
    std::swap(lhs.hashmap_bda_, rhs.hashmap_bda_);
    std::swap(lhs.shadow_sbt_map_, rhs.shadow_sbt_map_);
    std::swap(lhs.dispatcher_, rhs.dispatcher_);
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
