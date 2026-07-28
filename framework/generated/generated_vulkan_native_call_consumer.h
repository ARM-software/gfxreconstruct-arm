/*
** Copyright (c) 2026 Arm Limited <open-source-office@arm.com>
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

#ifndef  GFXRECON_GENERATED_VULKAN_NATIVE_CALL_CONSUMER_H
#define  GFXRECON_GENERATED_VULKAN_NATIVE_CALL_CONSUMER_H

#include "decode/vulkan_native_call_consumer_base.h"
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
class VulkanNativeCallConsumer : public VulkanNativeCallConsumerBase
{
  public:
    explicit VulkanNativeCallConsumer(VulkanNativeCallCallbacks& callbacks) :
        VulkanNativeCallConsumerBase(callbacks)
    {}
    ~VulkanNativeCallConsumer() override = default;

    void Process_vkAllocateCommandBuffers(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkCommandBufferAllocateInfo>* pAllocateInfo,
        HandlePointerDecoder<VkCommandBuffer>*      pCommandBuffers) override;

    void Process_vkAllocateDescriptorSets(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorSetAllocateInfo>* pAllocateInfo,
        HandlePointerDecoder<VkDescriptorSet>*      pDescriptorSets) override;

    void Process_vkAllocateMemory(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryAllocateInfo>* pAllocateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDeviceMemory>*       pMemory) override;

    void Process_vkBeginCommandBuffer(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo) override;

    void Process_vkBindBufferMemory(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            buffer,
        format::HandleId                            memory,
        VkDeviceSize                                memoryOffset) override;

    void Process_vkBindImageMemory(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            image,
        format::HandleId                            memory,
        VkDeviceSize                                memoryOffset) override;

    void Process_vkCmdBeginQuery(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            queryPool,
        uint32_t                                    query,
        VkQueryControlFlags                         flags) override;

    void Process_vkCmdBeginRenderPass(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderPassBeginInfo>* pRenderPassBegin,
        VkSubpassContents                           contents) override;

    void Process_vkCmdBindDescriptorSets(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        format::HandleId                            layout,
        uint32_t                                    firstSet,
        uint32_t                                    descriptorSetCount,
        HandlePointerDecoder<VkDescriptorSet>*      pDescriptorSets,
        uint32_t                                    dynamicOffsetCount,
        PointerDecoder<uint32_t>*                   pDynamicOffsets) override;

    void Process_vkCmdBindIndexBuffer(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        VkIndexType                                 indexType) override;

    void Process_vkCmdBindPipeline(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        format::HandleId                            pipeline) override;

    void Process_vkCmdBindVertexBuffers(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstBinding,
        uint32_t                                    bindingCount,
        HandlePointerDecoder<VkBuffer>*             pBuffers,
        PointerDecoder<VkDeviceSize>*               pOffsets) override;

    void Process_vkCmdBlitImage(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            srcImage,
        VkImageLayout                               srcImageLayout,
        format::HandleId                            dstImage,
        VkImageLayout                               dstImageLayout,
        uint32_t                                    regionCount,
        StructPointerDecoder<Decoded_VkImageBlit>*  pRegions,
        VkFilter                                    filter) override;

    void Process_vkCmdClearAttachments(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    attachmentCount,
        StructPointerDecoder<Decoded_VkClearAttachment>* pAttachments,
        uint32_t                                    rectCount,
        StructPointerDecoder<Decoded_VkClearRect>*  pRects) override;

    void Process_vkCmdClearColorImage(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            image,
        VkImageLayout                               imageLayout,
        StructPointerDecoder<Decoded_VkClearColorValue>* pColor,
        uint32_t                                    rangeCount,
        StructPointerDecoder<Decoded_VkImageSubresourceRange>* pRanges) override;

    void Process_vkCmdClearDepthStencilImage(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            image,
        VkImageLayout                               imageLayout,
        StructPointerDecoder<Decoded_VkClearDepthStencilValue>* pDepthStencil,
        uint32_t                                    rangeCount,
        StructPointerDecoder<Decoded_VkImageSubresourceRange>* pRanges) override;

    void Process_vkCmdCopyBuffer(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            srcBuffer,
        format::HandleId                            dstBuffer,
        uint32_t                                    regionCount,
        StructPointerDecoder<Decoded_VkBufferCopy>* pRegions) override;

    void Process_vkCmdCopyBufferToImage(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            srcBuffer,
        format::HandleId                            dstImage,
        VkImageLayout                               dstImageLayout,
        uint32_t                                    regionCount,
        StructPointerDecoder<Decoded_VkBufferImageCopy>* pRegions) override;

    void Process_vkCmdCopyImage(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            srcImage,
        VkImageLayout                               srcImageLayout,
        format::HandleId                            dstImage,
        VkImageLayout                               dstImageLayout,
        uint32_t                                    regionCount,
        StructPointerDecoder<Decoded_VkImageCopy>*  pRegions) override;

    void Process_vkCmdCopyImageToBuffer(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            srcImage,
        VkImageLayout                               srcImageLayout,
        format::HandleId                            dstBuffer,
        uint32_t                                    regionCount,
        StructPointerDecoder<Decoded_VkBufferImageCopy>* pRegions) override;

    void Process_vkCmdCopyQueryPoolResults(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery,
        uint32_t                                    queryCount,
        format::HandleId                            dstBuffer,
        VkDeviceSize                                dstOffset,
        VkDeviceSize                                stride,
        VkQueryResultFlags                          flags) override;

    void Process_vkCmdDispatch(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    groupCountX,
        uint32_t                                    groupCountY,
        uint32_t                                    groupCountZ) override;

    void Process_vkCmdDispatchIndirect(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset) override;

    void Process_vkCmdDraw(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    vertexCount,
        uint32_t                                    instanceCount,
        uint32_t                                    firstVertex,
        uint32_t                                    firstInstance) override;

    void Process_vkCmdDrawIndexed(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    indexCount,
        uint32_t                                    instanceCount,
        uint32_t                                    firstIndex,
        int32_t                                     vertexOffset,
        uint32_t                                    firstInstance) override;

    void Process_vkCmdDrawIndexedIndirect(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        uint32_t                                    drawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdDrawIndirect(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        uint32_t                                    drawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdEndQuery(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            queryPool,
        uint32_t                                    query) override;

    void Process_vkCmdEndRenderPass(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer) override;

    void Process_vkCmdExecuteCommands(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    commandBufferCount,
        HandlePointerDecoder<VkCommandBuffer>*      pCommandBuffers) override;

    void Process_vkCmdFillBuffer(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            dstBuffer,
        VkDeviceSize                                dstOffset,
        VkDeviceSize                                size,
        uint32_t                                    data) override;

    void Process_vkCmdNextSubpass(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkSubpassContents                           contents) override;

    void Process_vkCmdPipelineBarrier(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineStageFlags                        srcStageMask,
        VkPipelineStageFlags                        dstStageMask,
        VkDependencyFlags                           dependencyFlags,
        uint32_t                                    memoryBarrierCount,
        StructPointerDecoder<Decoded_VkMemoryBarrier>* pMemoryBarriers,
        uint32_t                                    bufferMemoryBarrierCount,
        StructPointerDecoder<Decoded_VkBufferMemoryBarrier>* pBufferMemoryBarriers,
        uint32_t                                    imageMemoryBarrierCount,
        StructPointerDecoder<Decoded_VkImageMemoryBarrier>* pImageMemoryBarriers) override;

    void Process_vkCmdPushConstants(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            layout,
        VkShaderStageFlags                          stageFlags,
        uint32_t                                    offset,
        uint32_t                                    size,
        PointerDecoder<uint8_t>*                    pValues) override;

    void Process_vkCmdResetEvent(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            event,
        VkPipelineStageFlags                        stageMask) override;

    void Process_vkCmdResetQueryPool(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery,
        uint32_t                                    queryCount) override;

    void Process_vkCmdResolveImage(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            srcImage,
        VkImageLayout                               srcImageLayout,
        format::HandleId                            dstImage,
        VkImageLayout                               dstImageLayout,
        uint32_t                                    regionCount,
        StructPointerDecoder<Decoded_VkImageResolve>* pRegions) override;

    void Process_vkCmdSetBlendConstants(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        PointerDecoder<float>*                      blendConstants) override;

    void Process_vkCmdSetDepthBias(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        float                                       depthBiasConstantFactor,
        float                                       depthBiasClamp,
        float                                       depthBiasSlopeFactor) override;

    void Process_vkCmdSetDepthBounds(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        float                                       minDepthBounds,
        float                                       maxDepthBounds) override;

    void Process_vkCmdSetEvent(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            event,
        VkPipelineStageFlags                        stageMask) override;

    void Process_vkCmdSetLineWidth(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        float                                       lineWidth) override;

    void Process_vkCmdSetScissor(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstScissor,
        uint32_t                                    scissorCount,
        StructPointerDecoder<Decoded_VkRect2D>*     pScissors) override;

    void Process_vkCmdSetStencilCompareMask(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    compareMask) override;

    void Process_vkCmdSetStencilReference(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    reference) override;

    void Process_vkCmdSetStencilWriteMask(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    writeMask) override;

    void Process_vkCmdSetViewport(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstViewport,
        uint32_t                                    viewportCount,
        StructPointerDecoder<Decoded_VkViewport>*   pViewports) override;

    void Process_vkCmdUpdateBuffer(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            dstBuffer,
        VkDeviceSize                                dstOffset,
        VkDeviceSize                                dataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkCmdWaitEvents(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    eventCount,
        HandlePointerDecoder<VkEvent>*              pEvents,
        VkPipelineStageFlags                        srcStageMask,
        VkPipelineStageFlags                        dstStageMask,
        uint32_t                                    memoryBarrierCount,
        StructPointerDecoder<Decoded_VkMemoryBarrier>* pMemoryBarriers,
        uint32_t                                    bufferMemoryBarrierCount,
        StructPointerDecoder<Decoded_VkBufferMemoryBarrier>* pBufferMemoryBarriers,
        uint32_t                                    imageMemoryBarrierCount,
        StructPointerDecoder<Decoded_VkImageMemoryBarrier>* pImageMemoryBarriers) override;

    void Process_vkCmdWriteTimestamp(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineStageFlagBits                     pipelineStage,
        format::HandleId                            queryPool,
        uint32_t                                    query) override;

    void Process_vkCreateBuffer(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkBuffer>*             pBuffer) override;

    void Process_vkCreateBufferView(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferViewCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkBufferView>*         pView) override;

    void Process_vkCreateCommandPool(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkCommandPoolCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkCommandPool>*        pCommandPool) override;

    void Process_vkCreateComputePipelines(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipelineCache,
        uint32_t                                    createInfoCount,
        StructPointerDecoder<Decoded_VkComputePipelineCreateInfo>* pCreateInfos,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkPipeline>*           pPipelines) override;

    void Process_vkCreateDescriptorPool(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorPoolCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDescriptorPool>*     pDescriptorPool) override;

    void Process_vkCreateDescriptorSetLayout(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorSetLayoutCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDescriptorSetLayout>* pSetLayout) override;

    void Process_vkCreateDevice(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkDeviceCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDevice>*             pDevice) override;

    void Process_vkCreateEvent(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkEventCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkEvent>*              pEvent) override;

    void Process_vkCreateFence(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkFenceCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkFence>*              pFence) override;

    void Process_vkCreateFramebuffer(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkFramebufferCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkFramebuffer>*        pFramebuffer) override;

    void Process_vkCreateGraphicsPipelines(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipelineCache,
        uint32_t                                    createInfoCount,
        StructPointerDecoder<Decoded_VkGraphicsPipelineCreateInfo>* pCreateInfos,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkPipeline>*           pPipelines) override;

    void Process_vkCreateImage(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImageCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkImage>*              pImage) override;

    void Process_vkCreateImageView(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImageViewCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkImageView>*          pView) override;

    void Process_vkCreateInstance(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        StructPointerDecoder<Decoded_VkInstanceCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkInstance>*           pInstance) override;

    void Process_vkCreatePipelineCache(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineCacheCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkPipelineCache>*      pPipelineCache) override;

    void Process_vkCreatePipelineLayout(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineLayoutCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkPipelineLayout>*     pPipelineLayout) override;

    void Process_vkCreateQueryPool(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkQueryPoolCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkQueryPool>*          pQueryPool) override;

    void Process_vkCreateRenderPass(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkRenderPassCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkRenderPass>*         pRenderPass) override;

    void Process_vkCreateSampler(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSamplerCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSampler>*            pSampler) override;

    void Process_vkCreateSemaphore(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSemaphoreCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSemaphore>*          pSemaphore) override;

    void Process_vkCreateShaderModule(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkShaderModuleCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkShaderModule>*       pShaderModule) override;

    void Process_vkDestroyBuffer(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            buffer,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyBufferView(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            bufferView,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyCommandPool(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            commandPool,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyDescriptorPool(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            descriptorPool,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyDescriptorSetLayout(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            descriptorSetLayout,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyDevice(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyEvent(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            event,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyFence(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            fence,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyFramebuffer(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            framebuffer,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyImage(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            image,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyImageView(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            imageView,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyInstance(
        const ApiCallInfo&                          call_info,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyPipeline(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            pipeline,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyPipelineCache(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            pipelineCache,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyPipelineLayout(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            pipelineLayout,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyQueryPool(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            queryPool,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyRenderPass(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            renderPass,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroySampler(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            sampler,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroySemaphore(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            semaphore,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyShaderModule(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            shaderModule,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDeviceWaitIdle(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device) override;

    void Process_vkEndCommandBuffer(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            commandBuffer) override;

    void Process_vkEnumeratePhysicalDevices(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        PointerDecoder<uint32_t>*                   pPhysicalDeviceCount,
        HandlePointerDecoder<VkPhysicalDevice>*     pPhysicalDevices) override;

    void Process_vkFlushMappedMemoryRanges(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    memoryRangeCount,
        StructPointerDecoder<Decoded_VkMappedMemoryRange>* pMemoryRanges) override;

    void Process_vkFreeCommandBuffers(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            commandPool,
        uint32_t                                    commandBufferCount,
        HandlePointerDecoder<VkCommandBuffer>*      pCommandBuffers) override;

    void Process_vkFreeDescriptorSets(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            descriptorPool,
        uint32_t                                    descriptorSetCount,
        HandlePointerDecoder<VkDescriptorSet>*      pDescriptorSets) override;

    void Process_vkFreeMemory(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            memory,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetBufferMemoryRequirements(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            buffer,
        StructPointerDecoder<Decoded_VkMemoryRequirements>* pMemoryRequirements) override;

    void Process_vkGetDeviceMemoryCommitment(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            memory,
        PointerDecoder<VkDeviceSize>*               pCommittedMemoryInBytes) override;

    void Process_vkGetDeviceQueue(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        uint32_t                                    queueFamilyIndex,
        uint32_t                                    queueIndex,
        HandlePointerDecoder<VkQueue>*              pQueue) override;

    void Process_vkGetEventStatus(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            event) override;

    void Process_vkGetFenceStatus(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            fence) override;

    void Process_vkGetImageMemoryRequirements(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            image,
        StructPointerDecoder<Decoded_VkMemoryRequirements>* pMemoryRequirements) override;

    void Process_vkGetImageSparseMemoryRequirements(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            image,
        PointerDecoder<uint32_t>*                   pSparseMemoryRequirementCount,
        StructPointerDecoder<Decoded_VkSparseImageMemoryRequirements>* pSparseMemoryRequirements) override;

    void Process_vkGetImageSubresourceLayout(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            image,
        StructPointerDecoder<Decoded_VkImageSubresource>* pSubresource,
        StructPointerDecoder<Decoded_VkSubresourceLayout>* pLayout) override;

    void Process_vkGetPhysicalDeviceFeatures(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceFeatures>* pFeatures) override;

    void Process_vkGetPhysicalDeviceFormatProperties(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        VkFormat                                    format,
        StructPointerDecoder<Decoded_VkFormatProperties>* pFormatProperties) override;

    void Process_vkGetPhysicalDeviceImageFormatProperties(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        VkFormat                                    format,
        VkImageType                                 type,
        VkImageTiling                               tiling,
        VkImageUsageFlags                           usage,
        VkImageCreateFlags                          flags,
        StructPointerDecoder<Decoded_VkImageFormatProperties>* pImageFormatProperties) override;

    void Process_vkGetPhysicalDeviceMemoryProperties(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceMemoryProperties>* pMemoryProperties) override;

    void Process_vkGetPhysicalDeviceProperties(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceProperties>* pProperties) override;

    void Process_vkGetPhysicalDeviceQueueFamilyProperties(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pQueueFamilyPropertyCount,
        StructPointerDecoder<Decoded_VkQueueFamilyProperties>* pQueueFamilyProperties) override;

    void Process_vkGetPhysicalDeviceSparseImageFormatProperties(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        VkFormat                                    format,
        VkImageType                                 type,
        VkSampleCountFlagBits                       samples,
        VkImageUsageFlags                           usage,
        VkImageTiling                               tiling,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkSparseImageFormatProperties>* pProperties) override;

    void Process_vkGetPipelineCacheData(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipelineCache,
        PointerDecoder<size_t>*                     pDataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkGetQueryPoolResults(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery,
        uint32_t                                    queryCount,
        size_t                                      dataSize,
        PointerDecoder<uint8_t>*                    pData,
        VkDeviceSize                                stride,
        VkQueryResultFlags                          flags) override;

    void Process_vkGetRenderAreaGranularity(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            renderPass,
        StructPointerDecoder<Decoded_VkExtent2D>*   pGranularity) override;

    void Process_vkInvalidateMappedMemoryRanges(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    memoryRangeCount,
        StructPointerDecoder<Decoded_VkMappedMemoryRange>* pMemoryRanges) override;

    void Process_vkMapMemory(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            memory,
        VkDeviceSize                                offset,
        VkDeviceSize                                size,
        VkMemoryMapFlags                            flags,
        PointerDecoder<uint64_t, void*>*            ppData) override;

    void Process_vkMergePipelineCaches(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            dstCache,
        uint32_t                                    srcCacheCount,
        HandlePointerDecoder<VkPipelineCache>*      pSrcCaches) override;

    void Process_vkQueueBindSparse(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            queue,
        uint32_t                                    bindInfoCount,
        StructPointerDecoder<Decoded_VkBindSparseInfo>* pBindInfo,
        format::HandleId                            fence) override;

    void Process_vkQueueSubmit(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            queue,
        uint32_t                                    submitCount,
        StructPointerDecoder<Decoded_VkSubmitInfo>* pSubmits,
        format::HandleId                            fence) override;

    void Process_vkQueueWaitIdle(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            queue) override;

    void Process_vkResetCommandBuffer(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            commandBuffer,
        VkCommandBufferResetFlags                   flags) override;

    void Process_vkResetCommandPool(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            commandPool,
        VkCommandPoolResetFlags                     flags) override;

    void Process_vkResetDescriptorPool(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            descriptorPool,
        VkDescriptorPoolResetFlags                  flags) override;

    void Process_vkResetEvent(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            event) override;

    void Process_vkResetFences(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    fenceCount,
        HandlePointerDecoder<VkFence>*              pFences) override;

    void Process_vkSetEvent(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            event) override;

    void Process_vkUnmapMemory(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            memory) override;

    void Process_vkUpdateDescriptorSets(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        uint32_t                                    descriptorWriteCount,
        StructPointerDecoder<Decoded_VkWriteDescriptorSet>* pDescriptorWrites,
        uint32_t                                    descriptorCopyCount,
        StructPointerDecoder<Decoded_VkCopyDescriptorSet>* pDescriptorCopies) override;

    void Process_vkWaitForFences(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    fenceCount,
        HandlePointerDecoder<VkFence>*              pFences,
        VkBool32                                    waitAll,
        uint64_t                                    timeout) override;

    void Process_vkUpdateDescriptorSetWithTemplate(
        const ApiCallInfo&               call_info,
        format::HandleId                 device,
        format::HandleId                 descriptorSet,
        format::HandleId                 descriptorUpdateTemplate,
        DescriptorUpdateTemplateDecoder* pData) override;

    void Process_vkBindBufferMemory2(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    bindInfoCount,
        StructPointerDecoder<Decoded_VkBindBufferMemoryInfo>* pBindInfos) override;

    void Process_vkBindImageMemory2(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    bindInfoCount,
        StructPointerDecoder<Decoded_VkBindImageMemoryInfo>* pBindInfos) override;

    void Process_vkCmdDispatchBase(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    baseGroupX,
        uint32_t                                    baseGroupY,
        uint32_t                                    baseGroupZ,
        uint32_t                                    groupCountX,
        uint32_t                                    groupCountY,
        uint32_t                                    groupCountZ) override;

    void Process_vkCmdSetDeviceMask(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    deviceMask) override;

    void Process_vkCreateDescriptorUpdateTemplate(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorUpdateTemplateCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDescriptorUpdateTemplate>* pDescriptorUpdateTemplate) override;

    void Process_vkCreateSamplerYcbcrConversion(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSamplerYcbcrConversionCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSamplerYcbcrConversion>* pYcbcrConversion) override;

    void Process_vkDestroyDescriptorUpdateTemplate(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            descriptorUpdateTemplate,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroySamplerYcbcrConversion(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            ycbcrConversion,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkEnumeratePhysicalDeviceGroups(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        PointerDecoder<uint32_t>*                   pPhysicalDeviceGroupCount,
        StructPointerDecoder<Decoded_VkPhysicalDeviceGroupProperties>* pPhysicalDeviceGroupProperties) override;

    void Process_vkGetBufferMemoryRequirements2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferMemoryRequirementsInfo2>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetDescriptorSetLayoutSupport(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorSetLayoutCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkDescriptorSetLayoutSupport>* pSupport) override;

    void Process_vkGetDeviceGroupPeerMemoryFeatures(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        uint32_t                                    heapIndex,
        uint32_t                                    localDeviceIndex,
        uint32_t                                    remoteDeviceIndex,
        PointerDecoder<VkPeerMemoryFeatureFlags>*   pPeerMemoryFeatures) override;

    void Process_vkGetDeviceQueue2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceQueueInfo2>* pQueueInfo,
        HandlePointerDecoder<VkQueue>*              pQueue) override;

    void Process_vkGetImageMemoryRequirements2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImageMemoryRequirementsInfo2>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetImageSparseMemoryRequirements2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImageSparseMemoryRequirementsInfo2>* pInfo,
        PointerDecoder<uint32_t>*                   pSparseMemoryRequirementCount,
        StructPointerDecoder<Decoded_VkSparseImageMemoryRequirements2>* pSparseMemoryRequirements) override;

    void Process_vkGetPhysicalDeviceExternalBufferProperties(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceExternalBufferInfo>* pExternalBufferInfo,
        StructPointerDecoder<Decoded_VkExternalBufferProperties>* pExternalBufferProperties) override;

    void Process_vkGetPhysicalDeviceExternalFenceProperties(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceExternalFenceInfo>* pExternalFenceInfo,
        StructPointerDecoder<Decoded_VkExternalFenceProperties>* pExternalFenceProperties) override;

    void Process_vkGetPhysicalDeviceExternalSemaphoreProperties(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceExternalSemaphoreInfo>* pExternalSemaphoreInfo,
        StructPointerDecoder<Decoded_VkExternalSemaphoreProperties>* pExternalSemaphoreProperties) override;

    void Process_vkGetPhysicalDeviceFeatures2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceFeatures2>* pFeatures) override;

    void Process_vkGetPhysicalDeviceFormatProperties2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        VkFormat                                    format,
        StructPointerDecoder<Decoded_VkFormatProperties2>* pFormatProperties) override;

    void Process_vkGetPhysicalDeviceImageFormatProperties2(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceImageFormatInfo2>* pImageFormatInfo,
        StructPointerDecoder<Decoded_VkImageFormatProperties2>* pImageFormatProperties) override;

    void Process_vkGetPhysicalDeviceMemoryProperties2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceMemoryProperties2>* pMemoryProperties) override;

    void Process_vkGetPhysicalDeviceProperties2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceProperties2>* pProperties) override;

    void Process_vkGetPhysicalDeviceQueueFamilyProperties2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pQueueFamilyPropertyCount,
        StructPointerDecoder<Decoded_VkQueueFamilyProperties2>* pQueueFamilyProperties) override;

    void Process_vkGetPhysicalDeviceSparseImageFormatProperties2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceSparseImageFormatInfo2>* pFormatInfo,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkSparseImageFormatProperties2>* pProperties) override;

    void Process_vkTrimCommandPool(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            commandPool,
        VkCommandPoolTrimFlags                      flags) override;

    void Process_vkCmdBeginRenderPass2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderPassBeginInfo>* pRenderPassBegin,
        StructPointerDecoder<Decoded_VkSubpassBeginInfo>* pSubpassBeginInfo) override;

    void Process_vkCmdDrawIndexedIndirectCount(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        format::HandleId                            countBuffer,
        VkDeviceSize                                countBufferOffset,
        uint32_t                                    maxDrawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdDrawIndirectCount(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        format::HandleId                            countBuffer,
        VkDeviceSize                                countBufferOffset,
        uint32_t                                    maxDrawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdEndRenderPass2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkSubpassEndInfo>* pSubpassEndInfo) override;

    void Process_vkCmdNextSubpass2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkSubpassBeginInfo>* pSubpassBeginInfo,
        StructPointerDecoder<Decoded_VkSubpassEndInfo>* pSubpassEndInfo) override;

    void Process_vkCreateRenderPass2(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkRenderPassCreateInfo2>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkRenderPass>*         pRenderPass) override;

    void Process_vkGetBufferDeviceAddress(
        const ApiCallInfo&                          call_info,
        VkDeviceAddress                             returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    void Process_vkGetBufferOpaqueCaptureAddress(
        const ApiCallInfo&                          call_info,
        uint64_t                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    void Process_vkGetDeviceMemoryOpaqueCaptureAddress(
        const ApiCallInfo&                          call_info,
        uint64_t                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceMemoryOpaqueCaptureAddressInfo>* pInfo) override;

    void Process_vkGetSemaphoreCounterValue(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            semaphore,
        PointerDecoder<uint64_t>*                   pValue) override;

    void Process_vkResetQueryPool(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery,
        uint32_t                                    queryCount) override;

    void Process_vkSignalSemaphore(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSemaphoreSignalInfo>* pSignalInfo) override;

    void Process_vkWaitSemaphores(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSemaphoreWaitInfo>* pWaitInfo,
        uint64_t                                    timeout) override;

    void Process_vkCmdBeginRendering(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderingInfo>* pRenderingInfo) override;

    void Process_vkCmdBindVertexBuffers2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstBinding,
        uint32_t                                    bindingCount,
        HandlePointerDecoder<VkBuffer>*             pBuffers,
        PointerDecoder<VkDeviceSize>*               pOffsets,
        PointerDecoder<VkDeviceSize>*               pSizes,
        PointerDecoder<VkDeviceSize>*               pStrides) override;

    void Process_vkCmdBlitImage2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkBlitImageInfo2>* pBlitImageInfo) override;

    void Process_vkCmdCopyBuffer2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyBufferInfo2>* pCopyBufferInfo) override;

    void Process_vkCmdCopyBufferToImage2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyBufferToImageInfo2>* pCopyBufferToImageInfo) override;

    void Process_vkCmdCopyImage2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyImageInfo2>* pCopyImageInfo) override;

    void Process_vkCmdCopyImageToBuffer2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyImageToBufferInfo2>* pCopyImageToBufferInfo) override;

    void Process_vkCmdEndRendering(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer) override;

    void Process_vkCmdPipelineBarrier2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDependencyInfo>* pDependencyInfo) override;

    void Process_vkCmdResetEvent2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            event,
        VkPipelineStageFlags2                       stageMask) override;

    void Process_vkCmdResolveImage2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkResolveImageInfo2>* pResolveImageInfo) override;

    void Process_vkCmdSetCullMode(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkCullModeFlags                             cullMode) override;

    void Process_vkCmdSetDepthBiasEnable(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthBiasEnable) override;

    void Process_vkCmdSetDepthBoundsTestEnable(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthBoundsTestEnable) override;

    void Process_vkCmdSetDepthCompareOp(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkCompareOp                                 depthCompareOp) override;

    void Process_vkCmdSetDepthTestEnable(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthTestEnable) override;

    void Process_vkCmdSetDepthWriteEnable(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthWriteEnable) override;

    void Process_vkCmdSetEvent2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            event,
        StructPointerDecoder<Decoded_VkDependencyInfo>* pDependencyInfo) override;

    void Process_vkCmdSetFrontFace(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkFrontFace                                 frontFace) override;

    void Process_vkCmdSetPrimitiveRestartEnable(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    primitiveRestartEnable) override;

    void Process_vkCmdSetPrimitiveTopology(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPrimitiveTopology                         primitiveTopology) override;

    void Process_vkCmdSetRasterizerDiscardEnable(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    rasterizerDiscardEnable) override;

    void Process_vkCmdSetScissorWithCount(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    scissorCount,
        StructPointerDecoder<Decoded_VkRect2D>*     pScissors) override;

    void Process_vkCmdSetStencilOp(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkStencilFaceFlags                          faceMask,
        VkStencilOp                                 failOp,
        VkStencilOp                                 passOp,
        VkStencilOp                                 depthFailOp,
        VkCompareOp                                 compareOp) override;

    void Process_vkCmdSetStencilTestEnable(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    stencilTestEnable) override;

    void Process_vkCmdSetViewportWithCount(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    viewportCount,
        StructPointerDecoder<Decoded_VkViewport>*   pViewports) override;

    void Process_vkCmdWaitEvents2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    eventCount,
        HandlePointerDecoder<VkEvent>*              pEvents,
        StructPointerDecoder<Decoded_VkDependencyInfo>* pDependencyInfos) override;

    void Process_vkCmdWriteTimestamp2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineStageFlags2                       stage,
        format::HandleId                            queryPool,
        uint32_t                                    query) override;

    void Process_vkCreatePrivateDataSlot(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPrivateDataSlotCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkPrivateDataSlot>*    pPrivateDataSlot) override;

    void Process_vkDestroyPrivateDataSlot(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            privateDataSlot,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetDeviceBufferMemoryRequirements(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceBufferMemoryRequirements>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetDeviceImageMemoryRequirements(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceImageMemoryRequirements>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetDeviceImageSparseMemoryRequirements(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceImageMemoryRequirements>* pInfo,
        PointerDecoder<uint32_t>*                   pSparseMemoryRequirementCount,
        StructPointerDecoder<Decoded_VkSparseImageMemoryRequirements2>* pSparseMemoryRequirements) override;

    void Process_vkGetPhysicalDeviceToolProperties(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pToolCount,
        StructPointerDecoder<Decoded_VkPhysicalDeviceToolProperties>* pToolProperties) override;

    void Process_vkGetPrivateData(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        VkObjectType                                objectType,
        uint64_t                                    objectHandle,
        format::HandleId                            privateDataSlot,
        PointerDecoder<uint64_t>*                   pData) override;

    void Process_vkQueueSubmit2(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            queue,
        uint32_t                                    submitCount,
        StructPointerDecoder<Decoded_VkSubmitInfo2>* pSubmits,
        format::HandleId                            fence) override;

    void Process_vkSetPrivateData(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        VkObjectType                                objectType,
        uint64_t                                    objectHandle,
        format::HandleId                            privateDataSlot,
        uint64_t                                    data) override;

    void Process_vkCmdBindDescriptorSets2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkBindDescriptorSetsInfo>* pBindDescriptorSetsInfo) override;

    void Process_vkCmdBindIndexBuffer2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        VkDeviceSize                                size,
        VkIndexType                                 indexType) override;

    void Process_vkCmdPushConstants2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPushConstantsInfo>* pPushConstantsInfo) override;

    void Process_vkCmdPushDescriptorSet(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        format::HandleId                            layout,
        uint32_t                                    set,
        uint32_t                                    descriptorWriteCount,
        StructPointerDecoder<Decoded_VkWriteDescriptorSet>* pDescriptorWrites) override;

    void Process_vkCmdPushDescriptorSet2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPushDescriptorSetInfo>* pPushDescriptorSetInfo) override;

    void Process_vkCmdSetLineStipple(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    lineStippleFactor,
        uint16_t                                    lineStipplePattern) override;

    void Process_vkCmdSetRenderingAttachmentLocations(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderingAttachmentLocationInfo>* pLocationInfo) override;

    void Process_vkCmdSetRenderingInputAttachmentIndices(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderingInputAttachmentIndexInfo>* pInputAttachmentIndexInfo) override;

    void Process_vkCopyImageToImage(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkCopyImageToImageInfo>* pCopyImageToImageInfo) override;

    void Process_vkCopyImageToMemory(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkCopyImageToMemoryInfo>* pCopyImageToMemoryInfo) override;

    void Process_vkCopyMemoryToImage(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkCopyMemoryToImageInfo>* pCopyMemoryToImageInfo) override;

    void Process_vkGetDeviceImageSubresourceLayout(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceImageSubresourceInfo>* pInfo,
        StructPointerDecoder<Decoded_VkSubresourceLayout2>* pLayout) override;

    void Process_vkGetImageSubresourceLayout2(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            image,
        StructPointerDecoder<Decoded_VkImageSubresource2>* pSubresource,
        StructPointerDecoder<Decoded_VkSubresourceLayout2>* pLayout) override;

    void Process_vkGetRenderingAreaGranularity(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkRenderingAreaInfo>* pRenderingAreaInfo,
        StructPointerDecoder<Decoded_VkExtent2D>*   pGranularity) override;

    void Process_vkMapMemory2(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryMapInfo>* pMemoryMapInfo,
        PointerDecoder<uint64_t, void*>*            ppData) override;

    void Process_vkTransitionImageLayout(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    transitionCount,
        StructPointerDecoder<Decoded_VkHostImageLayoutTransitionInfo>* pTransitions) override;

    void Process_vkUnmapMemory2(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryUnmapInfo>* pMemoryUnmapInfo) override;

    void Process_vkDestroySurfaceKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            instance,
        format::HandleId                            surface,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            surface,
        StructPointerDecoder<Decoded_VkSurfaceCapabilitiesKHR>* pSurfaceCapabilities) override;

    void Process_vkGetPhysicalDeviceSurfaceFormatsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            surface,
        PointerDecoder<uint32_t>*                   pSurfaceFormatCount,
        StructPointerDecoder<Decoded_VkSurfaceFormatKHR>* pSurfaceFormats) override;

    void Process_vkGetPhysicalDeviceSurfacePresentModesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            surface,
        PointerDecoder<uint32_t>*                   pPresentModeCount,
        PointerDecoder<VkPresentModeKHR>*           pPresentModes) override;

    void Process_vkGetPhysicalDeviceSurfaceSupportKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        format::HandleId                            surface,
        PointerDecoder<VkBool32>*                   pSupported) override;

    void Process_vkAcquireNextImage2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAcquireNextImageInfoKHR>* pAcquireInfo,
        PointerDecoder<uint32_t>*                   pImageIndex) override;

    void Process_vkAcquireNextImageKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        uint64_t                                    timeout,
        format::HandleId                            semaphore,
        format::HandleId                            fence,
        PointerDecoder<uint32_t>*                   pImageIndex) override;

    void Process_vkCreateSwapchainKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSwapchainCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSwapchainKHR>*       pSwapchain) override;

    void Process_vkDestroySwapchainKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetDeviceGroupPresentCapabilitiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceGroupPresentCapabilitiesKHR>* pDeviceGroupPresentCapabilities) override;

    void Process_vkGetDeviceGroupSurfacePresentModesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            surface,
        PointerDecoder<VkDeviceGroupPresentModeFlagsKHR>* pModes) override;

    void Process_vkGetPhysicalDevicePresentRectanglesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            surface,
        PointerDecoder<uint32_t>*                   pRectCount,
        StructPointerDecoder<Decoded_VkRect2D>*     pRects) override;

    void Process_vkGetSwapchainImagesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        PointerDecoder<uint32_t>*                   pSwapchainImageCount,
        HandlePointerDecoder<VkImage>*              pSwapchainImages) override;

    void Process_vkQueuePresentKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            queue,
        StructPointerDecoder<Decoded_VkPresentInfoKHR>* pPresentInfo) override;

    void Process_vkCreateDisplayModeKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            display,
        StructPointerDecoder<Decoded_VkDisplayModeCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDisplayModeKHR>*     pMode) override;

    void Process_vkCreateDisplayPlaneSurfaceKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkDisplaySurfaceCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkGetDisplayModePropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            display,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkDisplayModePropertiesKHR>* pProperties) override;

    void Process_vkGetDisplayPlaneCapabilitiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            mode,
        uint32_t                                    planeIndex,
        StructPointerDecoder<Decoded_VkDisplayPlaneCapabilitiesKHR>* pCapabilities) override;

    void Process_vkGetDisplayPlaneSupportedDisplaysKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    planeIndex,
        PointerDecoder<uint32_t>*                   pDisplayCount,
        HandlePointerDecoder<VkDisplayKHR>*         pDisplays) override;

    void Process_vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkDisplayPlanePropertiesKHR>* pProperties) override;

    void Process_vkGetPhysicalDeviceDisplayPropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkDisplayPropertiesKHR>* pProperties) override;

    void Process_vkCreateSharedSwapchainsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    swapchainCount,
        StructPointerDecoder<Decoded_VkSwapchainCreateInfoKHR>* pCreateInfos,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSwapchainKHR>*       pSwapchains) override;
#ifdef VK_USE_PLATFORM_XLIB_KHR

    void Process_vkCreateXlibSurfaceKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkXlibSurfaceCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkGetPhysicalDeviceXlibPresentationSupportKHR(
        const ApiCallInfo&                          call_info,
        VkBool32                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        uint64_t                                    dpy,
        size_t                                      visualID) override;
#endif /* VK_USE_PLATFORM_XLIB_KHR */
#ifdef VK_USE_PLATFORM_XCB_KHR

    void Process_vkCreateXcbSurfaceKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkXcbSurfaceCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkGetPhysicalDeviceXcbPresentationSupportKHR(
        const ApiCallInfo&                          call_info,
        VkBool32                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        uint64_t                                    connection,
        uint32_t                                    visual_id) override;
#endif /* VK_USE_PLATFORM_XCB_KHR */
#ifdef VK_USE_PLATFORM_WAYLAND_KHR

    void Process_vkCreateWaylandSurfaceKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkWaylandSurfaceCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkGetPhysicalDeviceWaylandPresentationSupportKHR(
        const ApiCallInfo&                          call_info,
        VkBool32                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        uint64_t                                    display) override;
#endif /* VK_USE_PLATFORM_WAYLAND_KHR */
#ifdef VK_USE_PLATFORM_ANDROID_KHR

    void Process_vkCreateAndroidSurfaceKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkAndroidSurfaceCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;
#endif /* VK_USE_PLATFORM_ANDROID_KHR */
#ifdef VK_USE_PLATFORM_WIN32_KHR

    void Process_vkCreateWin32SurfaceKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkWin32SurfaceCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkGetPhysicalDeviceWin32PresentationSupportKHR(
        const ApiCallInfo&                          call_info,
        VkBool32                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex) override;
#endif /* VK_USE_PLATFORM_WIN32_KHR */

    void Process_vkBindVideoSessionMemoryKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            videoSession,
        uint32_t                                    bindSessionMemoryInfoCount,
        StructPointerDecoder<Decoded_VkBindVideoSessionMemoryInfoKHR>* pBindSessionMemoryInfos) override;

    void Process_vkCmdBeginVideoCodingKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkVideoBeginCodingInfoKHR>* pBeginInfo) override;

    void Process_vkCmdControlVideoCodingKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkVideoCodingControlInfoKHR>* pCodingControlInfo) override;

    void Process_vkCmdEndVideoCodingKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkVideoEndCodingInfoKHR>* pEndCodingInfo) override;

    void Process_vkCreateVideoSessionKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkVideoSessionCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkVideoSessionKHR>*    pVideoSession) override;

    void Process_vkCreateVideoSessionParametersKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkVideoSessionParametersCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkVideoSessionParametersKHR>* pVideoSessionParameters) override;

    void Process_vkDestroyVideoSessionKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            videoSession,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyVideoSessionParametersKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            videoSessionParameters,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetPhysicalDeviceVideoCapabilitiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkVideoProfileInfoKHR>* pVideoProfile,
        StructPointerDecoder<Decoded_VkVideoCapabilitiesKHR>* pCapabilities) override;

    void Process_vkGetPhysicalDeviceVideoFormatPropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceVideoFormatInfoKHR>* pVideoFormatInfo,
        PointerDecoder<uint32_t>*                   pVideoFormatPropertyCount,
        StructPointerDecoder<Decoded_VkVideoFormatPropertiesKHR>* pVideoFormatProperties) override;

    void Process_vkGetVideoSessionMemoryRequirementsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            videoSession,
        PointerDecoder<uint32_t>*                   pMemoryRequirementsCount,
        StructPointerDecoder<Decoded_VkVideoSessionMemoryRequirementsKHR>* pMemoryRequirements) override;

    void Process_vkUpdateVideoSessionParametersKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            videoSessionParameters,
        StructPointerDecoder<Decoded_VkVideoSessionParametersUpdateInfoKHR>* pUpdateInfo) override;

    void Process_vkCmdDecodeVideoKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkVideoDecodeInfoKHR>* pDecodeInfo) override;

    void Process_vkCmdBeginRenderingKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderingInfo>* pRenderingInfo) override;

    void Process_vkCmdEndRenderingKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer) override;

    void Process_vkGetPhysicalDeviceFeatures2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceFeatures2>* pFeatures) override;

    void Process_vkGetPhysicalDeviceFormatProperties2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        VkFormat                                    format,
        StructPointerDecoder<Decoded_VkFormatProperties2>* pFormatProperties) override;

    void Process_vkGetPhysicalDeviceImageFormatProperties2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceImageFormatInfo2>* pImageFormatInfo,
        StructPointerDecoder<Decoded_VkImageFormatProperties2>* pImageFormatProperties) override;

    void Process_vkGetPhysicalDeviceMemoryProperties2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceMemoryProperties2>* pMemoryProperties) override;

    void Process_vkGetPhysicalDeviceProperties2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceProperties2>* pProperties) override;

    void Process_vkGetPhysicalDeviceQueueFamilyProperties2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pQueueFamilyPropertyCount,
        StructPointerDecoder<Decoded_VkQueueFamilyProperties2>* pQueueFamilyProperties) override;

    void Process_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceSparseImageFormatInfo2>* pFormatInfo,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkSparseImageFormatProperties2>* pProperties) override;

    void Process_vkCmdDispatchBaseKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    baseGroupX,
        uint32_t                                    baseGroupY,
        uint32_t                                    baseGroupZ,
        uint32_t                                    groupCountX,
        uint32_t                                    groupCountY,
        uint32_t                                    groupCountZ) override;

    void Process_vkCmdSetDeviceMaskKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    deviceMask) override;

    void Process_vkGetDeviceGroupPeerMemoryFeaturesKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        uint32_t                                    heapIndex,
        uint32_t                                    localDeviceIndex,
        uint32_t                                    remoteDeviceIndex,
        PointerDecoder<VkPeerMemoryFeatureFlags>*   pPeerMemoryFeatures) override;

    void Process_vkTrimCommandPoolKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            commandPool,
        VkCommandPoolTrimFlags                      flags) override;

    void Process_vkEnumeratePhysicalDeviceGroupsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        PointerDecoder<uint32_t>*                   pPhysicalDeviceGroupCount,
        StructPointerDecoder<Decoded_VkPhysicalDeviceGroupProperties>* pPhysicalDeviceGroupProperties) override;

    void Process_vkGetPhysicalDeviceExternalBufferPropertiesKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceExternalBufferInfo>* pExternalBufferInfo,
        StructPointerDecoder<Decoded_VkExternalBufferProperties>* pExternalBufferProperties) override;
#ifdef VK_USE_PLATFORM_WIN32_KHR

    void Process_vkGetMemoryWin32HandleKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryGetWin32HandleInfoKHR>* pGetWin32HandleInfo,
        PointerDecoder<uint64_t, void*>*            pHandle) override;

    void Process_vkGetMemoryWin32HandlePropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        VkExternalMemoryHandleTypeFlagBits          handleType,
        uint64_t                                    handle,
        StructPointerDecoder<Decoded_VkMemoryWin32HandlePropertiesKHR>* pMemoryWin32HandleProperties) override;
#endif /* VK_USE_PLATFORM_WIN32_KHR */

    void Process_vkGetMemoryFdKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryGetFdInfoKHR>* pGetFdInfo,
        PointerDecoder<int>*                        pFd) override;

    void Process_vkGetMemoryFdPropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        VkExternalMemoryHandleTypeFlagBits          handleType,
        int                                         fd,
        StructPointerDecoder<Decoded_VkMemoryFdPropertiesKHR>* pMemoryFdProperties) override;
#ifdef VK_USE_PLATFORM_WIN32_KHR
#endif /* VK_USE_PLATFORM_WIN32_KHR */

    void Process_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceExternalSemaphoreInfo>* pExternalSemaphoreInfo,
        StructPointerDecoder<Decoded_VkExternalSemaphoreProperties>* pExternalSemaphoreProperties) override;
#ifdef VK_USE_PLATFORM_WIN32_KHR

    void Process_vkGetSemaphoreWin32HandleKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSemaphoreGetWin32HandleInfoKHR>* pGetWin32HandleInfo,
        PointerDecoder<uint64_t, void*>*            pHandle) override;

    void Process_vkImportSemaphoreWin32HandleKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImportSemaphoreWin32HandleInfoKHR>* pImportSemaphoreWin32HandleInfo) override;
#endif /* VK_USE_PLATFORM_WIN32_KHR */

    void Process_vkGetSemaphoreFdKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSemaphoreGetFdInfoKHR>* pGetFdInfo,
        PointerDecoder<int>*                        pFd) override;

    void Process_vkImportSemaphoreFdKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImportSemaphoreFdInfoKHR>* pImportSemaphoreFdInfo) override;

    void Process_vkCmdPushDescriptorSetKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        format::HandleId                            layout,
        uint32_t                                    set,
        uint32_t                                    descriptorWriteCount,
        StructPointerDecoder<Decoded_VkWriteDescriptorSet>* pDescriptorWrites) override;

    void Process_vkUpdateDescriptorSetWithTemplateKHR(
        const ApiCallInfo&               call_info,
        format::HandleId                 device,
        format::HandleId                 descriptorSet,
        format::HandleId                 descriptorUpdateTemplate,
        DescriptorUpdateTemplateDecoder* pData) override;

    void Process_vkCreateDescriptorUpdateTemplateKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorUpdateTemplateCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDescriptorUpdateTemplate>* pDescriptorUpdateTemplate) override;

    void Process_vkDestroyDescriptorUpdateTemplateKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            descriptorUpdateTemplate,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkCmdBeginRenderPass2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderPassBeginInfo>* pRenderPassBegin,
        StructPointerDecoder<Decoded_VkSubpassBeginInfo>* pSubpassBeginInfo) override;

    void Process_vkCmdEndRenderPass2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkSubpassEndInfo>* pSubpassEndInfo) override;

    void Process_vkCmdNextSubpass2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkSubpassBeginInfo>* pSubpassBeginInfo,
        StructPointerDecoder<Decoded_VkSubpassEndInfo>* pSubpassEndInfo) override;

    void Process_vkCreateRenderPass2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkRenderPassCreateInfo2>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkRenderPass>*         pRenderPass) override;

    void Process_vkGetSwapchainStatusKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain) override;

    void Process_vkGetPhysicalDeviceExternalFencePropertiesKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceExternalFenceInfo>* pExternalFenceInfo,
        StructPointerDecoder<Decoded_VkExternalFenceProperties>* pExternalFenceProperties) override;
#ifdef VK_USE_PLATFORM_WIN32_KHR

    void Process_vkGetFenceWin32HandleKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkFenceGetWin32HandleInfoKHR>* pGetWin32HandleInfo,
        PointerDecoder<uint64_t, void*>*            pHandle) override;

    void Process_vkImportFenceWin32HandleKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImportFenceWin32HandleInfoKHR>* pImportFenceWin32HandleInfo) override;
#endif /* VK_USE_PLATFORM_WIN32_KHR */

    void Process_vkGetFenceFdKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkFenceGetFdInfoKHR>* pGetFdInfo,
        PointerDecoder<int>*                        pFd) override;

    void Process_vkImportFenceFdKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImportFenceFdInfoKHR>* pImportFenceFdInfo) override;

    void Process_vkAcquireProfilingLockKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAcquireProfilingLockInfoKHR>* pInfo) override;

    void Process_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        PointerDecoder<uint32_t>*                   pCounterCount,
        StructPointerDecoder<Decoded_VkPerformanceCounterKHR>* pCounters,
        StructPointerDecoder<Decoded_VkPerformanceCounterDescriptionKHR>* pCounterDescriptions) override;

    void Process_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkQueryPoolPerformanceCreateInfoKHR>* pPerformanceQueryCreateInfo,
        PointerDecoder<uint32_t>*                   pNumPasses) override;

    void Process_vkReleaseProfilingLockKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device) override;

    void Process_vkGetPhysicalDeviceSurfaceCapabilities2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceSurfaceInfo2KHR>* pSurfaceInfo,
        StructPointerDecoder<Decoded_VkSurfaceCapabilities2KHR>* pSurfaceCapabilities) override;

    void Process_vkGetPhysicalDeviceSurfaceFormats2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceSurfaceInfo2KHR>* pSurfaceInfo,
        PointerDecoder<uint32_t>*                   pSurfaceFormatCount,
        StructPointerDecoder<Decoded_VkSurfaceFormat2KHR>* pSurfaceFormats) override;

    void Process_vkGetDisplayModeProperties2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            display,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkDisplayModeProperties2KHR>* pProperties) override;

    void Process_vkGetDisplayPlaneCapabilities2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkDisplayPlaneInfo2KHR>* pDisplayPlaneInfo,
        StructPointerDecoder<Decoded_VkDisplayPlaneCapabilities2KHR>* pCapabilities) override;

    void Process_vkGetPhysicalDeviceDisplayPlaneProperties2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkDisplayPlaneProperties2KHR>* pProperties) override;

    void Process_vkGetPhysicalDeviceDisplayProperties2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkDisplayProperties2KHR>* pProperties) override;

    void Process_vkGetBufferMemoryRequirements2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferMemoryRequirementsInfo2>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetImageMemoryRequirements2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImageMemoryRequirementsInfo2>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetImageSparseMemoryRequirements2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImageSparseMemoryRequirementsInfo2>* pInfo,
        PointerDecoder<uint32_t>*                   pSparseMemoryRequirementCount,
        StructPointerDecoder<Decoded_VkSparseImageMemoryRequirements2>* pSparseMemoryRequirements) override;

    void Process_vkCreateSamplerYcbcrConversionKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSamplerYcbcrConversionCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSamplerYcbcrConversion>* pYcbcrConversion) override;

    void Process_vkDestroySamplerYcbcrConversionKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            ycbcrConversion,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkBindBufferMemory2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    bindInfoCount,
        StructPointerDecoder<Decoded_VkBindBufferMemoryInfo>* pBindInfos) override;

    void Process_vkBindImageMemory2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    bindInfoCount,
        StructPointerDecoder<Decoded_VkBindImageMemoryInfo>* pBindInfos) override;

    void Process_vkGetDescriptorSetLayoutSupportKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorSetLayoutCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkDescriptorSetLayoutSupport>* pSupport) override;

    void Process_vkCmdDrawIndexedIndirectCountKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        format::HandleId                            countBuffer,
        VkDeviceSize                                countBufferOffset,
        uint32_t                                    maxDrawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdDrawIndirectCountKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        format::HandleId                            countBuffer,
        VkDeviceSize                                countBufferOffset,
        uint32_t                                    maxDrawCount,
        uint32_t                                    stride) override;

    void Process_vkGetSemaphoreCounterValueKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            semaphore,
        PointerDecoder<uint64_t>*                   pValue) override;

    void Process_vkSignalSemaphoreKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSemaphoreSignalInfo>* pSignalInfo) override;

    void Process_vkWaitSemaphoresKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSemaphoreWaitInfo>* pWaitInfo,
        uint64_t                                    timeout) override;

    void Process_vkCmdSetFragmentShadingRateKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkExtent2D>*   pFragmentSize,
        PointerDecoder<VkFragmentShadingRateCombinerOpKHR>* combinerOps) override;

    void Process_vkGetPhysicalDeviceFragmentShadingRatesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pFragmentShadingRateCount,
        StructPointerDecoder<Decoded_VkPhysicalDeviceFragmentShadingRateKHR>* pFragmentShadingRates) override;

    void Process_vkCmdSetRenderingAttachmentLocationsKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderingAttachmentLocationInfo>* pLocationInfo) override;

    void Process_vkCmdSetRenderingInputAttachmentIndicesKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderingInputAttachmentIndexInfo>* pInputAttachmentIndexInfo) override;

    void Process_vkWaitForPresentKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        uint64_t                                    presentId,
        uint64_t                                    timeout) override;

    void Process_vkGetBufferDeviceAddressKHR(
        const ApiCallInfo&                          call_info,
        VkDeviceAddress                             returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    void Process_vkGetBufferOpaqueCaptureAddressKHR(
        const ApiCallInfo&                          call_info,
        uint64_t                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    void Process_vkGetDeviceMemoryOpaqueCaptureAddressKHR(
        const ApiCallInfo&                          call_info,
        uint64_t                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceMemoryOpaqueCaptureAddressInfo>* pInfo) override;

    void Process_vkCreateDeferredOperationKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDeferredOperationKHR>* pDeferredOperation) override;

    void Process_vkDeferredOperationJoinKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            operation) override;

    void Process_vkDestroyDeferredOperationKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            operation,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetDeferredOperationMaxConcurrencyKHR(
        const ApiCallInfo&                          call_info,
        uint32_t                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            operation) override;

    void Process_vkGetDeferredOperationResultKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            operation) override;

    void Process_vkGetPipelineExecutableInternalRepresentationsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineExecutableInfoKHR>* pExecutableInfo,
        PointerDecoder<uint32_t>*                   pInternalRepresentationCount,
        StructPointerDecoder<Decoded_VkPipelineExecutableInternalRepresentationKHR>* pInternalRepresentations) override;

    void Process_vkGetPipelineExecutablePropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineInfoKHR>* pPipelineInfo,
        PointerDecoder<uint32_t>*                   pExecutableCount,
        StructPointerDecoder<Decoded_VkPipelineExecutablePropertiesKHR>* pProperties) override;

    void Process_vkGetPipelineExecutableStatisticsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineExecutableInfoKHR>* pExecutableInfo,
        PointerDecoder<uint32_t>*                   pStatisticCount,
        StructPointerDecoder<Decoded_VkPipelineExecutableStatisticKHR>* pStatistics) override;

    void Process_vkMapMemory2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryMapInfo>* pMemoryMapInfo,
        PointerDecoder<uint64_t, void*>*            ppData) override;

    void Process_vkUnmapMemory2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryUnmapInfo>* pMemoryUnmapInfo) override;

    void Process_vkCmdEncodeVideoKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkVideoEncodeInfoKHR>* pEncodeInfo) override;

    void Process_vkGetEncodedVideoSessionParametersKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkVideoEncodeSessionParametersGetInfoKHR>* pVideoSessionParametersInfo,
        StructPointerDecoder<Decoded_VkVideoEncodeSessionParametersFeedbackInfoKHR>* pFeedbackInfo,
        PointerDecoder<size_t>*                     pDataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR>* pQualityLevelInfo,
        StructPointerDecoder<Decoded_VkVideoEncodeQualityLevelPropertiesKHR>* pQualityLevelProperties) override;

    void Process_vkCmdPipelineBarrier2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDependencyInfo>* pDependencyInfo) override;

    void Process_vkCmdResetEvent2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            event,
        VkPipelineStageFlags2                       stageMask) override;

    void Process_vkCmdSetEvent2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            event,
        StructPointerDecoder<Decoded_VkDependencyInfo>* pDependencyInfo) override;

    void Process_vkCmdWaitEvents2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    eventCount,
        HandlePointerDecoder<VkEvent>*              pEvents,
        StructPointerDecoder<Decoded_VkDependencyInfo>* pDependencyInfos) override;

    void Process_vkCmdWriteTimestamp2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineStageFlags2                       stage,
        format::HandleId                            queryPool,
        uint32_t                                    query) override;

    void Process_vkQueueSubmit2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            queue,
        uint32_t                                    submitCount,
        StructPointerDecoder<Decoded_VkSubmitInfo2>* pSubmits,
        format::HandleId                            fence) override;

    void Process_vkCmdBeginConditionalRendering2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkConditionalRenderingBeginInfo2EXT>* pConditionalRenderingBegin) override;

    void Process_vkCmdBeginTransformFeedback2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstCounterRange,
        uint32_t                                    counterRangeCount,
        StructPointerDecoder<Decoded_VkBindTransformFeedbackBuffer2InfoEXT>* pCounterInfos) override;

    void Process_vkCmdBindIndexBuffer3KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkBindIndexBuffer3InfoKHR>* pInfo) override;

    void Process_vkCmdBindTransformFeedbackBuffers2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstBinding,
        uint32_t                                    bindingCount,
        StructPointerDecoder<Decoded_VkBindTransformFeedbackBuffer2InfoEXT>* pBindingInfos) override;

    void Process_vkCmdBindVertexBuffers3KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstBinding,
        uint32_t                                    bindingCount,
        StructPointerDecoder<Decoded_VkBindVertexBuffer3InfoKHR>* pBindingInfos) override;

    void Process_vkCmdCopyImageToMemoryKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyDeviceMemoryImageInfoKHR>* pCopyMemoryInfo) override;

    void Process_vkCmdCopyMemoryKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyDeviceMemoryInfoKHR>* pCopyMemoryInfo) override;

    void Process_vkCmdCopyMemoryToImageKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyDeviceMemoryImageInfoKHR>* pCopyMemoryInfo) override;

    void Process_vkCmdCopyQueryPoolResultsToMemoryKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery,
        uint32_t                                    queryCount,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRangeKHR>* pDstRange,
        VkAddressCommandFlagsKHR                    dstFlags,
        VkQueryResultFlags                          queryResultFlags) override;

    void Process_vkCmdDispatchIndirect2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDispatchIndirect2InfoKHR>* pInfo) override;

    void Process_vkCmdDrawIndexedIndirect2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDrawIndirect2InfoKHR>* pInfo) override;

    void Process_vkCmdDrawIndexedIndirectCount2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDrawIndirectCount2InfoKHR>* pInfo) override;

    void Process_vkCmdDrawIndirect2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDrawIndirect2InfoKHR>* pInfo) override;

    void Process_vkCmdDrawIndirectByteCount2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    instanceCount,
        uint32_t                                    firstInstance,
        StructPointerDecoder<Decoded_VkBindTransformFeedbackBuffer2InfoEXT>* pCounterInfo,
        uint32_t                                    counterOffset,
        uint32_t                                    vertexStride) override;

    void Process_vkCmdDrawIndirectCount2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDrawIndirectCount2InfoKHR>* pInfo) override;

    void Process_vkCmdDrawMeshTasksIndirect2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDrawIndirect2InfoKHR>* pInfo) override;

    void Process_vkCmdDrawMeshTasksIndirectCount2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDrawIndirectCount2InfoKHR>* pInfo) override;

    void Process_vkCmdEndTransformFeedback2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstCounterRange,
        uint32_t                                    counterRangeCount,
        StructPointerDecoder<Decoded_VkBindTransformFeedbackBuffer2InfoEXT>* pCounterInfos) override;

    void Process_vkCmdFillMemoryKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDeviceAddressRangeKHR>* pDstRange,
        VkAddressCommandFlagsKHR                    dstFlags,
        uint32_t                                    data) override;

    void Process_vkCmdUpdateMemoryKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDeviceAddressRangeKHR>* pDstRange,
        VkAddressCommandFlagsKHR                    dstFlags,
        VkDeviceSize                                dataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkCmdWriteMarkerToMemoryAMD(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkMemoryMarkerInfoAMD>* pInfo) override;

    void Process_vkCreateAccelerationStructure2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAccelerationStructureCreateInfo2KHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructure) override;

    void Process_vkCmdBlitImage2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkBlitImageInfo2>* pBlitImageInfo) override;

    void Process_vkCmdCopyBuffer2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyBufferInfo2>* pCopyBufferInfo) override;

    void Process_vkCmdCopyBufferToImage2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyBufferToImageInfo2>* pCopyBufferToImageInfo) override;

    void Process_vkCmdCopyImage2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyImageInfo2>* pCopyImageInfo) override;

    void Process_vkCmdCopyImageToBuffer2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyImageToBufferInfo2>* pCopyImageToBufferInfo) override;

    void Process_vkCmdResolveImage2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkResolveImageInfo2>* pResolveImageInfo) override;

    void Process_vkCmdTraceRaysIndirect2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkDeviceAddress                             indirectDeviceAddress) override;

    void Process_vkGetDeviceBufferMemoryRequirementsKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceBufferMemoryRequirements>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetDeviceImageMemoryRequirementsKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceImageMemoryRequirements>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetDeviceImageSparseMemoryRequirementsKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceImageMemoryRequirements>* pInfo,
        PointerDecoder<uint32_t>*                   pSparseMemoryRequirementCount,
        StructPointerDecoder<Decoded_VkSparseImageMemoryRequirements2>* pSparseMemoryRequirements) override;

    void Process_vkCmdBindIndexBuffer2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        VkDeviceSize                                size,
        VkIndexType                                 indexType) override;

    void Process_vkGetDeviceImageSubresourceLayoutKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceImageSubresourceInfo>* pInfo,
        StructPointerDecoder<Decoded_VkSubresourceLayout2>* pLayout) override;

    void Process_vkGetImageSubresourceLayout2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            image,
        StructPointerDecoder<Decoded_VkImageSubresource2>* pSubresource,
        StructPointerDecoder<Decoded_VkSubresourceLayout2>* pLayout) override;

    void Process_vkGetRenderingAreaGranularityKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkRenderingAreaInfo>* pRenderingAreaInfo,
        StructPointerDecoder<Decoded_VkExtent2D>*   pGranularity) override;

    void Process_vkWaitForPresent2KHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkPresentWait2InfoKHR>* pPresentWait2Info) override;

    void Process_vkCreatePipelineBinariesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineBinaryCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        StructPointerDecoder<Decoded_VkPipelineBinaryHandlesInfoKHR>* pBinaries) override;

    void Process_vkDestroyPipelineBinaryKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            pipelineBinary,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetPipelineBinaryDataKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineBinaryDataInfoKHR>* pInfo,
        StructPointerDecoder<Decoded_VkPipelineBinaryKeyKHR>* pPipelineBinaryKey,
        PointerDecoder<size_t>*                     pPipelineBinaryDataSize,
        PointerDecoder<uint8_t>*                    pPipelineBinaryData) override;

    void Process_vkGetPipelineKeyKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineCreateInfoKHR>* pPipelineCreateInfo,
        StructPointerDecoder<Decoded_VkPipelineBinaryKeyKHR>* pPipelineKey) override;

    void Process_vkReleaseCapturedPipelineDataKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkReleaseCapturedPipelineDataInfoKHR>* pInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkReleaseSwapchainImagesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkReleaseSwapchainImagesInfoKHR>* pReleaseInfo) override;

    void Process_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkCooperativeMatrixPropertiesKHR>* pProperties) override;

    void Process_vkCmdSetLineStippleKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    lineStippleFactor,
        uint16_t                                    lineStipplePattern) override;

    void Process_vkGetCalibratedTimestampsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    timestampCount,
        StructPointerDecoder<Decoded_VkCalibratedTimestampInfoKHR>* pTimestampInfos,
        PointerDecoder<uint64_t>*                   pTimestamps,
        PointerDecoder<uint64_t>*                   pMaxDeviation) override;

    void Process_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pTimeDomainCount,
        PointerDecoder<VkTimeDomainKHR>*            pTimeDomains) override;

    void Process_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkBindDescriptorBufferEmbeddedSamplersInfoEXT>* pBindDescriptorBufferEmbeddedSamplersInfo) override;

    void Process_vkCmdBindDescriptorSets2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkBindDescriptorSetsInfo>* pBindDescriptorSetsInfo) override;

    void Process_vkCmdPushConstants2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPushConstantsInfo>* pPushConstantsInfo) override;

    void Process_vkCmdPushDescriptorSet2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPushDescriptorSetInfo>* pPushDescriptorSetInfo) override;

    void Process_vkCmdSetDescriptorBufferOffsets2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkSetDescriptorBufferOffsetsInfoEXT>* pSetDescriptorBufferOffsetsInfo) override;

    void Process_vkCmdCopyMemoryIndirectKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyMemoryIndirectInfoKHR>* pCopyMemoryIndirectInfo) override;

    void Process_vkCmdCopyMemoryToImageIndirectKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyMemoryToImageIndirectInfoKHR>* pCopyMemoryToImageIndirectInfo) override;

    void Process_vkGetDeviceFaultDebugInfoKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceFaultDebugInfoKHR>* pDebugInfo) override;

    void Process_vkGetDeviceFaultReportsKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint64_t                                    timeout,
        PointerDecoder<uint32_t>*                   pFaultCounts,
        StructPointerDecoder<Decoded_VkDeviceFaultInfoKHR>* pFaultInfo) override;

    void Process_vkCmdEndRendering2KHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderingEndInfoKHR>* pRenderingEndInfo) override;
#ifdef VK_USE_PLATFORM_ANDROID_KHR

    void Process_vkFrameBoundaryANDROID(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            semaphore,
        format::HandleId                            image) override;
#endif /* VK_USE_PLATFORM_ANDROID_KHR */

    void Process_vkCreateDebugReportCallbackEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkDebugReportCallbackCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDebugReportCallbackEXT>* pCallback) override;

    void Process_vkDebugReportMessageEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            instance,
        VkDebugReportFlagsEXT                       flags,
        VkDebugReportObjectTypeEXT                  objectType,
        uint64_t                                    object,
        size_t                                      location,
        int32_t                                     messageCode,
        StringDecoder*                              pLayerPrefix,
        StringDecoder*                              pMessage) override;

    void Process_vkDestroyDebugReportCallbackEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            instance,
        format::HandleId                            callback,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkCmdDebugMarkerBeginEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDebugMarkerMarkerInfoEXT>* pMarkerInfo) override;

    void Process_vkCmdDebugMarkerEndEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer) override;

    void Process_vkCmdDebugMarkerInsertEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDebugMarkerMarkerInfoEXT>* pMarkerInfo) override;

    void Process_vkDebugMarkerSetObjectNameEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDebugMarkerObjectNameInfoEXT>* pNameInfo) override;

    void Process_vkDebugMarkerSetObjectTagEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDebugMarkerObjectTagInfoEXT>* pTagInfo) override;

    void Process_vkCmdBeginQueryIndexedEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            queryPool,
        uint32_t                                    query,
        VkQueryControlFlags                         flags,
        uint32_t                                    index) override;

    void Process_vkCmdBeginTransformFeedbackEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstCounterBuffer,
        uint32_t                                    counterBufferCount,
        HandlePointerDecoder<VkBuffer>*             pCounterBuffers,
        PointerDecoder<VkDeviceSize>*               pCounterBufferOffsets) override;

    void Process_vkCmdBindTransformFeedbackBuffersEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstBinding,
        uint32_t                                    bindingCount,
        HandlePointerDecoder<VkBuffer>*             pBuffers,
        PointerDecoder<VkDeviceSize>*               pOffsets,
        PointerDecoder<VkDeviceSize>*               pSizes) override;

    void Process_vkCmdDrawIndirectByteCountEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    instanceCount,
        uint32_t                                    firstInstance,
        format::HandleId                            counterBuffer,
        VkDeviceSize                                counterBufferOffset,
        uint32_t                                    counterOffset,
        uint32_t                                    vertexStride) override;

    void Process_vkCmdEndQueryIndexedEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            queryPool,
        uint32_t                                    query,
        uint32_t                                    index) override;

    void Process_vkCmdEndTransformFeedbackEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstCounterBuffer,
        uint32_t                                    counterBufferCount,
        HandlePointerDecoder<VkBuffer>*             pCounterBuffers,
        PointerDecoder<VkDeviceSize>*               pCounterBufferOffsets) override;

    void Process_vkGetDeviceCombinedImageSamplerIndexNVX(
        const ApiCallInfo&                          call_info,
        uint64_t                                    returnValue,
        format::HandleId                            device,
        uint64_t                                    imageViewIndex,
        uint64_t                                    samplerIndex) override;

    void Process_vkGetImageViewAddressNVX(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            imageView,
        StructPointerDecoder<Decoded_VkImageViewAddressPropertiesNVX>* pProperties) override;

    void Process_vkGetImageViewHandle64NVX(
        const ApiCallInfo&                          call_info,
        uint64_t                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImageViewHandleInfoNVX>* pInfo) override;

    void Process_vkGetImageViewHandleNVX(
        const ApiCallInfo&                          call_info,
        uint32_t                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImageViewHandleInfoNVX>* pInfo) override;

    void Process_vkCmdDrawIndexedIndirectCountAMD(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        format::HandleId                            countBuffer,
        VkDeviceSize                                countBufferOffset,
        uint32_t                                    maxDrawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdDrawIndirectCountAMD(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        format::HandleId                            countBuffer,
        VkDeviceSize                                countBufferOffset,
        uint32_t                                    maxDrawCount,
        uint32_t                                    stride) override;

    void Process_vkGetShaderInfoAMD(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipeline,
        VkShaderStageFlagBits                       shaderStage,
        VkShaderInfoTypeAMD                         infoType,
        PointerDecoder<size_t>*                     pInfoSize,
        PointerDecoder<uint8_t>*                    pInfo) override;
#ifdef VK_USE_PLATFORM_GGP

    void Process_vkCreateStreamDescriptorSurfaceGGP(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkStreamDescriptorSurfaceCreateInfoGGP>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;
#endif /* VK_USE_PLATFORM_GGP */

    void Process_vkGetPhysicalDeviceExternalImageFormatPropertiesNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        VkFormat                                    format,
        VkImageType                                 type,
        VkImageTiling                               tiling,
        VkImageUsageFlags                           usage,
        VkImageCreateFlags                          flags,
        VkExternalMemoryHandleTypeFlagsNV           externalHandleType,
        StructPointerDecoder<Decoded_VkExternalImageFormatPropertiesNV>* pExternalImageFormatProperties) override;
#ifdef VK_USE_PLATFORM_WIN32_KHR

    void Process_vkGetMemoryWin32HandleNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            memory,
        VkExternalMemoryHandleTypeFlagsNV           handleType,
        PointerDecoder<uint64_t, void*>*            pHandle) override;
#endif /* VK_USE_PLATFORM_WIN32_KHR */
#ifdef VK_USE_PLATFORM_WIN32_KHR
#endif /* VK_USE_PLATFORM_WIN32_KHR */
#ifdef VK_USE_PLATFORM_VI_NN

    void Process_vkCreateViSurfaceNN(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkViSurfaceCreateInfoNN>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;
#endif /* VK_USE_PLATFORM_VI_NN */

    void Process_vkCmdBeginConditionalRenderingEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkConditionalRenderingBeginInfoEXT>* pConditionalRenderingBegin) override;

    void Process_vkCmdEndConditionalRenderingEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer) override;

    void Process_vkCmdSetViewportWScalingNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstViewport,
        uint32_t                                    viewportCount,
        StructPointerDecoder<Decoded_VkViewportWScalingNV>* pViewportWScalings) override;

    void Process_vkReleaseDisplayEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            display) override;
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT

    void Process_vkAcquireXlibDisplayEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint64_t                                    dpy,
        format::HandleId                            display) override;

    void Process_vkGetRandROutputDisplayEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint64_t                                    dpy,
        size_t                                      rrOutput,
        HandlePointerDecoder<VkDisplayKHR>*         pDisplay) override;
#endif /* VK_USE_PLATFORM_XLIB_XRANDR_EXT */

    void Process_vkGetPhysicalDeviceSurfaceCapabilities2EXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            surface,
        StructPointerDecoder<Decoded_VkSurfaceCapabilities2EXT>* pSurfaceCapabilities) override;

    void Process_vkDisplayPowerControlEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            display,
        StructPointerDecoder<Decoded_VkDisplayPowerInfoEXT>* pDisplayPowerInfo) override;

    void Process_vkGetSwapchainCounterEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        VkSurfaceCounterFlagBitsEXT                 counter,
        PointerDecoder<uint64_t>*                   pCounterValue) override;

    void Process_vkRegisterDeviceEventEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceEventInfoEXT>* pDeviceEventInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkFence>*              pFence) override;

    void Process_vkRegisterDisplayEventEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            display,
        StructPointerDecoder<Decoded_VkDisplayEventInfoEXT>* pDisplayEventInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkFence>*              pFence) override;

    void Process_vkGetPastPresentationTimingGOOGLE(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        PointerDecoder<uint32_t>*                   pPresentationTimingCount,
        StructPointerDecoder<Decoded_VkPastPresentationTimingGOOGLE>* pPresentationTimings) override;

    void Process_vkGetRefreshCycleDurationGOOGLE(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkRefreshCycleDurationGOOGLE>* pDisplayTimingProperties) override;

    void Process_vkCmdSetDiscardRectangleEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstDiscardRectangle,
        uint32_t                                    discardRectangleCount,
        StructPointerDecoder<Decoded_VkRect2D>*     pDiscardRectangles) override;

    void Process_vkCmdSetDiscardRectangleEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    discardRectangleEnable) override;

    void Process_vkCmdSetDiscardRectangleModeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkDiscardRectangleModeEXT                   discardRectangleMode) override;

    void Process_vkSetHdrMetadataEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        uint32_t                                    swapchainCount,
        HandlePointerDecoder<VkSwapchainKHR>*       pSwapchains,
        StructPointerDecoder<Decoded_VkHdrMetadataEXT>* pMetadata) override;
#ifdef VK_USE_PLATFORM_IOS_MVK

    void Process_vkCreateIOSSurfaceMVK(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkIOSSurfaceCreateInfoMVK>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;
#endif /* VK_USE_PLATFORM_IOS_MVK */
#ifdef VK_USE_PLATFORM_MACOS_MVK

    void Process_vkCreateMacOSSurfaceMVK(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkMacOSSurfaceCreateInfoMVK>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;
#endif /* VK_USE_PLATFORM_MACOS_MVK */

    void Process_vkCmdBeginDebugUtilsLabelEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDebugUtilsLabelEXT>* pLabelInfo) override;

    void Process_vkCmdEndDebugUtilsLabelEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer) override;

    void Process_vkCmdInsertDebugUtilsLabelEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDebugUtilsLabelEXT>* pLabelInfo) override;

    void Process_vkCreateDebugUtilsMessengerEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkDebugUtilsMessengerCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDebugUtilsMessengerEXT>* pMessenger) override;

    void Process_vkDestroyDebugUtilsMessengerEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            instance,
        format::HandleId                            messenger,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkQueueBeginDebugUtilsLabelEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            queue,
        StructPointerDecoder<Decoded_VkDebugUtilsLabelEXT>* pLabelInfo) override;

    void Process_vkQueueEndDebugUtilsLabelEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            queue) override;

    void Process_vkQueueInsertDebugUtilsLabelEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            queue,
        StructPointerDecoder<Decoded_VkDebugUtilsLabelEXT>* pLabelInfo) override;

    void Process_vkSetDebugUtilsObjectNameEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDebugUtilsObjectNameInfoEXT>* pNameInfo) override;

    void Process_vkSetDebugUtilsObjectTagEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDebugUtilsObjectTagInfoEXT>* pTagInfo) override;

    void Process_vkSubmitDebugUtilsMessageEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            instance,
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
        StructPointerDecoder<Decoded_VkDebugUtilsMessengerCallbackDataEXT>* pCallbackData) override;
#ifdef VK_USE_PLATFORM_ANDROID_KHR

    void Process_vkGetAndroidHardwareBufferPropertiesANDROID(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint64_t                                    buffer,
        StructPointerDecoder<Decoded_VkAndroidHardwareBufferPropertiesANDROID>* pProperties) override;

    void Process_vkGetMemoryAndroidHardwareBufferANDROID(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryGetAndroidHardwareBufferInfoANDROID>* pInfo,
        PointerDecoder<uint64_t, void*>*            pBuffer) override;
#endif /* VK_USE_PLATFORM_ANDROID_KHR */

    void Process_vkCmdSetSampleLocationsEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkSampleLocationsInfoEXT>* pSampleLocationsInfo) override;

    void Process_vkGetPhysicalDeviceMultisamplePropertiesEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        VkSampleCountFlagBits                       samples,
        StructPointerDecoder<Decoded_VkMultisamplePropertiesEXT>* pMultisampleProperties) override;

    void Process_vkGetImageDrmFormatModifierPropertiesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            image,
        StructPointerDecoder<Decoded_VkImageDrmFormatModifierPropertiesEXT>* pProperties) override;

    void Process_vkCreateValidationCacheEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkValidationCacheCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkValidationCacheEXT>* pValidationCache) override;

    void Process_vkDestroyValidationCacheEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            validationCache,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetValidationCacheDataEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            validationCache,
        PointerDecoder<size_t>*                     pDataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkMergeValidationCachesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            dstCache,
        uint32_t                                    srcCacheCount,
        HandlePointerDecoder<VkValidationCacheEXT>* pSrcCaches) override;

    void Process_vkCmdBindShadingRateImageNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            imageView,
        VkImageLayout                               imageLayout) override;

    void Process_vkCmdSetCoarseSampleOrderNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkCoarseSampleOrderTypeNV                   sampleOrderType,
        uint32_t                                    customSampleOrderCount,
        StructPointerDecoder<Decoded_VkCoarseSampleOrderCustomNV>* pCustomSampleOrders) override;

    void Process_vkCmdSetViewportShadingRatePaletteNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstViewport,
        uint32_t                                    viewportCount,
        StructPointerDecoder<Decoded_VkShadingRatePaletteNV>* pShadingRatePalettes) override;

    void Process_vkBindAccelerationStructureMemoryNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    bindInfoCount,
        StructPointerDecoder<Decoded_VkBindAccelerationStructureMemoryInfoNV>* pBindInfos) override;

    void Process_vkCmdBuildAccelerationStructureNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkAccelerationStructureInfoNV>* pInfo,
        format::HandleId                            instanceData,
        VkDeviceSize                                instanceOffset,
        VkBool32                                    update,
        format::HandleId                            dst,
        format::HandleId                            src,
        format::HandleId                            scratch,
        VkDeviceSize                                scratchOffset) override;

    void Process_vkCmdCopyAccelerationStructureNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            dst,
        format::HandleId                            src,
        VkCopyAccelerationStructureModeKHR          mode) override;

    void Process_vkCmdTraceRaysNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            raygenShaderBindingTableBuffer,
        VkDeviceSize                                raygenShaderBindingOffset,
        format::HandleId                            missShaderBindingTableBuffer,
        VkDeviceSize                                missShaderBindingOffset,
        VkDeviceSize                                missShaderBindingStride,
        format::HandleId                            hitShaderBindingTableBuffer,
        VkDeviceSize                                hitShaderBindingOffset,
        VkDeviceSize                                hitShaderBindingStride,
        format::HandleId                            callableShaderBindingTableBuffer,
        VkDeviceSize                                callableShaderBindingOffset,
        VkDeviceSize                                callableShaderBindingStride,
        uint32_t                                    width,
        uint32_t                                    height,
        uint32_t                                    depth) override;

    void Process_vkCmdWriteAccelerationStructuresPropertiesNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    accelerationStructureCount,
        HandlePointerDecoder<VkAccelerationStructureNV>* pAccelerationStructures,
        VkQueryType                                 queryType,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery) override;

    void Process_vkCompileDeferredNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipeline,
        uint32_t                                    shader) override;

    void Process_vkCreateAccelerationStructureNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAccelerationStructureCreateInfoNV>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkAccelerationStructureNV>* pAccelerationStructure) override;

    void Process_vkCreateRayTracingPipelinesNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipelineCache,
        uint32_t                                    createInfoCount,
        StructPointerDecoder<Decoded_VkRayTracingPipelineCreateInfoNV>* pCreateInfos,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkPipeline>*           pPipelines) override;

    void Process_vkDestroyAccelerationStructureNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            accelerationStructure,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetAccelerationStructureHandleNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            accelerationStructure,
        size_t                                      dataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkGetAccelerationStructureMemoryRequirementsNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAccelerationStructureMemoryRequirementsInfoNV>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetRayTracingShaderGroupHandlesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipeline,
        uint32_t                                    firstGroup,
        uint32_t                                    groupCount,
        size_t                                      dataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkGetRayTracingShaderGroupHandlesNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipeline,
        uint32_t                                    firstGroup,
        uint32_t                                    groupCount,
        size_t                                      dataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkGetMemoryHostPointerPropertiesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        VkExternalMemoryHandleTypeFlagBits          handleType,
        uint64_t                                    pHostPointer,
        StructPointerDecoder<Decoded_VkMemoryHostPointerPropertiesEXT>* pMemoryHostPointerProperties) override;

    void Process_vkCmdWriteBufferMarker2AMD(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineStageFlags2                       stage,
        format::HandleId                            dstBuffer,
        VkDeviceSize                                dstOffset,
        uint32_t                                    marker) override;

    void Process_vkCmdWriteBufferMarkerAMD(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineStageFlagBits                     pipelineStage,
        format::HandleId                            dstBuffer,
        VkDeviceSize                                dstOffset,
        uint32_t                                    marker) override;

    void Process_vkGetCalibratedTimestampsEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    timestampCount,
        StructPointerDecoder<Decoded_VkCalibratedTimestampInfoKHR>* pTimestampInfos,
        PointerDecoder<uint64_t>*                   pTimestamps,
        PointerDecoder<uint64_t>*                   pMaxDeviation) override;

    void Process_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pTimeDomainCount,
        PointerDecoder<VkTimeDomainKHR>*            pTimeDomains) override;
#ifdef VK_USE_PLATFORM_GGP
#endif /* VK_USE_PLATFORM_GGP */

    void Process_vkCmdDrawMeshTasksIndirectCountNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        format::HandleId                            countBuffer,
        VkDeviceSize                                countBufferOffset,
        uint32_t                                    maxDrawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdDrawMeshTasksIndirectNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        uint32_t                                    drawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdDrawMeshTasksNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    taskCount,
        uint32_t                                    firstTask) override;

    void Process_vkCmdSetExclusiveScissorEnableNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstExclusiveScissor,
        uint32_t                                    exclusiveScissorCount,
        PointerDecoder<VkBool32>*                   pExclusiveScissorEnables) override;

    void Process_vkCmdSetExclusiveScissorNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstExclusiveScissor,
        uint32_t                                    exclusiveScissorCount,
        StructPointerDecoder<Decoded_VkRect2D>*     pExclusiveScissors) override;

    void Process_vkCmdSetCheckpointNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint64_t                                    pCheckpointMarker) override;

    void Process_vkGetQueueCheckpointData2NV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            queue,
        PointerDecoder<uint32_t>*                   pCheckpointDataCount,
        StructPointerDecoder<Decoded_VkCheckpointData2NV>* pCheckpointData) override;

    void Process_vkGetQueueCheckpointDataNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            queue,
        PointerDecoder<uint32_t>*                   pCheckpointDataCount,
        StructPointerDecoder<Decoded_VkCheckpointDataNV>* pCheckpointData) override;

    void Process_vkGetPastPresentationTimingEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPastPresentationTimingInfoEXT>* pPastPresentationTimingInfo,
        StructPointerDecoder<Decoded_VkPastPresentationTimingPropertiesEXT>* pPastPresentationTimingProperties) override;

    void Process_vkGetSwapchainTimeDomainPropertiesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkSwapchainTimeDomainPropertiesEXT>* pSwapchainTimeDomainProperties,
        PointerDecoder<uint64_t>*                   pTimeDomainsCounter) override;

    void Process_vkGetSwapchainTimingPropertiesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkSwapchainTimingPropertiesEXT>* pSwapchainTimingProperties,
        PointerDecoder<uint64_t>*                   pSwapchainTimingPropertiesCounter) override;

    void Process_vkSetSwapchainPresentTimingQueueSizeEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        uint32_t                                    size) override;

    void Process_vkAcquirePerformanceConfigurationINTEL(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPerformanceConfigurationAcquireInfoINTEL>* pAcquireInfo,
        HandlePointerDecoder<VkPerformanceConfigurationINTEL>* pConfiguration) override;

    void Process_vkCmdSetPerformanceMarkerINTEL(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPerformanceMarkerInfoINTEL>* pMarkerInfo) override;

    void Process_vkCmdSetPerformanceOverrideINTEL(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPerformanceOverrideInfoINTEL>* pOverrideInfo) override;

    void Process_vkCmdSetPerformanceStreamMarkerINTEL(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPerformanceStreamMarkerInfoINTEL>* pMarkerInfo) override;

    void Process_vkGetPerformanceParameterINTEL(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        VkPerformanceParameterTypeINTEL             parameter,
        StructPointerDecoder<Decoded_VkPerformanceValueINTEL>* pValue) override;

    void Process_vkInitializePerformanceApiINTEL(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkInitializePerformanceApiInfoINTEL>* pInitializeInfo) override;

    void Process_vkQueueSetPerformanceConfigurationINTEL(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            queue,
        format::HandleId                            configuration) override;

    void Process_vkReleasePerformanceConfigurationINTEL(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            configuration) override;

    void Process_vkUninitializePerformanceApiINTEL(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device) override;

    void Process_vkSetLocalDimmingAMD(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            swapChain,
        VkBool32                                    localDimmingEnable) override;
#ifdef VK_USE_PLATFORM_FUCHSIA

    void Process_vkCreateImagePipeSurfaceFUCHSIA(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkImagePipeSurfaceCreateInfoFUCHSIA>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;
#endif /* VK_USE_PLATFORM_FUCHSIA */

    void Process_vkCreateMetalSurfaceEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkMetalSurfaceCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkGetBufferDeviceAddressEXT(
        const ApiCallInfo&                          call_info,
        VkDeviceAddress                             returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    void Process_vkGetPhysicalDeviceToolPropertiesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pToolCount,
        StructPointerDecoder<Decoded_VkPhysicalDeviceToolProperties>* pToolProperties) override;

    void Process_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkCooperativeMatrixPropertiesNV>* pProperties) override;

    void Process_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pCombinationCount,
        StructPointerDecoder<Decoded_VkFramebufferMixedSamplesCombinationNV>* pCombinations) override;
#ifdef VK_USE_PLATFORM_WIN32_KHR

    void Process_vkAcquireFullScreenExclusiveModeEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain) override;

    void Process_vkGetDeviceGroupSurfacePresentModes2EXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPhysicalDeviceSurfaceInfo2KHR>* pSurfaceInfo,
        PointerDecoder<VkDeviceGroupPresentModeFlagsKHR>* pModes) override;

    void Process_vkGetPhysicalDeviceSurfacePresentModes2EXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceSurfaceInfo2KHR>* pSurfaceInfo,
        PointerDecoder<uint32_t>*                   pPresentModeCount,
        PointerDecoder<VkPresentModeKHR>*           pPresentModes) override;

    void Process_vkReleaseFullScreenExclusiveModeEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain) override;
#endif /* VK_USE_PLATFORM_WIN32_KHR */

    void Process_vkCreateHeadlessSurfaceEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkHeadlessSurfaceCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkCmdSetLineStippleEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    lineStippleFactor,
        uint16_t                                    lineStipplePattern) override;

    void Process_vkResetQueryPoolEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery,
        uint32_t                                    queryCount) override;

    void Process_vkCmdBindVertexBuffers2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstBinding,
        uint32_t                                    bindingCount,
        HandlePointerDecoder<VkBuffer>*             pBuffers,
        PointerDecoder<VkDeviceSize>*               pOffsets,
        PointerDecoder<VkDeviceSize>*               pSizes,
        PointerDecoder<VkDeviceSize>*               pStrides) override;

    void Process_vkCmdSetCullModeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkCullModeFlags                             cullMode) override;

    void Process_vkCmdSetDepthBoundsTestEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthBoundsTestEnable) override;

    void Process_vkCmdSetDepthCompareOpEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkCompareOp                                 depthCompareOp) override;

    void Process_vkCmdSetDepthTestEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthTestEnable) override;

    void Process_vkCmdSetDepthWriteEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthWriteEnable) override;

    void Process_vkCmdSetFrontFaceEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkFrontFace                                 frontFace) override;

    void Process_vkCmdSetPrimitiveTopologyEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPrimitiveTopology                         primitiveTopology) override;

    void Process_vkCmdSetScissorWithCountEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    scissorCount,
        StructPointerDecoder<Decoded_VkRect2D>*     pScissors) override;

    void Process_vkCmdSetStencilOpEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkStencilFaceFlags                          faceMask,
        VkStencilOp                                 failOp,
        VkStencilOp                                 passOp,
        VkStencilOp                                 depthFailOp,
        VkCompareOp                                 compareOp) override;

    void Process_vkCmdSetStencilTestEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    stencilTestEnable) override;

    void Process_vkCmdSetViewportWithCountEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    viewportCount,
        StructPointerDecoder<Decoded_VkViewport>*   pViewports) override;

    void Process_vkCopyImageToImageEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkCopyImageToImageInfo>* pCopyImageToImageInfo) override;

    void Process_vkCopyImageToMemoryEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkCopyImageToMemoryInfo>* pCopyImageToMemoryInfo) override;

    void Process_vkCopyMemoryToImageEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkCopyMemoryToImageInfo>* pCopyMemoryToImageInfo) override;

    void Process_vkGetImageSubresourceLayout2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            image,
        StructPointerDecoder<Decoded_VkImageSubresource2>* pSubresource,
        StructPointerDecoder<Decoded_VkSubresourceLayout2>* pLayout) override;

    void Process_vkTransitionImageLayoutEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    transitionCount,
        StructPointerDecoder<Decoded_VkHostImageLayoutTransitionInfo>* pTransitions) override;

    void Process_vkReleaseSwapchainImagesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkReleaseSwapchainImagesInfoKHR>* pReleaseInfo) override;

    void Process_vkCmdBindPipelineShaderGroupNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        format::HandleId                            pipeline,
        uint32_t                                    groupIndex) override;

    void Process_vkCmdExecuteGeneratedCommandsNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    isPreprocessed,
        StructPointerDecoder<Decoded_VkGeneratedCommandsInfoNV>* pGeneratedCommandsInfo) override;

    void Process_vkCmdPreprocessGeneratedCommandsNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkGeneratedCommandsInfoNV>* pGeneratedCommandsInfo) override;

    void Process_vkCreateIndirectCommandsLayoutNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkIndirectCommandsLayoutCreateInfoNV>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkIndirectCommandsLayoutNV>* pIndirectCommandsLayout) override;

    void Process_vkDestroyIndirectCommandsLayoutNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            indirectCommandsLayout,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetGeneratedCommandsMemoryRequirementsNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkGeneratedCommandsMemoryRequirementsInfoNV>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkCmdSetDepthBias2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDepthBiasInfoEXT>* pDepthBiasInfo) override;

    void Process_vkAcquireDrmDisplayEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        int32_t                                     drmFd,
        format::HandleId                            display) override;

    void Process_vkGetDrmDisplayEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        int32_t                                     drmFd,
        uint32_t                                    connectorId,
        HandlePointerDecoder<VkDisplayKHR>*         display) override;

    void Process_vkCreatePrivateDataSlotEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPrivateDataSlotCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkPrivateDataSlot>*    pPrivateDataSlot) override;

    void Process_vkDestroyPrivateDataSlotEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            privateDataSlot,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetPrivateDataEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        VkObjectType                                objectType,
        uint64_t                                    objectHandle,
        format::HandleId                            privateDataSlot,
        PointerDecoder<uint64_t>*                   pData) override;

    void Process_vkSetPrivateDataEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        VkObjectType                                objectType,
        uint64_t                                    objectHandle,
        format::HandleId                            privateDataSlot,
        uint64_t                                    data) override;

    void Process_vkQueueSetPerfHintQCOM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            queue,
        StructPointerDecoder<Decoded_VkPerfHintInfoQCOM>* pPerfHintInfo) override;

    void Process_vkCmdBeginPerTileExecutionQCOM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPerTileBeginInfoQCOM>* pPerTileBeginInfo) override;

    void Process_vkCmdDispatchTileQCOM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDispatchTileInfoQCOM>* pDispatchTileInfo) override;

    void Process_vkCmdEndPerTileExecutionQCOM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkPerTileEndInfoQCOM>* pPerTileEndInfo) override;

    void Process_vkCmdBindDescriptorBufferEmbeddedSamplersEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        format::HandleId                            layout,
        uint32_t                                    set) override;

    void Process_vkCmdBindDescriptorBuffersEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    bufferCount,
        StructPointerDecoder<Decoded_VkDescriptorBufferBindingInfoEXT>* pBindingInfos) override;

    void Process_vkCmdSetDescriptorBufferOffsetsEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        format::HandleId                            layout,
        uint32_t                                    firstSet,
        uint32_t                                    setCount,
        PointerDecoder<uint32_t>*                   pBufferIndices,
        PointerDecoder<VkDeviceSize>*               pOffsets) override;

    void Process_vkGetDescriptorEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorGetInfoEXT>* pDescriptorInfo,
        size_t                                      dataSize,
        PointerDecoder<uint8_t>*                    pDescriptor) override;

    void Process_vkGetDescriptorSetLayoutBindingOffsetEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            layout,
        uint32_t                                    binding,
        PointerDecoder<VkDeviceSize>*               pOffset) override;

    void Process_vkGetDescriptorSetLayoutSizeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            layout,
        PointerDecoder<VkDeviceSize>*               pLayoutSizeInBytes) override;

    void Process_vkCmdSetFragmentShadingRateEnumNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkFragmentShadingRateNV                     shadingRate,
        PointerDecoder<VkFragmentShadingRateCombinerOpKHR>* combinerOps) override;

    void Process_vkGetDeviceFaultInfoEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceFaultCountsEXT>* pFaultCounts,
        StructPointerDecoder<Decoded_VkDeviceFaultInfoEXT>* pFaultInfo) override;
#ifdef VK_USE_PLATFORM_WIN32_KHR

    void Process_vkAcquireWinrtDisplayNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        format::HandleId                            display) override;

    void Process_vkGetWinrtDisplayNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    deviceRelativeId,
        HandlePointerDecoder<VkDisplayKHR>*         pDisplay) override;
#endif /* VK_USE_PLATFORM_WIN32_KHR */
#ifdef VK_USE_PLATFORM_DIRECTFB_EXT

    void Process_vkCreateDirectFBSurfaceEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkDirectFBSurfaceCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkGetPhysicalDeviceDirectFBPresentationSupportEXT(
        const ApiCallInfo&                          call_info,
        VkBool32                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        uint64_t                                    dfb) override;
#endif /* VK_USE_PLATFORM_DIRECTFB_EXT */

    void Process_vkCmdSetVertexInputEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    vertexBindingDescriptionCount,
        StructPointerDecoder<Decoded_VkVertexInputBindingDescription2EXT>* pVertexBindingDescriptions,
        uint32_t                                    vertexAttributeDescriptionCount,
        StructPointerDecoder<Decoded_VkVertexInputAttributeDescription2EXT>* pVertexAttributeDescriptions) override;
#ifdef VK_USE_PLATFORM_FUCHSIA

    void Process_vkGetMemoryZirconHandleFUCHSIA(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryGetZirconHandleInfoFUCHSIA>* pGetZirconHandleInfo,
        PointerDecoder<uint32_t>*                   pZirconHandle) override;

    void Process_vkGetMemoryZirconHandlePropertiesFUCHSIA(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        VkExternalMemoryHandleTypeFlagBits          handleType,
        uint32_t                                    zirconHandle,
        StructPointerDecoder<Decoded_VkMemoryZirconHandlePropertiesFUCHSIA>* pMemoryZirconHandleProperties) override;
#endif /* VK_USE_PLATFORM_FUCHSIA */
#ifdef VK_USE_PLATFORM_FUCHSIA

    void Process_vkGetSemaphoreZirconHandleFUCHSIA(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkSemaphoreGetZirconHandleInfoFUCHSIA>* pGetZirconHandleInfo,
        PointerDecoder<uint32_t>*                   pZirconHandle) override;

    void Process_vkImportSemaphoreZirconHandleFUCHSIA(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkImportSemaphoreZirconHandleInfoFUCHSIA>* pImportSemaphoreZirconHandleInfo) override;
#endif /* VK_USE_PLATFORM_FUCHSIA */

    void Process_vkCmdBindInvocationMaskHUAWEI(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            imageView,
        VkImageLayout                               imageLayout) override;

    void Process_vkGetMemoryRemoteAddressNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryGetRemoteAddressInfoNV>* pMemoryGetRemoteAddressInfo,
        PointerDecoder<uint64_t, void*>*            pAddress) override;

    void Process_vkCmdSetDepthBiasEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthBiasEnable) override;

    void Process_vkCmdSetLogicOpEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkLogicOp                                   logicOp) override;

    void Process_vkCmdSetPatchControlPointsEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    patchControlPoints) override;

    void Process_vkCmdSetPrimitiveRestartEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    primitiveRestartEnable) override;

    void Process_vkCmdSetRasterizerDiscardEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    rasterizerDiscardEnable) override;

    void Process_vkCreateScreenSurfaceQNX(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            instance,
        StructPointerDecoder<Decoded_VkScreenSurfaceCreateInfoQNX>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkSurfaceKHR>*         pSurface) override;

    void Process_vkGetPhysicalDeviceScreenPresentationSupportQNX(
        const ApiCallInfo&                          call_info,
        VkBool32                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        uint64_t                                    window) override;

    void Process_vkCmdSetColorWriteEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    attachmentCount,
        PointerDecoder<VkBool32>*                   pColorWriteEnables) override;

    void Process_vkCmdDrawMultiEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    drawCount,
        StructPointerDecoder<Decoded_VkMultiDrawInfoEXT>* pVertexInfo,
        uint32_t                                    instanceCount,
        uint32_t                                    firstInstance,
        uint32_t                                    stride) override;

    void Process_vkCmdDrawMultiIndexedEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    drawCount,
        StructPointerDecoder<Decoded_VkMultiDrawIndexedInfoEXT>* pIndexInfo,
        uint32_t                                    instanceCount,
        uint32_t                                    firstInstance,
        uint32_t                                    stride,
        PointerDecoder<int32_t>*                    pVertexOffset) override;

    void Process_vkBuildMicromapsEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            deferredOperation,
        uint32_t                                    infoCount,
        StructPointerDecoder<Decoded_VkMicromapBuildInfoEXT>* pInfos) override;

    void Process_vkCmdBuildMicromapsEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    infoCount,
        StructPointerDecoder<Decoded_VkMicromapBuildInfoEXT>* pInfos) override;

    void Process_vkCmdCopyMemoryToMicromapEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyMemoryToMicromapInfoEXT>* pInfo) override;

    void Process_vkCmdCopyMicromapEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyMicromapInfoEXT>* pInfo) override;

    void Process_vkCmdCopyMicromapToMemoryEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyMicromapToMemoryInfoEXT>* pInfo) override;

    void Process_vkCmdWriteMicromapsPropertiesEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    micromapCount,
        HandlePointerDecoder<VkMicromapEXT>*        pMicromaps,
        VkQueryType                                 queryType,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery) override;

    void Process_vkCopyMemoryToMicromapEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            deferredOperation,
        StructPointerDecoder<Decoded_VkCopyMemoryToMicromapInfoEXT>* pInfo) override;

    void Process_vkCopyMicromapEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            deferredOperation,
        StructPointerDecoder<Decoded_VkCopyMicromapInfoEXT>* pInfo) override;

    void Process_vkCopyMicromapToMemoryEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            deferredOperation,
        StructPointerDecoder<Decoded_VkCopyMicromapToMemoryInfoEXT>* pInfo) override;

    void Process_vkCreateMicromapEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMicromapCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkMicromapEXT>*        pMicromap) override;

    void Process_vkDestroyMicromapEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            micromap,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetDeviceMicromapCompatibilityEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMicromapVersionInfoEXT>* pVersionInfo,
        PointerDecoder<VkAccelerationStructureCompatibilityKHR>* pCompatibility) override;

    void Process_vkGetMicromapBuildSizesEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        VkAccelerationStructureBuildTypeKHR         buildType,
        StructPointerDecoder<Decoded_VkMicromapBuildInfoEXT>* pBuildInfo,
        StructPointerDecoder<Decoded_VkMicromapBuildSizesInfoEXT>* pSizeInfo) override;

    void Process_vkWriteMicromapsPropertiesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    micromapCount,
        HandlePointerDecoder<VkMicromapEXT>*        pMicromaps,
        VkQueryType                                 queryType,
        size_t                                      dataSize,
        PointerDecoder<uint8_t>*                    pData,
        size_t                                      stride) override;

    void Process_vkCmdDrawClusterHUAWEI(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    groupCountX,
        uint32_t                                    groupCountY,
        uint32_t                                    groupCountZ) override;

    void Process_vkCmdDrawClusterIndirectHUAWEI(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset) override;

    void Process_vkSetDeviceMemoryPriorityEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            memory,
        float                                       priority) override;

    void Process_vkCmdSetDispatchParametersARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDispatchParametersARM>* pDispatchParameters) override;

    void Process_vkGetDescriptorSetHostMappingVALVE(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            descriptorSet,
        PointerDecoder<uint64_t, void*>*            ppData) override;

    void Process_vkGetDescriptorSetLayoutHostMappingInfoVALVE(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDescriptorSetBindingReferenceVALVE>* pBindingReference,
        StructPointerDecoder<Decoded_VkDescriptorSetLayoutHostMappingInfoVALVE>* pHostMapping) override;

    void Process_vkCmdUpdatePipelineIndirectBufferNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        format::HandleId                            pipeline) override;

    void Process_vkGetPipelineIndirectDeviceAddressNV(
        const ApiCallInfo&                          call_info,
        VkDeviceAddress                             returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPipelineIndirectDeviceAddressInfoNV>* pInfo) override;

    void Process_vkGetPipelineIndirectMemoryRequirementsNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkComputePipelineCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkCmdSetAlphaToCoverageEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    alphaToCoverageEnable) override;

    void Process_vkCmdSetAlphaToOneEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    alphaToOneEnable) override;

    void Process_vkCmdSetColorBlendAdvancedEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstAttachment,
        uint32_t                                    attachmentCount,
        StructPointerDecoder<Decoded_VkColorBlendAdvancedEXT>* pColorBlendAdvanced) override;

    void Process_vkCmdSetColorBlendEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstAttachment,
        uint32_t                                    attachmentCount,
        PointerDecoder<VkBool32>*                   pColorBlendEnables) override;

    void Process_vkCmdSetColorBlendEquationEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstAttachment,
        uint32_t                                    attachmentCount,
        StructPointerDecoder<Decoded_VkColorBlendEquationEXT>* pColorBlendEquations) override;

    void Process_vkCmdSetColorWriteMaskEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstAttachment,
        uint32_t                                    attachmentCount,
        PointerDecoder<VkColorComponentFlags>*      pColorWriteMasks) override;

    void Process_vkCmdSetConservativeRasterizationModeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkConservativeRasterizationModeEXT          conservativeRasterizationMode) override;

    void Process_vkCmdSetCoverageModulationModeNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkCoverageModulationModeNV                  coverageModulationMode) override;

    void Process_vkCmdSetCoverageModulationTableEnableNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    coverageModulationTableEnable) override;

    void Process_vkCmdSetCoverageModulationTableNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    coverageModulationTableCount,
        PointerDecoder<float>*                      pCoverageModulationTable) override;

    void Process_vkCmdSetCoverageReductionModeNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkCoverageReductionModeNV                   coverageReductionMode) override;

    void Process_vkCmdSetCoverageToColorEnableNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    coverageToColorEnable) override;

    void Process_vkCmdSetCoverageToColorLocationNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    coverageToColorLocation) override;

    void Process_vkCmdSetDepthClampEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthClampEnable) override;

    void Process_vkCmdSetDepthClipEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    depthClipEnable) override;

    void Process_vkCmdSetDepthClipNegativeOneToOneEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    negativeOneToOne) override;

    void Process_vkCmdSetExtraPrimitiveOverestimationSizeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        float                                       extraPrimitiveOverestimationSize) override;

    void Process_vkCmdSetLineRasterizationModeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkLineRasterizationModeEXT                  lineRasterizationMode) override;

    void Process_vkCmdSetLineStippleEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    stippledLineEnable) override;

    void Process_vkCmdSetLogicOpEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    logicOpEnable) override;

    void Process_vkCmdSetPolygonModeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkPolygonMode                               polygonMode) override;

    void Process_vkCmdSetProvokingVertexModeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkProvokingVertexModeEXT                    provokingVertexMode) override;

    void Process_vkCmdSetRasterizationSamplesEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkSampleCountFlagBits                       rasterizationSamples) override;

    void Process_vkCmdSetRasterizationStreamEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    rasterizationStream) override;

    void Process_vkCmdSetRepresentativeFragmentTestEnableNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    representativeFragmentTestEnable) override;

    void Process_vkCmdSetSampleLocationsEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    sampleLocationsEnable) override;

    void Process_vkCmdSetSampleMaskEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkSampleCountFlagBits                       samples,
        PointerDecoder<VkSampleMask>*               pSampleMask) override;

    void Process_vkCmdSetShadingRateImageEnableNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    shadingRateImageEnable) override;

    void Process_vkCmdSetTessellationDomainOriginEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkTessellationDomainOrigin                  domainOrigin) override;

    void Process_vkCmdSetViewportSwizzleNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    firstViewport,
        uint32_t                                    viewportCount,
        StructPointerDecoder<Decoded_VkViewportSwizzleNV>* pViewportSwizzles) override;

    void Process_vkCmdSetViewportWScalingEnableNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    viewportWScalingEnable) override;

    void Process_vkBindTensorMemoryARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    bindInfoCount,
        StructPointerDecoder<Decoded_VkBindTensorMemoryInfoARM>* pBindInfos) override;

    void Process_vkCmdCopyTensorARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyTensorInfoARM>* pCopyTensorInfo) override;

    void Process_vkCreateTensorARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkTensorCreateInfoARM>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkTensorARM>*          pTensor) override;

    void Process_vkCreateTensorViewARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkTensorViewCreateInfoARM>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkTensorViewARM>*      pView) override;

    void Process_vkDestroyTensorARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            tensor,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyTensorViewARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            tensorView,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetDeviceTensorMemoryRequirementsARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDeviceTensorMemoryRequirementsARM>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetPhysicalDeviceExternalTensorPropertiesARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceExternalTensorInfoARM>* pExternalTensorInfo,
        StructPointerDecoder<Decoded_VkExternalTensorPropertiesARM>* pExternalTensorProperties) override;

    void Process_vkGetTensorMemoryRequirementsARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkTensorMemoryRequirementsInfoARM>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetShaderModuleCreateInfoIdentifierEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkShaderModuleCreateInfo>* pCreateInfo,
        StructPointerDecoder<Decoded_VkShaderModuleIdentifierEXT>* pIdentifier) override;

    void Process_vkGetShaderModuleIdentifierEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            shaderModule,
        StructPointerDecoder<Decoded_VkShaderModuleIdentifierEXT>* pIdentifier) override;

    void Process_vkBindOpticalFlowSessionImageNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            session,
        VkOpticalFlowSessionBindingPointNV          bindingPoint,
        format::HandleId                            view,
        VkImageLayout                               layout) override;

    void Process_vkCmdOpticalFlowExecuteNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            session,
        StructPointerDecoder<Decoded_VkOpticalFlowExecuteInfoNV>* pExecuteInfo) override;

    void Process_vkCreateOpticalFlowSessionNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkOpticalFlowSessionCreateInfoNV>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkOpticalFlowSessionNV>* pSession) override;

    void Process_vkDestroyOpticalFlowSessionNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            session,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetPhysicalDeviceOpticalFlowImageFormatsNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkOpticalFlowImageFormatInfoNV>* pOpticalFlowImageFormatInfo,
        PointerDecoder<uint32_t>*                   pFormatCount,
        StructPointerDecoder<Decoded_VkOpticalFlowImageFormatPropertiesNV>* pImageFormatProperties) override;
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#endif /* VK_USE_PLATFORM_ANDROID_KHR */

    void Process_vkAntiLagUpdateAMD(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAntiLagDataAMD>* pData) override;

    void Process_vkCmdBindShadersEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    stageCount,
        PointerDecoder<VkShaderStageFlagBits>*      pStages,
        HandlePointerDecoder<VkShaderEXT>*          pShaders) override;

    void Process_vkCmdSetDepthClampRangeEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkDepthClampModeEXT                         depthClampMode,
        StructPointerDecoder<Decoded_VkDepthClampRangeEXT>* pDepthClampRange) override;

    void Process_vkCreateShadersEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    createInfoCount,
        StructPointerDecoder<Decoded_VkShaderCreateInfoEXT>* pCreateInfos,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkShaderEXT>*          pShaders) override;

    void Process_vkDestroyShaderEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            shader,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetShaderBinaryDataEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            shader,
        PointerDecoder<size_t>*                     pDataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkGetDynamicRenderingTilePropertiesQCOM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkRenderingInfo>* pRenderingInfo,
        StructPointerDecoder<Decoded_VkTilePropertiesQCOM>* pProperties) override;

    void Process_vkGetFramebufferTilePropertiesQCOM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            framebuffer,
        PointerDecoder<uint32_t>*                   pPropertiesCount,
        StructPointerDecoder<Decoded_VkTilePropertiesQCOM>* pProperties) override;

    void Process_vkCmdConvertCooperativeVectorMatrixNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    infoCount,
        StructPointerDecoder<Decoded_VkConvertCooperativeVectorMatrixInfoNV>* pInfos) override;

    void Process_vkConvertCooperativeVectorMatrixNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkConvertCooperativeVectorMatrixInfoNV>* pInfo) override;

    void Process_vkGetPhysicalDeviceCooperativeVectorPropertiesNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkCooperativeVectorPropertiesNV>* pProperties) override;

    void Process_vkGetLatencyTimingsNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkGetLatencyMarkerInfoNV>* pLatencyMarkerInfo) override;

    void Process_vkLatencySleepNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkLatencySleepInfoNV>* pSleepInfo) override;

    void Process_vkQueueNotifyOutOfBandNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            queue,
        StructPointerDecoder<Decoded_VkOutOfBandQueueTypeInfoNV>* pQueueTypeInfo) override;

    void Process_vkSetLatencyMarkerNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkSetLatencyMarkerInfoNV>* pLatencyMarkerInfo) override;

    void Process_vkSetLatencySleepModeNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            swapchain,
        StructPointerDecoder<Decoded_VkLatencySleepModeInfoNV>* pSleepModeInfo) override;

    void Process_vkBindDataGraphPipelineSessionMemoryARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    bindInfoCount,
        StructPointerDecoder<Decoded_VkBindDataGraphPipelineSessionMemoryInfoARM>* pBindInfos) override;

    void Process_vkCmdDispatchDataGraphARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            session,
        StructPointerDecoder<Decoded_VkDataGraphPipelineDispatchInfoARM>* pInfo) override;

    void Process_vkCreateDataGraphPipelineSessionARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDataGraphPipelineSessionCreateInfoARM>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkDataGraphPipelineSessionARM>* pSession) override;

    void Process_vkCreateDataGraphPipelinesARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            deferredOperation,
        format::HandleId                            pipelineCache,
        uint32_t                                    createInfoCount,
        StructPointerDecoder<Decoded_VkDataGraphPipelineCreateInfoARM>* pCreateInfos,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkPipeline>*           pPipelines) override;

    void Process_vkDestroyDataGraphPipelineSessionARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            session,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetDataGraphPipelineAvailablePropertiesARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDataGraphPipelineInfoARM>* pPipelineInfo,
        PointerDecoder<uint32_t>*                   pPropertiesCount,
        PointerDecoder<VkDataGraphPipelinePropertyARM>* pProperties) override;

    void Process_vkGetDataGraphPipelinePropertiesARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDataGraphPipelineInfoARM>* pPipelineInfo,
        uint32_t                                    propertiesCount,
        StructPointerDecoder<Decoded_VkDataGraphPipelinePropertyQueryResultARM>* pProperties) override;

    void Process_vkGetDataGraphPipelineSessionBindPointRequirementsARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDataGraphPipelineSessionBindPointRequirementsInfoARM>* pInfo,
        PointerDecoder<uint32_t>*                   pBindPointRequirementCount,
        StructPointerDecoder<Decoded_VkDataGraphPipelineSessionBindPointRequirementARM>* pBindPointRequirements) override;

    void Process_vkGetDataGraphPipelineSessionMemoryRequirementsARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkDataGraphPipelineSessionMemoryRequirementsInfoARM>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            physicalDevice,
        StructPointerDecoder<Decoded_VkPhysicalDeviceQueueFamilyDataGraphProcessingEngineInfoARM>* pQueueFamilyDataGraphProcessingEngineInfo,
        StructPointerDecoder<Decoded_VkQueueFamilyDataGraphProcessingEnginePropertiesARM>* pQueueFamilyDataGraphProcessingEngineProperties) override;

    void Process_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        PointerDecoder<uint32_t>*                   pQueueFamilyDataGraphPropertyCount,
        StructPointerDecoder<Decoded_VkQueueFamilyDataGraphPropertiesARM>* pQueueFamilyDataGraphProperties) override;

    void Process_vkCmdSetAttachmentFeedbackLoopEnableEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkImageAspectFlags                          aspectMask) override;

    void Process_vkCmdBindTileMemoryQCOM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkTileMemoryBindInfoQCOM>* pTileMemoryBindInfo) override;

    void Process_vkCmdDecompressMemoryEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkDecompressMemoryInfoEXT>* pDecompressMemoryInfoEXT) override;

    void Process_vkCmdDecompressMemoryIndirectCountEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkMemoryDecompressionMethodFlagsEXT         decompressionMethod,
        VkDeviceAddress                             indirectCommandsAddress,
        VkDeviceAddress                             indirectCommandsCountAddress,
        uint32_t                                    maxDecompressionCount,
        uint32_t                                    stride) override;

    void Process_vkCmdBuildPartitionedAccelerationStructuresNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkBuildPartitionedAccelerationStructureInfoNV>* pBuildInfo) override;

    void Process_vkGetPartitionedAccelerationStructuresBuildSizesNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkPartitionedAccelerationStructureInstancesInputNV>* pInfo,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildSizesInfoKHR>* pSizeInfo) override;

    void Process_vkCmdExecuteGeneratedCommandsEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        VkBool32                                    isPreprocessed,
        StructPointerDecoder<Decoded_VkGeneratedCommandsInfoEXT>* pGeneratedCommandsInfo) override;

    void Process_vkCmdPreprocessGeneratedCommandsEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkGeneratedCommandsInfoEXT>* pGeneratedCommandsInfo,
        format::HandleId                            stateCommandBuffer) override;

    void Process_vkCreateIndirectCommandsLayoutEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkIndirectCommandsLayoutCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkIndirectCommandsLayoutEXT>* pIndirectCommandsLayout) override;

    void Process_vkCreateIndirectExecutionSetEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkIndirectExecutionSetCreateInfoEXT>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkIndirectExecutionSetEXT>* pIndirectExecutionSet) override;

    void Process_vkDestroyIndirectCommandsLayoutEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            indirectCommandsLayout,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkDestroyIndirectExecutionSetEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            indirectExecutionSet,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetGeneratedCommandsMemoryRequirementsEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkGeneratedCommandsMemoryRequirementsInfoEXT>* pInfo,
        StructPointerDecoder<Decoded_VkMemoryRequirements2>* pMemoryRequirements) override;

    void Process_vkUpdateIndirectExecutionSetPipelineEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            indirectExecutionSet,
        uint32_t                                    executionSetWriteCount,
        StructPointerDecoder<Decoded_VkWriteIndirectExecutionSetPipelineEXT>* pExecutionSetWrites) override;

    void Process_vkUpdateIndirectExecutionSetShaderEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            indirectExecutionSet,
        uint32_t                                    executionSetWriteCount,
        StructPointerDecoder<Decoded_VkWriteIndirectExecutionSetShaderEXT>* pExecutionSetWrites) override;

    void Process_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        PointerDecoder<uint32_t>*                   pPropertyCount,
        StructPointerDecoder<Decoded_VkCooperativeMatrixFlexibleDimensionsPropertiesNV>* pProperties) override;

    void Process_vkGetMemoryMetalHandleEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkMemoryGetMetalHandleInfoEXT>* pGetMetalHandleInfo,
        PointerDecoder<uint64_t, void*>*            pHandle) override;

    void Process_vkGetMemoryMetalHandlePropertiesEXT(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        VkExternalMemoryHandleTypeFlagBits          handleType,
        uint64_t                                    pHandle,
        StructPointerDecoder<Decoded_VkMemoryMetalHandlePropertiesEXT>* pMemoryMetalHandleProperties) override;

    void Process_vkEnumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        PointerDecoder<uint32_t>*                   pCounterCount,
        StructPointerDecoder<Decoded_VkPerformanceCounterARM>* pCounters,
        StructPointerDecoder<Decoded_VkPerformanceCounterDescriptionARM>* pCounterDescriptions) override;

    void Process_vkCmdEndRendering2EXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkRenderingEndInfoKHR>* pRenderingEndInfo) override;

    void Process_vkCmdBeginCustomResolveEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkBeginCustomResolveInfoEXT>* pBeginCustomResolveInfo) override;

    void Process_vkGetPhysicalDeviceQueueFamilyDataGraphEngineOperationPropertiesARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        StructPointerDecoder<Decoded_VkQueueFamilyDataGraphPropertiesARM>* pQueueFamilyDataGraphProperties,
        StructPointerDecoder<Decoded_VkBaseOutStructure>* pProperties) override;

    void Process_vkGetPhysicalDeviceQueueFamilyDataGraphOpticalFlowImageFormatsARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            physicalDevice,
        uint32_t                                    queueFamilyIndex,
        StructPointerDecoder<Decoded_VkQueueFamilyDataGraphPropertiesARM>* pQueueFamilyDataGraphProperties,
        StructPointerDecoder<Decoded_VkDataGraphOpticalFlowImageFormatInfoARM>* pOpticalFlowImageFormatInfo,
        PointerDecoder<uint32_t>*                   pFormatCount,
        StructPointerDecoder<Decoded_VkDataGraphOpticalFlowImageFormatPropertiesARM>* pImageFormatProperties) override;

    void Process_vkCmdSetComputeOccupancyPriorityNV(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkComputeOccupancyPriorityParametersNV>* pParameters) override;

    void Process_vkCmdSetPrimitiveRestartIndexEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    primitiveRestartIndex) override;

    void Process_vkAssertBufferARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkUpdateBufferInfoARM>* pInfo,
        PointerDecoder<uint32_t>*                   checksum,
        StringDecoder*                              comment) override;

    void Process_vkAssertMemoryARM(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkUpdateMemoryInfoARM>* pInfo,
        PointerDecoder<uint32_t>*                   checksum,
        StringDecoder*                              comment) override;

    void Process_vkCmdUpdateBuffer2ARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkUpdateBufferInfoARM>* pInfo) override;

    void Process_vkCmdUpdateMemory2ARM(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkUpdateMemoryInfoARM>* pInfo) override;

    void Process_vkCmdBuildAccelerationStructuresIndirectKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    infoCount,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
        PointerDecoder<VkDeviceAddress>*            pIndirectDeviceAddresses,
        PointerDecoder<uint32_t>*                   pIndirectStrides,
        PointerDecoder<uint32_t*>*                  ppMaxPrimitiveCounts) override;

    void Process_vkCmdBuildAccelerationStructuresKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    infoCount,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>* ppBuildRangeInfos) override;

    void Process_vkCmdCopyAccelerationStructureKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* pInfo) override;

    void Process_vkCmdCopyAccelerationStructureToMemoryKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyAccelerationStructureToMemoryInfoKHR>* pInfo) override;

    void Process_vkCmdCopyMemoryToAccelerationStructureKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkCopyMemoryToAccelerationStructureInfoKHR>* pInfo) override;

    void Process_vkCmdWriteAccelerationStructuresPropertiesKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    accelerationStructureCount,
        HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructures,
        VkQueryType                                 queryType,
        format::HandleId                            queryPool,
        uint32_t                                    firstQuery) override;

    void Process_vkCopyAccelerationStructureToMemoryKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            deferredOperation,
        StructPointerDecoder<Decoded_VkCopyAccelerationStructureToMemoryInfoKHR>* pInfo) override;

    void Process_vkCopyMemoryToAccelerationStructureKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            deferredOperation,
        StructPointerDecoder<Decoded_VkCopyMemoryToAccelerationStructureInfoKHR>* pInfo) override;

    void Process_vkCreateAccelerationStructureKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAccelerationStructureCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
        HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructure) override;

    void Process_vkDestroyAccelerationStructureKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        format::HandleId                            accelerationStructure,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    void Process_vkGetAccelerationStructureBuildSizesKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        VkAccelerationStructureBuildTypeKHR         buildType,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pBuildInfo,
        PointerDecoder<uint32_t>*                   pMaxPrimitiveCounts,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildSizesInfoKHR>* pSizeInfo) override;

    void Process_vkGetAccelerationStructureDeviceAddressKHR(
        const ApiCallInfo&                          call_info,
        VkDeviceAddress                             returnValue,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAccelerationStructureDeviceAddressInfoKHR>* pInfo) override;

    void Process_vkGetDeviceAccelerationStructureCompatibilityKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            device,
        StructPointerDecoder<Decoded_VkAccelerationStructureVersionInfoKHR>* pVersionInfo,
        PointerDecoder<VkAccelerationStructureCompatibilityKHR>* pCompatibility) override;

    void Process_vkWriteAccelerationStructuresPropertiesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        uint32_t                                    accelerationStructureCount,
        HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructures,
        VkQueryType                                 queryType,
        size_t                                      dataSize,
        PointerDecoder<uint8_t>*                    pData,
        size_t                                      stride) override;

    void Process_vkCreateRayTracingPipelinesKHR(
        const ApiCallInfo&                                               call_info,
        VkResult                                                         returnValue,
        format::HandleId                                                 device,
        format::HandleId                                                 deferredOperation,
        format::HandleId                                                 pipelineCache,
        uint32_t                                                         createInfoCount,
        StructPointerDecoder<Decoded_VkRayTracingPipelineCreateInfoKHR>* pCreateInfos,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>*             pAllocator,
        HandlePointerDecoder<VkPipeline>*                                pPipelines) override;

    void Process_vkCmdSetRayTracingPipelineStackSizeKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    pipelineStackSize) override;

    void Process_vkCmdTraceRaysIndirectKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pRaygenShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pMissShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pHitShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pCallableShaderBindingTable,
        VkDeviceAddress                             indirectDeviceAddress) override;

    void Process_vkCmdTraceRaysKHR(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pRaygenShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pMissShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pHitShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pCallableShaderBindingTable,
        uint32_t                                    width,
        uint32_t                                    height,
        uint32_t                                    depth) override;

    void Process_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(
        const ApiCallInfo&                          call_info,
        VkResult                                    returnValue,
        format::HandleId                            device,
        format::HandleId                            pipeline,
        uint32_t                                    firstGroup,
        uint32_t                                    groupCount,
        size_t                                      dataSize,
        PointerDecoder<uint8_t>*                    pData) override;

    void Process_vkGetRayTracingShaderGroupStackSizeKHR(
        const ApiCallInfo&                          call_info,
        VkDeviceSize                                returnValue,
        format::HandleId                            device,
        format::HandleId                            pipeline,
        uint32_t                                    group,
        VkShaderGroupShaderKHR                      groupShader) override;

    void Process_vkCmdDrawMeshTasksEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        uint32_t                                    groupCountX,
        uint32_t                                    groupCountY,
        uint32_t                                    groupCountZ) override;

    void Process_vkCmdDrawMeshTasksIndirectCountEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        format::HandleId                            countBuffer,
        VkDeviceSize                                countBufferOffset,
        uint32_t                                    maxDrawCount,
        uint32_t                                    stride) override;

    void Process_vkCmdDrawMeshTasksIndirectEXT(
        const ApiCallInfo&                          call_info,
        format::HandleId                            commandBuffer,
        format::HandleId                            buffer,
        VkDeviceSize                                offset,
        uint32_t                                    drawCount,
        uint32_t                                    stride) override;

  private:
    VkAccelerationStructureKHR MaterializeVkAccelerationStructureKHR(format::HandleId id);
    VkAccelerationStructureNV MaterializeVkAccelerationStructureNV(format::HandleId id);
    VkBuffer MaterializeVkBuffer(format::HandleId id);
    VkBufferView MaterializeVkBufferView(format::HandleId id);
    VkCommandBuffer MaterializeVkCommandBuffer(format::HandleId id);
    VkCommandPool MaterializeVkCommandPool(format::HandleId id);
    VkDataGraphPipelineSessionARM MaterializeVkDataGraphPipelineSessionARM(format::HandleId id);
    VkDebugReportCallbackEXT MaterializeVkDebugReportCallbackEXT(format::HandleId id);
    VkDebugUtilsMessengerEXT MaterializeVkDebugUtilsMessengerEXT(format::HandleId id);
    VkDeferredOperationKHR MaterializeVkDeferredOperationKHR(format::HandleId id);
    VkDescriptorPool MaterializeVkDescriptorPool(format::HandleId id);
    VkDescriptorSet MaterializeVkDescriptorSet(format::HandleId id);
    VkDescriptorSetLayout MaterializeVkDescriptorSetLayout(format::HandleId id);
    VkDescriptorUpdateTemplate MaterializeVkDescriptorUpdateTemplate(format::HandleId id);
    VkDescriptorUpdateTemplateKHR MaterializeVkDescriptorUpdateTemplateKHR(format::HandleId id);
    VkDevice MaterializeVkDevice(format::HandleId id);
    VkDeviceMemory MaterializeVkDeviceMemory(format::HandleId id);
    VkDisplayKHR MaterializeVkDisplayKHR(format::HandleId id);
    VkDisplayModeKHR MaterializeVkDisplayModeKHR(format::HandleId id);
    VkEvent MaterializeVkEvent(format::HandleId id);
    VkFence MaterializeVkFence(format::HandleId id);
    VkFramebuffer MaterializeVkFramebuffer(format::HandleId id);
    VkImage MaterializeVkImage(format::HandleId id);
    VkImageView MaterializeVkImageView(format::HandleId id);
    VkIndirectCommandsLayoutEXT MaterializeVkIndirectCommandsLayoutEXT(format::HandleId id);
    VkIndirectCommandsLayoutNV MaterializeVkIndirectCommandsLayoutNV(format::HandleId id);
    VkIndirectExecutionSetEXT MaterializeVkIndirectExecutionSetEXT(format::HandleId id);
    VkInstance MaterializeVkInstance(format::HandleId id);
    VkMicromapEXT MaterializeVkMicromapEXT(format::HandleId id);
    VkOpticalFlowSessionNV MaterializeVkOpticalFlowSessionNV(format::HandleId id);
    VkPerformanceConfigurationINTEL MaterializeVkPerformanceConfigurationINTEL(format::HandleId id);
    VkPhysicalDevice MaterializeVkPhysicalDevice(format::HandleId id);
    VkPipeline MaterializeVkPipeline(format::HandleId id);
    VkPipelineBinaryKHR MaterializeVkPipelineBinaryKHR(format::HandleId id);
    VkPipelineCache MaterializeVkPipelineCache(format::HandleId id);
    VkPipelineLayout MaterializeVkPipelineLayout(format::HandleId id);
    VkPrivateDataSlot MaterializeVkPrivateDataSlot(format::HandleId id);
    VkPrivateDataSlotEXT MaterializeVkPrivateDataSlotEXT(format::HandleId id);
    VkQueryPool MaterializeVkQueryPool(format::HandleId id);
    VkQueue MaterializeVkQueue(format::HandleId id);
    VkRenderPass MaterializeVkRenderPass(format::HandleId id);
    VkSampler MaterializeVkSampler(format::HandleId id);
    VkSamplerYcbcrConversion MaterializeVkSamplerYcbcrConversion(format::HandleId id);
    VkSamplerYcbcrConversionKHR MaterializeVkSamplerYcbcrConversionKHR(format::HandleId id);
    VkSemaphore MaterializeVkSemaphore(format::HandleId id);
    VkShaderEXT MaterializeVkShaderEXT(format::HandleId id);
    VkShaderModule MaterializeVkShaderModule(format::HandleId id);
    VkSurfaceKHR MaterializeVkSurfaceKHR(format::HandleId id);
    VkSwapchainKHR MaterializeVkSwapchainKHR(format::HandleId id);
    VkTensorARM MaterializeVkTensorARM(format::HandleId id);
    VkTensorViewARM MaterializeVkTensorViewARM(format::HandleId id);
    VkValidationCacheEXT MaterializeVkValidationCacheEXT(format::HandleId id);
    VkVideoSessionKHR MaterializeVkVideoSessionKHR(format::HandleId id);
    VkVideoSessionParametersKHR MaterializeVkVideoSessionParametersKHR(format::HandleId id);
};

const VulkanNativeCommandInfo* GetVulkanNativeCommandInfo(const char* name);

const VulkanNativeCommandInfo* GetVulkanNativeCommandInfo(uint32_t call_id);

const char* GetVulkanApiCallName(uint32_t call_id);

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_GENERATED_VULKAN_NATIVE_CALL_CONSUMER_H
