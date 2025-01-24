/*
** Copyright (c) 2018-2023 Valve Corporation
** Copyright (c) 2018-2023 LunarG, Inc.
** Copyright (c) 2023 Advanced Micro Devices, Inc.
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

/*
** This file is generated from the Khronos Vulkan XML API Registry.
**
*/

#ifndef  GFXRECON_GENERATED_VULKAN_STRUCT_HANDLE_MAPPERS_H
#define  GFXRECON_GENERATED_VULKAN_STRUCT_HANDLE_MAPPERS_H

#include "decode/common_object_info_table.h"
#include "decode/vulkan_pnext_node.h"
#include "format/platform_types.h"
#include "decode/custom_vulkan_struct_handle_mappers.h"
#include "generated/generated_vulkan_struct_decoders_forward.h"
#include "util/defines.h"

#include "vulkan/vulkan.h"
#include "vk_video/vulkan_video_codec_h264std.h"
#include "vk_video/vulkan_video_codec_h264std_decode.h"
#include "vk_video/vulkan_video_codec_h264std_encode.h"
#include "vk_video/vulkan_video_codec_h265std.h"
#include "vk_video/vulkan_video_codec_h265std_decode.h"
#include "vk_video/vulkan_video_codec_h265std_encode.h"
#include "vk_video/vulkan_video_codecs_common.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

void MapStructHandles(Decoded_VkBufferMemoryBarrier* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageMemoryBarrier* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSubmitInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMappedMemoryRange* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryAllocateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSparseMemoryBind* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSparseBufferMemoryBindInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSparseImageOpaqueMemoryBindInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSparseImageMemoryBind* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSparseImageMemoryBindInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindSparseInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBufferViewCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageViewCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkShaderModuleCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineShaderStageCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkComputePipelineCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGraphicsPipelineCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineLayoutCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSamplerCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyDescriptorSet* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDescriptorBufferInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDescriptorSetAllocateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDescriptorSetLayoutBinding* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDescriptorSetLayoutCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkFramebufferCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCommandBufferAllocateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCommandBufferInheritanceInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCommandBufferBeginInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderPassBeginInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindBufferMemoryInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindImageMemoryInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevice16BitStorageFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryDedicatedAllocateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryAllocateFlagsInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceGroupRenderPassBeginInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceGroupSubmitInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceGroupBindSparseInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindImageMemoryDeviceGroupInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceGroupProperties* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceGroupDeviceCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBufferMemoryRequirementsInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageMemoryRequirementsInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageSparseMemoryRequirementsInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageViewUsageCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMultiviewFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVariablePointersFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceProtectedMemoryFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkProtectedSubmitInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSamplerYcbcrConversionInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindImagePlaneMemoryInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceSamplerYcbcrConversionFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDescriptorUpdateTemplateCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkExternalMemoryImageCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkExportMemoryAllocateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderDrawParametersFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVulkan11Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVulkan12Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageFormatListCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevice8BitStorageFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderAtomicInt64Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderFloat16Int8Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDescriptorIndexingFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceScalarBlockLayoutFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageStencilUsageCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSamplerReductionModeCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVulkanMemoryModelFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImagelessFramebufferFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderPassAttachmentBeginInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceUniformBufferStandardLayoutFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceHostQueryResetFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceTimelineSemaphoreFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkTimelineSemaphoreSubmitInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSemaphoreWaitInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSemaphoreSignalInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceBufferDeviceAddressFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBufferDeviceAddressInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryOpaqueCaptureAddressAllocateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceMemoryOpaqueCaptureAddressInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVulkan13Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineCreationFeedbackCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderTerminateInvocationFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePrivateDataFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDevicePrivateDataCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePipelineCreationCacheControlFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBufferMemoryBarrier2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageMemoryBarrier2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDependencyInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSemaphoreSubmitInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCommandBufferSubmitInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSubmitInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceSynchronization2Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImageRobustnessFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyBufferInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyImageInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyBufferToImageInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyImageToBufferInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBlitImageInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkResolveImageInfo2* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceSubgroupSizeControlFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineShaderStageRequiredSubgroupSizeCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceInlineUniformBlockFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkWriteDescriptorSetInlineUniformBlock* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceTextureCompressionASTCHDRFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderingAttachmentInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderingInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineRenderingCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDynamicRenderingFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderIntegerDotProductFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMaintenance4Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceImageMemoryRequirements* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVulkan14Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceGlobalPriorityQueryFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderSubgroupRotateFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderFloatControls2Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderExpectAssumeFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceLineRasterizationFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVertexAttributeDivisorFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceIndexTypeUint8Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryMapInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryUnmapInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMaintenance5Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceImageSubresourceInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineCreateFlags2CreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDynamicRenderingLocalReadFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderingAttachmentLocationInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderingInputAttachmentIndexInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMaintenance6Features* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindMemoryStatus* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindDescriptorSetsInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPushConstantsInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPushDescriptorSetInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePipelineProtectedAccessFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePipelineRobustnessFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineRobustnessCreateInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceHostImageCopyFeatures* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyMemoryToImageInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyImageToMemoryInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyImageToImageInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkHostImageLayoutTransitionInfo* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSwapchainCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPresentInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageSwapchainCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindImageMemorySwapchainInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAcquireNextImageInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceGroupPresentInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplayModePropertiesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplayPlanePropertiesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplayPropertiesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplaySurfaceCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplayPresentInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoProfileListInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoPictureResourceInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoReferenceSlotInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindVideoSessionMemoryInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoSessionParametersCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoBeginCodingInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoDecodeInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoEncodeH264PictureInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoEncodeH265PictureInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoDecodeH264PictureInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportMemoryWin32HandleInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkExportMemoryWin32HandleInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryGetWin32HandleInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportMemoryFdInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryGetFdInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkWin32KeyedMutexAcquireReleaseInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportSemaphoreWin32HandleInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkD3D12FenceSubmitInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSemaphoreGetWin32HandleInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportSemaphoreFdInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSemaphoreGetFdInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPresentRegionsKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportFenceWin32HandleInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkFenceGetWin32HandleInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportFenceFdInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkFenceGetFdInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePerformanceQueryFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPerformanceQuerySubmitInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceSurfaceInfo2KHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplayProperties2KHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplayPlaneProperties2KHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplayModeProperties2KHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDisplayPlaneInfo2KHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePortabilitySubsetFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderClockFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoDecodeH265PictureInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineFragmentShadingRateStateCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFragmentShadingRateFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderingFragmentShadingRateAttachmentInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderQuadControlFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePresentWaitFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineExecutableInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineLibraryCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPresentIdKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePresentIdFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoEncodeInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoEncodeSessionParametersGetInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderMaximalReconvergenceFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePipelineBinaryFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDevicePipelineBinaryInternalCacheControlKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineBinaryCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineBinaryInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkReleaseCapturedPipelineDataInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineBinaryDataInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineBinaryHandlesInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCooperativeMatrixFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoDecodeAV1PictureInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVideoEncodeAV1FeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoEncodeAV1PictureInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVideoMaintenance1FeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoInlineQueryInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSetDescriptorBufferOffsetsInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindDescriptorBufferEmbeddedSamplersInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkVideoEncodeQuantizationMapInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVideoEncodeQuantizationMapFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMaintenance7FeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDebugMarkerObjectNameInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDebugMarkerObjectTagInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDedicatedAllocationImageCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDedicatedAllocationMemoryAllocateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceTransformFeedbackFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageViewHandleInfoNVX* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCornerSampledImageFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkExternalMemoryImageCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkExportMemoryAllocateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportMemoryWin32HandleInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkExportMemoryWin32HandleInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkWin32KeyedMutexAcquireReleaseInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageViewASTCDecodeModeEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceASTCDecodeFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkConditionalRenderingBeginInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceConditionalRenderingFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPresentTimesInfoGOOGLE* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMultiviewPerViewAttributesInfoNVX* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineDiscardRectangleStateCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDepthClipEnableFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRelaxedLineRasterizationFeaturesIMG* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDebugUtilsObjectNameInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDebugUtilsMessengerCallbackDataEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDebugUtilsObjectTagInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportAndroidHardwareBufferInfoANDROID* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryGetAndroidHardwareBufferInfoANDROID* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkExternalFormatANDROID* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAttachmentSampleCountInfoAMD* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderPassSampleLocationsBeginInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderSMBuiltinsFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageDrmFormatModifierListCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageDrmFormatModifierExplicitCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkShaderModuleValidationCacheCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShadingRateImageFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRayTracingPipelineCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeometryTrianglesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeometryAABBNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeometryDataNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeometryNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkBindAccelerationStructureMemoryInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkWriteDescriptorSetAccelerationStructureNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureMemoryRequirementsInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRepresentativeFragmentTestFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineRepresentativeFragmentTestStateCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportMemoryHostPointerInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineCompilerControlCreateInfoAMD* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceMemoryOverallocationCreateInfoAMD* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPresentFrameTokenGGP* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMeshShaderFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderImageFootprintFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceExclusiveScissorFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderIntegerFunctions2FeaturesINTEL* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFragmentDensityMapFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderingFragmentDensityMapAttachmentInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCoherentMemoryFeaturesAMD* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMemoryPriorityFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryPriorityAllocateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceBufferDeviceAddressFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkValidationFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCooperativeMatrixFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCoverageReductionModeFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceYcbcrImageArraysFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceProvokingVertexFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderAtomicFloatFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceExtendedDynamicStateFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMapMemoryPlacedFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSwapchainPresentFenceInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSwapchainPresentModeInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkReleaseSwapchainImagesInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGraphicsShaderGroupCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGraphicsPipelineShaderGroupsCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkIndirectCommandsStreamNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkIndirectCommandsLayoutTokenNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkIndirectCommandsLayoutCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeneratedCommandsInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeneratedCommandsMemoryRequirementsInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceInheritedViewportScissorFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderPassTransformBeginInfoQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDepthBiasControlFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDeviceMemoryReportFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceDeviceMemoryReportCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRobustness2FeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSamplerCustomBorderColorCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCustomBorderColorFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePresentBarrierFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDiagnosticsConfigFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceDiagnosticsConfigCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGraphicsPipelineLibraryCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFragmentShadingRateEnumsFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineFragmentShadingRateEnumStateCreateInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureGeometryMotionTrianglesDataNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRayTracingMotionBlurFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFragmentDensityMap2FeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImageCompressionControlFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageCompressionControlEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevice4444FormatsFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFaultFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceAddressBindingReportFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDepthClipControlFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePresentModeFifoLatestReadyFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportMemoryZirconHandleInfoFUCHSIA* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryGetZirconHandleInfoFUCHSIA* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImportSemaphoreZirconHandleInfoFUCHSIA* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSemaphoreGetZirconHandleInfoFUCHSIA* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceInvocationMaskFeaturesHUAWEI* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMemoryGetRemoteAddressInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceExternalMemoryRDMAFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFrameBoundaryFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkFrameBoundaryEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMultisampledRenderToSingleSampledInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceExtendedDynamicState2FeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceColorWriteEnableFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImageViewMinLodFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageViewMinLodCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMultiDrawFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImage2DViewOf3DFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderTileImageFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMicromapBuildInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMicromapCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceOpacityMicromapFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyMicromapToMemoryInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyMemoryToMicromapInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyMicromapInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureTrianglesOpacityMicromapEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDisplacementMicromapFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureTrianglesDisplacementMicromapNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceClusterCullingShaderFeaturesHUAWEI* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceClusterCullingShaderVrsFeaturesHUAWEI* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceBorderColorSwizzleFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSamplerBorderColorComponentMappingCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDeviceQueueShaderCoreControlCreateInfoARM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceSchedulingControlsFeaturesARM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImageSlicedViewOf3DFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageViewSlicedCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDescriptorSetHostMappingFeaturesVALVE* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkDescriptorSetBindingReferenceVALVE* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDepthClampZeroOneFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRenderPassStripedFeaturesARM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderPassStripeBeginInfoARM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRenderPassStripeSubmitInfoARM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDeviceGeneratedCommandsComputeFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkComputePipelineIndirectBufferInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineIndirectDeviceAddressInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceLinearColorAttachmentFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImageCompressionControlSwapchainFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageViewSampleWeightCreateInfoQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImageProcessingFeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceNestedCommandBufferFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceExtendedDynamicState3FeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceSubpassMergeFeedbackFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPipelineShaderStageModuleIdentifierCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceOpticalFlowFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkOpticalFlowImageFormatInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceLegacyDitheringFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceExternalFormatResolveFeaturesANDROID* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceAntiLagFeaturesAMD* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderObjectFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkShaderCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceTilePropertiesFeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceAmigoProfilingFeaturesSEC* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAmigoProfilingSubmitInfoSEC* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceExtendedSparseAddressSpaceFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceLegacyVertexAttributesFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderCoreBuiltinsFeaturesARM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkLatencySleepInfoNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkLatencySubmissionPresentIdNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkMultiviewPerViewRenderAreasRenderPassBeginInfoQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDevicePerStageDescriptorSetFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImageProcessing2FeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSamplerBlockMatchWindowCreateInfoQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCubicWeightsFeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkSamplerCubicWeightsCreateInfoQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceYcbcrDegammaFeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCubicClampFeaturesQCOM* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDescriptorPoolOverallocationFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRawAccessChainsFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCommandBufferInheritanceFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderAtomicFloat16VectorFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceShaderReplicatedCompositesFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRayTracingValidationFeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeneratedCommandsMemoryRequirementsInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkIndirectExecutionSetPipelineInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkIndirectExecutionSetShaderLayoutInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkIndirectExecutionSetShaderInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeneratedCommandsInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkWriteIndirectExecutionSetPipelineEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkIndirectCommandsLayoutCreateInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeneratedCommandsPipelineInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkGeneratedCommandsShaderInfoEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkWriteIndirectExecutionSetShaderEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceImageAlignmentControlFeaturesMESA* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkImageAlignmentControlCreateInfoMESA* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceDepthClampControlFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceHdrVividFeaturesHUAWEI* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceCooperativeMatrix2FeaturesNV* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceVertexAttributeRobustnessFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureGeometryTrianglesDataKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkWriteDescriptorSetAccelerationStructureKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceAccelerationStructureFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkAccelerationStructureDeviceAddressInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyAccelerationStructureToMemoryInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyMemoryToAccelerationStructureInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkCopyAccelerationStructureInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkRayTracingPipelineCreateInfoKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRayTracingPipelineFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceRayQueryFeaturesKHR* wrapper, const CommonObjectInfoTable& object_info_table);

void MapStructHandles(Decoded_VkPhysicalDeviceMeshShaderFeaturesEXT* wrapper, const CommonObjectInfoTable& object_info_table);

void MapPNextStructHandles(const void* value, void* wrapper, const CommonObjectInfoTable& object_info_table);

void AddStructHandles(format::HandleId parent_id, const Decoded_VkPhysicalDeviceGroupProperties* id_wrapper, const VkPhysicalDeviceGroupProperties* handle_struct, CommonObjectInfoTable* object_info_table);

void AddStructHandles(format::HandleId parent_id, const Decoded_VkDisplayPropertiesKHR* id_wrapper, const VkDisplayPropertiesKHR* handle_struct, CommonObjectInfoTable* object_info_table);

void AddStructHandles(format::HandleId parent_id, const Decoded_VkDisplayPlanePropertiesKHR* id_wrapper, const VkDisplayPlanePropertiesKHR* handle_struct, CommonObjectInfoTable* object_info_table);

void AddStructHandles(format::HandleId parent_id, const Decoded_VkDisplayModePropertiesKHR* id_wrapper, const VkDisplayModePropertiesKHR* handle_struct, CommonObjectInfoTable* object_info_table);

void AddStructHandles(format::HandleId parent_id, const Decoded_VkDisplayProperties2KHR* id_wrapper, const VkDisplayProperties2KHR* handle_struct, CommonObjectInfoTable* object_info_table);

void AddStructHandles(format::HandleId parent_id, const Decoded_VkDisplayPlaneProperties2KHR* id_wrapper, const VkDisplayPlaneProperties2KHR* handle_struct, CommonObjectInfoTable* object_info_table);

void AddStructHandles(format::HandleId parent_id, const Decoded_VkDisplayModeProperties2KHR* id_wrapper, const VkDisplayModeProperties2KHR* handle_struct, CommonObjectInfoTable* object_info_table);

void AddStructHandles(format::HandleId parent_id, const Decoded_VkPipelineBinaryHandlesInfoKHR* id_wrapper, const VkPipelineBinaryHandlesInfoKHR* handle_struct, CommonObjectInfoTable* object_info_table);

void SetStructHandleLengths(Decoded_VkPhysicalDeviceGroupProperties* wrapper);

void SetStructHandleLengths(Decoded_VkPipelineBinaryHandlesInfoKHR* wrapper);

#include "decode/common_struct_handle_mappers.h"
GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif
