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
#ifndef GFXRECON_TOOLS_OPTIMIZE_VULKAN_DEBUG_OBJECT_NAME_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_VULKAN_DEBUG_OBJECT_NAME_MODIFIER_H

#include "util/vulkan_modifier_base.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanDebugObjectNameModifier : public util::VulkanModifierBase
{
  public:
    bool CanOptimize() override { return true; }

    void Process_vkCreateInstance(const ApiCallInfo& call_info, args::CreateInstance& args) override;
    void Process_vkCreateDevice(const ApiCallInfo& call_info, args::CreateDevice& args) override;
    void Process_vkCreateFence(const ApiCallInfo& call_info, args::CreateFence& args) override;
    void Process_vkCreateSemaphore(const ApiCallInfo& call_info, args::CreateSemaphore& args) override;
    void Process_vkCreateEvent(const ApiCallInfo& call_info, args::CreateEvent& args) override;
    void Process_vkCreateQueryPool(const ApiCallInfo& call_info, args::CreateQueryPool& args) override;
    void Process_vkCreateBuffer(const ApiCallInfo& call_info, args::CreateBuffer& args) override;
    void Process_vkCreateBufferView(const ApiCallInfo& call_info, args::CreateBufferView& args) override;
    void Process_vkCreateImage(const ApiCallInfo& call_info, args::CreateImage& args) override;
    void Process_vkCreateImageView(const ApiCallInfo& call_info, args::CreateImageView& args) override;
    void Process_vkCreateShaderModule(const ApiCallInfo& call_info, args::CreateShaderModule& args) override;
    void Process_vkCreatePipelineCache(const ApiCallInfo& call_info, args::CreatePipelineCache& args) override;
    void Process_vkCreatePipelineLayout(const ApiCallInfo& call_info, args::CreatePipelineLayout& args) override;
    void Process_vkCreateSampler(const ApiCallInfo& call_info, args::CreateSampler& args) override;
    void Process_vkCreateDescriptorSetLayout(const ApiCallInfo&               call_info,
                                             args::CreateDescriptorSetLayout& args) override;
    void Process_vkCreateDescriptorPool(const ApiCallInfo& call_info, args::CreateDescriptorPool& args) override;
    void Process_vkCreateFramebuffer(const ApiCallInfo& call_info, args::CreateFramebuffer& args) override;
    void Process_vkCreateRenderPass(const ApiCallInfo& call_info, args::CreateRenderPass& args) override;
    void Process_vkCreateCommandPool(const ApiCallInfo& call_info, args::CreateCommandPool& args) override;
    void Process_vkCreateSwapchainKHR(const ApiCallInfo& call_info, args::CreateSwapchainKHR& args) override;
    void Process_vkCreateIndirectCommandsLayoutNV(const ApiCallInfo&                    call_info,
                                                  args::CreateIndirectCommandsLayoutNV& args) override;
    void Process_vkCreateIndirectCommandsLayoutEXT(const ApiCallInfo&                     call_info,
                                                   args::CreateIndirectCommandsLayoutEXT& args) override;
    void Process_vkCreateIndirectExecutionSetEXT(const ApiCallInfo&                   call_info,
                                                 args::CreateIndirectExecutionSetEXT& args) override;
    void Process_vkCreateDescriptorUpdateTemplate(const ApiCallInfo&                    call_info,
                                                  args::CreateDescriptorUpdateTemplate& args) override;
    void Process_vkCreateDescriptorUpdateTemplateKHR(const ApiCallInfo&                       call_info,
                                                     args::CreateDescriptorUpdateTemplateKHR& args) override;
    void Process_vkCreateSamplerYcbcrConversion(const ApiCallInfo&                  call_info,
                                                args::CreateSamplerYcbcrConversion& args) override;
    void Process_vkCreateSamplerYcbcrConversionKHR(const ApiCallInfo&                     call_info,
                                                   args::CreateSamplerYcbcrConversionKHR& args) override;
    void Process_vkCreateValidationCacheEXT(const ApiCallInfo&              call_info,
                                            args::CreateValidationCacheEXT& args) override;
    void Process_vkCreateRenderPass2(const ApiCallInfo& call_info, args::CreateRenderPass2& args) override;
    void Process_vkCreateRenderPass2KHR(const ApiCallInfo& call_info, args::CreateRenderPass2KHR& args) override;
    void Process_vkCreateAccelerationStructureNV(const ApiCallInfo&                   call_info,
                                                 args::CreateAccelerationStructureNV& args) override;
    void Process_vkCreateAccelerationStructureKHR(const ApiCallInfo&                    call_info,
                                                  args::CreateAccelerationStructureKHR& args) override;
    void Process_vkCreateDeferredOperationKHR(const ApiCallInfo&                call_info,
                                              args::CreateDeferredOperationKHR& args) override;
    void Process_vkCreatePrivateDataSlot(const ApiCallInfo& call_info, args::CreatePrivateDataSlot& args) override;
    void Process_vkCreatePrivateDataSlotEXT(const ApiCallInfo&              call_info,
                                            args::CreatePrivateDataSlotEXT& args) override;
    void Process_vkCreateVideoSessionKHR(const ApiCallInfo& call_info, args::CreateVideoSessionKHR& args) override;
    void Process_vkCreateVideoSessionParametersKHR(const ApiCallInfo&                     call_info,
                                                   args::CreateVideoSessionParametersKHR& args) override;
    void Process_vkCreateMicromapEXT(const ApiCallInfo& call_info, args::CreateMicromapEXT& args) override;
    void Process_vkCreateOpticalFlowSessionNV(const ApiCallInfo&                call_info,
                                              args::CreateOpticalFlowSessionNV& args) override;
    void Process_vkCreateGpaSessionAMD(const ApiCallInfo& call_info, args::CreateGpaSessionAMD& args) override;
    void Process_vkCreateTensorARM(const ApiCallInfo& call_info, args::CreateTensorARM& args) override;
    void Process_vkCreateTensorViewARM(const ApiCallInfo& call_info, args::CreateTensorViewARM& args) override;
    void Process_vkCreateDataGraphPipelineSessionARM(const ApiCallInfo&                       call_info,
                                                     args::CreateDataGraphPipelineSessionARM& args) override;
    void Process_vkCreateAccelerationStructure2KHR(const ApiCallInfo&                     call_info,
                                                   args::CreateAccelerationStructure2KHR& args) override;

  private:
    void AddDebugObjectName(const ApiCallInfo&      call_info,
                            VkResult                result,
                            format::HandleId        device,
                            VkObjectType            type,
                            const format::HandleId* handle);
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_VULKAN_DEBUG_OBJECT_NAME_MODIFIER_H
