#ifndef GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_MODIFIER_H

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

// Performs optimization of raytracing content
// In the first pass tracks memory modifications in order to indentify memory ranges
// containing device addresses and shader group handles
// In second pass injects FixDeviceAddress and FixShaderGroupHandle metacommand
// instructing the replayer to replace memory range with a device address or shader group handle value
class VulkanRayTracingModifier : public util::VulkanModifierBase
{
  public:
    VulkanRayTracingModifier() = default;
    VulkanRayTracingModifier(const VulkanOptimizationOptions& options);

    virtual bool CanOptimize() override;

    virtual void Process_vkGetRayTracingShaderGroupHandlesKHR(const ApiCallInfo&       call_info,
                                                              VkResult                 returnValue,
                                                              format::HandleId         device,
                                                              format::HandleId         pipeline,
                                                              uint32_t                 firstGroup,
                                                              uint32_t                 groupCount,
                                                              size_t                   dataSize,
                                                              PointerDecoder<uint8_t>* pData) override;

    virtual void
    ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override;

    virtual void ProcessInitBufferCommand(format::HandleId device_id,
                                          format::HandleId buffer_id,
                                          uint64_t         data_size,
                                          const uint8_t*   data) override;

    virtual void ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader& header,
                                                const format::AddressLocationInfo*           infos) override;

    virtual void ProcessFixShaderGroupHandleCommand(const format::FixShaderGroupHandleCommandHeader& header,
                                                    const format::ShaderHandleLocationInfo*          infos) override;

    virtual void
    ProcessAccelerationStructureCompactionDependencyCommand(format::HandleId                     parent,
                                                            const std::vector<format::HandleId>& children) override;

    virtual void
    ProcessSetOpaqueAddressCommand(format::HandleId device_id, format::HandleId object_id, uint64_t address) override;

    virtual void
    Process_vkGetBufferDeviceAddress(const ApiCallInfo&                                       call_info,
                                     VkDeviceAddress                                          returnValue,
                                     format::HandleId                                         device,
                                     StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    virtual void
    Process_vkGetBufferDeviceAddressKHR(const ApiCallInfo&                                       call_info,
                                        VkDeviceAddress                                          returnValue,
                                        format::HandleId                                         device,
                                        StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    virtual void
    Process_vkGetBufferDeviceAddressEXT(const ApiCallInfo&                                       call_info,
                                        VkDeviceAddress                                          returnValue,
                                        format::HandleId                                         device,
                                        StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    virtual void Process_vkGetAccelerationStructureDeviceAddressKHR(
        const ApiCallInfo&                                                         call_info,
        VkDeviceAddress                                                            returnValue,
        format::HandleId                                                           device,
        StructPointerDecoder<Decoded_VkAccelerationStructureDeviceAddressInfoKHR>* pInfo) override;

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

    virtual void Process_vkBindBufferMemory(const ApiCallInfo& call_info,
                                            VkResult           returnValue,
                                            format::HandleId   device,
                                            format::HandleId   buffer,
                                            format::HandleId   memory,
                                            VkDeviceSize       memory_offset) override;

    virtual void Process_vkBindImageMemory(const ApiCallInfo& call_info,
                                           VkResult           returnValue,
                                           format::HandleId   device,
                                           format::HandleId   image,
                                           format::HandleId   memory,
                                           VkDeviceSize       memory_offset) override;

    virtual void Process_vkBindBufferMemory2(const ApiCallInfo&                                    call_info,
                                             VkResult                                              returnValue,
                                             format::HandleId                                      device,
                                             uint32_t                                              bindInfoCount,
                                             StructPointerDecoder<Decoded_VkBindBufferMemoryInfo>* pBindInfos) override;

    virtual void Process_vkBindImageMemory2(const ApiCallInfo&                                   call_info,
                                            VkResult                                             returnValue,
                                            format::HandleId                                     device,
                                            uint32_t                                             bindInfoCount,
                                            StructPointerDecoder<Decoded_VkBindImageMemoryInfo>* pBindInfos) override;

    virtual void Process_vkCreateAccelerationStructureKHR(
        const ApiCallInfo&                                                  call_info,
        VkResult                                                            returnValue,
        format::HandleId                                                    device,
        StructPointerDecoder<Decoded_VkAccelerationStructureCreateInfoKHR>* pCreateInfo,
        StructPointerDecoder<Decoded_VkAllocationCallbacks>*                pAllocator,
        HandlePointerDecoder<VkAccelerationStructureKHR>*                   pAccelerationStructure) override;

    virtual void Process_vkGetAccelerationStructureBuildSizesKHR(
        const ApiCallInfo&                                                         call_info,
        format::HandleId                                                           device,
        VkAccelerationStructureBuildTypeKHR                                        buildType,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pBuildInfo,
        PointerDecoder<uint32_t>*                                                  pMaxPrimitiveCounts,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildSizesInfoKHR>*    pSizeInfo) override;

    virtual void
    Process_vkDestroyAccelerationStructureKHR(const ApiCallInfo& call_info,
                                              format::HandleId   device,
                                              format::HandleId   accelerationStructure,
                                              StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    virtual void Process_vkCmdBuildAccelerationStructuresKHR(
        const ApiCallInfo&                                                         call_info,
        format::HandleId                                                           commandBuffer,
        uint32_t                                                                   infoCount,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   ppBuildRangeInfos) override;

    virtual void Process_vkCmdCopyAccelerationStructureKHR(
        const ApiCallInfo&                                                call_info,
        format::HandleId                                                  commandBuffer,
        StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* pInfo) override;

    virtual void Process_vkCmdWriteAccelerationStructuresPropertiesKHR(
        const ApiCallInfo&                                call_info,
        format::HandleId                                  commandBuffer,
        uint32_t                                          accelerationStructureCount,
        HandlePointerDecoder<VkAccelerationStructureKHR>* pAccelerationStructures,
        VkQueryType                                       queryType,
        format::HandleId                                  queryPool,
        uint32_t                                          firstQuery) override;

    virtual void ProcessBuildVulkanAccelerationStructuresMetaCommand(
        format::HandleId                                                           device_id,
        uint32_t                                                                   info_count,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* geometry_infos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   range_infos,
        std::vector<std::vector<VkAccelerationStructureInstanceKHR>>&              instance_buffers_data) override;

    virtual void ProcessCopyVulkanAccelerationStructuresMetaCommand(
        format::HandleId                                                  device_id,
        StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* copy_infos) override;

    virtual void
    Process_vkCreateComputePipelines(const ApiCallInfo&                                         call_info,
                                     VkResult                                                   returnValue,
                                     format::HandleId                                           device,
                                     format::HandleId                                           pipelineCache,
                                     uint32_t                                                   createInfoCount,
                                     StructPointerDecoder<Decoded_VkComputePipelineCreateInfo>* pCreateInfos,
                                     StructPointerDecoder<Decoded_VkAllocationCallbacks>*       pAllocator,
                                     HandlePointerDecoder<VkPipeline>*                          pPipelines) override;

    virtual void Process_vkDestroyPipeline(const ApiCallInfo&                                   call_info,
                                           format::HandleId                                     device,
                                           format::HandleId                                     pipeline,
                                           StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

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

    virtual void Process_vkCmdBindPipeline(const ApiCallInfo&  call_info,
                                           format::HandleId    commandBuffer,
                                           VkPipelineBindPoint pipelineBindPoint,
                                           format::HandleId    pipeline) override;

    virtual void
    Process_vkUpdateDescriptorSets(const ApiCallInfo&                                  call_info,
                                   format::HandleId                                    device,
                                   uint32_t                                            descriptorWriteCount,
                                   StructPointerDecoder<Decoded_VkWriteDescriptorSet>* pDescriptorWrites,
                                   uint32_t                                            descriptorCopyCount,
                                   StructPointerDecoder<Decoded_VkCopyDescriptorSet>*  pDescriptorCopies) override;

    virtual void Process_vkCmdBindDescriptorSets(const ApiCallInfo&                     call_info,
                                                 format::HandleId                       commandBuffer,
                                                 VkPipelineBindPoint                    pipelineBindPoint,
                                                 format::HandleId                       layout,
                                                 uint32_t                               firstSet,
                                                 uint32_t                               descriptorSetCount,
                                                 HandlePointerDecoder<VkDescriptorSet>* pDescriptorSets,
                                                 uint32_t                               dynamicOffsetCount,
                                                 PointerDecoder<uint32_t>*              pDynamicOffsets) override;

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

    virtual void Process_vkCmdExecuteCommands(const ApiCallInfo&                     call_info,
                                              format::HandleId                       commandBuffer,
                                              uint32_t                               commandBufferCount,
                                              HandlePointerDecoder<VkCommandBuffer>* pCommandBuffers) override;

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

    virtual void Process_vkQueueSubmit(const ApiCallInfo&                          call_info,
                                       VkResult                                    returnValue,
                                       format::HandleId                            queue,
                                       uint32_t                                    submitCount,
                                       StructPointerDecoder<Decoded_VkSubmitInfo>* pSubmits,
                                       format::HandleId                            fence) override;

    virtual void Process_vkQueueSubmit2(const ApiCallInfo&                           call_info,
                                        VkResult                                     returnValue,
                                        format::HandleId                             queue,
                                        uint32_t                                     submitCount,
                                        StructPointerDecoder<Decoded_VkSubmitInfo2>* pSubmits,
                                        format::HandleId                             fence) override;

    virtual void Process_vkQueueSubmit2KHR(const ApiCallInfo&                           call_info,
                                           VkResult                                     returnValue,
                                           format::HandleId                             queue,
                                           uint32_t                                     submitCount,
                                           StructPointerDecoder<Decoded_VkSubmitInfo2>* pSubmits,
                                           format::HandleId                             fence) override;

  private:
    std::vector<format::ShaderHandleLocationInfo> GetShaderGroupHandlesInFillMemory(const void* data, size_t size);

    std::vector<format::AddressLocationInfo> GetBufferDeviceAddressesInFillMemory(
        std::vector<format::AddressLocationInfo>& as_locations, const void* data, size_t size);

    std::vector<format::AddressLocationInfo> GetAccelerationStructureDeviceAddressesInFillMemory(const void* data,
                                                                                                 size_t      size);

    void WriteFixShaderGroupHandleCmd(format::HandleId                  relation_id,
                                      uint64_t                          num_of_locations,
                                      format::ShaderHandleLocationInfo* locations);

    void WriteFixDeviceAddressCmd(format::HandleId             relation_id,
                                  uint64_t                     num_of_as_locations,
                                  format::AddressLocationInfo* as_locations,
                                  uint64_t                     num_of_buf_locations,
                                  format::AddressLocationInfo* buf_locations);

    void WriteInitBufferDataFixCmd();

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

    struct DeviceMemoryInfo
    {
        format::HandleId                 handle;
        uint64_t                         size;
        uint32_t                         type_index;
        VkMemoryAllocateFlags            flags;
        uint64_t                         creation_index;
        uint64_t                         destruction_index;
        std::vector<MemoryBindingRecord> memory_binding_records_;
    };

    struct AccelerationStructureInfo
    {
        format::HandleId               as_handle;
        format::HandleId               buf_handle;
        uint64_t                       size;
        uint64_t                       offset;
        VkAccelerationStructureTypeKHR type;
        VkDeviceAddress                device_address;
        bool                           bind_descriptor;
        uint64_t                       creation_index;
        uint64_t                       destruction_index;
    };

    struct AccelerationStructureBuildInfo
    {
        VkAccelerationStructureBuildGeometryInfoKHR                                      build_infos;
        std::vector<VkAccelerationStructureGeometryKHR>                                  geometry_infos;
        std::unordered_map<uint64_t, VkAccelerationStructureTrianglesOpacityMicromapEXT> omm_infos;
        std::unordered_map<uint64_t, std::vector<VkMicromapUsageEXT>>                    usage_infos;
        std::vector<uint32_t>                                                            primitive_counts;
        bool                                                                             is_first_built;
        bool                                                                             is_meta_copy;
        uint64_t                                                                         process_compacted_as_index;
        encode::ParameterBuffer                                                          create_parameter_buffer;
        encode::ParameterBuffer                                                          get_address_parameter_buffer;
        format::HandleId                                                                 source_of_compaction;
    };

    struct PipelineInfo
    {
        format::HandleId handle;
        VkStructureType  sType;
        uint64_t         creation_index;
        uint64_t         destruction_index;
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

    struct InitBufferInfo
    {
        format::HandleId                              buffer_id;
        format::HandleId                              device_id;
        std::vector<format::ShaderHandleLocationInfo> shader_handle_locations;
        std::vector<format::AddressLocationInfo>      device_address_locations;
        std::vector<uint8_t>                          init_buffer_data;
    };

  private:
    // -----buffer handle-----BufferObject
    std::unordered_map<format::HandleId, BufferInfo> buffer_entries_;

    // -----memory and binding-----MemoryObject
    std::unordered_map<format::HandleId, DeviceMemoryInfo> memory_binding_entries_;

    // -----acceleration structure handle-----AccelerationStructureObject
    std::unordered_map<format::HandleId, AccelerationStructureInfo> acceleration_structure_entries_;

    // -----acceleration structure handle-----AccelerationStructureBuildInfo
    std::unordered_map<format::HandleId, AccelerationStructureBuildInfo> acceleration_structure_build_infos_;

    // -----buffer device address-----handle id
    std::unordered_map<uint64_t, format::HandleId> buffer_device_addresses_;

    // -----acceleration structure device address-----handle id
    std::unordered_map<uint64_t, format::HandleId> acceleration_structure_device_addresses_;

    // -----pipeline handle-----group index-----SGH location info
    std::unordered_map<format::HandleId, std::unordered_map<uint64_t, format::ShaderHandleLocationInfo>>
        shader_group_handle_entries_;

    // All command buffer entries
    std::unordered_map<format::HandleId, CommandBufferInfo> command_buffer_entries_;
    // Command buffers in which binding of compute pipeline ocurred
    std::unordered_set<format::HandleId> command_buffers_with_compute_;

    // Fill memory indices, we reset them per vkQueueSubmit so we can tie them later
    std::vector<uint64_t> fill_memory_indices_per_submit_;
    // Filled during first pass
    std::list<uint64_t> fill_memory_indices_to_inspect_;

    std::unordered_set<format::HandleId> instance_buffers_;

    // -----init buffer handle-----InitBufferObject
    std::unordered_map<format::HandleId, InitBufferInfo> init_buffer_entries_;

    VulkanOptimizationOptions options_;

    bool heuristic_check_compute(format::HandleId command_buffer);
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_MODIFIER_H
