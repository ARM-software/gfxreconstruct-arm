/*
** Copyright (c) 2026 LunarG, Inc.
** Copyright (c) 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
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
#include "vulkan_debug_object_name_modifier.h"

#include "encode/parameter_encoder.h"
#include "encode/struct_pointer_encoder.h"
#include "format/api_call_id.h"
#include "generated/generated_vulkan_struct_encoders.h"
#include "util/to_string.h"

#include <algorithm>
#include <string>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

void VulkanDebugObjectNameModifier::AddDebugObjectName(const ApiCallInfo&      call_info,
                                                       VkResult                result,
                                                       format::HandleId        device,
                                                       VkObjectType            type,
                                                       const format::HandleId* handle)
{
    if (!IsModificationPass() || result != VK_SUCCESS || handle == nullptr || *handle == format::kNullHandleId)
    {
        return;
    }

    std::string name = util::ToString<VkObjectType>(type).substr(15) + '_' + std::to_string(*handle);

    auto call       = CreatePostCall();
    call->type      = NewCallDataType::ApiCall;
    call->call_id   = format::ApiCall_vkSetDebugUtilsObjectNameEXT;
    call->thread_id = call_info.thread_id;

    encode::ParameterEncoder encoder(&call->parameter_buffer);
    encoder.EncodeHandleIdValue(device);
    encoder.EncodeStructPtrPreamble(handle);
    encoder.EncodeEnumValue(VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
    encode::EncodePNextStruct(&encoder, nullptr);
    encoder.EncodeEnumValue(type);
    encoder.EncodeHandleIdValue(*handle);
    encoder.EncodeString(name.c_str());
    encoder.EncodeEnumValue(VK_SUCCESS);
}

void VulkanDebugObjectNameModifier::Process_vkCreateInstance(const ApiCallInfo& call_info, args::CreateInstance& args)
{
    if (!IsModificationPass() || args.pCreateInfo.GetPointer() == nullptr)
    {
        return;
    }

    const auto*              create_info = args.pCreateInfo.GetPointer();
    std::vector<const char*> extensions;
    if (create_info->enabledExtensionCount != 0 && create_info->ppEnabledExtensionNames != nullptr)
    {
        extensions.assign(create_info->ppEnabledExtensionNames,
                          create_info->ppEnabledExtensionNames + create_info->enabledExtensionCount);
    }
    const bool has_debug_utils = std::any_of(extensions.begin(), extensions.end(), [](const char* extension) {
        return extension != nullptr && std::string(extension) == VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    });
    if (has_debug_utils)
    {
        return;
    }

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    VkInstanceCreateInfo updated    = *create_info;
    updated.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    updated.ppEnabledExtensionNames = extensions.data();

    parameter_buffer_->Clear();
    encode::ParameterEncoder encoder(parameter_buffer_);
    encode::EncodeStructPtr(&encoder, &updated);
    encode::EncodeStructPtr(&encoder, args.pAllocator.GetPointer());
    encoder.EncodeHandleIdPtr(args.pInstance.GetPointer());
    encoder.EncodeEnumValue(args.result);
}

void VulkanDebugObjectNameModifier::Process_vkCreateDevice(const ApiCallInfo& call_info, args::CreateDevice& args)
{
    const auto* device = args.pDevice.GetPointer();
    if (device != nullptr)
    {
        AddDebugObjectName(call_info, args.result, *device, VK_OBJECT_TYPE_DEVICE, device);
    }
}

void VulkanDebugObjectNameModifier::Process_vkCreateFence(const ApiCallInfo& call_info, args::CreateFence& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_FENCE, args.pFence.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateSemaphore(const ApiCallInfo& call_info, args::CreateSemaphore& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_SEMAPHORE, args.pSemaphore.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateEvent(const ApiCallInfo& call_info, args::CreateEvent& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_EVENT, args.pEvent.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateQueryPool(const ApiCallInfo& call_info, args::CreateQueryPool& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_QUERY_POOL, args.pQueryPool.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateBuffer(const ApiCallInfo& call_info, args::CreateBuffer& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_BUFFER, args.pBuffer.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateBufferView(const ApiCallInfo&      call_info,
                                                               args::CreateBufferView& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_BUFFER_VIEW, args.pView.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateImage(const ApiCallInfo& call_info, args::CreateImage& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_IMAGE, args.pImage.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateImageView(const ApiCallInfo& call_info, args::CreateImageView& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_IMAGE_VIEW, args.pView.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateShaderModule(const ApiCallInfo&        call_info,
                                                                 args::CreateShaderModule& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_SHADER_MODULE, args.pShaderModule.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreatePipelineCache(const ApiCallInfo&         call_info,
                                                                  args::CreatePipelineCache& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_PIPELINE_CACHE, args.pPipelineCache.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreatePipelineLayout(const ApiCallInfo&          call_info,
                                                                   args::CreatePipelineLayout& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, args.pPipelineLayout.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateSampler(const ApiCallInfo& call_info, args::CreateSampler& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_SAMPLER, args.pSampler.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateDescriptorSetLayout(const ApiCallInfo&               call_info,
                                                                        args::CreateDescriptorSetLayout& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, args.pSetLayout.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateDescriptorPool(const ApiCallInfo&          call_info,
                                                                   args::CreateDescriptorPool& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, args.pDescriptorPool.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateFramebuffer(const ApiCallInfo&       call_info,
                                                                args::CreateFramebuffer& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_FRAMEBUFFER, args.pFramebuffer.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateRenderPass(const ApiCallInfo&      call_info,
                                                               args::CreateRenderPass& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_RENDER_PASS, args.pRenderPass.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateCommandPool(const ApiCallInfo&       call_info,
                                                                args::CreateCommandPool& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_COMMAND_POOL, args.pCommandPool.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateSwapchainKHR(const ApiCallInfo&        call_info,
                                                                 args::CreateSwapchainKHR& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, args.pSwapchain.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateIndirectCommandsLayoutNV(const ApiCallInfo& call_info,
                                                                             args::CreateIndirectCommandsLayoutNV& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV,
                       args.pIndirectCommandsLayout.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateIndirectCommandsLayoutEXT(
    const ApiCallInfo& call_info, args::CreateIndirectCommandsLayoutEXT& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT,
                       args.pIndirectCommandsLayout.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateIndirectExecutionSetEXT(const ApiCallInfo& call_info,
                                                                            args::CreateIndirectExecutionSetEXT& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT,
                       args.pIndirectExecutionSet.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateDescriptorUpdateTemplate(const ApiCallInfo& call_info,
                                                                             args::CreateDescriptorUpdateTemplate& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE,
                       args.pDescriptorUpdateTemplate.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateDescriptorUpdateTemplateKHR(
    const ApiCallInfo& call_info, args::CreateDescriptorUpdateTemplateKHR& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE,
                       args.pDescriptorUpdateTemplate.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateSamplerYcbcrConversion(const ApiCallInfo& call_info,
                                                                           args::CreateSamplerYcbcrConversion& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION,
                       args.pYcbcrConversion.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateSamplerYcbcrConversionKHR(
    const ApiCallInfo& call_info, args::CreateSamplerYcbcrConversionKHR& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION,
                       args.pYcbcrConversion.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateValidationCacheEXT(const ApiCallInfo&              call_info,
                                                                       args::CreateValidationCacheEXT& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_VALIDATION_CACHE_EXT, args.pValidationCache.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateRenderPass2(const ApiCallInfo&       call_info,
                                                                args::CreateRenderPass2& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_RENDER_PASS, args.pRenderPass.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateRenderPass2KHR(const ApiCallInfo&          call_info,
                                                                   args::CreateRenderPass2KHR& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_RENDER_PASS, args.pRenderPass.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateAccelerationStructureNV(const ApiCallInfo& call_info,
                                                                            args::CreateAccelerationStructureNV& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV,
                       args.pAccelerationStructure.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateAccelerationStructureKHR(const ApiCallInfo& call_info,
                                                                             args::CreateAccelerationStructureKHR& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
                       args.pAccelerationStructure.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateDeferredOperationKHR(const ApiCallInfo&                call_info,
                                                                         args::CreateDeferredOperationKHR& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR,
                       args.pDeferredOperation.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreatePrivateDataSlot(const ApiCallInfo&           call_info,
                                                                    args::CreatePrivateDataSlot& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_PRIVATE_DATA_SLOT, args.pPrivateDataSlot.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreatePrivateDataSlotEXT(const ApiCallInfo&              call_info,
                                                                       args::CreatePrivateDataSlotEXT& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_PRIVATE_DATA_SLOT, args.pPrivateDataSlot.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateVideoSessionKHR(const ApiCallInfo&           call_info,
                                                                    args::CreateVideoSessionKHR& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_VIDEO_SESSION_KHR, args.pVideoSession.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateVideoSessionParametersKHR(
    const ApiCallInfo& call_info, args::CreateVideoSessionParametersKHR& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR,
                       args.pVideoSessionParameters.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateMicromapEXT(const ApiCallInfo&       call_info,
                                                                args::CreateMicromapEXT& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_MICROMAP_EXT, args.pMicromap.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateOpticalFlowSessionNV(const ApiCallInfo&                call_info,
                                                                         args::CreateOpticalFlowSessionNV& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV, args.pSession.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateGpaSessionAMD(const ApiCallInfo&         call_info,
                                                                  args::CreateGpaSessionAMD& args)
{
    AddDebugObjectName(
        call_info, args.result, args.device, VK_OBJECT_TYPE_GPA_SESSION_AMD, args.pGpaSession.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateTensorARM(const ApiCallInfo& call_info, args::CreateTensorARM& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_TENSOR_ARM, args.pTensor.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateTensorViewARM(const ApiCallInfo&         call_info,
                                                                  args::CreateTensorViewARM& args)
{
    AddDebugObjectName(call_info, args.result, args.device, VK_OBJECT_TYPE_TENSOR_VIEW_ARM, args.pView.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateDataGraphPipelineSessionARM(
    const ApiCallInfo& call_info, args::CreateDataGraphPipelineSessionARM& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_DATA_GRAPH_PIPELINE_SESSION_ARM,
                       args.pSession.GetPointer());
}

void VulkanDebugObjectNameModifier::Process_vkCreateAccelerationStructure2KHR(
    const ApiCallInfo& call_info, args::CreateAccelerationStructure2KHR& args)
{
    AddDebugObjectName(call_info,
                       args.result,
                       args.device,
                       VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
                       args.pAccelerationStructure.GetPointer());
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
