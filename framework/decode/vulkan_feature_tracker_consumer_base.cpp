#include "decode/vulkan_feature_tracker_consumer_base.h"

#include "util/logging.h"

#include <cassert>
#include <stdexcept>

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
     * Members initialized with false are handlel
     * TODO: handle features initialized with true
     */
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

    // extensions & alias extensions
    ext_VK_EXT_swapchain_colorspace           = false;
    p_ext_VK_KHR_sampler_mirror_clamp_to_edge = &core12_.samplerMirrorClampToEdge;
}

void VulkanFeatureTrackerConsumerBase::Process_vkCreateInstance(
    const ApiCallInfo&                                   call_info,
    VkResult                                             returnValue,
    StructPointerDecoder<Decoded_VkInstanceCreateInfo>*  pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
    HandlePointerDecoder<VkInstance>*                    pInstance)
{
    if (!capture_mode_)
    {
        auto pCreateInfoDec = pCreateInfo->GetMetaStructPointer()->decoded_value;

        std::vector<std::string> extensions_vector(pCreateInfoDec->ppEnabledExtensionNames,
                                                   pCreateInfoDec->ppEnabledExtensionNames +
                                                       pCreateInfoDec->enabledExtensionCount);

        for (std::vector<std::string>::iterator it = extensions_vector.begin(); it != extensions_vector.end(); it++)
        {
            if ((ext_VK_EXT_swapchain_colorspace == false) && (*it == "VK_EXT_swapchain_colorspace"))
            {
                extensions_vector.erase(it);
                pCreateInfoDec->enabledExtensionCount--;
            }
        }

        const char* extensions[pCreateInfoDec->enabledExtensionCount]{};
        for (uint32_t i = 0; i < pCreateInfoDec->enabledExtensionCount; i++)
        {
            extensions[i] = extensions_vector[i].c_str();
        }

        pCreateInfoDec->ppEnabledExtensionNames = extensions;

        GFXRECON_ASSERT(encoding_buffer_ != nullptr);

        gfxrecon::encode::ParameterEncoder encoder(encoding_buffer_);
        EncodeStructPtr(&encoder, pCreateInfo->GetPointer());
        EncodeStructPtr(&encoder, pAllocator->GetPointer());
        encoder.EncodeHandleIdPtr(pInstance->GetPointer());
        encoder.EncodeEnumValue(returnValue);
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
        GFXRECON_WRITE_CONSOLE("Vulkan 1.0");
        if (capture_mode_)
        {
            capture_core10_ = *pEnabledFeatures;
        }
        else
        {
            *((VkPhysicalDeviceFeatures*)pEnabledFeatures) = output_core10_;
        }
    }

    void* pNext = const_cast<void*>(pCreateInfoDec->pNext);
    while (pNext != nullptr)
    {
        if (((VkBaseInStructure*)pNext)->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
        {
            GFXRECON_WRITE_CONSOLE("Vulkan 1.0");
            if (capture_mode_)
            {
                capture_core10_ = ((VkPhysicalDeviceFeatures2*)pNext)->features;
            }
            else
            {
                ((VkPhysicalDeviceFeatures2*)pNext)->features = output_core10_;
            }
        }
        else if (((VkBaseInStructure*)pNext)->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES)
        {
            GFXRECON_WRITE_CONSOLE("Vulkan 1.1");
            if (capture_mode_)
            {
                capture_core11_ = *((VkPhysicalDeviceVulkan11Features*)pNext);
            }
            else
            {
                *((VkPhysicalDeviceVulkan11Features*)pNext) = output_core11_;
            }
        }
        else if (((VkBaseInStructure*)pNext)->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES)
        {
            GFXRECON_WRITE_CONSOLE("Vulkan 1.2");
            if (capture_mode_)
            {
                capture_core12_ = *((VkPhysicalDeviceVulkan12Features*)pNext);
            }
            else
            {
                *((VkPhysicalDeviceVulkan12Features*)pNext) = output_core12_;
            }
        }
        else if (((VkBaseInStructure*)pNext)->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
        {
            GFXRECON_WRITE_CONSOLE("Vulkan 1.3");

            if (capture_mode_)
            {
                capture_core13_ = *((VkPhysicalDeviceVulkan13Features*)pNext);
            }
            else
            {
                *((VkPhysicalDeviceVulkan13Features*)pNext) = output_core13_;
            }
        }
        pNext = ((void*)(((VkBaseInStructure*)pNext)->pNext));
    }

    if (!capture_mode_)
    {
        std::vector<std::string> extensions_vector(pCreateInfoDec->ppEnabledExtensionNames,
                                                   pCreateInfoDec->ppEnabledExtensionNames +
                                                       pCreateInfoDec->enabledExtensionCount);

        for (std::vector<std::string>::iterator it = extensions_vector.begin(); it != extensions_vector.end(); it++)
        {
            if ((*p_ext_VK_KHR_sampler_mirror_clamp_to_edge == false) && (*it == "VK_KHR_sampler_mirror_clamp_to_edge"))
            {
                extensions_vector.erase(it);
                pCreateInfoDec->enabledExtensionCount--;
            }
        }

        const char* extensions[pCreateInfoDec->enabledExtensionCount]{};
        for (uint32_t i = 0; i < pCreateInfoDec->enabledExtensionCount; i++)
        {
            extensions[i] = extensions_vector[i].c_str();
        }

        pCreateInfoDec->ppEnabledExtensionNames = extensions;

        GFXRECON_ASSERT(encoding_buffer_ != nullptr);

        gfxrecon::encode::ParameterEncoder encoder(encoding_buffer_);
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
        core12_.samplerMirrorClampToEdge;
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
        ext_VK_EXT_swapchain_colorspace = true;
    }
}

void VulkanFeatureTrackerConsumerBase::ProcessFeatures()
{
    ProcessCore10Features();
    ProcessCore11Features();
    ProcessCore12Features();
    ProcessCore13Features();
}
void VulkanFeatureTrackerConsumerBase::ProcessCore10Features()
{
    // bitwise AND every member
    output_core10_.robustBufferAccess  = core10_.robustBufferAccess & capture_core10_.robustBufferAccess;
    output_core10_.fullDrawIndexUint32 = core10_.fullDrawIndexUint32 & capture_core10_.fullDrawIndexUint32;
    output_core10_.imageCubeArray      = core10_.imageCubeArray & capture_core10_.imageCubeArray;
    output_core10_.independentBlend    = core10_.independentBlend & capture_core10_.independentBlend;
    output_core10_.geometryShader      = core10_.geometryShader & capture_core10_.geometryShader;
    output_core10_.tessellationShader  = core10_.tessellationShader & capture_core10_.tessellationShader;
    output_core10_.sampleRateShading   = core10_.sampleRateShading & capture_core10_.sampleRateShading;
    output_core10_.dualSrcBlend        = core10_.dualSrcBlend & capture_core10_.dualSrcBlend;
    output_core10_.logicOp             = core10_.logicOp & capture_core10_.logicOp;
    output_core10_.multiDrawIndirect   = core10_.multiDrawIndirect & capture_core10_.multiDrawIndirect;
    output_core10_.drawIndirectFirstInstance =
        core10_.drawIndirectFirstInstance & capture_core10_.drawIndirectFirstInstance;
    output_core10_.depthClamp             = core10_.depthClamp & capture_core10_.depthClamp;
    output_core10_.depthBiasClamp         = core10_.depthBiasClamp & capture_core10_.depthBiasClamp;
    output_core10_.fillModeNonSolid       = core10_.fillModeNonSolid & capture_core10_.fillModeNonSolid;
    output_core10_.depthBounds            = core10_.depthBounds & capture_core10_.depthBounds;
    output_core10_.wideLines              = core10_.wideLines & capture_core10_.wideLines;
    output_core10_.largePoints            = core10_.largePoints & capture_core10_.largePoints;
    output_core10_.alphaToOne             = core10_.alphaToOne & capture_core10_.alphaToOne;
    output_core10_.multiViewport          = core10_.multiViewport & capture_core10_.multiViewport;
    output_core10_.samplerAnisotropy      = core10_.samplerAnisotropy & capture_core10_.samplerAnisotropy;
    output_core10_.textureCompressionETC2 = core10_.textureCompressionETC2 & capture_core10_.textureCompressionETC2;
    output_core10_.textureCompressionASTC_LDR =
        core10_.textureCompressionASTC_LDR & capture_core10_.textureCompressionASTC_LDR;
    output_core10_.textureCompressionBC    = core10_.textureCompressionBC & capture_core10_.textureCompressionBC;
    output_core10_.occlusionQueryPrecise   = core10_.occlusionQueryPrecise & capture_core10_.occlusionQueryPrecise;
    output_core10_.pipelineStatisticsQuery = core10_.pipelineStatisticsQuery & capture_core10_.pipelineStatisticsQuery;
    output_core10_.vertexPipelineStoresAndAtomics =
        core10_.vertexPipelineStoresAndAtomics & capture_core10_.vertexPipelineStoresAndAtomics;
    output_core10_.fragmentStoresAndAtomics =
        core10_.fragmentStoresAndAtomics & capture_core10_.fragmentStoresAndAtomics;
    output_core10_.shaderTessellationAndGeometryPointSize =
        core10_.shaderTessellationAndGeometryPointSize & capture_core10_.shaderTessellationAndGeometryPointSize;
    output_core10_.shaderImageGatherExtended =
        core10_.shaderImageGatherExtended & capture_core10_.shaderImageGatherExtended;
    output_core10_.shaderStorageImageExtendedFormats =
        core10_.shaderStorageImageExtendedFormats & capture_core10_.shaderStorageImageExtendedFormats;
    output_core10_.shaderStorageImageMultisample =
        core10_.shaderStorageImageMultisample & capture_core10_.shaderStorageImageMultisample;
    output_core10_.shaderStorageImageReadWithoutFormat =
        core10_.shaderStorageImageReadWithoutFormat & capture_core10_.shaderStorageImageReadWithoutFormat;
    output_core10_.shaderStorageImageWriteWithoutFormat =
        core10_.shaderStorageImageWriteWithoutFormat & capture_core10_.shaderStorageImageWriteWithoutFormat;
    output_core10_.shaderUniformBufferArrayDynamicIndexing =
        core10_.shaderUniformBufferArrayDynamicIndexing & capture_core10_.shaderUniformBufferArrayDynamicIndexing;
    output_core10_.shaderSampledImageArrayDynamicIndexing =
        core10_.shaderSampledImageArrayDynamicIndexing & capture_core10_.shaderSampledImageArrayDynamicIndexing;
    output_core10_.shaderStorageBufferArrayDynamicIndexing =
        core10_.shaderStorageBufferArrayDynamicIndexing & capture_core10_.shaderStorageBufferArrayDynamicIndexing;
    output_core10_.shaderStorageImageArrayDynamicIndexing =
        core10_.shaderStorageImageArrayDynamicIndexing & capture_core10_.shaderStorageImageArrayDynamicIndexing;
    output_core10_.shaderClipDistance      = core10_.shaderClipDistance & capture_core10_.shaderClipDistance;
    output_core10_.shaderCullDistance      = core10_.shaderCullDistance & capture_core10_.shaderCullDistance;
    output_core10_.shaderFloat64           = core10_.shaderFloat64 & capture_core10_.shaderFloat64;
    output_core10_.shaderInt64             = core10_.shaderInt64 & capture_core10_.shaderInt64;
    output_core10_.shaderInt16             = core10_.shaderInt16 & capture_core10_.shaderInt16;
    output_core10_.shaderResourceResidency = core10_.shaderResourceResidency & capture_core10_.shaderResourceResidency;
    output_core10_.shaderResourceMinLod    = core10_.shaderResourceMinLod & capture_core10_.shaderResourceMinLod;
    output_core10_.variableMultisampleRate = core10_.variableMultisampleRate & capture_core10_.variableMultisampleRate;
    output_core10_.sparseBinding           = core10_.sparseBinding & capture_core10_.sparseBinding;
    output_core10_.sparseResidencyBuffer   = core10_.sparseResidencyBuffer & capture_core10_.sparseResidencyBuffer;
    output_core10_.sparseResidencyImage2D  = core10_.sparseResidencyImage2D & capture_core10_.sparseResidencyImage2D;
    output_core10_.sparseResidencyImage3D  = core10_.sparseResidencyImage3D & capture_core10_.sparseResidencyImage3D;
    output_core10_.sparseResidency2Samples = core10_.sparseResidency2Samples & capture_core10_.sparseResidency2Samples;
    output_core10_.sparseResidency4Samples = core10_.sparseResidency4Samples & capture_core10_.sparseResidency4Samples;
    output_core10_.sparseResidency8Samples = core10_.sparseResidency8Samples & capture_core10_.sparseResidency8Samples;
    output_core10_.sparseResidency16Samples =
        core10_.sparseResidency16Samples & capture_core10_.sparseResidency16Samples;
    output_core10_.sparseResidencyAliased = core10_.sparseResidencyAliased & capture_core10_.sparseResidencyAliased;
    output_core10_.inheritedQueries       = core10_.inheritedQueries & capture_core10_.inheritedQueries;
}

void VulkanFeatureTrackerConsumerBase::ProcessCore11Features()
{
    // bitwise AND every member except sType and pNext
    output_core11_.sType = capture_core11_.sType;
    output_core11_.pNext = capture_core11_.pNext;

    output_core11_.storageBuffer16BitAccess =
        core11_.storageBuffer16BitAccess & capture_core11_.storageBuffer16BitAccess;
    output_core11_.uniformAndStorageBuffer16BitAccess =
        core11_.uniformAndStorageBuffer16BitAccess & capture_core11_.uniformAndStorageBuffer16BitAccess;
    output_core11_.storagePushConstant16   = core11_.storagePushConstant16 & capture_core11_.storagePushConstant16;
    output_core11_.storageInputOutput16    = core11_.storageInputOutput16 & capture_core11_.storageInputOutput16;
    output_core11_.multiview               = core11_.multiview & capture_core11_.multiview;
    output_core11_.multiviewGeometryShader = core11_.multiviewGeometryShader & capture_core11_.multiviewGeometryShader;
    output_core11_.multiviewTessellationShader =
        core11_.multiviewTessellationShader & capture_core11_.multiviewTessellationShader;
    output_core11_.variablePointersStorageBuffer =
        core11_.variablePointersStorageBuffer & capture_core11_.variablePointersStorageBuffer;
    output_core11_.variablePointers       = core11_.variablePointers & capture_core11_.variablePointers;
    output_core11_.protectedMemory        = core11_.protectedMemory & capture_core11_.protectedMemory;
    output_core11_.samplerYcbcrConversion = core11_.samplerYcbcrConversion & capture_core11_.samplerYcbcrConversion;
    output_core11_.shaderDrawParameters   = core11_.shaderDrawParameters & capture_core11_.shaderDrawParameters;
}
void VulkanFeatureTrackerConsumerBase::ProcessCore12Features()
{
    // bitwise AND every member except sType and pNext
    output_core12_.sType = capture_core12_.sType;
    output_core12_.pNext = capture_core12_.pNext;

    output_core12_.samplerMirrorClampToEdge =
        core12_.samplerMirrorClampToEdge & capture_core12_.samplerMirrorClampToEdge;
    output_core12_.drawIndirectCount       = core12_.drawIndirectCount & capture_core12_.drawIndirectCount;
    output_core12_.storageBuffer8BitAccess = core12_.storageBuffer8BitAccess & capture_core12_.storageBuffer8BitAccess;
    output_core12_.uniformAndStorageBuffer8BitAccess =
        core12_.uniformAndStorageBuffer8BitAccess & capture_core12_.uniformAndStorageBuffer8BitAccess;
    output_core12_.storagePushConstant8 = core12_.storagePushConstant8 & capture_core12_.storagePushConstant8;
    output_core12_.shaderBufferInt64Atomics =
        core12_.shaderBufferInt64Atomics & capture_core12_.shaderBufferInt64Atomics;
    output_core12_.shaderSharedInt64Atomics =
        core12_.shaderSharedInt64Atomics & capture_core12_.shaderSharedInt64Atomics;
    output_core12_.shaderFloat16      = core12_.shaderFloat16 & capture_core12_.shaderFloat16;
    output_core12_.shaderInt8         = core12_.shaderInt8 & capture_core12_.shaderInt8;
    output_core12_.descriptorIndexing = core12_.descriptorIndexing & capture_core12_.descriptorIndexing;
    output_core12_.shaderInputAttachmentArrayDynamicIndexing =
        core12_.shaderInputAttachmentArrayDynamicIndexing & capture_core12_.shaderInputAttachmentArrayDynamicIndexing;
    output_core12_.shaderUniformTexelBufferArrayDynamicIndexing =
        core12_.shaderUniformTexelBufferArrayDynamicIndexing &
        capture_core12_.shaderUniformTexelBufferArrayDynamicIndexing;
    output_core12_.shaderStorageTexelBufferArrayDynamicIndexing =
        core12_.shaderStorageTexelBufferArrayDynamicIndexing &
        capture_core12_.shaderStorageTexelBufferArrayDynamicIndexing;
    output_core12_.shaderUniformBufferArrayNonUniformIndexing =
        core12_.shaderUniformBufferArrayNonUniformIndexing & capture_core12_.shaderUniformBufferArrayNonUniformIndexing;
    output_core12_.shaderSampledImageArrayNonUniformIndexing =
        core12_.shaderSampledImageArrayNonUniformIndexing & capture_core12_.shaderSampledImageArrayNonUniformIndexing;
    output_core12_.shaderStorageBufferArrayNonUniformIndexing =
        core12_.shaderStorageBufferArrayNonUniformIndexing & capture_core12_.shaderStorageBufferArrayNonUniformIndexing;
    output_core12_.shaderStorageImageArrayNonUniformIndexing =
        core12_.shaderStorageImageArrayNonUniformIndexing & capture_core12_.shaderStorageImageArrayNonUniformIndexing;
    output_core12_.shaderInputAttachmentArrayNonUniformIndexing =
        core12_.shaderInputAttachmentArrayNonUniformIndexing &
        capture_core12_.shaderInputAttachmentArrayNonUniformIndexing;
    output_core12_.shaderUniformTexelBufferArrayNonUniformIndexing =
        core12_.shaderUniformTexelBufferArrayNonUniformIndexing &
        capture_core12_.shaderUniformTexelBufferArrayNonUniformIndexing;
    output_core12_.shaderStorageTexelBufferArrayNonUniformIndexing =
        core12_.shaderStorageTexelBufferArrayNonUniformIndexing &
        capture_core12_.shaderStorageTexelBufferArrayNonUniformIndexing;
    output_core12_.descriptorBindingUniformBufferUpdateAfterBind =
        core12_.descriptorBindingUniformBufferUpdateAfterBind &
        capture_core12_.descriptorBindingUniformBufferUpdateAfterBind;
    output_core12_.descriptorBindingSampledImageUpdateAfterBind =
        core12_.descriptorBindingSampledImageUpdateAfterBind &
        capture_core12_.descriptorBindingSampledImageUpdateAfterBind;
    output_core12_.descriptorBindingStorageImageUpdateAfterBind =
        core12_.descriptorBindingStorageImageUpdateAfterBind &
        capture_core12_.descriptorBindingStorageImageUpdateAfterBind;
    output_core12_.descriptorBindingStorageBufferUpdateAfterBind =
        core12_.descriptorBindingStorageBufferUpdateAfterBind &
        capture_core12_.descriptorBindingStorageBufferUpdateAfterBind;
    output_core12_.descriptorBindingUniformTexelBufferUpdateAfterBind =
        core12_.descriptorBindingUniformTexelBufferUpdateAfterBind &
        capture_core12_.descriptorBindingUniformTexelBufferUpdateAfterBind;
    output_core12_.descriptorBindingStorageTexelBufferUpdateAfterBind =
        core12_.descriptorBindingStorageTexelBufferUpdateAfterBind &
        capture_core12_.descriptorBindingStorageTexelBufferUpdateAfterBind;
    output_core12_.descriptorBindingUpdateUnusedWhilePending =
        core12_.descriptorBindingUpdateUnusedWhilePending & capture_core12_.descriptorBindingUpdateUnusedWhilePending;
    output_core12_.descriptorBindingPartiallyBound =
        core12_.descriptorBindingPartiallyBound & capture_core12_.descriptorBindingPartiallyBound;
    output_core12_.descriptorBindingVariableDescriptorCount =
        core12_.descriptorBindingVariableDescriptorCount & capture_core12_.descriptorBindingVariableDescriptorCount;
    output_core12_.runtimeDescriptorArray = core12_.runtimeDescriptorArray & capture_core12_.runtimeDescriptorArray;
    output_core12_.samplerFilterMinmax    = core12_.samplerFilterMinmax & capture_core12_.samplerFilterMinmax;
    output_core12_.scalarBlockLayout      = core12_.scalarBlockLayout & capture_core12_.scalarBlockLayout;
    output_core12_.imagelessFramebuffer   = core12_.imagelessFramebuffer & capture_core12_.imagelessFramebuffer;
    output_core12_.uniformBufferStandardLayout =
        core12_.uniformBufferStandardLayout & capture_core12_.uniformBufferStandardLayout;
    output_core12_.shaderSubgroupExtendedTypes =
        core12_.shaderSubgroupExtendedTypes & capture_core12_.shaderSubgroupExtendedTypes;
    output_core12_.separateDepthStencilLayouts =
        core12_.separateDepthStencilLayouts & capture_core12_.separateDepthStencilLayouts;
    output_core12_.hostQueryReset      = core12_.hostQueryReset & capture_core12_.hostQueryReset;
    output_core12_.timelineSemaphore   = core12_.timelineSemaphore & capture_core12_.timelineSemaphore;
    output_core12_.bufferDeviceAddress = core12_.bufferDeviceAddress & capture_core12_.bufferDeviceAddress;
    output_core12_.bufferDeviceAddressCaptureReplay =
        core12_.bufferDeviceAddressCaptureReplay & capture_core12_.bufferDeviceAddressCaptureReplay;
    output_core12_.bufferDeviceAddressMultiDevice =
        core12_.bufferDeviceAddressMultiDevice & capture_core12_.bufferDeviceAddressMultiDevice;
    output_core12_.vulkanMemoryModel = core12_.vulkanMemoryModel & capture_core12_.vulkanMemoryModel;
    output_core12_.vulkanMemoryModelDeviceScope =
        core12_.vulkanMemoryModelDeviceScope & capture_core12_.vulkanMemoryModelDeviceScope;
    output_core12_.vulkanMemoryModelAvailabilityVisibilityChains =
        core12_.vulkanMemoryModelAvailabilityVisibilityChains &
        capture_core12_.vulkanMemoryModelAvailabilityVisibilityChains;
    output_core12_.shaderOutputViewportIndex =
        core12_.shaderOutputViewportIndex & capture_core12_.shaderOutputViewportIndex;
    output_core12_.shaderOutputLayer = core12_.shaderOutputLayer & capture_core12_.shaderOutputLayer;
    output_core12_.subgroupBroadcastDynamicId =
        core12_.subgroupBroadcastDynamicId & capture_core12_.subgroupBroadcastDynamicId;
}
void VulkanFeatureTrackerConsumerBase::ProcessCore13Features()
{
    // bitwise AND every member except sType and pNext
    output_core13_.sType = capture_core13_.sType;
    output_core13_.pNext = capture_core13_.pNext;

    output_core13_.robustImageAccess  = core13_.robustImageAccess & capture_core13_.robustImageAccess;
    output_core13_.inlineUniformBlock = core13_.inlineUniformBlock & capture_core13_.inlineUniformBlock;
    output_core13_.descriptorBindingInlineUniformBlockUpdateAfterBind =
        core13_.descriptorBindingInlineUniformBlockUpdateAfterBind &
        capture_core13_.descriptorBindingInlineUniformBlockUpdateAfterBind;
    output_core13_.pipelineCreationCacheControl =
        core13_.pipelineCreationCacheControl & capture_core13_.pipelineCreationCacheControl;
    output_core13_.privateData = core13_.privateData & capture_core13_.privateData;
    output_core13_.shaderDemoteToHelperInvocation =
        core13_.shaderDemoteToHelperInvocation & capture_core13_.shaderDemoteToHelperInvocation;
    output_core13_.shaderTerminateInvocation =
        core13_.shaderTerminateInvocation & capture_core13_.shaderTerminateInvocation;
    output_core13_.subgroupSizeControl  = core13_.subgroupSizeControl & capture_core13_.subgroupSizeControl;
    output_core13_.computeFullSubgroups = core13_.computeFullSubgroups & capture_core13_.computeFullSubgroups;
    output_core13_.synchronization2     = core13_.synchronization2 & capture_core13_.synchronization2;
    output_core13_.textureCompressionASTC_HDR =
        core13_.textureCompressionASTC_HDR & capture_core13_.textureCompressionASTC_HDR;
    output_core13_.shaderZeroInitializeWorkgroupMemory =
        core13_.shaderZeroInitializeWorkgroupMemory & capture_core13_.shaderZeroInitializeWorkgroupMemory;
    output_core13_.dynamicRendering        = core13_.dynamicRendering & capture_core13_.dynamicRendering;
    output_core13_.shaderIntegerDotProduct = core13_.shaderIntegerDotProduct & capture_core13_.shaderIntegerDotProduct;
    output_core13_.maintenance4            = core13_.maintenance4 & capture_core13_.maintenance4;
}
void VulkanFeatureTrackerConsumerBase::PrintCore10Features(VkPhysicalDeviceFeatures core10)
{
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("\tCore10");
    GFXRECON_WRITE_CONSOLE("%d robustBufferAccess", core10.robustBufferAccess);
    GFXRECON_WRITE_CONSOLE("%d fullDrawIndexUint32", core10.fullDrawIndexUint32);
    GFXRECON_WRITE_CONSOLE("%d imageCubeArray", core10.imageCubeArray);
    GFXRECON_WRITE_CONSOLE("%d independentBlend", core10.independentBlend);
    GFXRECON_WRITE_CONSOLE("%d geometryShader", core10.geometryShader);
    GFXRECON_WRITE_CONSOLE("%d tessellationShader", core10.tessellationShader);
    GFXRECON_WRITE_CONSOLE("%d sampleRateShading", core10.sampleRateShading);
    GFXRECON_WRITE_CONSOLE("%d dualSrcBlend", core10.dualSrcBlend);
    GFXRECON_WRITE_CONSOLE("%d logicOp", core10.logicOp);
    GFXRECON_WRITE_CONSOLE("%d multiDrawIndirect", core10.multiDrawIndirect);
    GFXRECON_WRITE_CONSOLE("%d drawIndirectFirstInstance", core10.drawIndirectFirstInstance);
    GFXRECON_WRITE_CONSOLE("%d depthClamp", core10.depthClamp);
    GFXRECON_WRITE_CONSOLE("%d depthBiasClamp", core10.depthBiasClamp);
    GFXRECON_WRITE_CONSOLE("%d fillModeNonSolid", core10.fillModeNonSolid);
    GFXRECON_WRITE_CONSOLE("%d depthBounds", core10.depthBounds);
    GFXRECON_WRITE_CONSOLE("%d wideLines", core10.wideLines);
    GFXRECON_WRITE_CONSOLE("%d largePoints", core10.largePoints);
    GFXRECON_WRITE_CONSOLE("%d alphaToOne", core10.alphaToOne);
    GFXRECON_WRITE_CONSOLE("%d multiViewport", core10.multiViewport);
    GFXRECON_WRITE_CONSOLE("%d samplerAnisotropy", core10.samplerAnisotropy);
    GFXRECON_WRITE_CONSOLE("%d textureCompressionETC2", core10.textureCompressionETC2);
    GFXRECON_WRITE_CONSOLE("%d textureCompressionASTC_LDR", core10.textureCompressionASTC_LDR);
    GFXRECON_WRITE_CONSOLE("%d textureCompressionBC", core10.textureCompressionBC);
    GFXRECON_WRITE_CONSOLE("%d occlusionQueryPrecise", core10.occlusionQueryPrecise);
    GFXRECON_WRITE_CONSOLE("%d pipelineStatisticsQuery", core10.pipelineStatisticsQuery);
    GFXRECON_WRITE_CONSOLE("%d vertexPipelineStoresAndAtomics", core10.vertexPipelineStoresAndAtomics);
    GFXRECON_WRITE_CONSOLE("%d fragmentStoresAndAtomics", core10.fragmentStoresAndAtomics);
    GFXRECON_WRITE_CONSOLE("%d shaderTessellationAndGeometryPointSize", core10.shaderTessellationAndGeometryPointSize);
    GFXRECON_WRITE_CONSOLE("%d shaderImageGatherExtended", core10.shaderImageGatherExtended);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageImageExtendedFormats", core10.shaderStorageImageExtendedFormats);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageImageMultisample", core10.shaderStorageImageMultisample);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageImageReadWithoutFormat", core10.shaderStorageImageReadWithoutFormat);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageImageWriteWithoutFormat", core10.shaderStorageImageWriteWithoutFormat);
    GFXRECON_WRITE_CONSOLE("%d shaderUniformBufferArrayDynamicIndexing ",
                           core10.shaderUniformBufferArrayDynamicIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderSampledImageArrayDynamicIndexing", core10.shaderSampledImageArrayDynamicIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageBufferArrayDynamicIndexing ",
                           core10.shaderStorageBufferArrayDynamicIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageImageArrayDynamicIndexing", core10.shaderStorageImageArrayDynamicIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderClipDistance", core10.shaderClipDistance);
    GFXRECON_WRITE_CONSOLE("%d shaderCullDistance", core10.shaderCullDistance);
    GFXRECON_WRITE_CONSOLE("%d shaderFloat64", core10.shaderFloat64);
    GFXRECON_WRITE_CONSOLE("%d shaderInt64", core10.shaderInt64);
    GFXRECON_WRITE_CONSOLE("%d shaderInt16", core10.shaderInt16);
    GFXRECON_WRITE_CONSOLE("%d shaderResourceResidency", core10.shaderResourceResidency);
    GFXRECON_WRITE_CONSOLE("%d shaderResourceMinLod", core10.shaderResourceMinLod);
    GFXRECON_WRITE_CONSOLE("%d sparseBinding", core10.sparseBinding);
    GFXRECON_WRITE_CONSOLE("%d sparseResidencyBuffer", core10.sparseResidencyBuffer);
    GFXRECON_WRITE_CONSOLE("%d sparseResidencyImage2D", core10.sparseResidencyImage2D);
    GFXRECON_WRITE_CONSOLE("%d sparseResidencyImage3D", core10.sparseResidencyImage3D);
    GFXRECON_WRITE_CONSOLE("%d sparseResidency2Samples", core10.sparseResidency2Samples);
    GFXRECON_WRITE_CONSOLE("%d sparseResidency4Samples", core10.sparseResidency4Samples);
    GFXRECON_WRITE_CONSOLE("%d sparseResidency8Samples", core10.sparseResidency8Samples);
    GFXRECON_WRITE_CONSOLE("%d sparseResidency16Samples", core10.sparseResidency16Samples);
    GFXRECON_WRITE_CONSOLE("%d sparseResidencyAliased", core10.sparseResidencyAliased);
    GFXRECON_WRITE_CONSOLE("%d variableMultisampleRate", core10.variableMultisampleRate);
    GFXRECON_WRITE_CONSOLE("%d inheritedQueries", core10.inheritedQueries);
    GFXRECON_WRITE_CONSOLE("");
}

void VulkanFeatureTrackerConsumerBase::PrintCore11Features(VkPhysicalDeviceVulkan11Features core11)
{
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("\tCore11");
    GFXRECON_WRITE_CONSOLE("%d storageBuffer16BitAccess", core11.storageBuffer16BitAccess);
    GFXRECON_WRITE_CONSOLE("%d uniformAndStorageBuffer16BitAccess", core11.uniformAndStorageBuffer16BitAccess);
    GFXRECON_WRITE_CONSOLE("%d storagePushConstant16", core11.storagePushConstant16);
    GFXRECON_WRITE_CONSOLE("%d storageInputOutput16", core11.storageInputOutput16);
    GFXRECON_WRITE_CONSOLE("%d multiview", core11.multiview);
    GFXRECON_WRITE_CONSOLE("%d multiviewGeometryShader", core11.multiviewGeometryShader);
    GFXRECON_WRITE_CONSOLE("%d multiviewTessellationShader", core11.multiviewTessellationShader);
    GFXRECON_WRITE_CONSOLE("%d variablePointersStorageBuffer", core11.variablePointersStorageBuffer);
    GFXRECON_WRITE_CONSOLE("%d variablePointers", core11.variablePointers);
    GFXRECON_WRITE_CONSOLE("%d protectedMemory", core11.protectedMemory);
    GFXRECON_WRITE_CONSOLE("%d samplerYcbcrConversion", core11.samplerYcbcrConversion);
    GFXRECON_WRITE_CONSOLE("%d shaderDrawParameters", core11.shaderDrawParameters);
    GFXRECON_WRITE_CONSOLE("");
}

void VulkanFeatureTrackerConsumerBase::PrintCore12Features(VkPhysicalDeviceVulkan12Features core12)
{
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("\tCore12");

    GFXRECON_WRITE_CONSOLE("%d samplerMirrorClampToEdge", core12.samplerMirrorClampToEdge);
    GFXRECON_WRITE_CONSOLE("%d drawIndirectCount", core12.drawIndirectCount);
    GFXRECON_WRITE_CONSOLE("%d storageBuffer8BitAccess", core12.storageBuffer8BitAccess);
    GFXRECON_WRITE_CONSOLE("%d uniformAndStorageBuffer8BitAccess", core12.uniformAndStorageBuffer8BitAccess);
    GFXRECON_WRITE_CONSOLE("%d storagePushConstant8", core12.storagePushConstant8);
    GFXRECON_WRITE_CONSOLE("%d shaderBufferInt64Atomics", core12.shaderBufferInt64Atomics);
    GFXRECON_WRITE_CONSOLE("%d shaderSharedInt64Atomics", core12.shaderSharedInt64Atomics);
    GFXRECON_WRITE_CONSOLE("%d shaderFloat16", core12.shaderFloat16);
    GFXRECON_WRITE_CONSOLE("%d shaderInt8", core12.shaderInt8);
    GFXRECON_WRITE_CONSOLE("%d descriptorIndexing", core12.descriptorIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderInputAttachmentArrayDynamicIndexing",
                           core12.shaderInputAttachmentArrayDynamicIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderUniformTexelBufferArrayDynamicIndexing",
                           core12.shaderUniformTexelBufferArrayDynamicIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageTexelBufferArrayDynamicIndexing",
                           core12.shaderStorageTexelBufferArrayDynamicIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderUniformBufferArrayNonUniformIndexing",
                           core12.shaderUniformBufferArrayNonUniformIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderSampledImageArrayNonUniformIndexing",
                           core12.shaderSampledImageArrayNonUniformIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageBufferArrayNonUniformIndexing",
                           core12.shaderStorageBufferArrayNonUniformIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderInputAttachmentArrayNonUniformIndexing",
                           core12.shaderInputAttachmentArrayNonUniformIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderUniformTexelBufferArrayNonUniformIndexing",
                           core12.shaderUniformTexelBufferArrayNonUniformIndexing);
    GFXRECON_WRITE_CONSOLE("%d shaderStorageTexelBufferArrayNonUniformIndexing",
                           core12.shaderStorageTexelBufferArrayNonUniformIndexing);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingUniformBufferUpdateAfterBind",
                           core12.descriptorBindingUniformBufferUpdateAfterBind);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingSampledImageUpdateAfterBind",
                           core12.descriptorBindingSampledImageUpdateAfterBind);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingStorageImageUpdateAfterBind",
                           core12.descriptorBindingStorageImageUpdateAfterBind);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingStorageBufferUpdateAfterBind",
                           core12.descriptorBindingStorageBufferUpdateAfterBind);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingUniformTexelBufferUpdateAfterBind",
                           core12.descriptorBindingUniformTexelBufferUpdateAfterBind);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingStorageTexelBufferUpdateAfterBind",
                           core12.descriptorBindingStorageTexelBufferUpdateAfterBind);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingUpdateUnusedWhilePending",
                           core12.descriptorBindingUpdateUnusedWhilePending);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingPartiallyBound", core12.descriptorBindingPartiallyBound);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingVariableDescriptorCount",
                           core12.descriptorBindingVariableDescriptorCount);
    GFXRECON_WRITE_CONSOLE("%d runtimeDescriptorArray", core12.runtimeDescriptorArray);
    GFXRECON_WRITE_CONSOLE("%d samplerFilterMinmax", core12.samplerFilterMinmax);
    GFXRECON_WRITE_CONSOLE("%d scalarBlockLayout", core12.scalarBlockLayout);
    GFXRECON_WRITE_CONSOLE("%d imagelessFramebuffer", core12.imagelessFramebuffer);
    GFXRECON_WRITE_CONSOLE("%d uniformBufferStandardLayout", core12.uniformBufferStandardLayout);
    GFXRECON_WRITE_CONSOLE("%d shaderSubgroupExtendedTypes", core12.shaderSubgroupExtendedTypes);
    GFXRECON_WRITE_CONSOLE("%d separateDepthStencilLayouts", core12.separateDepthStencilLayouts);
    GFXRECON_WRITE_CONSOLE("%d hostQueryReset", core12.hostQueryReset);
    GFXRECON_WRITE_CONSOLE("%d timelineSemaphore", core12.timelineSemaphore);
    GFXRECON_WRITE_CONSOLE("%d bufferDeviceAddress", core12.bufferDeviceAddress);
    GFXRECON_WRITE_CONSOLE("%d bufferDeviceAddressCaptureReplay", core12.bufferDeviceAddressCaptureReplay);
    GFXRECON_WRITE_CONSOLE("%d bufferDeviceAddressMultiDevice", core12.bufferDeviceAddressMultiDevice);
    GFXRECON_WRITE_CONSOLE("%d vulkanMemoryModel", core12.vulkanMemoryModel);
    GFXRECON_WRITE_CONSOLE("%d vulkanMemoryModelAvailabilityVisibilityChains",
                           core12.vulkanMemoryModelAvailabilityVisibilityChains);
    GFXRECON_WRITE_CONSOLE("%d shaderOutputViewportIndex", core12.shaderOutputViewportIndex);
    GFXRECON_WRITE_CONSOLE("%d shaderOutputLayer", core12.shaderOutputLayer);
    GFXRECON_WRITE_CONSOLE("%d subgroupBroadcastDynamicId", core12.subgroupBroadcastDynamicId);
    GFXRECON_WRITE_CONSOLE("");
}

void VulkanFeatureTrackerConsumerBase::PrintCore13Features(VkPhysicalDeviceVulkan13Features core13)
{
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("\tCore13");
    GFXRECON_WRITE_CONSOLE("%d robustImageAccess", core13.robustImageAccess);
    GFXRECON_WRITE_CONSOLE("%d inlineUniformBlock", core13.inlineUniformBlock);
    GFXRECON_WRITE_CONSOLE("%d descriptorBindingInlineUniformBlockUpdateAfterBind",
                           core13.descriptorBindingInlineUniformBlockUpdateAfterBind);
    GFXRECON_WRITE_CONSOLE("%d pipelineCreationCacheControl", core13.pipelineCreationCacheControl);
    GFXRECON_WRITE_CONSOLE("%d privateData", core13.privateData);
    GFXRECON_WRITE_CONSOLE("%d shaderDemoteToHelperInvocation", core13.shaderDemoteToHelperInvocation);
    GFXRECON_WRITE_CONSOLE("%d shaderTerminateInvocation", core13.shaderTerminateInvocation);
    GFXRECON_WRITE_CONSOLE("%d subgroupSizeControl", core13.subgroupSizeControl);
    GFXRECON_WRITE_CONSOLE("%d computeFullSubgroups", core13.computeFullSubgroups);
    GFXRECON_WRITE_CONSOLE("%d synchronization2", core13.synchronization2);
    GFXRECON_WRITE_CONSOLE("%d textureCompressionASTC_HDR", core13.textureCompressionASTC_HDR);
    GFXRECON_WRITE_CONSOLE("%d shaderZeroInitializeWorkgroupMemory", core13.shaderZeroInitializeWorkgroupMemory);
    GFXRECON_WRITE_CONSOLE("%d dynamicRendering", core13.dynamicRendering);
    GFXRECON_WRITE_CONSOLE("%d shaderIntegerDotProduct", core13.shaderIntegerDotProduct);
    GFXRECON_WRITE_CONSOLE("%d maintenance4", core13.maintenance4);
    GFXRECON_WRITE_CONSOLE("");
}

void VulkanFeatureTrackerConsumerBase::PrintAllFeatures()
{
    GFXRECON_WRITE_CONSOLE("ORIGINAL Features:");
    GFXRECON_WRITE_CONSOLE("");
    PrintCore10Features(capture_core10_);
    PrintCore11Features(capture_core11_);
    PrintCore12Features(capture_core12_);
    PrintCore13Features(capture_core13_);
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("");

    GFXRECON_WRITE_CONSOLE("DETECTED Features:");
    GFXRECON_WRITE_CONSOLE("");
    PrintCore10Features(core10_);
    PrintCore11Features(core11_);
    PrintCore12Features(core12_);
    PrintCore13Features(core13_);
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("");

    GFXRECON_WRITE_CONSOLE("OUTPUTED Features:");
    GFXRECON_WRITE_CONSOLE("");
    PrintCore10Features(output_core10_);
    PrintCore11Features(output_core11_);
    PrintCore12Features(output_core12_);
    PrintCore13Features(output_core13_);
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("");
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
