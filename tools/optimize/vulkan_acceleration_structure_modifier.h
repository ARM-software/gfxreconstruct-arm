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
#ifndef GFXRECON_TOOLS_OPTIMIZE_VULKAN_ACCELERATION_STRUCTURE_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_VULKAN_ACCELERATION_STRUCTURE_MODIFIER_H

#include <cstdint>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

#include "decode/api_decoder.h"
#include "encode/parameter_buffer.h"
#include "format/format.h"
#include "util/defines.h"
#include "util/vulkan_modifier_base.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

// Reconstructs AS build-size queries and compaction dependencies for replay.
class VulkanAccelerationStructureModifier : public util::VulkanModifierBase
{
  public:
    bool CanOptimize() override { return true; }
    void
    ProcessAccelerationStructureCompactionDependencyCommand(format::HandleId                     parent,
                                                            const std::vector<format::HandleId>& children) override;

    void
    ProcessSetOpaqueAddressCommand(format::HandleId device_id, format::HandleId object_id, uint64_t address) override;

    void ProcessVulkanBuildAccelerationStructuresCommand(
        format::HandleId                                                           device_id,
        uint32_t                                                                   info_count,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* geometry_infos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   range_infos,
        std::vector<std::vector<VkAccelerationStructureInstanceKHR>>&              instance_buffers_data) override;

    void ProcessVulkanCopyAccelerationStructuresCommand(
        format::HandleId                                                  device_id,
        StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* copy_infos) override;

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

    void Process_vkBindBufferMemory(const ApiCallInfo& call_info, args::BindBufferMemory& args) override;

    void Process_vkBindBufferMemory2(const ApiCallInfo& call_info, args::BindBufferMemory2& args) override;

    void Process_vkBindBufferMemory2KHR(const ApiCallInfo& call_info, args::BindBufferMemory2KHR& args) override;

    void Process_vkCreateAccelerationStructureKHR(const ApiCallInfo&                    call_info,
                                                  args::CreateAccelerationStructureKHR& args) override;

    void Process_vkCreateAccelerationStructure2KHR(const ApiCallInfo&                     call_info,
                                                   args::CreateAccelerationStructure2KHR& args) override;

    void Process_vkGetAccelerationStructureBuildSizesKHR(const ApiCallInfo&                           call_info,
                                                         args::GetAccelerationStructureBuildSizesKHR& args) override;

    void Process_vkCmdBuildAccelerationStructuresKHR(const ApiCallInfo&                       call_info,
                                                     args::CmdBuildAccelerationStructuresKHR& args) override;

    void Process_vkCmdCopyAccelerationStructureKHR(const ApiCallInfo&                     call_info,
                                                   args::CmdCopyAccelerationStructureKHR& args) override;

  private:
    template <typename CreateArgs>
    void ProcessCreateAccelerationStructure(const ApiCallInfo& call_info, CreateArgs& args);
    void ProcessBuildAccelerationStructures(
        uint32_t                                                                   infoCount,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   ppBuildRangeInfos);
    void TrackBufferDeviceAddress(VkDeviceAddress address, format::HandleId buffer_id);

    struct BufferInfo
    {
        uint64_t         size;
        uint64_t         creation_index;
        uint64_t         destruction_index;
        VkDeviceAddress  device_address{ 0 };
        format::HandleId memory_handle_id{ 0 };
        VkDeviceSize     memory_offset{ 0 };
    };

    struct AccelerationStructureInfo
    {
        format::HandleId               buf_handle;
        VkAccelerationStructureTypeKHR type;
        VkDeviceAddress                device_address;
    };

    struct AccelerationStructureBuildInfo
    {
        VkAccelerationStructureBuildGeometryInfoKHR                                      info{};
        std::vector<VkAccelerationStructureGeometryKHR>                                  geometries;
        std::unordered_map<uint64_t, VkAccelerationStructureTrianglesOpacityMicromapEXT> omm_infos_ext;
        std::unordered_map<uint64_t, VkAccelerationStructureTrianglesOpacityMicromapKHR> omm_infos_khr;
        std::unordered_map<uint64_t, VkAccelerationStructureGeometryMicromapDataKHR>     micromap_infos_khr;
        std::unordered_map<uint64_t, format::HandleId>                                   geometry_omm_id_map;
        std::unordered_map<uint64_t, std::vector<VkMicromapUsageEXT>>                    usage_infos_ext;
        std::unordered_map<uint64_t, std::vector<VkMicromapUsageKHR>>                    usage_infos_khr;
        std::vector<uint32_t>                                                            primitive_counts;
        bool                                                                             is_first_built{ false };
        bool                                                                             is_meta_copy{ false };
        uint64_t                process_compacted_as_index{ 0 };
        encode::ParameterBuffer create_parameter_buffer;
        format::ApiCallId       create_call_id{ format::ApiCallId::ApiCall_vkCreateAccelerationStructureKHR };
        encode::ParameterBuffer get_address_parameter_buffer;
        format::HandleId        source_of_compaction{ format::kNullHandleId };
    };

    void EncodeVkGetAccelerationStructureBuildSizesKHR(format::HandleId device, AccelerationStructureBuildInfo& info);
    std::unordered_map<format::HandleId, BufferInfo>                     buffer_entries_;
    std::unordered_map<format::HandleId, AccelerationStructureInfo>      acceleration_structure_entries_;
    std::unordered_map<format::HandleId, AccelerationStructureBuildInfo> acceleration_structure_build_infos_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
#endif
