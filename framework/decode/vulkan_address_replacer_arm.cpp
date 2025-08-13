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

#include "decode/vulkan_address_replacer_arm.h"
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

VulkanAddressReplacerARM::buffer_context_t::~buffer_context_t()
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

VulkanAddressReplacerARM::acceleration_structure_asset_t::~acceleration_structure_asset_t()
{
    if (handle != VK_NULL_HANDLE && destroy_fn != nullptr && device != VK_NULL_HANDLE)
    {
        destroy_fn(device, handle, nullptr);
    }
}

VulkanAddressReplacerARM::VulkanAddressReplacerARM(const VulkanDeviceInfo*              device_info,
                                                   const graphics::VulkanDeviceTable*   device_table,
                                                   const decode::CommonObjectInfoTable& object_table) :
    device_table_(device_table),
    device_info_(device_info), object_table_(&object_table)
{
    GFXRECON_ASSERT(device_info != nullptr && device_table != nullptr)

    const VulkanPhysicalDeviceInfo* physical_device_info =
        object_table.GetVkPhysicalDeviceInfo(device_info_->parent_id);
    get_device_address_fn_ = physical_device_info->capture_api_version >= VK_API_VERSION_1_2
                                 ? device_table->GetBufferDeviceAddress
                                 : device_table->GetBufferDeviceAddressKHR;

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

VulkanAddressReplacerARM::VulkanAddressReplacerARM(VulkanAddressReplacerARM&& other) noexcept :
    VulkanAddressReplacerARM()
{
    swap(*this, other);
}

VulkanAddressReplacerARM::~VulkanAddressReplacerARM()
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

void VulkanAddressReplacerARM::ProcessCmdTraceRays(
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
                    "VulkanAddressReplacerARM::ProcessCmdTraceRays: missing buffer_info->replay_address, remap failed")
            }
        }
    };

    // in-place remap: capture-addresses -> replay-addresses
    address_remap(raygen_sbt);
    address_remap(miss_sbt);
    address_remap(hit_sbt);
    address_remap(callable_sbt);
}

void VulkanAddressReplacerARM::ProcessCmdBuildAccelerationStructuresKHR(
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
}

void VulkanAddressReplacerARM::ProcessGetDescriptorEXT(const VulkanDeviceInfo*           device_info,
                                                       VkDescriptorGetInfoEXT*           descriptorInfo,
                                                       const VulkanDeviceAddressTracker& address_tracker)
{
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
                "VulkanAddressReplacer::ProcessGetDescriptorEXT: missing buffer_info->replay_address, remap failed");
        }
    };

    std::unordered_set<VkAccelerationStructureKHR> as_set;
    auto accelerationStruct_address_remap = [&address_tracker, &as_set](VkDeviceAddress& capture_address) {
        if (capture_address == 0)
        {
            return;
        }
        auto accelerationStruct_info = address_tracker.GetAccelerationStructureByCaptureDeviceAddress(capture_address);

        if (accelerationStruct_info != nullptr && accelerationStruct_info->replay_address != 0)
        {
            // keep track of used handles
            as_set.insert(accelerationStruct_info->handle);
            // in-place address-remap via const-cast
            capture_address = accelerationStruct_info->replay_address;
        }
        else
        {
            GFXRECON_LOG_WARNING("VulkanAddressReplacer::ProcessGetDescriptorEXT: missing "
                                 "accelerationStruct_info->replay_address, remap failed");
        }
    };

    auto&                       descriptorData = descriptorInfo->data;
    VkDescriptorAddressInfoEXT* addressInfo;

    switch (descriptorInfo->type)
    {
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            addressInfo = const_cast<VkDescriptorAddressInfoEXT*>(descriptorData.pUniformTexelBuffer);
            address_remap(addressInfo->address);
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            addressInfo = const_cast<VkDescriptorAddressInfoEXT*>(descriptorData.pStorageTexelBuffer);
            address_remap(addressInfo->address);
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            addressInfo = const_cast<VkDescriptorAddressInfoEXT*>(descriptorData.pUniformBuffer);
            address_remap(addressInfo->address);
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            addressInfo = const_cast<VkDescriptorAddressInfoEXT*>(descriptorData.pStorageBuffer);
            address_remap(addressInfo->address);
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            accelerationStruct_address_remap(descriptorData.accelerationStructure);
            break;
        default:
            break;
    }
}

void VulkanAddressReplacerARM::ProcessCmdBindDescriptorBuffersEXT(const VulkanCommandBufferInfo*    commandBuffer_info,
                                                                  uint32_t                          bufferCount,
                                                                  VkDescriptorBufferBindingInfoEXT* bindingInfos,
                                                                  const VulkanDeviceAddressTracker& address_tracker)
{
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
            GFXRECON_LOG_WARNING("VulkanAddressReplacer::CmdBindDescriptorBuffersEXT: missing "
                                                         "buffer_info->replay_address, remap failed");
        }
    };

    for (uint32_t i = 0; i < bufferCount; i++)
    {
        auto& bindingInfo = bindingInfos[i];
        address_remap(bindingInfo.address);
    }
}

bool VulkanAddressReplacerARM::init_pipeline()
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
        GFXRECON_LOG_FATAL("VulkanAddressReplacerARM: failed in vkCreatePipelineLayout");
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
            GFXRECON_LOG_FATAL("VulkanAddressReplacerARM: failed in vkCreateShaderModule");
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
            GFXRECON_LOG_ERROR("VulkanAddressReplacerARM: pipeline creation failed");
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
    if (create_pipeline(pipeline_layout_, g_replacer_bda_binary_comp, pipeline_bda_) != VK_SUCCESS)
    {
        return false;
    }
    return true;
}

bool VulkanAddressReplacerARM::create_buffer(size_t                                      num_bytes,
                                             VulkanAddressReplacerARM::buffer_context_t& buffer_context,
                                             uint32_t                                    usage_flags)
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
    buffer_context.device_address          = get_device_address_fn_(device_info_->handle, &address_info);

    // map buffer
    result = buffer_context.resource_allocator->MapResourceMemoryDirect(
        VK_WHOLE_SIZE, 0, &buffer_context.mapped_data, buffer_context.allocator_data);
    return result == VK_SUCCESS;
}

void VulkanAddressReplacerARM::barrier(VkCommandBuffer      command_buffer,
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

void swap(VulkanAddressReplacerARM& lhs, VulkanAddressReplacerARM& rhs) noexcept
{
    std::swap(lhs.device_table_, rhs.device_table_);
    std::swap(lhs.memory_properties_, rhs.memory_properties_);
    std::swap(lhs.capture_ray_properties_, rhs.capture_ray_properties_);
    std::swap(lhs.replay_ray_properties_, rhs.replay_ray_properties_);
    std::swap(lhs.valid_sbt_alignment_, rhs.valid_sbt_alignment_);
    std::swap(lhs.device_info_, rhs.device_info_);
    std::swap(lhs.get_device_address_fn_, rhs.get_device_address_fn_);
    std::swap(lhs.pipeline_layout_, rhs.pipeline_layout_);
    std::swap(lhs.pipeline_sbt_, rhs.pipeline_sbt_);
    std::swap(lhs.pipeline_bda_, rhs.pipeline_bda_);
    std::swap(lhs.pipeline_context_sbt_, rhs.pipeline_context_sbt_);
    std::swap(lhs.pipeline_context_bda_, rhs.pipeline_context_bda_);
    std::swap(lhs.hashmap_sbt_, rhs.hashmap_sbt_);
    std::swap(lhs.hashmap_bda_, rhs.hashmap_bda_);
    std::swap(lhs.shadow_sbt_map_, rhs.shadow_sbt_map_);
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
