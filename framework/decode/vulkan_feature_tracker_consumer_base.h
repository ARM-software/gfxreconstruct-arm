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

#ifndef GFXRECON_DECODE_VULKAN_FEATURE_TRACKER_CONSUMER_BASE_H
#define GFXRECON_DECODE_VULKAN_FEATURE_TRACKER_CONSUMER_BASE_H

#include "decode/referenced_resource_table.h"
#include "generated/generated_vulkan_consumer.h"
#include "util/defines.h"
#include "util/memory_output_stream.h"
#include "encode/parameter_buffer.h"
#include "util/vulkan_modifier_base.h"

#include "vulkan/vulkan.h"

#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanFeatureTrackerConsumerBase : public util::VulkanModifierBase
{
  public:
    VulkanFeatureTrackerConsumerBase();

    bool CanOptimize() override;

    void PrintAllFeatures();

    virtual void Process_vkCreateInstance(const ApiCallInfo&                                   call_info,
                                          VkResult                                             returnValue,
                                          StructPointerDecoder<Decoded_VkInstanceCreateInfo>*  pCreateInfo,
                                          StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                          HandlePointerDecoder<VkInstance>*                    pInstance) override;

    virtual void Process_vkCreateDevice(const ApiCallInfo&                                   call_info,
                                        VkResult                                             returnValue,
                                        format::HandleId                                     physicalDevice,
                                        StructPointerDecoder<Decoded_VkDeviceCreateInfo>*    pCreateInfo,
                                        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                        HandlePointerDecoder<VkDevice>*                      pDevice) override;

    virtual void Process_vkCmdBeginRendering(const ApiCallInfo&                             call_info,
                                             format::HandleId                               commandBuffer,
                                             StructPointerDecoder<Decoded_VkRenderingInfo>* pRenderingInfo) override;

    virtual void Process_vkCmdBeginRenderingKHR(const ApiCallInfo&                             call_info,
                                                format::HandleId                               commandBuffer,
                                                StructPointerDecoder<Decoded_VkRenderingInfo>* pRenderingInfo) override;

    virtual void Process_vkCreateBuffer(const ApiCallInfo&                                   call_info,
                                        VkResult                                             returnValue,
                                        format::HandleId                                     device,
                                        StructPointerDecoder<Decoded_VkBufferCreateInfo>*    pCreateInfo,
                                        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                        HandlePointerDecoder<VkBuffer>*                      pBuffer) override;
    virtual void Process_vkCreateImage(const ApiCallInfo&                                   call_info,
                                       VkResult                                             returnValue,
                                       format::HandleId                                     device,
                                       StructPointerDecoder<Decoded_VkImageCreateInfo>*     pCreateInfo,
                                       StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                       HandlePointerDecoder<VkImage>*                       pImage) override;

    virtual void Process_vkCreateImageView(const ApiCallInfo&                                   call_info,
                                           VkResult                                             returnValue,
                                           format::HandleId                                     device,
                                           StructPointerDecoder<Decoded_VkImageViewCreateInfo>* pCreateInfo,
                                           StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                           HandlePointerDecoder<VkImageView>*                   pView) override;
    virtual void
    Process_vkCreateGraphicsPipelines(const ApiCallInfo&                                          call_info,
                                      VkResult                                                    returnValue,
                                      format::HandleId                                            device,
                                      format::HandleId                                            pipelineCache,
                                      uint32_t                                                    createInfoCount,
                                      StructPointerDecoder<Decoded_VkGraphicsPipelineCreateInfo>* pCreateInfos,
                                      StructPointerDecoder<Decoded_VkAllocationCallbacks>*        pAllocator,
                                      HandlePointerDecoder<VkPipeline>*                           pPipelines) override;

    virtual void
    Process_vkCreateComputePipelines(const ApiCallInfo&                                         call_info,
                                     VkResult                                                   returnValue,
                                     format::HandleId                                           device,
                                     format::HandleId                                           pipelineCache,
                                     uint32_t                                                   createInfoCount,
                                     StructPointerDecoder<Decoded_VkComputePipelineCreateInfo>* pCreateInfos,
                                     StructPointerDecoder<Decoded_VkAllocationCallbacks>*       pAllocator,
                                     HandlePointerDecoder<VkPipeline>*                          pPipelines) override;

    virtual void Process_vkCreateRayTracingPipelinesKHR(
        const ApiCallInfo&                                               call_info,
        VkResult                                                         returnValue,
        format::HandleId                                                 device,
        format::HandleId                                                 deferredOperation,
        format::HandleId                                                 pipelineCache,
        uint32_t                                                         createInfoCount,
        StructPointerDecoder<Decoded_VkRayTracingPipelineCreateInfoKHR>* pCreateInfos,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>*             pAllocator,
        HandlePointerDecoder<VkPipeline>*                                pPipelines) override;

    virtual void Process_vkCmdDrawIndirect(const ApiCallInfo& call_info,
                                           format::HandleId   commandBuffer,
                                           format::HandleId   buffer,
                                           VkDeviceSize       offset,
                                           uint32_t           drawCount,
                                           uint32_t           stride) override;

    virtual void Process_vkCmdDrawIndirectCount(const ApiCallInfo& call_info,
                                                format::HandleId   commandBuffer,
                                                format::HandleId   buffer,
                                                VkDeviceSize       offset,
                                                format::HandleId   countBuffer,
                                                VkDeviceSize       countBufferOffset,
                                                uint32_t           maxDrawCount,
                                                uint32_t           stride) override;

    virtual void Process_vkCmdDrawIndexedIndirect(const ApiCallInfo& call_info,
                                                  format::HandleId   commandBuffer,
                                                  format::HandleId   buffer,
                                                  VkDeviceSize       offset,
                                                  uint32_t           drawCount,
                                                  uint32_t           stride) override;

    virtual void Process_vkCmdDrawIndexedIndirectCount(const ApiCallInfo& call_info,
                                                       format::HandleId   commandBuffer,
                                                       format::HandleId   buffer,
                                                       VkDeviceSize       offset,
                                                       format::HandleId   countBuffer,
                                                       VkDeviceSize       countBufferOffset,
                                                       uint32_t           maxDrawCount,
                                                       uint32_t           stride) override;

    virtual void
    Process_vkBeginCommandBuffer(const ApiCallInfo&                                      call_info,
                                 VkResult                                                returnValue,
                                 format::HandleId                                        commandBuffer,
                                 StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo) override;

    virtual void Process_vkCmdSetPolygonModeEXT(const ApiCallInfo& call_info,
                                                format::HandleId   commandBuffer,
                                                VkPolygonMode      polygonMode) override;

    virtual void Process_vkCmdSetViewport(const ApiCallInfo&                        call_info,
                                          format::HandleId                          commandBuffer,
                                          uint32_t                                  firstViewport,
                                          uint32_t                                  viewportCount,
                                          StructPointerDecoder<Decoded_VkViewport>* pViewports) override;

    virtual void Process_vkCmdSetScissor(const ApiCallInfo&                      call_info,
                                         format::HandleId                        commandBuffer,
                                         uint32_t                                firstScissor,
                                         uint32_t                                scissorCount,
                                         StructPointerDecoder<Decoded_VkRect2D>* pScissors) override;

    virtual void
    Process_vkCmdSetExclusiveScissorNV(const ApiCallInfo&                      call_info,
                                       format::HandleId                        commandBuffer,
                                       uint32_t                                firstExclusiveScissor,
                                       uint32_t                                exclusiveScissorCount,
                                       StructPointerDecoder<Decoded_VkRect2D>* pExclusiveScissors) override;

    virtual void Process_vkCreateSampler(const ApiCallInfo&                                   call_info,
                                         VkResult                                             returnValue,
                                         format::HandleId                                     device,
                                         StructPointerDecoder<Decoded_VkSamplerCreateInfo>*   pCreateInfo,
                                         StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                         HandlePointerDecoder<VkSampler>*                     pSampler) override;

    virtual void Process_vkCmdWriteAccelerationStructuresPropertiesNV(
        const ApiCallInfo&                               call_info,
        format::HandleId                                 commandBuffer,
        uint32_t                                         accelerationStructureCount,
        HandlePointerDecoder<VkAccelerationStructureNV>* pAccelerationStructures,
        VkQueryType                                      queryType,
        format::HandleId                                 queryPool,
        uint32_t                                         firstQuery) override;

    virtual void Process_vkWriteMicromapsPropertiesEXT(const ApiCallInfo&                   call_info,
                                                       VkResult                             returnValue,
                                                       format::HandleId                     device,
                                                       uint32_t                             micromapCount,
                                                       HandlePointerDecoder<VkMicromapEXT>* pMicromaps,
                                                       VkQueryType                          queryType,
                                                       size_t                               dataSize,
                                                       PointerDecoder<uint8_t>*             pData,
                                                       size_t                               stride) override;

    virtual void Process_vkCmdWriteMicromapsPropertiesEXT(const ApiCallInfo&                   call_info,
                                                          format::HandleId                     commandBuffer,
                                                          uint32_t                             micromapCount,
                                                          HandlePointerDecoder<VkMicromapEXT>* pMicromaps,
                                                          VkQueryType                          queryType,
                                                          format::HandleId                     queryPool,
                                                          uint32_t                             firstQuery) override;

    virtual void Process_vkWriteAccelerationStructuresPropertiesKHR(
        const ApiCallInfo&                                call_info,
        VkResult                                          returnValue,
        format::HandleId                                  device,
        uint32_t                                          accelerationStructureCount,
        HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructures,
        VkQueryType                                       queryType,
        size_t                                            dataSize,
        PointerDecoder<uint8_t>*                          pData,
        size_t                                            stride) override;

    virtual void Process_vkCmdWriteAccelerationStructuresPropertiesKHR(
        const ApiCallInfo&                                call_info,
        format::HandleId                                  commandBuffer,
        uint32_t                                          accelerationStructureCount,
        HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructures,
        VkQueryType                                       queryType,
        format::HandleId                                  queryPool,
        uint32_t                                          firstQuery) override;

    virtual void Process_vkResetQueryPool(const ApiCallInfo& call_info,
                                          format::HandleId   device,
                                          format::HandleId   queryPool,
                                          uint32_t           firstQuery,
                                          uint32_t           queryCount) override;

    virtual void Process_vkCreateSwapchainKHR(const ApiCallInfo&                                      call_info,
                                              VkResult                                                returnValue,
                                              format::HandleId                                        device,
                                              StructPointerDecoder<Decoded_VkSwapchainCreateInfoKHR>* pCreateInfo,
                                              StructPointerDecoder<Decoded_VkAllocationCallbacks>*    pAllocator,
                                              HandlePointerDecoder<VkSwapchainKHR>* pSwapchain) override;

    virtual void
    Process_vkCreateSharedSwapchainsKHR(const ApiCallInfo&                                      call_info,
                                        VkResult                                                returnValue,
                                        format::HandleId                                        device,
                                        uint32_t                                                swapchainCount,
                                        StructPointerDecoder<Decoded_VkSwapchainCreateInfoKHR>* pCreateInfos,
                                        StructPointerDecoder<Decoded_VkAllocationCallbacks>*    pAllocator,
                                        HandlePointerDecoder<VkSwapchainKHR>*                   pSwapchains) override;

    virtual void Process_vkCmdBindIndexBuffer(const ApiCallInfo& call_info,
                                              format::HandleId   commandBuffer,
                                              format::HandleId   buffer,
                                              VkDeviceSize       offset,
                                              VkIndexType        indexType) override;

    virtual void Process_vkCmdBindIndexBuffer2(const ApiCallInfo& call_info,
                                               format::HandleId   commandBuffer,
                                               format::HandleId   buffer,
                                               VkDeviceSize       offset,
                                               VkDeviceSize       size,
                                               VkIndexType        indexType) override;

    virtual void Process_vkCmdBindIndexBuffer2KHR(const ApiCallInfo& call_info,
                                                  format::HandleId   commandBuffer,
                                                  format::HandleId   buffer,
                                                  VkDeviceSize       offset,
                                                  VkDeviceSize       size,
                                                  VkIndexType        indexType) override;

    virtual void Process_vkCmdSetDepthBias(const ApiCallInfo& call_info,
                                           format::HandleId   commandBuffer,
                                           float              depthBiasConstantFactor,
                                           float              depthBiasClamp,
                                           float              depthBiasSlopeFactor) override;

    virtual void
    Process_vkCmdSetLineWidth(const ApiCallInfo& call_info, format::HandleId commandBuffer, float lineWidth) override;

    virtual void Process_vkCmdBeginQuery(const ApiCallInfo&  call_info,
                                         format::HandleId    commandBuffer,
                                         format::HandleId    queryPool,
                                         uint32_t            query,
                                         VkQueryControlFlags flags) override;

    virtual void Process_vkCreateShaderModule(const ApiCallInfo&                                      call_info,
                                              VkResult                                                returnValue,
                                              format::HandleId                                        device,
                                              StructPointerDecoder<Decoded_VkShaderModuleCreateInfo>* pCreateInfo,
                                              StructPointerDecoder<Decoded_VkAllocationCallbacks>*    pAllocator,
                                              HandlePointerDecoder<VkShaderModule>* pShaderModule) override;

    virtual void Process_vkCreateSemaphore(const ApiCallInfo&                                   call_info,
                                           VkResult                                             returnValue,
                                           format::HandleId                                     device,
                                           StructPointerDecoder<Decoded_VkSemaphoreCreateInfo>* pCreateInfo,
                                           StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                           HandlePointerDecoder<VkSemaphore>*                   pSemaphore) override;

    virtual void
    Process_vkGetBufferDeviceAddress(const ApiCallInfo&                                       call_info,
                                     VkDeviceAddress                                          returnValue,
                                     format::HandleId                                         device,
                                     StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    virtual void
    Process_vkGetBufferOpaqueCaptureAddress(const ApiCallInfo&                                       call_info,
                                            uint64_t                                                 returnValue,
                                            format::HandleId                                         device,
                                            StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

  private:
    void parse_SPIRV(const uint32_t* code, uint32_t code_size);

    void checkSwapchainColorspaceEXT(VkColorSpaceKHR s);

    void Process_VkPipelineShaderStageCreateInfo(const VkPipelineShaderStageCreateInfo* info);

    bool ProcessInstanceExtensions();
    bool ProcessDeviceExtensions();

    bool ProcessCore10Features();
    bool ProcessCore11Features();
    bool ProcessCore12Features();
    bool ProcessCore13Features();
    bool ProcessCore14Features();

  private:
    // capture_corexx_ holds the data in the order of passing (first encounter is the first element)
    // output_corexx_ holds the data in the reverse order of passing (first encounter is the last element)
    // same logic for capture_xxxxx_extensions_vector_ and output_xxxxx_extensions_vector_

    std::vector<std::string>              core10_members_as_strings_{};
    VkPhysicalDeviceFeatures              core10_{};
    std::vector<VkPhysicalDeviceFeatures> capture_core10_{};
    std::vector<VkPhysicalDeviceFeatures> output_core10_{};

    std::vector<std::string>                      core11_members_as_strings_{};
    VkPhysicalDeviceVulkan11Features              core11_{};
    std::vector<VkPhysicalDeviceVulkan11Features> capture_core11_{};
    std::vector<VkPhysicalDeviceVulkan11Features> output_core11_{};

    std::vector<std::string>                      core12_members_as_strings_{};
    VkPhysicalDeviceVulkan12Features              core12_{};
    std::vector<VkPhysicalDeviceVulkan12Features> capture_core12_{};
    std::vector<VkPhysicalDeviceVulkan12Features> output_core12_{};

    std::vector<std::string>                      core13_members_as_strings_{};
    VkPhysicalDeviceVulkan13Features              core13_{};
    std::vector<VkPhysicalDeviceVulkan13Features> capture_core13_{};
    std::vector<VkPhysicalDeviceVulkan13Features> output_core13_{};

    std::vector<std::string>                      core14_members_as_strings_{};
    VkPhysicalDeviceVulkan14Features              core14_{};
    std::vector<VkPhysicalDeviceVulkan14Features> capture_core14_{};
    std::vector<VkPhysicalDeviceVulkan14Features> output_core14_{};

    std::vector<std::vector<std::string>> capture_instance_extensions_vector_{};
    std::vector<std::vector<std::string>> capture_device_extensions_vector_{};

    std::unordered_map<std::string, VkBool32> supported_instance_extensions_map_{};
    std::unordered_map<std::string, VkBool32> supported_device_extensions_map_{};

    std::vector<std::vector<std::string>> output_instance_extensions_vector_{};
    std::vector<std::vector<std::string>> output_device_extensions_vector_{};

    std::string consumer_output_log_{};
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_FEATURE_TRACKER_CONSUMER_BASE_H
