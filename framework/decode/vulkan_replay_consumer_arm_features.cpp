/*
** Copyright (c) 2025 LunarG, Inc.
** Copyright (c) 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "vulkan_replay_consumer_arm_features.h"
#include "vulkan_replay_consumer_base.h"
#include "generated/generated_vulkan_enum_to_string.h"
#include "util/to_string.h"
#include "graphics/vulkan_util.h"
#include "graphics/vulkan_feature_util.h"
#include "graphics/vulkan_struct_get_pnext.h"
#ifdef __linux__
#include <sys/resource.h>
#endif
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanReplayConsumerArmFeatures::VulkanReplayConsumerArmFeatures(VulkanReplayConsumerBase* consumer) :
    consumer_(consumer)
{
    util::MarkingLayersUtil::instance().SetInfoTable(consumer_->object_info_table_);
    for (const std::string& name : consumer_->options_.marking_layers_names)
    {
        util::MarkingLayersUtil::instance().AddLayerName(name);
    }
}

void VulkanReplayConsumerArmFeatures::ReplaceDeviceAddresses(VulkanCommandBufferInfo* command_buffer_info, void* data)
{
    VulkanDeviceInfo* device_info = consumer_->object_info_table_->GetVkDeviceInfo(command_buffer_info->parent_id);
    GFXRECON_ASSERT(device_info != nullptr);
    VulkanResourceAllocator* allocator = device_info->allocator.get();
    GFXRECON_ASSERT(allocator != nullptr);

    if (!allocator->SupportsOpaqueDeviceAddresses())
    {
        for (format::AddressLocationInfo& location : consumer_->other_address_locations)
        {
            uint64_t* old_value_ptr =
                reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(data) + location.offset_in_memory);
            GFXRECON_ASSERT(*old_value_ptr == location.adjusted_address);
            *old_value_ptr = location.new_address;
        }
        consumer_->other_address_locations.clear();
    }
}

void VulkanReplayConsumerArmFeatures::LogFrameDebugInfo()
{
    if (util::Log::WillOutputMessage(util::LoggingSeverity::kDebug))
    {
#ifdef __linux__
        const long    pages     = sysconf(_SC_AVPHYS_PAGES);
        const long    page_size = sysconf(_SC_PAGE_SIZE);
        const long    available = pages * page_size;
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        long  curr_rss = -1;
        FILE* fp       = NULL;
        if ((fp = fopen("/proc/self/statm", "r")))
        {
            if (fscanf(fp, "%*s%ld", &curr_rss) == 1)
            {
                curr_rss *= page_size;
            }
            fclose(fp);
        }
        const double f = 1024.0 * 1024.0;
        GFXRECON_LOG_DEBUG("Frame %d memory (mb): %.02f max RSS, %.02f current RSS, %.02f available",
                           consumer_->application_->GetCurrentFrameNumber() + 1,
                           (double)usage.ru_maxrss / 1024.0,
                           (double)curr_rss / f,
                           (double)available / f);
#else
        GFXRECON_LOG_DEBUG("Completed frame %d", consumer_->application_->GetCurrentFrameNumber() + 1);
#endif // WIN32
    }
}

void VulkanReplayConsumerArmFeatures::DisableSubpassFusion(
    const StructPointerDecoder<Decoded_VkRenderPassCreateInfo>* pCreateInfo)
{
    if (consumer_->options_.disable_subpass_fusion)
    {
        if (pCreateInfo->GetPointer() != nullptr && pCreateInfo->GetPointer()->dependencyCount > 0)
        {
            VkSubpassDependency* dependencies =
                const_cast<VkSubpassDependency*>(pCreateInfo->GetPointer()->pDependencies);
            for (uint32_t i = 0; i < pCreateInfo->GetPointer()->dependencyCount; ++i)
            {
                if (dependencies[i].srcSubpass == VK_SUBPASS_EXTERNAL ||
                    dependencies[i].dstSubpass == VK_SUBPASS_EXTERNAL)
                {
                    continue;
                }
                dependencies[i].srcStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dependencies[i].srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
                dependencies[i].dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
            }
        }
    }
}

void VulkanReplayConsumerArmFeatures::ProcessFillMemoryCommandDeviceAddresses(const uint8_t* data)
{

    for (format::AddressLocationInfo& location : consumer_->device_memory_address_locations)
    {
        auto old_value_ptr = (uint64_t*)(data + location.offset_in_memory);
        auto ov            = *old_value_ptr;
        GFXRECON_ASSERT(ov == location.adjusted_address);
        *old_value_ptr = location.new_address;
    }
    consumer_->device_memory_address_locations.clear();

    for (format::ShaderHandleLocationInfo& location : consumer_->shader_group_handle_locations)
    {
        auto old_value_ptr = (uint8_t*)(data + location.offset_in_memory);
        GFXRECON_ASSERT(0 == std::memcmp(location.original_handles, old_value_ptr, location.group_size));
        std::memcpy(old_value_ptr, location.new_handles, location.group_size);
    }
    consumer_->shader_group_handle_locations.clear();

    for (auto& entry : consumer_->descriptor_locations)
    {
        const auto& replacement = entry.second;
        uint8_t*    dest        = (uint8_t*)(data + replacement.location.descriptor_offset_in_memory);
        util::platform::MemoryCopy(
            dest, replacement.replay_data.size(), replacement.replay_data.data(), replacement.replay_data.size());
    }
    consumer_->descriptor_locations.clear();
}

void VulkanReplayConsumerArmFeatures::ProcessDeviceFaultData(VkResult                    replay,
                                                             VkDevice                    lost_device,
                                                             PFN_vkGetDeviceFaultInfoEXT func)
{
    if (consumer_->device_fault_supported_ && replay == VK_ERROR_DEVICE_LOST)
    {
        graphics::DeviceFaultData device_fault_info = graphics::QueryDeviceFaultData(func, lost_device);
        for (const auto& address_info : device_fault_info.address_infos_)
        {
            GFXRECON_LOG_ERROR("Address type: %s",
                               util::ToString<VkDeviceFaultAddressTypeEXT>(address_info.addressType).c_str());
            GFXRECON_LOG_ERROR("Reported address: %" PRIu64, address_info.reportedAddress);
            GFXRECON_LOG_ERROR("Address precision: %" PRIu64, address_info.addressPrecision);
        }

        for (const auto& vendor_info : device_fault_info.vendor_infos_)
        {
            GFXRECON_LOG_ERROR("Vendor description: %s", vendor_info.description);
            GFXRECON_LOG_ERROR("Vendor fault code: %" PRIu64, vendor_info.vendorFaultCode);
            GFXRECON_LOG_ERROR("Vendor fault data: %" PRIu64, vendor_info.vendorFaultData);
        }
        if (consumer_->device_fault_vendor_data_supported_ && !device_fault_info.vendor_binary_data_.empty())
        {
            uint8_t        bytes_read         = 0;
            const uint8_t* vendor_binary_data = device_fault_info.vendor_binary_data_.data();
            uint32_t       header_size        = *reinterpret_cast<const uint32_t*>(vendor_binary_data);
            bytes_read += sizeof(uint32_t);
            VkDeviceFaultVendorBinaryHeaderVersionEXT header_version =
                *reinterpret_cast<const VkDeviceFaultVendorBinaryHeaderVersionEXT*>(vendor_binary_data + bytes_read);
            bytes_read += sizeof(VkDeviceFaultVendorBinaryHeaderVersionEXT);
            GFXRECON_LOG_ERROR("Header version: %s",
                               util::ToString<VkDeviceFaultVendorBinaryHeaderVersionEXT>(header_version).c_str());
            switch (header_version)
            {
                case VK_DEVICE_FAULT_VENDOR_BINARY_HEADER_VERSION_ONE_EXT:
                {
                    VkDeviceFaultVendorBinaryHeaderVersionOneEXT header{ header_size, header_version };
                    ConsumeVendorBinaryDataHeader(vendor_binary_data + bytes_read, header);
                    break;
                }

                default:
                {
                    GFXRECON_LOG_ERROR("Vendor binary data header version is not supported");
                    break;
                }
            }
        }
    }
}

void VulkanReplayConsumerArmFeatures::ConsumeVendorBinaryDataHeader(
    const uint8_t* vendor_binary_data, VkDeviceFaultVendorBinaryHeaderVersionOneEXT& header)
{
    if (sizeof(header) == consumer_->device_fault_vendor_binary_dump_v1_header_size_)
    {
        // Structure size complies to Vulkan specification
        util::platform::MemoryCopy(&header, sizeof(header), vendor_binary_data, sizeof(header));
    }
    else
    {
        // Structure size does not comply to Vulkan specification, manually read header
        uint32_t bytes_read = 0;
        header.vendorID     = *reinterpret_cast<const uint32_t*>(vendor_binary_data);
        bytes_read += sizeof(header.vendorID);

        header.deviceID = *reinterpret_cast<const uint32_t*>(vendor_binary_data + bytes_read);
        bytes_read += sizeof(header.deviceID);

        header.driverVersion = *reinterpret_cast<const uint32_t*>(vendor_binary_data + bytes_read);
        bytes_read += sizeof(header.driverVersion);

        util::platform::MemoryCopy(&header.pipelineCacheUUID,
                                   sizeof(uint8_t) * VK_UUID_SIZE,
                                   vendor_binary_data + bytes_read,
                                   sizeof(uint8_t) * VK_UUID_SIZE);
        bytes_read += sizeof(uint8_t) * VK_UUID_SIZE;

        header.applicationNameOffset = *reinterpret_cast<const uint32_t*>(vendor_binary_data + bytes_read);
        bytes_read += sizeof(header.applicationNameOffset);

        header.applicationVersion = *reinterpret_cast<const uint32_t*>(vendor_binary_data + bytes_read);
        bytes_read += sizeof(header.applicationVersion);

        header.engineNameOffset = *reinterpret_cast<const uint32_t*>(vendor_binary_data + bytes_read);
        bytes_read += sizeof(header.engineNameOffset);

        header.engineVersion = *reinterpret_cast<const uint32_t*>(vendor_binary_data + bytes_read);
        bytes_read += sizeof(header.engineVersion);

        header.apiVersion = *reinterpret_cast<const uint32_t*>(vendor_binary_data + bytes_read);
        bytes_read += sizeof(header.apiVersion);
    }

    GFXRECON_LOG_ERROR("Vendor ID: " PRIu32, header.vendorID);
    GFXRECON_LOG_ERROR("Device ID: " PRIu32, header.deviceID);
    GFXRECON_LOG_ERROR("Driver version: " PRIu32, header.driverVersion);
    for (uint32_t i = 0; i < VK_UUID_SIZE; ++i)
    {
        GFXRECON_LOG_ERROR("Pipeline cache UUID %" PRIu32 ": %" PRIu32, header.pipelineCacheUUID[i]);
    }
    GFXRECON_LOG_ERROR("Application name offset: " PRIu32, header.applicationNameOffset);
    GFXRECON_LOG_ERROR("Application version: " PRIu32, header.applicationVersion);
    GFXRECON_LOG_ERROR("Engine name offset: " PRIu32, header.engineNameOffset);
    GFXRECON_LOG_ERROR("Engine version: " PRIu32, header.engineVersion);
    GFXRECON_LOG_ERROR("API version: " PRIu32, header.apiVersion);
}

void VulkanReplayConsumerArmFeatures::EnableMarkingLayerExtension(
    const std::vector<VkLayerProperties>& available_layers, std::vector<const char*>& modified_layers)
{
    for (const std::string& name : consumer_->options_.marking_layers_names)
    {
        if (graphics::feature_util::IsSupportedLayer(available_layers, name.c_str()))
        {
            modified_layers.push_back(name.c_str());
        }
        else
        {
            GFXRECON_LOG_WARNING("Failed to enable layer '%s' requested by replay", name.c_str());
        }
    }
}

void VulkanReplayConsumerArmFeatures::UseExtFrameBoundary(uint32_t                    submitCount,
                                                          const Decoded_VkSubmitInfo* submit_info_data)
{
    std::vector<VkFrameBoundaryEXT>   inserted_frame_boundaries;
    std::vector<std::vector<VkImage>> inserted_frame_boundaries_images;
    if (consumer_->options_.use_ext_frame_boundary)
    {
        for (uint32_t i = 0; i < submitCount; ++i)
        {
            size_t                  command_buffer_count = submit_info_data[i].pCommandBuffers.GetLength();
            const format::HandleId* command_buffer_ids   = submit_info_data[i].pCommandBuffers.GetPointer();
            for (uint32_t j = 0; j < command_buffer_count; ++j)
            {
                const VulkanCommandBufferInfo* command_buffer_info =
                    consumer_->GetObjectInfoTable().GetVkCommandBufferInfo(command_buffer_ids[j]);

                if (command_buffer_info->is_frame_boundary)
                {
                    FillFrameBoundaryExtFromCommandBufferInfo(command_buffer_info,
                                                              &inserted_frame_boundaries.emplace_back(),
                                                              inserted_frame_boundaries_images.emplace_back());
                    InsertFrameBoundaryExt(submit_info_data[i].decoded_value, &inserted_frame_boundaries.back());
                    break;
                }
            }
        }
    }
}

void VulkanReplayConsumerArmFeatures::UseExtFrameBoundary(uint32_t                     submitCount,
                                                          const Decoded_VkSubmitInfo2* submit_info_data)
{
    std::vector<VkFrameBoundaryEXT>   inserted_frame_boundaries;
    std::vector<std::vector<VkImage>> inserted_frame_boundaries_images;
    if (consumer_->options_.use_ext_frame_boundary)
    {
        for (uint32_t i = 0; i < submitCount; ++i)
        {
            size_t     command_buffer_count = submit_info_data[i].pCommandBufferInfos->GetLength();
            const auto command_buffer_infos = submit_info_data[i].pCommandBufferInfos->GetMetaStructPointer();

            for (uint32_t j = 0; j < command_buffer_count; ++j)
            {
                const VulkanCommandBufferInfo* command_buffer_info =
                    consumer_->GetObjectInfoTable().GetVkCommandBufferInfo(command_buffer_infos[j].commandBuffer);

                if (command_buffer_info->is_frame_boundary)
                {
                    FillFrameBoundaryExtFromCommandBufferInfo(command_buffer_info,
                                                              &inserted_frame_boundaries.emplace_back(),
                                                              inserted_frame_boundaries_images.emplace_back());
                    InsertFrameBoundaryExt(submit_info_data[i].decoded_value, &inserted_frame_boundaries.back());
                    break;
                }
            }
        }
    }
}

void VulkanReplayConsumerArmFeatures::FillFrameBoundaryExtFromCommandBufferInfo(
    const VulkanCommandBufferInfo* command_buffer_info,
    VkFrameBoundaryEXT*            frame_boundary,
    std::vector<VkImage>&          frame_boundary_images)
{
    assert(command_buffer_info->is_frame_boundary);

    frame_boundary_images.clear();

    for (size_t i = 0; i < command_buffer_info->frame_buffer_ids.size(); ++i)
    {
        auto framebuffer_info =
            consumer_->object_info_table_->GetVkFramebufferInfo(command_buffer_info->frame_buffer_ids[i]);

        for (size_t j = 0; j < framebuffer_info->attachment_image_view_ids.size(); ++j)
        {
            auto image_view_id   = framebuffer_info->attachment_image_view_ids[j];
            auto image_view_info = consumer_->object_info_table_->GetVkImageViewInfo(image_view_id);
            auto image_info      = consumer_->object_info_table_->GetVkImageInfo(image_view_info->image_id);

            frame_boundary_images.push_back(image_info->handle);
        }
    }

    frame_boundary->sType       = VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT;
    frame_boundary->pNext       = nullptr;
    frame_boundary->flags       = VK_FRAME_BOUNDARY_FRAME_END_BIT_EXT;
    frame_boundary->frameID     = consumer_->application_->GetCurrentFrameNumber();
    frame_boundary->imageCount  = frame_boundary_images.size();
    frame_boundary->pImages     = frame_boundary_images.data();
    frame_boundary->bufferCount = 0;
    frame_boundary->pBuffers    = nullptr;
    frame_boundary->tagName     = consumer_->application_->GetCurrentFrameNumber();
    frame_boundary->tagSize     = command_buffer_info->frame_boundary_label.size();
    frame_boundary->pTag        = command_buffer_info->frame_boundary_label.data();
}

void VulkanReplayConsumerArmFeatures::InsertFrameBoundaryExt(void*                     pnext_chain,
                                                             const VkFrameBoundaryEXT* frame_boundary)
{
    VkBaseOutStructure* current = reinterpret_cast<VkBaseOutStructure*>(pnext_chain);
    while (current->pNext != nullptr)
    {
        current = current->pNext;

        if (current->sType == VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT)
        {
            GFXRECON_LOG_WARNING(
                "Trying to insert VkFrameBoundaryEXT but there already is one. The new one will be ignored.");
            return;
        }
    }

    current->pNext = reinterpret_cast<VkBaseOutStructure*>(&frame_boundary);
}

void VulkanReplayConsumerArmFeatures::SetPhysicalDevicePropertiesDescriptorBuffer(
    VulkanPhysicalDeviceInfo*          physical_device_info,
    const VkPhysicalDeviceProperties2* capture_properties,
    const VkPhysicalDeviceProperties2* replay_properties)
{
    if (auto descriptor_buffer_capture_pros =
            graphics::vulkan_struct_get_pnext<VkPhysicalDeviceDescriptorBufferPropertiesEXT>(capture_properties))
    {
        physical_device_info->capture_descriptor_buffer_properties = *descriptor_buffer_capture_pros;
    }

    if (auto descriptor_buffer_replay_pros =
            graphics::vulkan_struct_get_pnext<VkPhysicalDeviceDescriptorBufferPropertiesEXT>(replay_properties))
    {
        physical_device_info->replay_device_info->descriptor_buffer_properties        = *descriptor_buffer_replay_pros;
        physical_device_info->replay_device_info->descriptor_buffer_properties->pNext = nullptr;
    }
}

GFXRECON_END_NAMESPACE(gfxrecon)
GFXRECON_END_NAMESPACE(decode)