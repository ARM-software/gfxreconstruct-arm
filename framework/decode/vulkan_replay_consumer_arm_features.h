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

#ifndef GFXRECON_DECODE_VULKAN_REPLAY_CONSUMER_ARM_FEATURES_H
#define GFXRECON_DECODE_VULKAN_REPLAY_CONSUMER_ARM_FEATURES_H

#include "util/defines.h"
#include "vulkan/vulkan_core.h"
#include "generated/generated_vulkan_struct_decoders.h"
#include "decode/vulkan_object_info.h"
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanReplayConsumerBase;

class VulkanReplayConsumerArmFeatures
{
  public:
    VulkanReplayConsumerArmFeatures(VulkanReplayConsumerBase* consumer);
    void ProcessFillMemoryCommandDeviceAddresses(const uint8_t* data);
    void ProcessDeviceFaultData(VkResult replay, VkDevice lost_device, PFN_vkGetDeviceFaultInfoEXT func);
    void ConsumeVendorBinaryDataHeader(const uint8_t*                                vendor_binary_data,
                                       VkDeviceFaultVendorBinaryHeaderVersionOneEXT& header);
    void EnableMarkingLayerExtension(const std::vector<VkLayerProperties>& available_layers,
                                     std::vector<const char*>&             modified_layers);

    void UseExtFrameBoundary(uint32_t submitCount, const Decoded_VkSubmitInfo* submit_info_data);
    void UseExtFrameBoundary(uint32_t submitCount, const Decoded_VkSubmitInfo2* submit_info_data);

    void FillFrameBoundaryExtFromCommandBufferInfo(const VulkanCommandBufferInfo* command_buffer_info,
                                                   VkFrameBoundaryEXT*            frame_boundary,
                                                   std::vector<VkImage>&          frame_boundary_images);

    void InsertFrameBoundaryExt(void* pnext_chain, const VkFrameBoundaryEXT* frame_boundary);

    void DisableSubpassFusion(const StructPointerDecoder<Decoded_VkRenderPassCreateInfo>* pCreateInfo);

    void LogFrameDebugInfo();

    void ReplaceDeviceAddresses(VulkanCommandBufferInfo* command_buffer_info, void* data);

    bool UseExtFrameBoundaryAndroid(const VulkanDeviceInfo* device_info, VkSemaphore semaphore, VkImage image);

    void SetPhysicalDevicePropertiesDescriptorBuffer(VulkanPhysicalDeviceInfo*          physical_device_info,
                                                     const VkPhysicalDeviceProperties2* capture_properties,
                                                     const VkPhysicalDeviceProperties2* replay_properties);

  private:
    VulkanReplayConsumerBase* consumer_;
};
GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_REPLAY_CONSUMER_ARM_FEATURES_H