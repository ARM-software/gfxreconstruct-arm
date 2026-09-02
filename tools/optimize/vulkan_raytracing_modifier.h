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

#ifndef GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_MODIFIER_H

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vulkan/vulkan_core.h>

#include "decode/api_decoder.h"
#include "format/format.h"
#include "util/defines.h"
#include "encode/parameter_buffer.h"
#include "util/vulkan_modifier_base.h"
#include "decode/vulkan_optimize_options.h"

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

    bool CanOptimize() override;

    void ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override;

    void ProcessInitBufferCommand(format::HandleId device_id,
                                  format::HandleId buffer_id,
                                  uint64_t         data_size,
                                  const uint8_t*   data) override;

    void ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader&    header,
                                        const std::vector<format::AddressLocationInfo>& infos) override;

    void ProcessFixShaderGroupHandleCommand(const format::FixShaderGroupHandleCommandHeader&     header,
                                            const std::vector<format::ShaderHandleLocationInfo>& infos) override;

    void
    ProcessAccelerationStructureCompactionDependencyCommand(format::HandleId                     parent,
                                                            const std::vector<format::HandleId>& children) override;

    void
    ProcessSetOpaqueAddressCommand(format::HandleId device_id, format::HandleId object_id, uint64_t address) override;

    void ProcessFrameEndMarker(uint64_t frame_number) override
    {
        command_buffers_with_compute_.clear();
        transfer_ranges_.clear();
    }

    void ProcessVulkanBuildAccelerationStructuresCommand(
        format::HandleId                                                           device_id,
        uint32_t                                                                   info_count,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* geometry_infos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   range_infos,
        std::vector<std::vector<VkAccelerationStructureInstanceKHR>>&              instance_buffers_data) override;

    void ProcessVulkanCopyAccelerationStructuresCommand(
        format::HandleId                                                  device_id,
        StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* copy_infos) override;

    void Process_vkGetRayTracingShaderGroupHandlesKHR(const ApiCallInfo&                        call_info,
                                                      args::GetRayTracingShaderGroupHandlesKHR& args) override;

    void Process_vkGetBufferDeviceAddress(const ApiCallInfo& call_info, args::GetBufferDeviceAddress& args) override;

    void Process_vkGetBufferDeviceAddressKHR(const ApiCallInfo&               call_info,
                                             args::GetBufferDeviceAddressKHR& args) override;

    void Process_vkGetBufferDeviceAddressEXT(const ApiCallInfo&               call_info,
                                             args::GetBufferDeviceAddressEXT& args) override;

    void
    Process_vkGetAccelerationStructureDeviceAddressKHR(const ApiCallInfo&                              call_info,
                                                       args::GetAccelerationStructureDeviceAddressKHR& args) override;

    void Process_vkCreateBuffer(const ApiCallInfo& call_info, args::CreateBuffer& args) override;

    void Process_vkDestroyBuffer(const ApiCallInfo& call_info, args::DestroyBuffer& args) override;

    void Process_vkAllocateMemory(const ApiCallInfo& call_info, args::AllocateMemory& args) override;

    void Process_vkFreeMemory(const ApiCallInfo& call_info, args::FreeMemory& args) override;

    void Process_vkBindBufferMemory(const ApiCallInfo& call_info, args::BindBufferMemory& args) override;

    void Process_vkBindBufferMemory2(const ApiCallInfo& call_info, args::BindBufferMemory2& args) override;

    void Process_vkBindBufferMemory2KHR(const ApiCallInfo& call_info, args::BindBufferMemory2KHR& args) override;

    void Process_vkBindImageMemory(const ApiCallInfo& call_info, args::BindImageMemory& args) override;

    void Process_vkBindImageMemory2(const ApiCallInfo& call_info, args::BindImageMemory2& args) override;

    void Process_vkCreateAccelerationStructureKHR(const ApiCallInfo&                    call_info,
                                                  args::CreateAccelerationStructureKHR& args) override;

    void Process_vkGetAccelerationStructureBuildSizesKHR(const ApiCallInfo&                           call_info,
                                                         args::GetAccelerationStructureBuildSizesKHR& args) override;

    void Process_vkDestroyAccelerationStructureKHR(const ApiCallInfo&                     call_info,
                                                   args::DestroyAccelerationStructureKHR& args) override;

    void Process_vkCmdBuildAccelerationStructuresKHR(const ApiCallInfo&                       call_info,
                                                     args::CmdBuildAccelerationStructuresKHR& args) override;

    void Process_vkCmdCopyAccelerationStructureKHR(const ApiCallInfo&                     call_info,
                                                   args::CmdCopyAccelerationStructureKHR& args) override;

    void Process_vkCmdWriteAccelerationStructuresPropertiesKHR(
        const ApiCallInfo& call_info, args::CmdWriteAccelerationStructuresPropertiesKHR& args) override;

    void Process_vkCreateGraphicsPipelines(const ApiCallInfo& call_info, args::CreateGraphicsPipelines& args) override;

    void Process_vkCreateComputePipelines(const ApiCallInfo& call_info, args::CreateComputePipelines& args) override;

    void Process_vkCreateRayTracingPipelinesKHR(const ApiCallInfo&                  call_info,
                                                args::CreateRayTracingPipelinesKHR& args) override;

    void Process_vkCreateRayTracingPipelinesNV(const ApiCallInfo&                 call_info,
                                               args::CreateRayTracingPipelinesNV& args) override;

    void Process_vkAllocateCommandBuffers(const ApiCallInfo& call_info, args::AllocateCommandBuffers& args) override;

    void Process_vkBeginCommandBuffer(const ApiCallInfo& call_info, args::BeginCommandBuffer& args) override;

    void Process_vkCmdBindPipeline(const ApiCallInfo& call_info, args::CmdBindPipeline& args) override;

    void Process_vkUpdateDescriptorSets(const ApiCallInfo& call_info, args::UpdateDescriptorSets& args) override;

    void Process_vkCmdBindDescriptorSets(const ApiCallInfo& call_info, args::CmdBindDescriptorSets& args) override;

    void Process_vkCmdCopyBuffer(const ApiCallInfo& call_info, args::CmdCopyBuffer& args) override;

    void Process_vkCmdCopyBuffer2(const ApiCallInfo& call_info, args::CmdCopyBuffer2& args) override;

    void Process_vkCmdCopyBuffer2KHR(const ApiCallInfo& call_info, args::CmdCopyBuffer2KHR& args) override;

    void Process_vkCmdExecuteCommands(const ApiCallInfo& call_info, args::CmdExecuteCommands& args) override;

    void Process_vkFreeCommandBuffers(const ApiCallInfo& call_info, args::FreeCommandBuffers& args) override;

    void Process_vkCmdUpdateBuffer(const ApiCallInfo& call_info, args::CmdUpdateBuffer& args) override;

    void Process_vkCmdPushConstants(const ApiCallInfo& call_info, args::CmdPushConstants& args) override;

    void Process_vkCmdPushConstants2(const ApiCallInfo& call_info, args::CmdPushConstants2& args) override;

    void Process_vkCmdPushConstants2KHR(const ApiCallInfo& call_info, args::CmdPushConstants2KHR& args) override;

    void Process_vkQueueSubmit(const ApiCallInfo& call_info, args::QueueSubmit& args) override;

    void Process_vkQueueSubmit2(const ApiCallInfo& call_info, args::QueueSubmit2& args) override;

    void Process_vkQueueSubmit2KHR(const ApiCallInfo& call_info, args::QueueSubmit2KHR& args) override;

    void Process_vkCmdUpdateBuffer2ARM(const ApiCallInfo& call_info, args::CmdUpdateBuffer2ARM& args) override;

    void Process_vkFlushMappedMemoryRanges(const ApiCallInfo& call_info, args::FlushMappedMemoryRanges& args) override;

    void Process_vkDestroyPipeline(const ApiCallInfo& call_info, args::DestroyPipeline& args) override;

  private:
    void ProcessCmdPushConstants(format::HandleId         commandBuffer,
                                 format::HandleId         layout,
                                 VkShaderStageFlags       stageFlags,
                                 uint32_t                 offset,
                                 uint32_t                 size,
                                 PointerDecoder<uint8_t>* pValues);

    void ProcessBuildAccelerationStructures(
        uint32_t                                                                   infoCount,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   ppBuildRangeInfos);

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
        VkDeviceAddress     device_address{ 0 };
        format::HandleId    memory_handle_id{ 0 };
        VkDeviceSize        memory_offset{ 0 };
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
        VkAccelerationStructureBuildGeometryInfoKHR                                      info;
        std::vector<VkAccelerationStructureGeometryKHR>                                  geometries;
        std::unordered_map<uint64_t, VkAccelerationStructureTrianglesOpacityMicromapEXT> omm_infos;
        std::unordered_map<uint64_t, format::HandleId>                                   geometry_omm_id_map;
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
        format::HandleId handle{ format::kNullHandleId };
        uint64_t         creation_index{ 0 };
        uint64_t         destruction_index{ std::numeric_limits<uint64_t>::max() };
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

    // -----acceleration structure device address-----set of unique handle ids
    std::unordered_map<uint64_t, std::unordered_set<format::HandleId>> acceleration_structure_device_addresses_;

    std::unordered_map<format::HandleId, PipelineInfo> pipelines_;

    // -----pipeline handle-----group index-----SGH location info
    std::unordered_map<uint64_t, std::vector<format::ShaderHandleLocationInfo>> shader_group_handle_entries_;

    // All command buffer entries
    std::unordered_map<format::HandleId, CommandBufferInfo> command_buffer_entries_;
    // Command buffers in which binding of compute pipeline ocurred
    std::unordered_set<format::HandleId> command_buffers_with_compute_;

    // Fill memory indices, we reset them per vkQueueSubmit so we can tie them later
    std::vector<uint64_t> fill_memory_indices_per_submit_;
    // Filled during first pass
    std::list<uint64_t> fill_memory_indices_to_inspect_;

    std::set<std::pair<VkDeviceAddress, VkDeviceAddress>> instance_buffer_ranges_;
    std::set<std::pair<VkDeviceAddress, VkDeviceAddress>> transfer_ranges_;

    // -----init buffer handle-----InitBufferObject
    std::unordered_map<format::HandleId, InitBufferInfo> init_buffer_entries_;

    VulkanOptimizationOptions options_;

    bool skip_address_replacement{ false };

    bool HeuristicCheck(format::HandleId command_buffer);

    void EncodeVkGetAccelerationStructureBuildSizesKHR(format::HandleId device, AccelerationStructureBuildInfo& info);
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_MODIFIER_H
