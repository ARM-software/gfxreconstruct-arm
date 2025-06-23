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

#include "decode/vulkan_feature_tracker_consumer_base.h"

#include "util/logging.h"

#include <cassert>
#include <stdexcept>
#include <algorithm>

#include "generated/generated_vulkan_api_call_encoders.h"

#include "encode/custom_vulkan_encoder_commands.h"
#include "encode/custom_vulkan_array_size_2d.h"
#include "encode/parameter_encoder.h"
#include "encode/struct_pointer_encoder.h"
#include "encode/vulkan_capture_manager.h"
#include "encode/vulkan_handle_wrapper_util.h"
#include "encode/vulkan_handle_wrappers.h"
#include "format/api_call_id.h"
#include "generated/generated_vulkan_command_buffer_util.h"
#include "generated/generated_vulkan_struct_handle_wrappers.h"
#include "util/defines.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanFeatureTrackerConsumerBase::VulkanFeatureTrackerConsumerBase()
{
    /*
     * IMPORTANT:
     * Members initialized with true are not handled
     * Members initialized with false are handled
     * TODO: handle features initialized with true
     */

    /*
     * NAMING CONVENTION:
     * corexx_        -> internal variable modified when detecting an used/unused feature
     * capture_corexx -> the features requested by the app
     * output_corexx  -> result meant to be inserted in the trace (instead of capture_corexx)
     * Same logic applies for extensions handling naming
     */

    // TODO: Have all lines below(inside the constructor) generated from a json file
    // TODO: Have the ProcessCoreXXFeatures methods be generated to be more readable
    // TODO: Have a generated file that calls Process_STRUCTNAME as the current way of processing functions results in
    // having the same handling for the same struct in multiple functions

    core10_.robustBufferAccess                      = true;
    core10_.fullDrawIndexUint32                     = true;
    core10_.imageCubeArray                          = false;
    core10_.independentBlend                        = false;
    core10_.geometryShader                          = false;
    core10_.tessellationShader                      = false;
    core10_.sampleRateShading                       = false;
    core10_.dualSrcBlend                            = false;
    core10_.logicOp                                 = false;
    core10_.multiDrawIndirect                       = false;
    core10_.drawIndirectFirstInstance               = true;
    core10_.depthClamp                              = false;
    core10_.depthBiasClamp                          = true;
    core10_.fillModeNonSolid                        = false;
    core10_.depthBounds                             = false;
    core10_.wideLines                               = true;
    core10_.largePoints                             = true;
    core10_.alphaToOne                              = false;
    core10_.multiViewport                           = false;
    core10_.samplerAnisotropy                       = false;
    core10_.textureCompressionETC2                  = true;
    core10_.textureCompressionASTC_LDR              = true;
    core10_.textureCompressionBC                    = true;
    core10_.occlusionQueryPrecise                   = true;
    core10_.pipelineStatisticsQuery                 = false;
    core10_.vertexPipelineStoresAndAtomics          = true;
    core10_.fragmentStoresAndAtomics                = true;
    core10_.shaderTessellationAndGeometryPointSize  = true;
    core10_.shaderImageGatherExtended               = true;
    core10_.shaderStorageImageExtendedFormats       = true;
    core10_.shaderStorageImageMultisample           = false;
    core10_.shaderStorageImageReadWithoutFormat     = true;
    core10_.shaderStorageImageWriteWithoutFormat    = true;
    core10_.shaderUniformBufferArrayDynamicIndexing = true;
    core10_.shaderSampledImageArrayDynamicIndexing  = true;
    core10_.shaderStorageBufferArrayDynamicIndexing = true;
    core10_.shaderStorageImageArrayDynamicIndexing  = true;
    core10_.shaderClipDistance                      = true;
    core10_.shaderCullDistance                      = true;
    core10_.shaderFloat64                           = true;
    core10_.shaderInt64                             = true;
    core10_.shaderInt16                             = true;
    core10_.shaderResourceResidency                 = true;
    core10_.shaderResourceMinLod                    = true;
    core10_.variableMultisampleRate                 = true;
    core10_.sparseBinding                           = false;
    core10_.sparseResidencyBuffer                   = false;
    core10_.sparseResidencyImage2D                  = false;
    core10_.sparseResidencyImage3D                  = false;
    core10_.sparseResidency2Samples                 = false;
    core10_.sparseResidency4Samples                 = false;
    core10_.sparseResidency8Samples                 = false;
    core10_.sparseResidency16Samples                = false;
    core10_.sparseResidencyAliased                  = false;
    core10_.inheritedQueries                        = false;

    core10_members_as_strings_ = { "robustBufferAccess",
                                   "fullDrawIndexUint32",
                                   "imageCubeArray",
                                   "independentBlend",
                                   "geometryShader",
                                   "tessellationShader",
                                   "sampleRateShading",
                                   "dualSrcBlend",
                                   "logicOp",
                                   "multiDrawIndirect",
                                   "drawIndirectFirstInstance",
                                   "depthClamp",
                                   "depthBiasClamp",
                                   "fillModeNonSolid",
                                   "depthBounds",
                                   "wideLines",
                                   "largePoints",
                                   "alphaToOne",
                                   "multiViewport",
                                   "samplerAnisotropy",
                                   "textureCompressionETC2",
                                   "textureCompressionASTC_LDR",
                                   "textureCompressionBC",
                                   "occlusionQueryPrecise",
                                   "pipelineStatisticsQuery",
                                   "vertexPipelineStoresAndAtomics",
                                   "fragmentStoresAndAtomics",
                                   "shaderTessellationAndGeometryPointSize",
                                   "shaderImageGatherExtended",
                                   "shaderStorageImageExtendedFormats",
                                   "shaderStorageImageMultisample",
                                   "shaderStorageImageReadWithoutFormat",
                                   "shaderStorageImageWriteWithoutFormat",
                                   "shaderUniformBufferArrayDynamicIndexing "
                                   "shaderSampledImageArrayDynamicIndexing",
                                   "shaderStorageBufferArrayDynamicIndexing "
                                   "shaderStorageImageArrayDynamicIndexing",
                                   "shaderClipDistance",
                                   "shaderCullDistance",
                                   "shaderFloat64",
                                   "shaderInt64",
                                   "shaderInt16",
                                   "shaderResourceResidency",
                                   "shaderResourceMinLod",
                                   "sparseBinding",
                                   "sparseResidencyBuffer",
                                   "sparseResidencyImage2D",
                                   "sparseResidencyImage3D",
                                   "sparseResidency2Samples",
                                   "sparseResidency4Samples",
                                   "sparseResidency8Samples",
                                   "sparseResidency16Samples",
                                   "sparseResidencyAliased",
                                   "variableMultisampleRate",
                                   "inheritedQueries" };

    core11_.storageBuffer16BitAccess           = true;
    core11_.uniformAndStorageBuffer16BitAccess = true;
    core11_.storagePushConstant16              = true;
    core11_.storageInputOutput16               = true;
    core11_.multiview                          = true;
    core11_.multiviewGeometryShader            = true;
    core11_.multiviewTessellationShader        = true;
    core11_.variablePointersStorageBuffer      = true;
    core11_.variablePointers                   = true;
    core11_.protectedMemory                    = true;
    core11_.samplerYcbcrConversion             = true;
    core11_.shaderDrawParameters               = true;

    core11_members_as_strings_ = { "storageBuffer16BitAccess",
                                   "uniformAndStorageBuffer16BitAccess",
                                   "storagePushConstant16",
                                   "storageInputOutput16",
                                   "multiview",
                                   "multiviewGeometryShader",
                                   "multiviewTessellationShader",
                                   "variablePointersStorageBuffer",
                                   "variablePointers",
                                   "protectedMemory",
                                   "samplerYcbcrConversion",
                                   "shaderDrawParameters" };

    core12_.samplerMirrorClampToEdge                           = false;
    core12_.drawIndirectCount                                  = false;
    core12_.storageBuffer8BitAccess                            = true;
    core12_.uniformAndStorageBuffer8BitAccess                  = true;
    core12_.storagePushConstant8                               = true;
    core12_.shaderBufferInt64Atomics                           = true;
    core12_.shaderSharedInt64Atomics                           = true;
    core12_.shaderFloat16                                      = true;
    core12_.shaderInt8                                         = true;
    core12_.descriptorIndexing                                 = true;
    core12_.shaderInputAttachmentArrayDynamicIndexing          = true;
    core12_.shaderUniformTexelBufferArrayDynamicIndexing       = true;
    core12_.shaderStorageTexelBufferArrayDynamicIndexing       = true;
    core12_.shaderUniformBufferArrayNonUniformIndexing         = true;
    core12_.shaderSampledImageArrayNonUniformIndexing          = true;
    core12_.shaderStorageBufferArrayNonUniformIndexing         = true;
    core12_.shaderStorageImageArrayNonUniformIndexing          = true;
    core12_.shaderInputAttachmentArrayNonUniformIndexing       = true;
    core12_.shaderUniformTexelBufferArrayNonUniformIndexing    = true;
    core12_.shaderStorageTexelBufferArrayNonUniformIndexing    = true;
    core12_.descriptorBindingUniformBufferUpdateAfterBind      = true;
    core12_.descriptorBindingSampledImageUpdateAfterBind       = true;
    core12_.descriptorBindingStorageImageUpdateAfterBind       = true;
    core12_.descriptorBindingStorageBufferUpdateAfterBind      = true;
    core12_.descriptorBindingUniformTexelBufferUpdateAfterBind = true;
    core12_.descriptorBindingStorageTexelBufferUpdateAfterBind = true;
    core12_.descriptorBindingUpdateUnusedWhilePending          = true;
    core12_.descriptorBindingPartiallyBound                    = true;
    core12_.descriptorBindingVariableDescriptorCount           = true;
    core12_.runtimeDescriptorArray                             = true;
    core12_.samplerFilterMinmax                                = true;
    core12_.scalarBlockLayout                                  = true;
    core12_.imagelessFramebuffer                               = true;
    core12_.uniformBufferStandardLayout                        = true;
    core12_.shaderSubgroupExtendedTypes                        = true;
    core12_.separateDepthStencilLayouts                        = true;
    core12_.hostQueryReset                                     = false;
    core12_.timelineSemaphore                                  = true;
    core12_.bufferDeviceAddress                                = true;
    core12_.bufferDeviceAddressCaptureReplay                   = true;
    core12_.bufferDeviceAddressMultiDevice                     = true;
    core12_.vulkanMemoryModel                                  = true;
    core12_.vulkanMemoryModelDeviceScope                       = true;
    core12_.vulkanMemoryModelAvailabilityVisibilityChains      = true;
    core12_.shaderOutputViewportIndex                          = true;
    core12_.shaderOutputLayer                                  = true;
    core12_.subgroupBroadcastDynamicId                         = true;

    core12_members_as_strings_ = { "samplerMirrorClampToEdge",
                                   "drawIndirectCount",
                                   "storageBuffer8BitAccess",
                                   "uniformAndStorageBuffer8BitAccess",
                                   "storagePushConstant8",
                                   "shaderBufferInt64Atomics",
                                   "shaderSharedInt64Atomics",
                                   "shaderFloat16",
                                   "shaderInt8",
                                   "descriptorIndexing",
                                   "shaderInputAttachmentArrayDynamicIndexing",
                                   "shaderUniformTexelBufferArrayDynamicIndexing",
                                   "shaderStorageTexelBufferArrayDynamicIndexing",
                                   "shaderUniformBufferArrayNonUniformIndexing",
                                   "shaderSampledImageArrayNonUniformIndexing",
                                   "shaderStorageBufferArrayNonUniformIndexing",
                                   "shaderInputAttachmentArrayNonUniformIndexing",
                                   "shaderUniformTexelBufferArrayNonUniformIndexing",
                                   "shaderStorageTexelBufferArrayNonUniformIndexing",
                                   "descriptorBindingUniformBufferUpdateAfterBind",
                                   "descriptorBindingSampledImageUpdateAfterBind",
                                   "descriptorBindingStorageImageUpdateAfterBind",
                                   "descriptorBindingStorageBufferUpdateAfterBind",
                                   "descriptorBindingUniformTexelBufferUpdateAfterBind",
                                   "descriptorBindingStorageTexelBufferUpdateAfterBind",
                                   "descriptorBindingUpdateUnusedWhilePending",
                                   "descriptorBindingPartiallyBound",
                                   "descriptorBindingVariableDescriptorCount",
                                   "runtimeDescriptorArray",
                                   "samplerFilterMinmax",
                                   "scalarBlockLayout",
                                   "imagelessFramebuffer",
                                   "uniformBufferStandardLayout",
                                   "shaderSubgroupExtendedTypes",
                                   "separateDepthStencilLayouts",
                                   "hostQueryReset",
                                   "timelineSemaphore",
                                   "bufferDeviceAddress",
                                   "bufferDeviceAddressCaptureReplay",
                                   "bufferDeviceAddressMultiDevice",
                                   "vulkanMemoryModel",
                                   "vulkanMemoryModelAvailabilityVisibilityChains",
                                   "shaderOutputViewportIndex",
                                   "shaderOutputLayer",
                                   "subgroupBroadcastDynamicId" };

    core13_.robustImageAccess                                  = true;
    core13_.inlineUniformBlock                                 = true;
    core13_.descriptorBindingInlineUniformBlockUpdateAfterBind = true;
    core13_.pipelineCreationCacheControl                       = true;
    core13_.privateData                                        = true;
    core13_.shaderDemoteToHelperInvocation                     = true;
    core13_.shaderTerminateInvocation                          = true;
    core13_.subgroupSizeControl                                = true;
    core13_.computeFullSubgroups                               = true;
    core13_.synchronization2                                   = true;
    core13_.textureCompressionASTC_HDR                         = true;
    core13_.shaderZeroInitializeWorkgroupMemory                = true;
    core13_.dynamicRendering                                   = false;
    core13_.shaderIntegerDotProduct                            = true;
    core13_.maintenance4                                       = true;

    core13_members_as_strings_ = { "robustImageAccess",
                                   "inlineUniformBlock",
                                   "descriptorBindingInlineUniformBlockUpdateAfterBind",
                                   "pipelineCreationCacheControl",
                                   "privateData",
                                   "shaderDemoteToHelperInvocation",
                                   "shaderTerminateInvocation",
                                   "subgroupSizeControl",
                                   "computeFullSubgroups",
                                   "synchronization2",
                                   "textureCompressionASTC_HDR",
                                   "shaderZeroInitializeWorkgroupMemory",
                                   "dynamicRendering",
                                   "shaderIntegerDotProduct",
                                   "maintenance4" };

    // extensions & alias extensions
    supported_instance_extensions_map_ = { { VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, false } };
    supported_device_extensions_map_   = { { VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME, false } };
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateInstance(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    StructPointerDecoder<Decoded_VkInstanceCreateInfo>*  pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkInstance>*                    pInstance)
{
    auto pCreateInfoDec = pCreateInfo->GetMetaStructPointer()->decoded_value;

    if (pCreateInfoDec->enabledExtensionCount)
    {
        if (!IsModificationPass())
        {
            std::vector<std::string> extensions_vector(pCreateInfoDec->ppEnabledExtensionNames,
                                                       pCreateInfoDec->ppEnabledExtensionNames +
                                                           pCreateInfoDec->enabledExtensionCount);
            capture_instance_extensions_vector_.push_back(extensions_vector);
        }
        else
        {
            assert(pCreateInfoDec->enabledExtensionCount >= output_instance_extensions_vector_.back().size());

            std::vector<const char*> extensions(output_instance_extensions_vector_.back().size());
            for (uint32_t i = 0; i < extensions.size(); i++)
            {
                extensions[i] = output_instance_extensions_vector_.back()[i].c_str();
            }

            pCreateInfoDec->ppEnabledExtensionNames = extensions.data();
            pCreateInfoDec->enabledExtensionCount   = extensions.size();

            parameter_buffer_->Clear();

            gfxrecon::encode::ParameterEncoder encoder(parameter_buffer_);
            EncodeStructPtr(&encoder, pCreateInfo->GetPointer());
            EncodeStructPtr(&encoder, pAllocator->GetPointer());
            encoder.EncodeHandleIdPtr(pInstance->GetPointer());
            encoder.EncodeEnumValue(returnValue);

            output_instance_extensions_vector_.pop_back();
        }
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateDevice(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    format::HandleId                                     physicalDevice,
    StructPointerDecoder<Decoded_VkDeviceCreateInfo>*    pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkDevice>*                      pDevice)
{
    auto pCreateInfoDec = pCreateInfo->GetMetaStructPointer()->decoded_value;

    auto pEnabledFeatures = pCreateInfoDec->pEnabledFeatures;

    if (pEnabledFeatures != nullptr)
    {
        if (!IsModificationPass())
        {
            capture_core10_.push_back(*pEnabledFeatures);
        }
        else
        {
            *((VkPhysicalDeviceFeatures*)pEnabledFeatures) = output_core10_.back();
            output_core10_.pop_back();
        }
    }

    void* pNext = const_cast<void*>(pCreateInfoDec->pNext);
    while (pNext != nullptr)
    {
        if (((VkBaseInStructure*)pNext)->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
        {
            if (!IsModificationPass())
            {
                capture_core10_.push_back(((VkPhysicalDeviceFeatures2*)pNext)->features);
            }
            else
            {
                ((VkPhysicalDeviceFeatures2*)pNext)->features = output_core10_.back();
                output_core10_.pop_back();
            }
        }
        else if (((VkBaseInStructure*)pNext)->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES)
        {
            if (!IsModificationPass())
            {
                capture_core11_.push_back(*((VkPhysicalDeviceVulkan11Features*)pNext));
            }
            else
            {
                output_core11_.back().pNext                 = ((VkPhysicalDeviceVulkan11Features*)pNext)->pNext;
                *((VkPhysicalDeviceVulkan11Features*)pNext) = output_core11_.back();
                output_core11_.pop_back();
            }
        }
        else if (((VkBaseInStructure*)pNext)->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)
        {
            if (!IsModificationPass())
            {
                capture_core12_.push_back(*((VkPhysicalDeviceVulkan12Features*)pNext));
            }
            else
            {
                output_core12_.back().pNext                 = ((VkPhysicalDeviceVulkan12Features*)pNext)->pNext;
                *((VkPhysicalDeviceVulkan12Features*)pNext) = output_core12_.back();
                output_core12_.pop_back();
            }
        }
        else if (((VkBaseInStructure*)pNext)->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
        {
            if (!IsModificationPass())
            {
                capture_core13_.push_back(*((VkPhysicalDeviceVulkan13Features*)pNext));
            }
            else
            {
                output_core13_.back().pNext                 = ((VkPhysicalDeviceVulkan13Features*)pNext)->pNext;
                *((VkPhysicalDeviceVulkan13Features*)pNext) = output_core13_.back();
                output_core13_.pop_back();
            }
        }
        pNext = ((void*)(((VkBaseInStructure*)pNext)->pNext));
    }

    // TODO: find a way around having this variables here. The reason they are here is because if we assign the fields
    // in pCreateInfo to point to them they need to be alive until the EncodeStructPtr call.
    std::vector<const char*> extensions;
    std::vector<std::string> copy_of_instance_data;

    if (pCreateInfoDec->enabledExtensionCount)
    {
        if (!IsModificationPass())
        {
            std::vector<std::string> extensions_vector(pCreateInfoDec->ppEnabledExtensionNames,
                                                       pCreateInfoDec->ppEnabledExtensionNames +
                                                           pCreateInfoDec->enabledExtensionCount);
            capture_device_extensions_vector_.push_back(extensions_vector);
        }
        else
        {
            // TODO: Remove this copy_of_instance_data. This is currently required since the pointers need to be
            // available until encoding, but we don't know if we can pop the original vector at that time
            copy_of_instance_data = output_device_extensions_vector_.back();
            output_device_extensions_vector_.pop_back();

            assert(pCreateInfoDec->enabledExtensionCount >= copy_of_instance_data.size());

            for (uint32_t i = 0; i < copy_of_instance_data.size(); i++)
            {
                extensions.push_back(copy_of_instance_data[i].c_str());
            }

            pCreateInfoDec->ppEnabledExtensionNames = extensions.data();
            pCreateInfoDec->enabledExtensionCount   = extensions.size();
        }
    }

    if (IsModificationPass())
    {
        parameter_buffer_->Clear();

        gfxrecon::encode::ParameterEncoder encoder(parameter_buffer_);
        encoder.EncodeHandleIdValue(physicalDevice);
        EncodeStructPtr(&encoder, pCreateInfo->GetPointer());
        EncodeStructPtr(&encoder, pAllocator->GetPointer());
        encoder.EncodeHandleIdPtr(pDevice->GetPointer());
        encoder.EncodeEnumValue(returnValue);
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdBeginRendering(
    const ApiCallInfo&                             call_info,
    format::HandleId                               commandBuffer,
    StructPointerDecoder<Decoded_VkRenderingInfo>* pRenderingInfo)
{
    core13_.dynamicRendering = true;
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdBeginRenderingKHR(
    const ApiCallInfo&                             call_info,
    format::HandleId                               commandBuffer,
    StructPointerDecoder<Decoded_VkRenderingInfo>* pRenderingInfo)
{
    core13_.dynamicRendering = true;
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateBuffer(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    format::HandleId                                     device,
    StructPointerDecoder<Decoded_VkBufferCreateInfo>*    pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkBuffer>*                      pBuffer)
{
    auto pCreateInfoDec = pCreateInfo->GetMetaStructPointer()->decoded_value;

    if (pCreateInfoDec->flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)
    {
        core10_.sparseResidencyAliased = true;
    }
    if (pCreateInfoDec->flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)
    {
        core10_.sparseBinding = true;
    }
    if (pCreateInfoDec->flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT)
    {
        core10_.sparseResidencyBuffer = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateImage(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    format::HandleId                                     device,
    StructPointerDecoder<Decoded_VkImageCreateInfo>*     pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkImage>*                       pImage)
{
    auto pCreateInfoDec = pCreateInfo->GetMetaStructPointer()->decoded_value;

    if ((pCreateInfoDec->usage & VK_IMAGE_USAGE_STORAGE_BIT) && (pCreateInfoDec->samples != VK_SAMPLE_COUNT_1_BIT))
    {
        core10_.shaderStorageImageMultisample = true;
    }
    if (pCreateInfoDec->flags & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)
    {
        core10_.sparseResidencyAliased = true;
    }
    if (pCreateInfoDec->flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)
    {
        core10_.sparseBinding = true;
    }
    if (pCreateInfoDec->flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT && pCreateInfoDec->imageType == VK_IMAGE_TYPE_2D)
    {
        if (pCreateInfoDec->samples == VK_SAMPLE_COUNT_1_BIT)
        {
            core10_.sparseResidencyImage2D = true;
        }
        if (pCreateInfoDec->samples == VK_SAMPLE_COUNT_2_BIT)
        {
            core10_.sparseResidency2Samples = true;
        }
        if (pCreateInfoDec->samples == VK_SAMPLE_COUNT_4_BIT)
        {
            core10_.sparseResidency4Samples = true;
        }
        if (pCreateInfoDec->samples == VK_SAMPLE_COUNT_8_BIT)
        {
            core10_.sparseResidency8Samples = true;
        }
        if (pCreateInfoDec->samples == VK_SAMPLE_COUNT_16_BIT)
        {
            core10_.sparseResidency16Samples = true;
        }
    }
    if (pCreateInfoDec->flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT && pCreateInfoDec->imageType == VK_IMAGE_TYPE_3D)
    {
        core10_.sparseResidencyImage3D = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateImageView(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    format::HandleId                                     device,
    StructPointerDecoder<Decoded_VkImageViewCreateInfo>* pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkImageView>*                   pView)
{
    auto pCreateInfoDec = pCreateInfo->GetMetaStructPointer()->decoded_value;
    if (pCreateInfoDec->viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
    {
        core10_.imageCubeArray = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateGraphicsPipelines(
    const ApiCallInfo&                                          call_info,
    VkResult                                                    returnValue,
    format::HandleId                                            device,
    format::HandleId                                            pipelineCache,
    uint32_t                                                    createInfoCount,
    StructPointerDecoder<Decoded_VkGraphicsPipelineCreateInfo>* pCreateInfos,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>*        pAllocator,
    HandlePointerDecoder<VkPipeline>*                           pPipelines)
{
    VkGraphicsPipelineCreateInfo* pCreateInfosDec = pCreateInfos->GetMetaStructPointer()->decoded_value;

    for (uint32_t i = 0; i < createInfoCount; i++)
    {
        if (pCreateInfosDec[i].pMultisampleState != nullptr &&
            pCreateInfosDec[i].pMultisampleState->sampleShadingEnable == VK_TRUE)
        {
            core10_.sampleRateShading = true;
        }

        for (uint32_t j = 0; j < pCreateInfosDec[i].stageCount; j++)
        {
            if (pCreateInfosDec[i].pStages[j].stage == VK_SHADER_STAGE_GEOMETRY_BIT)
            {
                core10_.geometryShader = true;
            }
            else if (pCreateInfosDec[i].pStages[j].stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
                     pCreateInfosDec[i].pStages[j].stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
            {
                core10_.tessellationShader = true;
            }
        }
    }

    for (uint32_t i = 0; i < createInfoCount; i++)
    {
        if (pCreateInfosDec[i].pColorBlendState == nullptr)
        {
            continue;
        }

        auto     pColorBlendState = pCreateInfosDec[i].pColorBlendState;
        auto     pAttachments     = pColorBlendState->pAttachments;
        uint32_t attachmentCount  = pColorBlendState->attachmentCount;
        for (uint32_t j = 0; j < attachmentCount; j++)
        {
            const VkBlendFactor factors[4] = { VK_BLEND_FACTOR_SRC1_COLOR,
                                               VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
                                               VK_BLEND_FACTOR_SRC1_ALPHA,
                                               VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA };
            for (uint32_t k = 0; k < 4; k++)
            {
                if (pAttachments->srcColorBlendFactor == factors[k])
                {
                    core10_.dualSrcBlend = true;
                }
                if (pAttachments->dstColorBlendFactor == factors[k])
                {
                    core10_.dualSrcBlend = true;
                }
                if (pAttachments->srcAlphaBlendFactor == factors[k])
                {
                    core10_.dualSrcBlend = true;
                }
                if (pAttachments->dstAlphaBlendFactor == factors[k])
                {
                    core10_.dualSrcBlend = true;
                }
            }
        }
    }

    for (uint32_t i = 0; i < createInfoCount; i++)
    {
        if (pCreateInfosDec[i].pColorBlendState != nullptr &&
            pCreateInfosDec[i].pColorBlendState->logicOpEnable == VK_TRUE)
        {
            core10_.logicOp = true;
        }
        if (pCreateInfosDec[i].pViewportState != nullptr && (pCreateInfosDec[i].pViewportState->viewportCount > 1 ||
                                                             pCreateInfosDec[i].pViewportState->scissorCount > 1))
        {
            core10_.multiViewport = true;
        }
        if (pCreateInfosDec[i].pMultisampleState != nullptr &&
            pCreateInfosDec[i].pMultisampleState->alphaToOneEnable == VK_TRUE)
        {
            core10_.alphaToOne = true;
        }
        if (pCreateInfosDec[i].pDepthStencilState != nullptr &&
            pCreateInfosDec[i].pDepthStencilState->depthBoundsTestEnable == VK_TRUE)
        {
            core10_.depthBounds = true;
        }
        if (pCreateInfosDec[i].pRasterizationState != nullptr &&
            (pCreateInfosDec[i].pRasterizationState->polygonMode == VK_POLYGON_MODE_POINT ||
             pCreateInfosDec[i].pRasterizationState->polygonMode == VK_POLYGON_MODE_LINE))
        {
            core10_.fillModeNonSolid = true;
        }
        if (pCreateInfosDec[i].pRasterizationState != nullptr &&
            pCreateInfosDec[i].pRasterizationState->depthClampEnable == VK_TRUE)
        {
            core10_.depthClamp = true;
        }
    }

    for (uint32_t i = 0; i < createInfoCount; i++)
    {
        if (pCreateInfosDec[i].pColorBlendState == nullptr)
        {
            continue;
        }

        auto     pAttachments    = pCreateInfosDec[i].pColorBlendState->pAttachments;
        uint32_t attachmentCount = pCreateInfosDec[i].pColorBlendState->attachmentCount;

        if (attachmentCount == 1)
        {
            // can't compare if they are the same if there's only one
            break;
        }
        for (uint32_t j = 1; j < attachmentCount; j++)
        {
            if (pAttachments[j].blendEnable != pAttachments[j - 1].blendEnable ||
                pAttachments[j].srcColorBlendFactor != pAttachments[j - 1].srcColorBlendFactor ||
                pAttachments[j].dstColorBlendFactor != pAttachments[j - 1].dstColorBlendFactor ||
                pAttachments[j].colorBlendOp != pAttachments[j - 1].colorBlendOp ||
                pAttachments[j].srcAlphaBlendFactor != pAttachments[j - 1].srcAlphaBlendFactor ||
                pAttachments[j].dstAlphaBlendFactor != pAttachments[j - 1].dstAlphaBlendFactor ||
                pAttachments[j].alphaBlendOp != pAttachments[j - 1].alphaBlendOp ||
                pAttachments[j].colorWriteMask != pAttachments[j - 1].colorWriteMask)
            {
                core10_.independentBlend = true;
            }
        }
    }

    if (pCreateInfosDec->pViewportState != nullptr)
    {
        void* pNext = const_cast<void*>(pCreateInfosDec->pViewportState->pNext);
        while (pNext != nullptr)
        {
            if (((VkBaseInStructure*)pNext)->sType ==
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV)
            {
                auto exclusiveScissorCount =
                    ((VkPipelineViewportExclusiveScissorStateCreateInfoNV*)pNext)->exclusiveScissorCount;
                if ((exclusiveScissorCount != 0) && (exclusiveScissorCount != 1))
                {
                    core10_.multiViewport = true;
                }
            }
            pNext = ((void*)(((VkBaseInStructure*)pNext)->pNext));
        }
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdDrawIndirect(const ApiCallInfo& call_info,
                                                                 format::HandleId   commandBuffer,
                                                                 format::HandleId   buffer,
                                                                 VkDeviceSize       offset,
                                                                 uint32_t           drawCount,
                                                                 uint32_t           stride)
{
    if (drawCount != 0 && drawCount != 1)
    {
        core10_.multiDrawIndirect = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdDrawIndirectCount(const ApiCallInfo& call_info,
                                                                      format::HandleId   commandBuffer,
                                                                      format::HandleId   buffer,
                                                                      VkDeviceSize       offset,
                                                                      format::HandleId   countBuffer,
                                                                      VkDeviceSize       countBufferOffset,
                                                                      uint32_t           maxDrawCount,
                                                                      uint32_t           stride)
{
    core12_.drawIndirectCount = true;
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdDrawIndexedIndirect(const ApiCallInfo& call_info,
                                                                        format::HandleId   commandBuffer,
                                                                        format::HandleId   buffer,
                                                                        VkDeviceSize       offset,
                                                                        uint32_t           drawCount,
                                                                        uint32_t           stride)
{
    if (drawCount != 0 && drawCount != 1)
    {
        core10_.multiDrawIndirect = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdDrawIndexedIndirectCount(const ApiCallInfo& call_info,
                                                                             format::HandleId   commandBuffer,
                                                                             format::HandleId   buffer,
                                                                             VkDeviceSize       offset,
                                                                             format::HandleId   countBuffer,
                                                                             VkDeviceSize       countBufferOffset,
                                                                             uint32_t           maxDrawCount,
                                                                             uint32_t           stride)
{
    core12_.drawIndirectCount = true;
}

void VulkanFeatureTrackerConsumerBase::Process_vkBeginCommandBuffer(
    const ApiCallInfo&                                      call_info,
    VkResult                                                returnValue,
    format::HandleId                                        commandBuffer,
    StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo)
{
    if (pBeginInfo->GetMetaStructPointer() == nullptr ||
        pBeginInfo->GetMetaStructPointer()->decoded_value->pInheritanceInfo == nullptr)
    {
        return;
    }

    auto pInheritanceInfoDec = pBeginInfo->GetMetaStructPointer()->decoded_value->pInheritanceInfo;

    // Potential TODO, bitwise OR new VkQueryControlFlagBits values (currently only VK_QUERY_CONTROL_PRECISE_BIT exists)
    if (pInheritanceInfoDec->occlusionQueryEnable != VK_FALSE ||
        ((pInheritanceInfoDec->queryFlags & ~(VK_QUERY_CONTROL_PRECISE_BIT)) == 0))
    {
        core10_.inheritedQueries = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdSetPolygonModeEXT(const ApiCallInfo& call_info,
                                                                      format::HandleId   commandBuffer,
                                                                      VkPolygonMode      polygonMode)
{
    if (polygonMode == VK_POLYGON_MODE_POINT || polygonMode == VK_POLYGON_MODE_LINE)
    {
        core10_.fillModeNonSolid = true;
    }
}
void VulkanFeatureTrackerConsumerBase::Process_vkCmdSetViewport(const ApiCallInfo&                        call_info,
                                                                format::HandleId                          commandBuffer,
                                                                uint32_t                                  firstViewport,
                                                                uint32_t                                  viewportCount,
                                                                StructPointerDecoder<Decoded_VkViewport>* pViewports)
{
    if (firstViewport != 0 || viewportCount != 1)
    {
        core10_.multiViewport = true;
    }
}
void VulkanFeatureTrackerConsumerBase::Process_vkCmdSetScissor(const ApiCallInfo&                      call_info,
                                                               format::HandleId                        commandBuffer,
                                                               uint32_t                                firstScissor,
                                                               uint32_t                                scissorCount,
                                                               StructPointerDecoder<Decoded_VkRect2D>* pScissors)
{
    if (firstScissor != 0 || scissorCount != 1)
    {
        core10_.multiViewport = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdSetExclusiveScissorNV(
    const ApiCallInfo&                      call_info,
    format::HandleId                        commandBuffer,
    uint32_t                                firstExclusiveScissor,
    uint32_t                                exclusiveScissorCount,
    StructPointerDecoder<Decoded_VkRect2D>* pExclusiveScissors)
{
    if (firstExclusiveScissor != 0 || exclusiveScissorCount != 1)
    {
        core10_.multiViewport = true;
    }
}
void VulkanFeatureTrackerConsumerBase::Process_vkCreateSampler(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    format::HandleId                                     device,
    StructPointerDecoder<Decoded_VkSamplerCreateInfo>*   pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkSampler>*                     pSampler)
{
    auto pCreateInfoDec = pCreateInfo->GetMetaStructPointer()->decoded_value;

    if (pCreateInfoDec->anisotropyEnable == VK_TRUE)
    {
        core10_.samplerAnisotropy = true;
    }

    if (pCreateInfoDec->addressModeU == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE ||
        pCreateInfoDec->addressModeV == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE ||
        pCreateInfoDec->addressModeW == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE)
    {
        core12_.samplerMirrorClampToEdge                                                     = true;
        supported_device_extensions_map_[VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME] = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdWriteAccelerationStructuresPropertiesNV(
    const ApiCallInfo&                               call_info,
    format::HandleId                                 commandBuffer,
    uint32_t                                         accelerationStructureCount,
    HandlePointerDecoder<VkAccelerationStructureNV>* pAccelerationStructures,
    VkQueryType                                      queryType,
    format::HandleId                                 queryPool,
    uint32_t                                         firstQuery)
{
    if (queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS)
    {
        core10_.pipelineStatisticsQuery = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkWriteMicromapsPropertiesEXT(
    const ApiCallInfo&                   call_info,
    VkResult                             returnValue,
    format::HandleId                     device,
    uint32_t                             micromapCount,
    HandlePointerDecoder<VkMicromapEXT>* pMicromaps,
    VkQueryType                          queryType,
    size_t                               dataSize,
    PointerDecoder<uint8_t>*             pData,
    size_t                               stride)
{
    if (queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS)
    {
        core10_.pipelineStatisticsQuery = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdWriteMicromapsPropertiesEXT(
    const ApiCallInfo&                   call_info,
    format::HandleId                     commandBuffer,
    uint32_t                             micromapCount,
    HandlePointerDecoder<VkMicromapEXT>* pMicromaps,
    VkQueryType                          queryType,
    format::HandleId                     queryPool,
    uint32_t                             firstQuery)
{
    if (queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS)
    {
        core10_.pipelineStatisticsQuery = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkWriteAccelerationStructuresPropertiesKHR(
    const ApiCallInfo&                                call_info,
    VkResult                                          returnValue,
    format::HandleId                                  device,
    uint32_t                                          accelerationStructureCount,
    HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructures,
    VkQueryType                                       queryType,
    size_t                                            dataSize,
    PointerDecoder<uint8_t>*                          pData,
    size_t                                            stride)
{
    if (queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS)
    {
        core10_.pipelineStatisticsQuery = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkCmdWriteAccelerationStructuresPropertiesKHR(
    const ApiCallInfo&                                call_info,
    format::HandleId                                  commandBuffer,
    uint32_t                                          accelerationStructureCount,
    HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructures,
    VkQueryType                                       queryType,
    format::HandleId                                  queryPool,
    uint32_t                                          firstQuery)
{
    if (queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS)
    {
        core10_.pipelineStatisticsQuery = true;
    }
}

void VulkanFeatureTrackerConsumerBase::Process_vkResetQueryPool(const ApiCallInfo& call_info,
                                                                format::HandleId   device,
                                                                format::HandleId   queryPool,
                                                                uint32_t           firstQuery,
                                                                uint32_t           queryCount)
{
    core12_.hostQueryReset = true;
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateSwapchainKHR(
    const ApiCallInfo&                                      call_info,
    VkResult                                                returnValue,
    format::HandleId                                        device,
    StructPointerDecoder<Decoded_VkSwapchainCreateInfoKHR>* pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>*    pAllocator,
    HandlePointerDecoder<VkSwapchainKHR>*                   pSwapchain)
{
    auto pCreateInfoDec = pCreateInfo->GetMetaStructPointer()->decoded_value;
    checkSwapchainColorspaceEXT(pCreateInfoDec->imageColorSpace);
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateSharedSwapchainsKHR(
    const ApiCallInfo&                                      call_info,
    VkResult                                                returnValue,
    format::HandleId                                        device,
    uint32_t                                                swapchainCount,
    StructPointerDecoder<Decoded_VkSwapchainCreateInfoKHR>* pCreateInfos,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>*    pAllocator,
    HandlePointerDecoder<VkSwapchainKHR>*                   pSwapchains)
{
    auto pCreateInfosDec = pCreateInfos->GetMetaStructPointer()->decoded_value;
    for (uint32_t i = 0; i < swapchainCount; i++)
    {
        checkSwapchainColorspaceEXT(pCreateInfosDec[i].imageColorSpace);
    }
}

void VulkanFeatureTrackerConsumerBase::checkSwapchainColorspaceEXT(VkColorSpaceKHR s)
{
    if (s == VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT || s == VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT ||
        s == VK_COLOR_SPACE_BT2020_LINEAR_EXT || s == VK_COLOR_SPACE_BT709_LINEAR_EXT ||
        s == VK_COLOR_SPACE_BT709_NONLINEAR_EXT || s == VK_COLOR_SPACE_DCI_P3_LINEAR_EXT ||
        s == VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT || s == VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT ||
        s == VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT || s == VK_COLOR_SPACE_DOLBYVISION_EXT ||
        s == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT || s == VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT ||
        s == VK_COLOR_SPACE_HDR10_HLG_EXT || s == VK_COLOR_SPACE_HDR10_ST2084_EXT ||
        s == VK_COLOR_SPACE_PASS_THROUGH_EXT)
    {
        supported_instance_extensions_map_[VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME] = true;
    }
}

bool VulkanFeatureTrackerConsumerBase::CanOptimize()
{
    bool result = false;

    bool core10_detected_unused_feature = ProcessCore10Features();
    bool core11_detected_unused_feature = ProcessCore11Features();
    bool core12_detected_unused_feature = ProcessCore12Features();
    bool core13_detected_unused_feature = ProcessCore13Features();

    bool detected_device_unused_extensions   = ProcessDeviceExtensions();
    bool detected_instance_unused_extensions = ProcessInstanceExtensions();

    result = core10_detected_unused_feature || core11_detected_unused_feature || core12_detected_unused_feature ||
             core13_detected_unused_feature || detected_device_unused_extensions || detected_instance_unused_extensions;

    if (result)
    {
        printf("%s", consumer_output_log_.c_str());
        consumer_output_log_.clear();
    }

    return result;
}

bool VulkanFeatureTrackerConsumerBase::ProcessInstanceExtensions()
{
    // TODO: improve/rewrite logging
    bool detected_unused_extension_global = false;
    bool detected_unused_extension_local  = false;

    std::string output_log{};

    for (uint64_t i = 0; i < capture_instance_extensions_vector_.size(); i++)
    {
        detected_unused_extension_local = false;
        output_log += "\tFrom vkCreateInstance encounter number " + std::to_string(i + 1) + ":\r\n";

        output_instance_extensions_vector_.push_back(capture_instance_extensions_vector_[i]);

        for (auto it = supported_instance_extensions_map_.begin(); it != supported_instance_extensions_map_.end(); it++)
        {
            std::string extension_name = it->first;
            VkBool32    is_used        = it->second;

            auto it2 = std::find(output_instance_extensions_vector_[i].begin(),
                                 output_instance_extensions_vector_[i].end(),
                                 extension_name);

            if (!is_used && (it2 != output_instance_extensions_vector_[i].end()))
            {
                detected_unused_extension_global = true;
                detected_unused_extension_local  = true;
                output_instance_extensions_vector_[i].erase(it2);
                output_log += "\t\t" + extension_name + "\r\n";
            }
        }

        if (!detected_unused_extension_local)
        {
            output_log += "\t\tNone\r\n";
        }
    }
    if (detected_unused_extension_global)
    {
        consumer_output_log_ += "Instance Extensions to be removed:\r\n" + output_log;
    }

    std::reverse(output_instance_extensions_vector_.begin(), output_instance_extensions_vector_.end());

    return detected_unused_extension_global;
}

bool VulkanFeatureTrackerConsumerBase::ProcessDeviceExtensions()
{
    // TODO: improve/rewrite logging
    bool detected_unused_extension_global = false;
    bool detected_unused_extension_local  = false;

    std::string output_log{};

    for (uint64_t i = 0; i < capture_device_extensions_vector_.size(); i++)
    {
        detected_unused_extension_local = false;
        output_log += "\tFrom vkCreateDevice encounter number " + std::to_string(i + 1) + ":\r\n";

        output_device_extensions_vector_.push_back(capture_device_extensions_vector_[i]);

        for (auto it = supported_device_extensions_map_.begin(); it != supported_device_extensions_map_.end(); it++)
        {
            std::string extension_name = it->first;
            VkBool32    is_used        = it->second;

            auto it2 = std::find(
                output_device_extensions_vector_[i].begin(), output_device_extensions_vector_[i].end(), extension_name);

            if (!is_used && (it2 != output_device_extensions_vector_[i].end()))
            {
                detected_unused_extension_global = true;
                detected_unused_extension_local  = true;
                output_device_extensions_vector_[i].erase(it2);
                output_log += "\t\t" + extension_name + "\r\n";
            }
        }

        if (!detected_unused_extension_local)
        {
            output_log += "\t\tNone\r\n";
        }
    }

    if (detected_unused_extension_global)
    {
        consumer_output_log_ += "Device Extensions to be removed:\r\n" + output_log;
    }

    std::reverse(output_device_extensions_vector_.begin(), output_device_extensions_vector_.end());

    return detected_unused_extension_global;
}

bool VulkanFeatureTrackerConsumerBase::ProcessCore10Features()
{
    // TODO: improve/rewrite logging
    bool detected_unused_feature_global = false;
    bool detected_unused_feature_local  = false;

    std::string output_log{};

    for (uint64_t i = 0; i < capture_core10_.size(); i++)
    {
        detected_unused_feature_local = false;
        output_log += "\tFrom vkCreateDevice encounter number " + std::to_string(i + 1) + ":\r\n";

        output_core10_.push_back(capture_core10_[i]);

        // iterate through struct members, this is a workaround until this function will be generated
        VkBool32* p_output_core10   = (VkBool32*)(&(output_core10_[i]));
        VkBool32* p_core10          = (VkBool32*)(&core10_);
        VkBool32* p_capture_core10_ = (VkBool32*)(&(capture_core10_[i]));

        for (uint64_t j = 0; j < core10_members_as_strings_.size(); j++)
        {
            *p_output_core10 = (*p_core10) & (*p_capture_core10_);
            if ((*p_output_core10) != (*p_capture_core10_))
            {
                output_log += "\t\t" + core10_members_as_strings_[j] + "\r\n";
                detected_unused_feature_global = true;
                detected_unused_feature_local  = true;
            }
            p_output_core10++;
            p_core10++;
            p_capture_core10_++;
        }

        if (!detected_unused_feature_local)
        {
            output_log += "\t\tNone\r\n";
        }
    }

    if (detected_unused_feature_global)
    {
        consumer_output_log_ += "Core10 Features to be removed:\r\n" + output_log;
    }

    std::reverse(output_core10_.begin(), output_core10_.end());

    return detected_unused_feature_global;
}

bool VulkanFeatureTrackerConsumerBase::ProcessCore11Features()
{
    // TODO: improve/rewrite logging
    bool detected_unused_feature_global = false;
    bool detected_unused_feature_local  = false;

    std::string output_log{};

    for (uint64_t i = 0; i < capture_core11_.size(); i++)
    {
        detected_unused_feature_local = false;
        output_log += "\tFrom vkCreateDevice encounter number " + std::to_string(i + 1) + ":\r\n";

        output_core11_.push_back(capture_core11_[i]);

        // bitwise AND every member except sType and pNext
        // iterate through struct members, this is a workaround until this function will be generated
        VkBool32* p_output_core11   = (VkBool32*)(&(output_core11_[i].storageBuffer16BitAccess));
        VkBool32* p_core11          = (VkBool32*)(&core11_.storageBuffer16BitAccess);
        VkBool32* p_capture_core11_ = (VkBool32*)(&(capture_core11_[i].storageBuffer16BitAccess));

        for (uint64_t j = 0; j < core11_members_as_strings_.size(); j++)
        {
            *p_output_core11 = (*p_core11) & (*p_capture_core11_);
            if ((*p_output_core11) != (*p_capture_core11_))
            {
                output_log += "\t\t" + core11_members_as_strings_[j] + "\r\n";
                detected_unused_feature_global = true;
                detected_unused_feature_local  = true;
            }
            p_output_core11++;
            p_core11++;
            p_capture_core11_++;
        }

        if (!detected_unused_feature_local)
        {
            output_log += "\t\tNone\r\n";
        }
    }

    if (detected_unused_feature_global)
    {
        consumer_output_log_ += "Core11 Features to be removed:\r\n" + output_log;
    }

    std::reverse(output_core11_.begin(), output_core11_.end());

    return detected_unused_feature_global;
}

bool VulkanFeatureTrackerConsumerBase::ProcessCore12Features()
{
    // TODO: improve/rewrite logging
    bool detected_unused_feature_global = false;
    bool detected_unused_feature_local  = false;

    std::string output_log{};

    for (uint64_t i = 0; i < capture_core12_.size(); i++)
    {
        detected_unused_feature_local = false;
        output_log += "\tFrom vkCreateDevice encounter number " + std::to_string(i + 1) + ":\r\n";

        output_core12_.push_back(capture_core12_[i]);

        // bitwise AND every member except sType and pNext
        // iterate through struct members, this is a workaround until this function will be generated
        VkBool32* p_output_core12   = (VkBool32*)(&(output_core12_[i].samplerMirrorClampToEdge));
        VkBool32* p_core12          = (VkBool32*)(&core12_.samplerMirrorClampToEdge);
        VkBool32* p_capture_core12_ = (VkBool32*)(&(capture_core12_[i].samplerMirrorClampToEdge));

        for (uint64_t j = 0; j < core12_members_as_strings_.size(); j++)
        {
            *p_output_core12 = (*p_core12) & (*p_capture_core12_);
            if ((*p_output_core12) != (*p_capture_core12_))
            {
                output_log += "\t\t" + core12_members_as_strings_[j] + "\r\n";
                detected_unused_feature_global = true;
                detected_unused_feature_local  = true;
            }
            p_output_core12++;
            p_core12++;
            p_capture_core12_++;
        }

        if (!detected_unused_feature_local)
        {
            output_log += "\t\tNone\r\n";
        }
    }
    if (detected_unused_feature_global)
    {
        consumer_output_log_ += "Core12 Features to be removed:\r\n" + output_log;
    }

    std::reverse(output_core12_.begin(), output_core12_.end());

    return detected_unused_feature_global;
}

bool VulkanFeatureTrackerConsumerBase::ProcessCore13Features()
{
    // TODO: improve/rewrite logging
    bool detected_unused_feature_global = false;
    bool detected_unused_feature_local  = false;

    std::string output_log{};

    for (uint64_t i = 0; i < capture_core13_.size(); i++)
    {
        detected_unused_feature_local = false;
        output_log += "\tFrom vkCreateDevice encounter number " + std::to_string(i + 1) + ":\r\n";

        output_core13_.push_back(capture_core13_[i]);

        // bitwise AND every member except sType and pNext
        // iterate through struct members, this is a workaround until this function will be generated
        VkBool32* p_output_core13   = (VkBool32*)(&(output_core13_[i].robustImageAccess));
        VkBool32* p_core13          = (VkBool32*)(&core13_.robustImageAccess);
        VkBool32* p_capture_core13_ = (VkBool32*)(&(capture_core13_[i].robustImageAccess));

        for (uint64_t j = 0; j < core13_members_as_strings_.size(); j++)
        {
            *p_output_core13 = (*p_core13) & (*p_capture_core13_);
            if ((*p_output_core13) != (*p_capture_core13_))
            {
                output_log += "\t\t" + core13_members_as_strings_[j] + "\r\n";
                detected_unused_feature_global = true;
                detected_unused_feature_local  = true;
            }
            p_output_core13++;
            p_core13++;
            p_capture_core13_++;
        }

        if (!detected_unused_feature_local)
        {
            output_log += "\t\tNone\r\n";
        }
    }

    if (detected_unused_feature_global)
    {
        consumer_output_log_ += "Core13 Features to be removed:\r\n" + output_log;
    }

    std::reverse(output_core13_.begin(), output_core13_.end());

    return detected_unused_feature_global;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
