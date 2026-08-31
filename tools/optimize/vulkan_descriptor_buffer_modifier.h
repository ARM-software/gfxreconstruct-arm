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
#include "decode/vulkan_optimize_options.h"

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

    bool CanOptimize() override;

    void ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override;

    void ProcessFixDescriptorDataCommand(const format::FixDescriptorDataCommandHeader&          header,
                                         const std::vector<format::DescriptorDataLocationInfo>& infos) override;

    void
    ProcessFixShadowMemoryCommand(format::HandleId memory_id, uint64_t map_memory, uint64_t shadow_memory) override;

    void Process_vkCreateBuffer(const ApiCallInfo& call_info, args::CreateBuffer& args) override;

    void Process_vkDestroyBuffer(const ApiCallInfo& call_info, args::DestroyBuffer& args) override;

    void Process_vkAllocateMemory(const ApiCallInfo& call_info, args::AllocateMemory& args) override;

    void Process_vkFreeMemory(const ApiCallInfo& call_info, args::FreeMemory& args) override;

    void Process_vkMapMemory(const ApiCallInfo& call_info, args::MapMemory& args) override;

    void Process_vkMapMemory2(const ApiCallInfo& call_info, args::MapMemory2& args) override;

    void Process_vkUnmapMemory(const ApiCallInfo& call_info, args::UnmapMemory& args) override;

    void Process_vkUnmapMemory2(const ApiCallInfo& call_info, args::UnmapMemory2& args) override;

    void Process_vkBindBufferMemory(const ApiCallInfo& call_info, args::BindBufferMemory& args) override;

    void Process_vkBindBufferMemory2(const ApiCallInfo& call_info, args::BindBufferMemory2& args) override;

    void Process_vkAllocateCommandBuffers(const ApiCallInfo& call_info, args::AllocateCommandBuffers& args) override;

    void Process_vkBeginCommandBuffer(const ApiCallInfo& call_info, args::BeginCommandBuffer& args) override;

    void Process_vkCmdCopyBuffer(const ApiCallInfo& call_info, args::CmdCopyBuffer& args) override;

    void Process_vkCmdCopyBuffer2(const ApiCallInfo& call_info, args::CmdCopyBuffer2& args) override;

    void Process_vkCmdCopyBuffer2KHR(const ApiCallInfo& call_info, args::CmdCopyBuffer2KHR& args) override;

    void Process_vkFreeCommandBuffers(const ApiCallInfo& call_info, args::FreeCommandBuffers& args) override;

    void Process_vkCmdUpdateBuffer(const ApiCallInfo& call_info, args::CmdUpdateBuffer& args) override;

    void Process_vkCmdPushConstants(const ApiCallInfo& call_info, args::CmdPushConstants& args) override;

    void Process_vkGetDescriptorEXT(const ApiCallInfo& call_info, args::GetDescriptorEXT& args) override;

  private:
    std::vector<format::DescriptorDataLocationInfo>
    GetDescriptorsInFillMemory(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data);

    void                        RecordDescriptor(uint64_t descriptor_addr, size_t data_size, const uint8_t* data);
    const std::vector<uint8_t>& GetDescriptorPayload(const format::DescriptorDataLocationInfo& location) const;

    void FindCopiedDescriptors(uint64_t                                         memory_id,
                               uint64_t                                         offset,
                               uint64_t                                         size,
                               const uint8_t*                                   data,
                               std::vector<format::DescriptorDataLocationInfo>* locations);

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

    // -----address of memory saved descriptor ----- descriptor location history
    using DescriptorHistory     = std::vector<format::DescriptorDataLocationInfo>;
    using DescriptorLocationMap = std::unordered_map<uint64_t, DescriptorHistory>;

    // -----memory handle id ----- address of memory saved descriptor ----- descriptor location history
    std::unordered_map<format::HandleId, DescriptorLocationMap> device_memory_descriptor_locations;

    struct DescriptorVersion
    {
        uint64_t descriptor_addr;
        uint64_t version;
    };

    VulkanOptimizationOptions options_;
    // vkGetDescriptorEXT payload generations recorded during the first pass. Version numbers are one-based.
    std::unordered_map<uint64_t, std::vector<std::vector<uint8_t>>> descriptor_histories_;
    std::unordered_map<uint64_t, uint64_t>                          descriptor_versions_seen_;
    std::unordered_map<uint64_t, std::vector<DescriptorVersion>>    descriptor_versions_by_prefix_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_VULKAN_DESCRIPTOR_BUFFER_MODIFIER_H