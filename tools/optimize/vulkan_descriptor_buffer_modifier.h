#ifndef GFXRECON_TOOLS_OPTIMIZE_VULKAN_DESCRIPTOR_BUFFER_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_VULKAN_DESCRIPTOR_BUFFER_MODIFIER_H

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vulkan/vulkan_core.h>

#include "decode/api_decoder.h"
#include "format/format.h"
#include "util/defines.h"
#include "encode/parameter_buffer.h"
#include "util/vulkan_modifier_base.h"
#include "vulkan_optimize_options.h"

#include <list>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

// Performs optimization of descriptor buffer content
// In the first pass tracks memory modifications in order to indentify memory ranges
// containing device addresses and shader group handles
// In second pass injects FixDeviceAddress and FixShaderGroupHandle metacommand
// instructing the replayer to replace memory range with a device address or shader group handle value
class VulkanDescriptorBufferModifier : public util::VulkanModifierBase
{
  public:
    VulkanDescriptorBufferModifier() = default;
    VulkanDescriptorBufferModifier(const VulkanOptimizationOptions& options);

    virtual bool CanOptimize() override;

    virtual void
    ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override;

    virtual void ProcessFixDescriptorDataCommand(const format::FixDescriptorDataCommandHeader& header,
                                                 const format::DescriptorDataLocationInfo*     infos) override;

    virtual void
    ProcessFixShadowMemoryCommand(format::HandleId memory_id, uint64_t map_memory, uint64_t shadow_memory) override;

    virtual void Process_vkCreateBuffer(const ApiCallInfo&                                   call_info,
                                        VkResult                                             returnValue,
                                        format::HandleId                                     device,
                                        StructPointerDecoder<Decoded_VkBufferCreateInfo>*    pCreateInfo,
                                        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                        HandlePointerDecoder<VkBuffer>*                      pBuffer) override;

    virtual void Process_vkDestroyBuffer(const ApiCallInfo&                                   call_info,
                                         format::HandleId                                     device,
                                         format::HandleId                                     buffer,
                                         StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    virtual void Process_vkAllocateMemory(const ApiCallInfo&                                   call_info,
                                          VkResult                                             returnValue,
                                          format::HandleId                                     device,
                                          StructPointerDecoder<Decoded_VkMemoryAllocateInfo>*  pAllocateInfo,
                                          StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                          HandlePointerDecoder<VkDeviceMemory>*                pMemory) override;

    virtual void Process_vkFreeMemory(const ApiCallInfo&                                   call_info,
                                      format::HandleId                                     device,
                                      format::HandleId                                     memory,
                                      StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    virtual void Process_vkMapMemory(const ApiCallInfo&               call_info,
                                     VkResult                         returnValue,
                                     format::HandleId                 device,
                                     format::HandleId                 memory,
                                     VkDeviceSize                     offset,
                                     VkDeviceSize                     size,
                                     VkMemoryMapFlags                 flags,
                                     PointerDecoder<uint64_t, void*>* ppData) override;

    virtual void Process_vkMapMemory2(const ApiCallInfo&                             call_info,
                                      VkResult                                       returnValue,
                                      format::HandleId                               device,
                                      StructPointerDecoder<Decoded_VkMemoryMapInfo>* pMemoryMapInfo,
                                      PointerDecoder<uint64_t, void*>*               ppData) override;

    virtual void Process_vkUnmapMemory2(const ApiCallInfo&                               call_info,
                                        VkResult                                         returnValue,
                                        format::HandleId                                 device,
                                        StructPointerDecoder<Decoded_VkMemoryUnmapInfo>* pMemoryUnmapInfo) override;

    virtual void
    Process_vkUnmapMemory(const ApiCallInfo& call_info, format::HandleId device, format::HandleId memory) override;

    virtual void Process_vkBindBufferMemory(const ApiCallInfo& call_info,
                                            VkResult           returnValue,
                                            format::HandleId   device,
                                            format::HandleId   buffer,
                                            format::HandleId   memory,
                                            VkDeviceSize       memory_offset) override;

    virtual void Process_vkBindBufferMemory2(const ApiCallInfo&                                    call_info,
                                             VkResult                                              returnValue,
                                             format::HandleId                                      device,
                                             uint32_t                                              bindInfoCount,
                                             StructPointerDecoder<Decoded_VkBindBufferMemoryInfo>* pBindInfos) override;

    virtual void
    Process_vkAllocateCommandBuffers(const ApiCallInfo&                                         call_info,
                                     VkResult                                                   returnValue,
                                     format::HandleId                                           device,
                                     StructPointerDecoder<Decoded_VkCommandBufferAllocateInfo>* pAllocateInfo,
                                     HandlePointerDecoder<VkCommandBuffer>* pCommandBuffers) override;

    virtual void
    Process_vkBeginCommandBuffer(const ApiCallInfo&                                      call_info,
                                 VkResult                                                returnValue,
                                 format::HandleId                                        commandBuffer,
                                 StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo) override;

    virtual void Process_vkCmdCopyBuffer(const ApiCallInfo&                          call_info,
                                         format::HandleId                            commandBuffer,
                                         format::HandleId                            srcBuffer,
                                         format::HandleId                            dstBuffer,
                                         uint32_t                                    regionCount,
                                         StructPointerDecoder<Decoded_VkBufferCopy>* pRegions) override;

    virtual void Process_vkCmdCopyBuffer2(const ApiCallInfo&                               call_info,
                                          format::HandleId                                 commandBuffer,
                                          StructPointerDecoder<Decoded_VkCopyBufferInfo2>* pCopyBufferInfo) override;

    virtual void Process_vkCmdCopyBuffer2KHR(const ApiCallInfo&                               call_info,
                                             format::HandleId                                 commandBuffer,
                                             StructPointerDecoder<Decoded_VkCopyBufferInfo2>* pCopyBufferInfo) override;

    virtual void Process_vkFreeCommandBuffers(const ApiCallInfo&                     call_info,
                                              format::HandleId                       device,
                                              format::HandleId                       commandPool,
                                              uint32_t                               commandBufferCount,
                                              HandlePointerDecoder<VkCommandBuffer>* pCommandBuffers) override;

    virtual void Process_vkCmdUpdateBuffer(const ApiCallInfo&       call_info,
                                           format::HandleId         commandBuffer,
                                           format::HandleId         dstBuffer,
                                           VkDeviceSize             dstOffset,
                                           VkDeviceSize             dataSize,
                                           PointerDecoder<uint8_t>* pData) override;

    virtual void Process_vkCmdPushConstants(const ApiCallInfo&       call_info,
                                            format::HandleId         commandBuffer,
                                            format::HandleId         layout,
                                            VkShaderStageFlags       stageFlags,
                                            uint32_t                 offset,
                                            uint32_t                 size,
                                            PointerDecoder<uint8_t>* pValues) override;

    virtual void Process_vkGetDescriptorEXT(const ApiCallInfo&                                    call_info,
                                            format::HandleId                                      device,
                                            StructPointerDecoder<Decoded_VkDescriptorGetInfoEXT>* pDescriptorInfo,
                                            size_t                                                dataSize,
                                            PointerDecoder<uint8_t>*                              pDescriptor) override;

  private:
    std::vector<format::DescriptorDataLocationInfo>
    GetDescriptorsInFillMemory(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data);

    void WriteFixDescriptorDataCmd(format::HandleId                    memory_id,
                                   uint64_t                            num_of_locations,
                                   format::DescriptorDataLocationInfo* desc_locations);

  private:
    struct BufferInfo
    {
        format::HandleId    handle;
        uint64_t            size;
        VkBufferUsageFlags  usage;
        VkBufferCreateFlags flags;
        uint64_t            creation_index;
        uint64_t            destruction_index;
    };

    struct MemoryBindingRecord
    {
        format::HandleId handle;
        bool             isBuffer;
        uint64_t         offset;
    };

    struct MemoryMapping
    {
        VkDeviceSize offset;
        VkDeviceSize size;
        uint64_t     map_memory;
        uint64_t     shadow_memory;
    };

    struct DeviceMemoryInfo
    {
        format::HandleId                 handle;
        uint64_t                         size;
        uint32_t                         type_index;
        VkMemoryAllocateFlags            flags;
        uint64_t                         creation_index;
        uint64_t                         destruction_index;
        MemoryMapping                    mapping;
        std::vector<MemoryBindingRecord> memory_binding_records_;
    };

    struct CommandBufferInfo
    {
        CommandBufferInfo(format::HandleId          handle,
                          format::HandleId          device_id,
                          VkCommandBufferLevel      level,
                          VkCommandBufferUsageFlags usage,
                          uint64_t                  creation_index) :
            handle_(handle),
            device_id_(device_id), level_(level), usage_(usage), creation_index_(creation_index)
        {}
        format::HandleId          handle_;
        format::HandleId          device_id_;
        VkCommandBufferLevel      level_;
        VkCommandBufferUsageFlags usage_;
        uint64_t                  creation_index_;
        uint64_t                  destruction_index_;
    };

  private:
    // -----buffer handle-----BufferObject
    std::unordered_map<format::HandleId, BufferInfo> buffer_entries_;

    // -----memory and binding-----MemoryObject
    std::unordered_map<format::HandleId, DeviceMemoryInfo> memory_binding_entries_;

    // All command buffer entries
    std::unordered_map<format::HandleId, CommandBufferInfo> command_buffer_entries_;

    // -----address of memory saved descriptor ----- pair <desc location info,descriptor data>
    typedef std::unordered_map<uint64_t, std::pair<format::DescriptorDataLocationInfo, std::vector<uint8_t>>>
        DescriptorLocationMap;

    // -----memory handle id ----- address of memory saved descriptor ----- pair <desc location info, descriptor data>
    std::unordered_map<format::HandleId, DescriptorLocationMap> device_memory_descriptor_locations;

    // address of memory saved descriptor ----- desc location info
    DescriptorLocationMap descriptor_locations;

    VulkanOptimizationOptions options_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_VULKAN_DESCRIPTOR_BUFFER_MODIFIER_H