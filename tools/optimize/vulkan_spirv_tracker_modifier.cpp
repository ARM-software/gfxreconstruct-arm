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
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdlib>
#include <iostream>

#include "vulkan_spirv_tracker_modifier.h"
#include "format/format.h"
#include "util/defines.h"
#include "util/memory_output_stream.h"
#include "util/json_util.h"
#include "encode/parameter_buffer.h"
#include "encode/struct_pointer_encoder.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

namespace
{

constexpr char kRewritePlanJsonEnvVar[] = "GFXRECON_VULKAN_SPIRV_TRACKER_REWRITE_PLAN_JSON";

void InitializeRewritePlanJsonOptions(const std::string& output_path, bool verbose)
{
    const std::filesystem::path json_path(output_path);

    util::JsonOptions::root_dir         = json_path.parent_path().string();
    util::JsonOptions::data_sub_dir     = "";
    util::JsonOptions::format           = util::JsonFormat::JSON;
    util::JsonOptions::dump_binaries    = false;
    util::JsonOptions::expand_flags     = false;
    util::JsonOptions::hex_handles      = false;
    util::JsonOptions::verbose          = verbose;
    util::JsonOptions::checksum         = false;
    util::JsonOptions::checksum_trigger = 0;
}

const char* UnsupportedDispatchBufferAddressUseSiteReasonToString(UnsupportedDispatchBufferAddressUseSiteReason reason)
{
    switch (reason)
    {
        case UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitComponentCount:
            return "UnsupportedBitComponentCount";
        case UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitLocation:
            return "UnsupportedBitLocation";
        case UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitAlignment:
            return "UnsupportedBitAlignment";
        case UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitWidth:
            return "UnsupportedBitWidth";
        case UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedStorageClass:
            return "UnsupportedStorageClass";
        case UnsupportedDispatchBufferAddressUseSiteReason::MissingBlockInfo:
            return "MissingBlockInfo";
        case UnsupportedDispatchBufferAddressUseSiteReason::BlockRangeOutOfBounds:
            return "BlockRangeOutOfBounds";
        case UnsupportedDispatchBufferAddressUseSiteReason::MissingPushConstantBlockInfo:
            return "MissingPushConstantBlockInfo";
        case UnsupportedDispatchBufferAddressUseSiteReason::PushConstantRangeOutOfBounds:
            return "PushConstantRangeOutOfBounds";
        default:
            return "Unknown";
    }
}

const char* RejectedDeviceAddressUseSiteReasonToString(RejectedDeviceAddressUseSiteReason reason)
{
    switch (reason)
    {
        case RejectedDeviceAddressUseSiteReason::ZeroValue:
            return "ZeroValue";
        case RejectedDeviceAddressUseSiteReason::UnknownDeviceAddress:
            return "UnknownDeviceAddress";
        case RejectedDeviceAddressUseSiteReason::NotLiveAtUseTime:
            return "NotLiveAtUseTime";
        default:
            return "Unknown";
    }
}

const char* UnresolvedDeviceAddressUseSiteReasonToString(UnresolvedDeviceAddressUseSiteReason reason)
{
    switch (reason)
    {
        case UnresolvedDeviceAddressUseSiteReason::MissingBufferProvenance:
            return "MissingBufferProvenance";
        case UnresolvedDeviceAddressUseSiteReason::MissingPushConstantProvenance:
            return "MissingPushConstantProvenance";
        case UnresolvedDeviceAddressUseSiteReason::UncoveredRange:
            return "UncoveredRange";
        case UnresolvedDeviceAddressUseSiteReason::CrossSpanBoundary:
            return "CrossSpanBoundary";
        case UnresolvedDeviceAddressUseSiteReason::UnsupportedRootType:
            return "UnsupportedRootType";
        default:
            return "Unknown";
    }
}

} // namespace

VulkanSpirvTrackModifier::VulkanSpirvTrackModifier(const VulkanSpirvTrackerOptions& options) :
    options_(options), m_verbose(options.verbose)
{
    // analysis outputs are pass-local state: start empty for each modifier run
    verified_build_as_use_sites_.clear();
    rejected_build_as_use_sites_.clear();
    resolved_build_as_use_sites_.clear();
    unresolved_build_as_use_sites_.clear();
    unsupported_dispatch_buffer_address_use_sites_.clear();
    verified_dispatch_buffer_address_use_sites_.clear();
    rejected_dispatch_buffer_address_use_sites_.clear();
    resolved_dispatch_buffer_address_use_sites_.clear();
    unresolved_dispatch_buffer_address_use_sites_.clear();
    provenance_tracker_.Reset();

    if (options.error_on_buffers_incomplete)
    {
        m_flags |= ERROR_RAISE_ON_BUFFERS_INCOMPLETE;
    }
}

bool VulkanSpirvTrackModifier::CanOptimize()
{
    return fixup_location_builder_.HasFixups();
}

void VulkanSpirvTrackModifier::FinalizeAnalysis()
{
    // Guard against running finalize more than once for the same modifier instance.
    if (analysis_finalized_)
    {
        return;
    }

    // TODO: Optionally write the analysis results to JSON.

    // Build shared pure-data fixup inputs from all resolved device-address use-sites.
    const auto all_fixup_inputs = BuildFixupInputs();

    // Hand all_fixup_inputs to fixup_location_builder_ so it can build root-source-local fixup locations.
    fixup_location_builder_.Build(all_fixup_inputs);

    if (options_.fixup_json_path.has_value())
    {
        InitializeRewritePlanJsonOptions(options_.fixup_json_path->string(), m_verbose);
        fixup_location_builder_.WriteRewritePlanJson(options_.fixup_json_path->string());
    }
    else if (const char* rewrite_plan_json_path = std::getenv(kRewritePlanJsonEnvVar);
             (rewrite_plan_json_path != nullptr) && (rewrite_plan_json_path[0] != '\0'))
    {
        InitializeRewritePlanJsonOptions(rewrite_plan_json_path, m_verbose);
        fixup_location_builder_.WriteRewritePlanJson(rewrite_plan_json_path);
    }

    // Leave the modifier ready for modification-pass metadata emission.
    analysis_finalized_ = true;
}

void VulkanSpirvTrackModifier::PrepareForModificationPass()
{
    // Reuse the same FillMemory serial numbering scheme in both passes so the
    // modification pass can query fixups with the same source ids built during analysis.
    next_fill_memory_serial_id_ = 0;
}

void VulkanSpirvTrackModifier::WriteFixDeviceAddressCmd(
    format::HandleId relation_id, const std::vector<format::AddressLocationInfo>& address_locations)
{
    if (address_locations.empty())
    {
        return;
    }

    auto new_call       = CreatePreCall();
    new_call->type      = NewCallDataType::MetaDataCall;
    new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
    new_call->thread_id = 1;

    format::FixDeviceAddressCommandHeader fix_cmd = {};

    fix_cmd.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
    fix_cmd.meta_header.block_header.size =
        format::GetMetaDataBlockBaseSize(fix_cmd) + (address_locations.size() * sizeof(format::AddressLocationInfo));
    fix_cmd.meta_header.meta_data_id =
        format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kFixDeviceAddressCommand);
    fix_cmd.relation_id      = relation_id;
    fix_cmd.num_of_locations = address_locations.size();

    new_call->parameter_buffer.Write(&fix_cmd, sizeof(format::FixDeviceAddressCommandHeader));
    new_call->parameter_buffer.Write(address_locations.data(),
                                     address_locations.size() * sizeof(format::AddressLocationInfo));
}

std::vector<DeviceAddressFixupInput> VulkanSpirvTrackModifier::BuildFixupInputs() const
{
    std::vector<DeviceAddressFixupInput> inputs;
    inputs.reserve(resolved_build_as_use_sites_.size() + resolved_dispatch_buffer_address_use_sites_.size());

    for (const auto& resolved : resolved_build_as_use_sites_)
    {
        const auto& verified = resolved.verified_use_site;
        inputs.push_back({ verified.referenced_as,
                           verified.referenced_as_size,
                           verified.referenced_as_base_address,
                           verified.value,
                           resolved.resolved_root });
    }

    for (const auto& resolved : resolved_dispatch_buffer_address_use_sites_)
    {
        const auto& verified = resolved.verified_use_site;
        inputs.push_back({ verified.referenced_buffer,
                           verified.referenced_buffer_size,
                           verified.referenced_buffer_base_address,
                           verified.value,
                           resolved.resolved_root });
    }

    return inputs;
}

std::vector<VulkanSpirvTrackModifier::FillMemoryBufferOverlap>
VulkanSpirvTrackModifier::CollectFillMemoryBufferOverlaps(uint64_t memory_id,
                                                          uint64_t fill_offset,
                                                          uint64_t fill_size) const
{
    std::vector<FillMemoryBufferOverlap> overlaps;

    const auto memory_entry_iter = memory_binding_entries_.find(memory_id);
    if (memory_entry_iter == memory_binding_entries_.end())
    {
        return overlaps;
    }

    const DeviceMemoryInfo& mem_info = memory_entry_iter->second;

    // FillMemoryCommand offsets are relative to the mapped pointer, so convert
    // the write range into the underlying memory-object coordinate space first.
    const uint64_t fill_begin_mem = mem_info.mapping.offset + fill_offset;
    const uint64_t fill_end_mem   = fill_begin_mem + fill_size;

    for (const MemoryBindingRecord& binding : mem_info.memory_binding_records_)
    {
        if (!binding.isBuffer)
        {
            continue;
        }

        const auto buffer_iter = buffer_entries_.find(binding.handle);
        if (buffer_iter == buffer_entries_.end())
        {
            continue;
        }

        const BufferInfo& buffer_info    = buffer_iter->second;
        const uint64_t    bind_begin_mem = binding.offset;
        const uint64_t    bind_end_mem   = bind_begin_mem + buffer_info.size;
        const uint64_t    overlap_begin  = std::max(fill_begin_mem, bind_begin_mem);
        const uint64_t    overlap_end    = std::min(fill_end_mem, bind_end_mem);

        // non overlap
        if (overlap_begin >= overlap_end)
        {
            continue;
        }

        // Re-express the overlap in the two coordinates the later steps need:
        // buffer-relative offset for the destination bytes, and fill-relative
        // offset for the source payload bytes.
        overlaps.push_back(FillMemoryBufferOverlap{ binding.handle,
                                                    static_cast<VkDeviceSize>(overlap_begin - bind_begin_mem),
                                                    overlap_begin - fill_begin_mem,
                                                    overlap_end - overlap_begin });
    }

    return overlaps;
}

void VulkanSpirvTrackModifier::ProcessFillMemoryCommand(uint64_t       memory_id,
                                                        uint64_t       offset,
                                                        uint64_t       size,
                                                        const uint8_t* data)
{
    // Pass-local stable identity for this traced FillMemoryCommand.
    const uint64_t fill_serial_id = next_fill_memory_serial_id_++;

    if (IsModificationPass())
    {
        // Rebuild the same FillMemory serial numbering in the modification pass
        // so this command can query the fixups finalized during analysis.
        const FixupLocations* fixups =
            fixup_location_builder_.GetFixupLocations(ProvenanceRootType::FillMemory, fill_serial_id);
        if ((fixups != nullptr) && !fixups->address_locations.empty())
        {
            WriteFixDeviceAddressCmd(memory_id, fixups->address_locations);
        }
        return;
    }

    const auto overlaps = CollectFillMemoryBufferOverlaps(memory_id, offset, size);

    for (const auto& overlap : overlaps)
    {
        // First update the canonical shadow bytes for the affected buffer slice.
        auto& buffer = buffer_entries_.at(overlap.buffer);
        std::memcpy(buffer.data.data() + overlap.offset_in_buffer, data + overlap.offset_in_data, overlap.size);

        // Flatten the same write into provenance state so the current bytes and their current origin stay in sync.
        RootWriteToBufferRange write = { overlap.buffer,
                                         overlap.offset_in_buffer,
                                         overlap.size,
                                         ProvenanceRootType::FillMemory,
                                         ProvenanceRootKey{ fill_serial_id, overlap.offset_in_data } };
        provenance_tracker_.FlattenBufferProvenanceForRootWrite(write);
    }
}

void VulkanSpirvTrackModifier::Process_vkCreateBuffer(const ApiCallInfo& call_info, args::CreateBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle                   = *args.pBuffer.GetPointer();
    buffer_entries_[handle].handle            = handle;
    buffer_entries_[handle].size              = args.pCreateInfo.GetPointer()->size;
    buffer_entries_[handle].usage             = args.pCreateInfo.GetPointer()->usage;
    buffer_entries_[handle].flags             = args.pCreateInfo.GetPointer()->flags;
    buffer_entries_[handle].creation_index    = call_info.index;
    buffer_entries_[handle].destruction_index = UINT64_MAX;
    buffer_entries_[handle].data.resize(buffer_entries_[handle].size);
}

void VulkanSpirvTrackModifier::Process_vkDestroyBuffer(const ApiCallInfo& call_info, args::DestroyBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }
    if (buffer_entries_.find(args.buffer) != buffer_entries_.end())
    {
        buffer_entries_[args.buffer].destruction_index = call_info.index;
    }
}

void VulkanSpirvTrackModifier::Process_vkAllocateMemory(const ApiCallInfo& call_info, args::AllocateMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle                           = *args.pMemory.GetPointer();
    memory_binding_entries_[handle].handle            = handle;
    memory_binding_entries_[handle].size              = args.pAllocateInfo.GetPointer()->allocationSize;
    memory_binding_entries_[handle].type_index        = args.pAllocateInfo.GetPointer()->memoryTypeIndex;
    memory_binding_entries_[handle].creation_index    = call_info.index;
    memory_binding_entries_[handle].destruction_index = UINT64_MAX;

    const auto* meta_flags_info =
        GetPNextMetaStruct<Decoded_VkMemoryAllocateFlagsInfo>(args.pAllocateInfo.GetMetaStructPointer()->pNext);
    if (meta_flags_info)
    {
        memory_binding_entries_[handle].flags = meta_flags_info->decoded_value->flags;
    }
}

void VulkanSpirvTrackModifier::Process_vkFreeMemory(const ApiCallInfo& call_info, args::FreeMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[args.memory].destruction_index = call_info.index;
    }
}

void VulkanSpirvTrackModifier::Process_vkBindBufferMemory(const ApiCallInfo& call_info, args::BindBufferMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[args.memory].memory_binding_records_.push_back(
            { args.buffer, true, args.memoryOffset });
    }
}

void VulkanSpirvTrackModifier::Process_vkBindBufferMemory2(const ApiCallInfo& call_info, args::BindBufferMemory2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto* bind_infos      = args.pBindInfos.GetPointer();
    const auto* meta_bind_infos = args.pBindInfos.GetMetaStructPointer();

    for (uint32_t i = 0; i < args.bindInfoCount; ++i)
    {
        if (memory_binding_entries_.find(meta_bind_infos[i].memory) != memory_binding_entries_.end())
        {
            memory_binding_entries_[meta_bind_infos[i].memory].memory_binding_records_.push_back(
                { meta_bind_infos[i].buffer, true, bind_infos[i].memoryOffset });
        }
    }
}

void VulkanSpirvTrackModifier::Process_vkMapMemory(const ApiCallInfo& call_info, args::MapMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        if (args.size == VK_WHOLE_SIZE)
        {
            GFXRECON_ASSERT(args.offset <= memory_binding_entries_[args.memory].size);
            args.size = memory_binding_entries_[args.memory].size - args.offset;
        }

        memory_binding_entries_[args.memory].mapping.offset        = args.offset;
        memory_binding_entries_[args.memory].mapping.size          = args.size;
        memory_binding_entries_[args.memory].mapping.map_memory    = *args.ppData.GetPointer();
        memory_binding_entries_[args.memory].mapping.shadow_memory = 0;
    }
}

void VulkanSpirvTrackModifier::Process_vkMapMemory2(const ApiCallInfo& call_info, args::MapMemory2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto* map_info      = args.pMemoryMapInfo.GetPointer();
    const auto* meta_map_info = args.pMemoryMapInfo.GetMetaStructPointer();

    format::HandleId memory = meta_map_info->memory;
    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        VkDeviceSize in_size = map_info->size;
        if (map_info->size == VK_WHOLE_SIZE)
        {
            GFXRECON_ASSERT(map_info->offset <= memory_binding_entries_[memory].size);
            in_size = memory_binding_entries_[memory].size - map_info->offset;
        }

        memory_binding_entries_[memory].mapping.offset        = map_info->offset;
        memory_binding_entries_[memory].mapping.size          = in_size;
        memory_binding_entries_[memory].mapping.map_memory    = *args.ppData.GetPointer();
        memory_binding_entries_[memory].mapping.shadow_memory = 0;
    }
}

void VulkanSpirvTrackModifier::Process_vkUnmapMemory(const ApiCallInfo& call_info, args::UnmapMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (memory_binding_entries_.find(args.memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[args.memory].mapping.offset        = 0;
        memory_binding_entries_[args.memory].mapping.size          = 0;
        memory_binding_entries_[args.memory].mapping.map_memory    = 0;
        memory_binding_entries_[args.memory].mapping.shadow_memory = 0;
    }
}

void VulkanSpirvTrackModifier::Process_vkUnmapMemory2(const ApiCallInfo& call_info, args::UnmapMemory2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto* meta_unmap_info = args.pMemoryUnmapInfo.GetMetaStructPointer();

    format::HandleId memory = meta_unmap_info->memory;
    if (memory_binding_entries_.find(memory) != memory_binding_entries_.end())
    {
        memory_binding_entries_[memory].mapping.offset        = 0;
        memory_binding_entries_[memory].mapping.size          = 0;
        memory_binding_entries_[memory].mapping.map_memory    = 0;
        memory_binding_entries_[memory].mapping.shadow_memory = 0;
    }
}

void VulkanSpirvTrackModifier::Process_vkCreateAccelerationStructureKHR(const ApiCallInfo&                    call_info,
                                                                        args::CreateAccelerationStructureKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle = *args.pAccelerationStructure.GetPointer();

    acceleration_structure_entries_[handle].handle            = handle;
    acceleration_structure_entries_[handle].buf_handle        = args.pCreateInfo.GetMetaStructPointer()->buffer;
    acceleration_structure_entries_[handle].size              = args.pCreateInfo.GetPointer()->size;
    acceleration_structure_entries_[handle].offset            = args.pCreateInfo.GetPointer()->offset;
    acceleration_structure_entries_[handle].type              = args.pCreateInfo.GetPointer()->type;
    acceleration_structure_entries_[handle].creation_index    = call_info.index;
    acceleration_structure_entries_[handle].destruction_index = UINT64_MAX;

    VkDeviceAddress back_buffer_address =
        buffer_entries_[args.pCreateInfo.GetMetaStructPointer()->buffer].device_address;
    if (back_buffer_address != 0)
    {
        VkDeviceAddress address = back_buffer_address + args.pCreateInfo.GetPointer()->offset;
        acceleration_structure_entries_[handle].device_address = address;
        acceleration_structure_device_addresses_[address].insert(handle);
    }
}

void VulkanSpirvTrackModifier::Process_vkDestroyAccelerationStructureKHR(const ApiCallInfo& call_info,
                                                                         args::DestroyAccelerationStructureKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }
    if (acceleration_structure_entries_.find(args.accelerationStructure) != acceleration_structure_entries_.end())
    {
        acceleration_structure_entries_[args.accelerationStructure].destruction_index = call_info.index;
    }
}

void VulkanSpirvTrackModifier::Process_vkCreateShaderModule(const ApiCallInfo&        call_info,
                                                            args::CreateShaderModule& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId                handleId = *args.pShaderModule.GetPointer();
    const VkShaderModuleCreateInfo* info     = args.pCreateInfo.GetPointer();

    shader_module_entries_[handleId].handle            = handleId;
    shader_module_entries_[handleId].creation_index    = call_info.index;
    shader_module_entries_[handleId].destruction_index = UINT64_MAX;
    shader_module_entries_[handleId].codeSize          = info->codeSize;
    shader_module_entries_[handleId].pCode             = { info->pCode, info->pCode + info->codeSize / 4 };
}

void VulkanSpirvTrackModifier::Process_vkCreateDescriptorSetLayout(const ApiCallInfo&               call_info,
                                                                   args::CreateDescriptorSetLayout& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId                       handle = *args.pSetLayout.GetPointer();
    const VkDescriptorSetLayoutCreateInfo* info   = args.pCreateInfo.GetPointer();

    const auto* meta_flag_info = GetPNextMetaStruct<Decoded_VkDescriptorSetLayoutBindingFlagsCreateInfo>(
        args.pCreateInfo.GetMetaStructPointer()->pNext);
    const auto* meta_mutable_type_info = GetPNextMetaStruct<Decoded_VkMutableDescriptorTypeCreateInfoEXT>(
        args.pCreateInfo.GetMetaStructPointer()->pNext);

    set_layout_entries_[handle].handle            = handle;
    set_layout_entries_[handle].creation_index    = call_info.index;
    set_layout_entries_[handle].destruction_index = UINT64_MAX;
    set_layout_entries_[handle].flags             = info->flags;

    for (uint32_t i = 0; i < info->bindingCount; i++)
    {
        Binding binding;
        binding.binding         = info->pBindings[i].binding;
        binding.type            = info->pBindings[i].descriptorType;
        binding.stageFlags      = info->pBindings[i].stageFlags;
        binding.descriptorCount = info->pBindings[i].descriptorCount;
        binding.binding_flags   = (meta_flag_info == nullptr) ? 0 : meta_flag_info->pBindingFlags.GetPointer()[i];

        if ((binding.type == VK_DESCRIPTOR_TYPE_MUTABLE_EXT) && (meta_mutable_type_info != nullptr) &&
            (meta_mutable_type_info->decoded_value != nullptr) &&
            (meta_mutable_type_info->pMutableDescriptorTypeLists != nullptr) &&
            (i < meta_mutable_type_info->decoded_value->mutableDescriptorTypeListCount))
        {
            const auto& mutable_type_list = meta_mutable_type_info->pMutableDescriptorTypeLists->GetPointer()[i];
            const auto& meta_mutable_type_list =
                meta_mutable_type_info->pMutableDescriptorTypeLists->GetMetaStructPointer()[i];

            for (uint32_t type_index = 0; type_index < mutable_type_list.descriptorTypeCount; ++type_index)
            {
                binding.mutable_descriptor_types.push_back(
                    meta_mutable_type_list.pDescriptorTypes.GetPointer()[type_index]);
            }
        }

        set_layout_entries_[handle].bindings[binding.binding] = binding;
    }
}

void VulkanSpirvTrackModifier::Process_vkDestroyDescriptorSetLayout(const ApiCallInfo&                call_info,
                                                                    args::DestroyDescriptorSetLayout& args)
{
    if (IsModificationPass())
    {
        return;
    }
    if (set_layout_entries_.find(args.descriptorSetLayout) != set_layout_entries_.end())
    {
        set_layout_entries_[args.descriptorSetLayout].destruction_index = call_info.index;
    }
}

void VulkanSpirvTrackModifier::Process_vkGetDescriptorSetLayoutSizeEXT(const ApiCallInfo&                   call_info,
                                                                       args::GetDescriptorSetLayoutSizeEXT& args)
{
    if (set_layout_entries_.find(args.layout) != set_layout_entries_.end())
    {
        set_layout_entries_[args.layout].size = *args.pLayoutSizeInBytes.GetPointer();
    }
}

void VulkanSpirvTrackModifier::Process_vkGetDescriptorSetLayoutBindingOffsetEXT(
    const ApiCallInfo& call_info, args::GetDescriptorSetLayoutBindingOffsetEXT& args)
{
    if (set_layout_entries_.find(args.layout) != set_layout_entries_.end())
    {
        set_layout_entries_[args.layout].bindings[args.binding].offset = *args.pOffset.GetPointer();
    }
}

void VulkanSpirvTrackModifier::Process_vkCreateDescriptorPool(const ApiCallInfo&          call_info,
                                                              args::CreateDescriptorPool& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle = *args.pDescriptorPool.GetPointer();

    descriptor_pool_entries_[handle].handle            = handle;
    descriptor_pool_entries_[handle].creation_index    = call_info.index;
    descriptor_pool_entries_[handle].destruction_index = UINT64_MAX;
}

void VulkanSpirvTrackModifier::Process_vkDestroyDescriptorPool(const ApiCallInfo&           call_info,
                                                               args::DestroyDescriptorPool& args)
{
    if (IsModificationPass())
    {
        return;
    }
    if (descriptor_pool_entries_.find(args.descriptorPool) != descriptor_pool_entries_.end())
    {
        descriptor_pool_entries_[args.descriptorPool].destruction_index = call_info.index;
    }
    for (auto id : descriptor_pool_entries_[args.descriptorPool].descriptorSets)
    {
        descriptor_set_entries_.at(id).destruction_index = call_info.index;
    }
}

void VulkanSpirvTrackModifier::Process_vkAllocateDescriptorSets(const ApiCallInfo&            call_info,
                                                                args::AllocateDescriptorSets& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (args.result != VK_SUCCESS)
    {
        return;
    }

    const Decoded_VkDescriptorSetAllocateInfo* alloc_info = args.pAllocateInfo.GetMetaStructPointer();

    const auto* meta_count_info =
        GetPNextMetaStruct<Decoded_VkDescriptorSetVariableDescriptorCountAllocateInfoEXT>(alloc_info->pNext);

    for (uint32_t i = 0; i < alloc_info->decoded_value->descriptorSetCount; i++)
    {
        format::HandleId setLayout      = alloc_info->pSetLayouts.GetPointer()[i];
        format::HandleId descriptorSet  = args.pDescriptorSets.GetPointer()[i];
        format::HandleId descriptorPool = alloc_info->descriptorPool;

        if (descriptorSet == 0)
        {
            GFXRECON_LOG_INFO(
                "call %llu vkAllocateDescriptorSets: descriptorSet is 0. Maybe something is error during tracing.",
                call_info.index);
            continue;
        }

        descriptor_pool_entries_.at(descriptorPool).descriptorSets.push_back(descriptorSet);

        descriptor_set_entries_[descriptorSet].handle            = descriptorSet;
        descriptor_set_entries_[descriptorSet].creation_index    = call_info.index;
        descriptor_set_entries_[descriptorSet].destruction_index = UINT64_MAX;

        // filling binding
        for (const auto& layout_binding_iter : set_layout_entries_[setLayout].bindings)
        {
            const Binding& binding = layout_binding_iter.second;
            GFXRECON_ASSERT(descriptor_set_entries_[descriptorSet].binding_descriptor_array.find(binding.binding) ==
                            descriptor_set_entries_[descriptorSet].binding_descriptor_array.end());

            descriptor_set_entries_[descriptorSet].binding_descriptor_array[binding.binding].binding = binding.binding;
            descriptor_set_entries_[descriptorSet].binding_descriptor_array[binding.binding].type    = binding.type;
            descriptor_set_entries_[descriptorSet].binding_descriptor_array[binding.binding].mutable_descriptor_types =
                binding.mutable_descriptor_types;
            descriptor_set_entries_[descriptorSet].binding_descriptor_array[binding.binding].descriptorCount =
                binding.descriptorCount;
            descriptor_set_entries_[descriptorSet].binding_descriptor_array[binding.binding].stageFlags =
                binding.stageFlags;
            descriptor_set_entries_[descriptorSet].binding_descriptor_array[binding.binding].binding_flags =
                binding.binding_flags;

            if ((meta_count_info != nullptr) &&
                (binding.binding_flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT) ==
                    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)
            {
                descriptor_set_entries_[descriptorSet].binding_descriptor_array[binding.binding].descriptorCount =
                    meta_count_info->pDescriptorCounts.GetPointer()[i];
            }
            if (binding.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
            {
                descriptor_set_entries_[descriptorSet]
                    .binding_descriptor_array[binding.binding]
                    .inline_uniform_block_data.resize(binding.descriptorCount, 0);
                descriptor_set_entries_[descriptorSet]
                    .binding_descriptor_array[binding.binding]
                    .inline_uniform_block_written.resize(binding.descriptorCount, 0);
            }
        }
    }
}

void VulkanSpirvTrackModifier::Process_vkUpdateDescriptorSets(const ApiCallInfo&          call_info,
                                                              args::UpdateDescriptorSets& args)
{
    if (IsModificationPass())
    {
        return;
    }

    auto binding_accepts_descriptor_type = [](const DescriptorArray& binding_array, VkDescriptorType descriptor_type) {
        if (binding_array.type == descriptor_type)
        {
            return true;
        }
        if (binding_array.type != VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
        {
            return false;
        }
        for (VkDescriptorType mutable_type : binding_array.mutable_descriptor_types)
        {
            if (mutable_type == descriptor_type)
            {
                return true;
            }
        }
        return false;
    };

    for (uint32_t i = 0; i < args.descriptorWriteCount; i++)
    {
        const auto& write      = args.pDescriptorWrites.GetPointer()[i];
        const auto& meta_write = args.pDescriptorWrites.GetMetaStructPointer()[i];

        auto descriptor_set_iter = descriptor_set_entries_.find(meta_write.dstSet);
        GFXRECON_ASSERT(descriptor_set_iter != descriptor_set_entries_.end());

        auto binding_array_iter = descriptor_set_iter->second.binding_descriptor_array.find(write.dstBinding);
        GFXRECON_ASSERT(binding_array_iter != descriptor_set_iter->second.binding_descriptor_array.end());

        GFXRECON_ASSERT(binding_accepts_descriptor_type(binding_array_iter->second, write.descriptorType));

        uint32_t alloc_descriptor_count = binding_array_iter->second.descriptorCount;
        if ((write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
             write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
             write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
             write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) &&
            write.pBufferInfo)
        {
            for (uint32_t s = 0; s < write.descriptorCount; s++)
            {
                uint32_t array_index = write.dstArrayElement + s;

                const auto& meta_buffer_info = meta_write.pBufferInfo->GetMetaStructPointer()[s];
                if (meta_buffer_info.buffer == format::kNullHandleId)
                {
                    GFXRECON_ASSERT(meta_buffer_info.decoded_value->range == VK_WHOLE_SIZE);
                    GFXRECON_ASSERT(meta_buffer_info.decoded_value->offset == 0);
                }
                binding_array_iter->second.descriptor_entries[array_index] = {
                    .type   = write.descriptorType,
                    .buffer = { meta_buffer_info.buffer,
                                meta_buffer_info.decoded_value->range,
                                meta_buffer_info.decoded_value->offset }
                };
            }
        }
        else if (write.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
            const auto* meta_inline_uniform_block =
                GetPNextMetaStruct<Decoded_VkWriteDescriptorSetInlineUniformBlock>(meta_write.pNext);
            if ((meta_inline_uniform_block == nullptr) || (meta_inline_uniform_block->decoded_value == nullptr))
            {
                GFXRECON_LOG_INFO("call %llu vkUpdateDescriptorSets: missing VkWriteDescriptorSetInlineUniformBlock in "
                                  "pNext for binding %u.",
                                  call_info.index,
                                  write.dstBinding);
                continue;
            }

            const auto* inline_uniform_block = meta_inline_uniform_block->decoded_value;
            GFXRECON_ASSERT(inline_uniform_block->sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK);

            uint32_t size        = write.descriptorCount;
            uint32_t offset      = write.dstArrayElement;
            auto&    inline_data = binding_array_iter->second.inline_uniform_block_data;
            GFXRECON_ASSERT(size == inline_uniform_block->dataSize);
            GFXRECON_ASSERT(size + offset <= inline_data.size());

            const uint8_t* src_data = meta_inline_uniform_block->pData.GetPointer();
            GFXRECON_ASSERT((size == 0) || (src_data != nullptr));

            util::platform::MemoryCopy(inline_data.data() + offset, size, src_data, size);
        }
        else if (write.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            const auto* meta_structure =
                GetPNextMetaStruct<Decoded_VkWriteDescriptorSetAccelerationStructureKHR>(meta_write.pNext);
            if ((meta_structure == nullptr) || (meta_structure->decoded_value == nullptr))
            {
                GFXRECON_LOG_INFO(
                    "call %llu vkUpdateDescriptorSets: missing VkWriteDescriptorSetAccelerationStructureKHR in "
                    "pNext for binding %u.",
                    call_info.index,
                    write.dstBinding);
                continue;
            }

            const auto* structure = meta_structure->decoded_value;
            GFXRECON_ASSERT(structure->sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
            GFXRECON_ASSERT(write.descriptorCount == structure->accelerationStructureCount);

            const auto* handles = meta_structure->pAccelerationStructures.GetPointer();
            for (uint32_t s = 0; s < write.descriptorCount; s++)
            {
                uint32_t array_index = write.dstArrayElement + s;
                auto     handle      = handles[s];

                binding_array_iter->second.descriptor_entries[array_index] = { .type         = write.descriptorType,
                                                                               .acceleration = { handle } };
            }
        }
        else
        {
            GFXRECON_LOG_DEBUG("call %llu vkUpdateDescriptorSets: unhandled descriptor type %s.",
                               call_info.index,
                               util::ToString<VkDescriptorType>(write.descriptorType).c_str());
        }
    }
}

void VulkanSpirvTrackModifier::Process_vkCreatePipelineLayout(const ApiCallInfo&          call_info,
                                                              args::CreatePipelineLayout& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle      = *args.pPipelineLayout.GetPointer();
    const auto*      create_info = args.pCreateInfo.GetMetaStructPointer();

    GFXRECON_ASSERT(pipeline_layout_entries_.find(handle) == pipeline_layout_entries_.end());

    pipeline_layout_entries_[handle].handle            = handle;
    pipeline_layout_entries_[handle].creation_index    = call_info.index;
    pipeline_layout_entries_[handle].destruction_index = UINT64_MAX;
    pipeline_layout_entries_[handle].flags             = create_info->decoded_value->flags;

    uint32_t dynamic_start_index = 0;
    for (uint32_t i = 0; i < create_info->decoded_value->setLayoutCount; i++)
    {
        pipeline_layout_entries_[handle].setLayouts.push_back(create_info->pSetLayouts.GetPointer()[i]);

        // build dynamic binding map
        for (const auto& layout_binding_iter : set_layout_entries_[create_info->pSetLayouts.GetPointer()[i]].bindings)
        {
            const Binding& binding = layout_binding_iter.second;
            if (binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            {
                pipeline_layout_entries_[handle].dynamicBindingRef[i].emplace_back(
                    i, binding.binding, dynamic_start_index, binding.descriptorCount, binding.type);
                dynamic_start_index += binding.descriptorCount;
            }
        }
    }

    for (uint32_t i = 0; i < create_info->decoded_value->pushConstantRangeCount; i++)
    {
        VkPushConstantRange range = create_info->decoded_value->pPushConstantRanges[i];
        pipeline_layout_entries_[handle].pushConstantRanges.emplace_back(range);

#define PUSHRANGEINDICES(flagBits)                                                    \
    if ((range.stageFlags & flagBits) == flagBits)                                    \
    {                                                                                 \
        pipeline_layout_entries_[handle].rangeIndicesPerStage[flagBits].push_back(i); \
    }
        PUSHRANGEINDICES(VK_SHADER_STAGE_VERTEX_BIT);
        PUSHRANGEINDICES(VK_SHADER_STAGE_FRAGMENT_BIT);
        PUSHRANGEINDICES(VK_SHADER_STAGE_COMPUTE_BIT);
    }

    for (const auto& it : pipeline_layout_entries_[handle].rangeIndicesPerStage)
    {
        uint32_t offset = UINT32_MAX;
        uint32_t size   = 0;
        for (const auto& index : it.second)
        {
            const VkPushConstantRange& range = pipeline_layout_entries_[handle].pushConstantRanges[index];
            if (range.offset < offset)
            {
                offset = range.offset;
            }
            if (size < range.offset + range.size)
            {
                size = range.offset + range.size;
            }
        }
        pipeline_layout_entries_[handle].mergedRangePerStage[it.first] = { offset, size - offset };
    }
}

void VulkanSpirvTrackModifier::Process_vkDestroyPipelineLayout(const ApiCallInfo&           call_info,
                                                               args::DestroyPipelineLayout& args)
{
    if (IsModificationPass())
    {
        return;
    }
    if (pipeline_layout_entries_.find(args.pipelineLayout) != pipeline_layout_entries_.end())
    {
        pipeline_layout_entries_[args.pipelineLayout].destruction_index = call_info.index;
    }
}

void VulkanSpirvTrackModifier::Process_vkCreateComputePipelines(const ApiCallInfo&            call_info,
                                                                args::CreateComputePipelines& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto* create_info = args.pCreateInfos.GetMetaStructPointer();

    for (uint32_t idx = 0; idx < args.createInfoCount; idx++)
    {
        format::HandleId handle                     = args.pPipelines.GetPointer()[idx];
        pipeline_entries_[handle].handle            = handle;
        pipeline_entries_[handle].creation_index    = call_info.index;
        pipeline_entries_[handle].destruction_index = UINT64_MAX;
        pipeline_entries_[handle].layout            = create_info[idx].layout;

        Decoded_VkPipelineShaderStageCreateInfo* stage_info = create_info[idx].stage;

        ShaderStage stage;
        stage.pName                   = stage_info->pName.GetPointer();
        stage.module                  = stage_info->module;
        stage.stageFlagBit            = stage_info->decoded_value->stage;
        stage.specialization_dataSize = 0;

        const auto* spec_info = stage_info->pSpecializationInfo->GetMetaStructPointer();
        if (spec_info)
        {
            uint32_t entry_count = spec_info->decoded_value->mapEntryCount;

            const auto* entries = spec_info->pMapEntries->GetPointer();
            if (entries)
            {
                stage.specialization_dataSize = spec_info->decoded_value->dataSize;
                stage.specialization_constant = { spec_info->pData.GetPointer(),
                                                  spec_info->pData.GetPointer() + spec_info->decoded_value->dataSize };

                for (uint32_t i = 0; i < entry_count; i++)
                {
                    stage.specialization_offset_entries[entries[i].constantID] = entries[i].offset;
                }
            }
        }
        pipeline_entries_[handle].stages.push_back(stage);
    }
}

void VulkanSpirvTrackModifier::Process_vkCreateGraphicsPipelines(const ApiCallInfo&             call_info,
                                                                 args::CreateGraphicsPipelines& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto* create_info = args.pCreateInfos.GetMetaStructPointer();

    for (uint32_t idx = 0; idx < args.createInfoCount; idx++)
    {
        format::HandleId handle                     = args.pPipelines.GetPointer()[idx];
        pipeline_entries_[handle].handle            = handle;
        pipeline_entries_[handle].creation_index    = call_info.index;
        pipeline_entries_[handle].destruction_index = UINT64_MAX;
        pipeline_entries_[handle].layout            = create_info[idx].layout;

        const auto* stage_info = create_info[idx].pStages->GetMetaStructPointer();

        for (uint32_t i = 0; i < create_info[idx].decoded_value->stageCount; i++)
        {
            ShaderStage stage;
            stage.pName                   = stage_info[i].pName.GetPointer();
            stage.module                  = stage_info[i].module;
            stage.stageFlagBit            = stage_info[i].decoded_value->stage;
            stage.specialization_dataSize = 0;

            const auto* spec_info = stage_info[i].pSpecializationInfo->GetMetaStructPointer();
            if (spec_info)
            {
                uint32_t    entry_count = spec_info->decoded_value->mapEntryCount;
                const auto* entries     = spec_info->pMapEntries->GetPointer();

                if (entries)
                {
                    stage.specialization_dataSize = spec_info->decoded_value->dataSize;
                    stage.specialization_constant = { spec_info->pData.GetPointer(),
                                                      spec_info->pData.GetPointer() +
                                                          spec_info->decoded_value->dataSize };

                    for (uint32_t j = 0; j < entry_count; j++)
                    {
                        stage.specialization_offset_entries[entries[i].constantID] = entries[i].offset;
                    }
                }
            }
            pipeline_entries_[handle].stages.push_back(stage);
        }
    }
}

void VulkanSpirvTrackModifier::Process_vkDestroyPipeline(const ApiCallInfo& call_info, args::DestroyPipeline& args)
{
    if (IsModificationPass())
    {
        return;
    }
    if (pipeline_entries_.find(args.pipeline) != pipeline_entries_.end())
    {
        pipeline_entries_[args.pipeline].destruction_index = call_info.index;
    }
}

void VulkanSpirvTrackModifier::Process_vkAllocateCommandBuffers(const ApiCallInfo&            call_info,
                                                                args::AllocateCommandBuffers& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkCommandBufferAllocateInfo* alloc_info = args.pAllocateInfo.GetPointer();

    for (uint32_t i = 0; i < alloc_info->commandBufferCount; i++)
    {
        format::HandleId handle = args.pCommandBuffers.GetPointer()[i];
        GFXRECON_ASSERT(command_buffer_entries_.find(handle) == command_buffer_entries_.end());

        command_buffer_entries_[handle].handle            = handle;
        command_buffer_entries_[handle].device_id         = args.device;
        command_buffer_entries_[handle].creation_index    = call_info.index;
        command_buffer_entries_[handle].destruction_index = UINT64_MAX;
        command_buffer_entries_[handle].state             = CommandBufferLifeCycle::Inital;
    }
}

void VulkanSpirvTrackModifier::Process_vkFreeCommandBuffers(const ApiCallInfo&        call_info,
                                                            args::FreeCommandBuffers& args)
{
    if (IsModificationPass())
    {
        return;
    }

    for (uint32_t i = 0; i < args.commandBufferCount; i++)
    {
        format::HandleId handle = args.pCommandBuffers.GetPointer()[i];
        auto             entry  = command_buffer_entries_.find(handle);
        if (entry != command_buffer_entries_.end())
        {
            entry->second.destruction_index = call_info.index;
            entry->second.state             = CommandBufferLifeCycle::Invalid;
        }

        command_buffer_recording.erase(handle);
        command_buffer_submit_recordings.erase(handle);
        command_buffer_state.erase(handle);
        provenance_tracker_.ErasePushConstantProvenance(handle);
    }
}

void VulkanSpirvTrackModifier::Process_vkResetCommandBuffer(const ApiCallInfo&        call_info,
                                                            args::ResetCommandBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state == CommandBufferLifeCycle::Invalid)
    {
        return;
    }

    if (command_buffer_recording.find(args.commandBuffer) != command_buffer_recording.end() &&
        command_buffer_entries_[args.commandBuffer].state == CommandBufferLifeCycle::Pending)
    {
        GFXRECON_LOG_DEBUG("command buffer %llu is in pending state. Return from reset.", args.commandBuffer);
        return;
    }

    command_buffer_entries_[args.commandBuffer].state = CommandBufferLifeCycle::Inital;

    command_buffer_recording.erase(args.commandBuffer);
    command_buffer_submit_recordings.erase(args.commandBuffer);
    command_buffer_state.erase(args.commandBuffer);
    provenance_tracker_.ErasePushConstantProvenance(args.commandBuffer);
}

void VulkanSpirvTrackModifier::Process_vkBeginCommandBuffer(const ApiCallInfo&        call_info,
                                                            args::BeginCommandBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state == CommandBufferLifeCycle::Invalid)
    {
        return;
    }

    if (command_buffer_recording.find(args.commandBuffer) != command_buffer_recording.end() &&
        command_buffer_entries_[args.commandBuffer].state == CommandBufferLifeCycle::Pending)
    {
        GFXRECON_LOG_INFO("command buffer %llu is in pending state. Return from begin.", args.commandBuffer);
        return;
    }

    if (command_buffer_recording.find(args.commandBuffer) != command_buffer_recording.end() &&
        (command_buffer_entries_[args.commandBuffer].state == CommandBufferLifeCycle::Recording ||
         command_buffer_entries_[args.commandBuffer].state == CommandBufferLifeCycle::Executable))
    {
        GFXRECON_LOG_INFO("command buffer %llu has been in recording/executable state and will be reset now.",
                          args.commandBuffer);
    }

    command_buffer_entries_[args.commandBuffer].state = CommandBufferLifeCycle::Recording;

    command_buffer_recording.erase(args.commandBuffer);
    command_buffer_submit_recordings.erase(args.commandBuffer);
    command_buffer_state.erase(args.commandBuffer);

    command_buffer_recording[args.commandBuffer].command_buffer = args.commandBuffer;
    command_buffer_recording[args.commandBuffer].bind_point     = VK_PIPELINE_BIND_POINT_MAX_ENUM;

    command_buffer_state[args.commandBuffer].command_buffer = args.commandBuffer;
    command_buffer_state[args.commandBuffer].push_constant.assign(256, 0); // MAX_PUSHCONSTANT_SIZE
    provenance_tracker_.ClearPushConstantProvenance(args.commandBuffer);
}

void VulkanSpirvTrackModifier::Process_vkEndCommandBuffer(const ApiCallInfo& call_info, args::EndCommandBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }
    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state == CommandBufferLifeCycle::Invalid)
    {
        return;
    }
    command_buffer_entries_.at(args.commandBuffer).state = CommandBufferLifeCycle::Executable;
}

void VulkanSpirvTrackModifier::Process_vkGetBufferDeviceAddress(const ApiCallInfo&            call_info,
                                                                args::GetBufferDeviceAddress& args)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                     = args.pInfo.GetMetaStructPointer()->buffer;
    buffer_device_addresses_[args.result]     = buffer_id;
    buffer_entries_[buffer_id].device_address = args.result;
}

void VulkanSpirvTrackModifier::Process_vkGetBufferDeviceAddressKHR(const ApiCallInfo&               call_info,
                                                                   args::GetBufferDeviceAddressKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                     = args.pInfo.GetMetaStructPointer()->buffer;
    buffer_device_addresses_[args.result]     = buffer_id;
    buffer_entries_[buffer_id].device_address = args.result;
}

void VulkanSpirvTrackModifier::Process_vkGetBufferDeviceAddressEXT(const ApiCallInfo&               call_info,
                                                                   args::GetBufferDeviceAddressEXT& args)
{
    if (IsModificationPass())
    {
        return;
    }
    const auto& buffer_id                     = args.pInfo.GetMetaStructPointer()->buffer;
    buffer_device_addresses_[args.result]     = buffer_id;
    buffer_entries_[buffer_id].device_address = args.result;
}

void VulkanSpirvTrackModifier::Process_vkGetAccelerationStructureDeviceAddressKHR(
    const ApiCallInfo& call_info, args::GetAccelerationStructureDeviceAddressKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto& as_id = args.pInfo.GetMetaStructPointer()->accelerationStructure;
    acceleration_structure_device_addresses_[args.result].insert(as_id);
    if (acceleration_structure_entries_.find(as_id) != acceleration_structure_entries_.end())
    {
        acceleration_structure_entries_[as_id].device_address = args.result;
        if (buffer_device_addresses_.find(args.result) != buffer_device_addresses_.end())
        {
            buffer_device_addresses_.erase(args.result);
        }
    }
}

void VulkanSpirvTrackModifier::Process_vkCmdBindPipeline(const ApiCallInfo& call_info, args::CmdBindPipeline& args)
{
    if (IsModificationPass())
    {
        return;
    }

    // As long as in Recording state, the command_buffer_recording contains the commandBuffer value.
    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    command_buffer_recording[args.commandBuffer].pipelines[args.pipelineBindPoint] = args.pipeline;
}

void VulkanSpirvTrackModifier::Process_vkCmdBindDescriptorSets(const ApiCallInfo&           call_info,
                                                               args::CmdBindDescriptorSets& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    DescriptorStateCommand command;
    command.type       = DescriptorStateCommand::Type::BindDescriptorSets;
    command.bind_point = args.pipelineBindPoint;

    for (uint32_t i = 0; i < args.descriptorSetCount; i++)
    {
        format::HandleId descriptor_set = args.pDescriptorSets.GetPointer()[i];
        command.descriptor_sets.push_back({ args.firstSet + i, descriptor_set, args.layout });
    }

    if (args.dynamicOffsetCount > 0)
    {
        uint32_t   consumed_index = 0;
        uint32_t   consumed_count = 0;
        uint32_t   remained_count = args.dynamicOffsetCount;
        const auto layout_iter    = pipeline_layout_entries_.find(args.layout);
        for (uint32_t i = 0; i < args.descriptorSetCount; i++)
        {
            const auto ref_iter = layout_iter->second.dynamicBindingRef.find(args.firstSet + i);
            if (ref_iter != layout_iter->second.dynamicBindingRef.end())
            {
                if (remained_count == 0)
                {
                    GFXRECON_LOG_INFO("call %llu vkCmdBindDescriptorSets: Dynamic offset mismatch on set[%u].",
                                      call_info.index,
                                      args.firstSet + i);
                    break;
                }

                DynamicBindingOffsetMap elem;
                elem.set = args.firstSet + i;
                for (const DynamicBindingRef& ref : ref_iter->second)
                {
                    if (remained_count >= ref.elementCount)
                    {
                        consumed_count = ref.elementCount;
                        remained_count -= ref.elementCount;
                    }
                    else if (remained_count == 0)
                    {
                        GFXRECON_LOG_INFO(
                            "call %llu vkCmdBindDescriptorSets: Dynamic offset mismatch on set[%u] binding[%u].",
                            call_info.index,
                            args.firstSet + i,
                            ref.binding);
                        break;
                    }
                    else
                    {
                        consumed_count = remained_count;
                        remained_count = 0;
                        GFXRECON_LOG_INFO(
                            "call %llu vkCmdBindDescriptorSets: Dynamic offset mismatch on set[%u] binding[%u].",
                            call_info.index,
                            args.firstSet + i,
                            ref.binding);
                    }
                    elem.binding_offsets[ref.binding] = { args.pDynamicOffsets.GetPointer() + consumed_index,
                                                          args.pDynamicOffsets.GetPointer() + consumed_index +
                                                              consumed_count };
                    consumed_index += consumed_count;
                }
                command.dynamic_offsets[elem.set] = std::move(elem);
            }
        }
    }

    if (!command.descriptor_sets.empty())
    {
        command_buffer_recording[args.commandBuffer].descriptor_state_commands.push_back(std::move(command));
    }
}

void VulkanSpirvTrackModifier::Process_vkCmdBindDescriptorBuffersEXT(const ApiCallInfo&                 call_info,
                                                                     args::CmdBindDescriptorBuffersEXT& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    DescriptorStateCommand command;
    command.type = DescriptorStateCommand::Type::BindDescriptorBuffers;

    const auto* binding_info      = args.pBindingInfos.GetPointer();
    const auto* meta_binding_info = args.pBindingInfos.GetMetaStructPointer();
    for (uint32_t i = 0; i < args.bufferCount; i++)
    {
        format::HandleId    handle       = 0;
        VkDeviceAddress     base_address = 0;
        VkBufferUsageFlags2 usage2       = binding_info[i].usage;
        bool                resolved     = false;
        auto                iter         = findEntryFromBufferDeviceAddress(binding_info[i].address);

        const auto* usage2_struct_info =
            GetPNextMetaStruct<Decoded_VkBufferUsageFlags2CreateInfo>(meta_binding_info[i].pNext);
        if (usage2_struct_info != nullptr && usage2_struct_info->decoded_value != nullptr)
        {
            usage2 = usage2_struct_info->decoded_value->usage;
        }

        if (iter != buffer_device_addresses_.end())
        {
            base_address = iter->first;
            handle       = iter->second;

            const auto* ext_struct_info =
                GetPNextMetaStruct<Decoded_VkDescriptorBufferBindingPushDescriptorBufferHandleEXT>(
                    meta_binding_info[i].pNext);
            if (ext_struct_info == nullptr || ext_struct_info->buffer == handle)
            {
                resolved = true;
            }
            else
            {
                GFXRECON_LOG_INFO(
                    "call %llu vkCmdBindDescriptorBuffersEXT: bound descriptor buffer handle %llu does not match "
                    "VkDescriptorBufferBindingPushDescriptorBufferHandleEXT buffer %llu.",
                    call_info.index,
                    handle,
                    ext_struct_info->buffer);
            }
        }
        else
        {
            GFXRECON_LOG_INFO("call %llu vkCmdBindDescriptorBuffersEXT: the device address 0x%lx of bound "
                              "descriptor buffer does not existed.",
                              call_info.index,
                              binding_info[i].address);
        }
        command.descriptor_buffers.push_back(
            { handle, base_address, binding_info[i].address, binding_info[i].usage, usage2, resolved });
    }

    command_buffer_recording[args.commandBuffer].descriptor_state_commands.push_back(std::move(command));
}

void VulkanSpirvTrackModifier::Process_vkCmdSetDescriptorBufferOffsetsEXT(const ApiCallInfo& call_info,
                                                                          args::CmdSetDescriptorBufferOffsetsEXT& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    DescriptorStateCommand command;
    command.type       = DescriptorStateCommand::Type::SetDescriptorBufferOffsets;
    command.bind_point = args.pipelineBindPoint;

    for (uint32_t i = 0; i < args.setCount; i++)
    {
        uint32_t     buffer_index = args.pBufferIndices.GetPointer()[i];
        VkDeviceSize offset       = args.pOffsets.GetPointer()[i];

        command.descriptor_buffer_offsets.emplace_back(args.firstSet + i, buffer_index, offset, args.layout);
    }

    if (!command.descriptor_buffer_offsets.empty())
    {
        command_buffer_recording[args.commandBuffer].descriptor_state_commands.push_back(std::move(command));
    }
}

void VulkanSpirvTrackModifier::Process_vkCmdPushConstants(const ApiCallInfo& call_info, args::CmdPushConstants& args)
{
    if (IsModificationPass())
    {
        const FixupLocations* fixups =
            fixup_location_builder_.GetFixupLocations(ProvenanceRootType::PushConstants, call_info.index);
        if ((fixups != nullptr) && !fixups->address_locations.empty())
        {
            const auto command_buffer_iter = command_buffer_entries_.find(args.commandBuffer);
            GFXRECON_ASSERT(command_buffer_iter != command_buffer_entries_.end());
            if (command_buffer_iter != command_buffer_entries_.end())
            {
                // Modification pass only needs the owning device id for metadata emission here.
                WriteFixDeviceAddressCmd(command_buffer_iter->second.device_id, fixups->address_locations);
            }
        }
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    std::vector<uint8_t> values(args.size);
    std::memcpy(values.data(), args.pValues.GetPointer(), args.size);

    command_buffer_recording[args.commandBuffer].push_constants.push_back(
        { args.offset, args.size, args.stageFlags, args.layout, std::move(values), call_info.index });
}

void VulkanSpirvTrackModifier::Process_vkCmdFillBuffer(const ApiCallInfo& call_info, args::CmdFillBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    CommandBufferRecording& recording = command_buffer_recording[args.commandBuffer];

    uint8_t*         data_ptr = reinterpret_cast<uint8_t*>(&args.data);
    BufferWriteEvent event(SourceType::Fill, args.dstBuffer);
    event.srcPointer = { data_ptr, data_ptr + sizeof(uint32_t) };
    event.regions.emplace_back(0, args.dstOffset, args.size);

    recording.action_command_list.emplace_back(std::move(event));
    recording.in_operation = true;
}

void VulkanSpirvTrackModifier::Process_vkCmdUpdateBuffer(const ApiCallInfo& call_info, args::CmdUpdateBuffer& args)
{
    if (IsModificationPass())
    {
        const FixupLocations* fixups =
            fixup_location_builder_.GetFixupLocations(ProvenanceRootType::UpdateBuffer, call_info.index);
        if ((fixups != nullptr) && !fixups->address_locations.empty())
        {
            const auto command_buffer_iter = command_buffer_entries_.find(args.commandBuffer);
            GFXRECON_ASSERT(command_buffer_iter != command_buffer_entries_.end());
            if (command_buffer_iter != command_buffer_entries_.end())
            {
                // Modification pass only needs the owning device id for metadata emission here.
                WriteFixDeviceAddressCmd(command_buffer_iter->second.device_id, fixups->address_locations);
            }
        }
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    CommandBufferRecording& recording = command_buffer_recording[args.commandBuffer];

    BufferWriteEvent event(SourceType::Update, args.dstBuffer);
    event.srcPointer = { args.pData.GetPointer(), args.pData.GetPointer() + args.dataSize };
    event.regions.emplace_back(0, args.dstOffset, args.dataSize);
    event.call_index = call_info.index;

    recording.action_command_list.emplace_back(std::move(event));
    recording.in_operation = true;
}

void VulkanSpirvTrackModifier::Process_vkCmdCopyBuffer(const ApiCallInfo& call_info, args::CmdCopyBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    CommandBufferRecording& recording = command_buffer_recording[args.commandBuffer];
    const VkBufferCopy*     regions   = args.pRegions.GetPointer();

    BufferWriteEvent event(SourceType::CopyBuffer, args.dstBuffer, args.srcBuffer);
    for (uint32_t i = 0; i < args.regionCount; i++)
    {
        event.regions.emplace_back(regions[i]);
    }

    recording.action_command_list.emplace_back(std::move(event));
    recording.in_operation = true;
}

void VulkanSpirvTrackModifier::Process_vkCmdCopyBuffer2(const ApiCallInfo& call_info, args::CmdCopyBuffer2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    CommandBufferRecording& recording      = command_buffer_recording[args.commandBuffer];
    const auto*             meta_copy_info = args.pCopyBufferInfo.GetMetaStructPointer();
    const auto*             copy2          = meta_copy_info->pRegions->GetPointer();

    BufferWriteEvent event(SourceType::CopyBuffer, meta_copy_info->dstBuffer, meta_copy_info->srcBuffer);
    for (uint32_t i = 0; i < meta_copy_info->decoded_value->regionCount; i++)
    {
        event.regions.emplace_back(copy2[i].srcOffset, copy2[i].dstOffset, copy2[i].size);
    }

    recording.action_command_list.emplace_back(std::move(event));
    recording.in_operation = true;
}

void VulkanSpirvTrackModifier::Process_vkCmdCopyBuffer2KHR(const ApiCallInfo& call_info, args::CmdCopyBuffer2KHR& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    CommandBufferRecording& recording      = command_buffer_recording[args.commandBuffer];
    const auto*             meta_copy_info = args.pCopyBufferInfo.GetMetaStructPointer();
    const auto*             copy2          = meta_copy_info->pRegions->GetPointer();

    BufferWriteEvent event(SourceType::CopyBuffer, meta_copy_info->dstBuffer, meta_copy_info->srcBuffer);
    for (uint32_t i = 0; i < meta_copy_info->decoded_value->regionCount; i++)
    {
        event.regions.emplace_back(copy2[i].srcOffset, copy2[i].dstOffset, copy2[i].size);
    }

    recording.action_command_list.emplace_back(std::move(event));
    recording.in_operation = true;
}

void VulkanSpirvTrackModifier::Process_vkCmdBuildAccelerationStructuresKHR(
    const ApiCallInfo& call_info, args::CmdBuildAccelerationStructuresKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }
    CommandBufferRecording& recording = command_buffer_recording.at(args.commandBuffer);

    for (uint32_t info_index = 0; info_index < args.infoCount; info_index++)
    {
        const auto* build_range_info         = args.ppBuildRangeInfos.GetPointer()[info_index];
        const auto& build_geometry_meta_info = args.pInfos.GetMetaStructPointer()[info_index];
        const auto& build_geometry_info      = args.pInfos.GetPointer()[info_index];

        BuildAccelerationStructureAction build_action = {
            call_info.index, info_index, build_geometry_meta_info.dstAccelerationStructure, {}
        };

        const auto* geometries = build_geometry_meta_info.pGeometries->GetPointer();

        // TODO: support ppGeometries
        GFXRECON_ASSERT(geometries != nullptr);

        for (uint32_t geometry_index = 0; geometry_index < build_geometry_info.geometryCount; geometry_index++)
        {
            const VkAccelerationStructureGeometryKHR& geometry = geometries[geometry_index];

            switch (geometry.geometryType)
            {
                case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                {
                    GFXRECON_ASSERT(build_geometry_info.type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
                    break;
                }
                case VK_GEOMETRY_TYPE_AABBS_KHR:
                {
                    GFXRECON_ASSERT(build_geometry_info.type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
                    break;
                }
                case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                {
                    GFXRECON_ASSERT(build_geometry_info.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);
                    if (geometry.geometry.instances.arrayOfPointers == VK_TRUE)
                    {
                        GFXRECON_LOG_INFO("call %llu CmdBuildAccelerationStructures: arrayOfPointers == VK_TRUE is not "
                                          "supported yet.",
                                          call_info.index);
                        break;
                    }

                    VkDeviceAddress address = geometry.geometry.instances.data.deviceAddress;
                    auto            entry   = findEntryFromBufferDeviceAddress(address);
                    if (entry == buffer_device_addresses_.end())
                    {
                        GFXRECON_LOG_INFO("call %llu CmdBuildAccelerationStructures: non instance buffer match address "
                                          "%llu for tlas.",
                                          call_info.index,
                                          address);
                        break;
                    }

                    const uint64_t base_offset_in_buffer = address - entry->first;
                    const uint32_t primitive_count       = build_range_info[geometry_index].primitiveCount;
                    const uint32_t primitive_offset      = build_range_info[geometry_index].primitiveOffset;

                    // Expand each accelerationStructureReference slot consumed by
                    // this TLAS geometry into a buffer-local use-site candidate.
                    for (uint32_t i = 0; i < primitive_count; ++i)
                    {
                        uint64_t slot_offset =
                            base_offset_in_buffer + primitive_offset +
                            static_cast<uint64_t>(i) * sizeof(VkAccelerationStructureInstanceKHR) +
                            offsetof(VkAccelerationStructureInstanceKHR, accelerationStructureReference);
                        build_action.candidate_use_sites.push_back({ entry->second, slot_offset });
                    }
                    break;
                }
                default:
                {
                    GFXRECON_LOG_ERROR("Unexpected geometry type in CmdBuildAccelerationStructures.");
                    break;
                }
            }
        }

        recording.action_command_list.emplace_back(std::move(build_action));
    }

    recording.in_operation = true;
}

void VulkanSpirvTrackModifier::Process_vkCmdDispatch(const ApiCallInfo& call_info, args::CmdDispatch& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }
    command_buffer_recording[args.commandBuffer].bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    command_buffer_submit_recordings[args.commandBuffer].emplace_back(command_buffer_recording[args.commandBuffer]);
    resetRecording(args.commandBuffer);
}

void VulkanSpirvTrackModifier::Process_vkCmdDispatchIndirect(const ApiCallInfo&         call_info,
                                                             args::CmdDispatchIndirect& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    command_buffer_recording[args.commandBuffer].bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    command_buffer_submit_recordings[args.commandBuffer].emplace_back(command_buffer_recording[args.commandBuffer]);
    resetRecording(args.commandBuffer);
}

void VulkanSpirvTrackModifier::Process_vkCmdDraw(const ApiCallInfo& call_info, args::CmdDraw& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    command_buffer_recording[args.commandBuffer].bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    command_buffer_submit_recordings[args.commandBuffer].emplace_back(command_buffer_recording[args.commandBuffer]);
    resetRecording(args.commandBuffer);
}

void VulkanSpirvTrackModifier::Process_vkCmdDrawIndexed(const ApiCallInfo& call_info, args::CmdDrawIndexed& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    command_buffer_recording[args.commandBuffer].bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    command_buffer_submit_recordings[args.commandBuffer].emplace_back(command_buffer_recording[args.commandBuffer]);
    resetRecording(args.commandBuffer);
}

void VulkanSpirvTrackModifier::Process_vkCmdDrawIndirect(const ApiCallInfo& call_info, args::CmdDrawIndirect& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    command_buffer_recording[args.commandBuffer].bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    command_buffer_submit_recordings[args.commandBuffer].emplace_back(command_buffer_recording[args.commandBuffer]);
    resetRecording(args.commandBuffer);
}

void VulkanSpirvTrackModifier::Process_vkCmdDrawIndexedIndirect(const ApiCallInfo&            call_info,
                                                                args::CmdDrawIndexedIndirect& args)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_buffer_entries_.find(args.commandBuffer) == command_buffer_entries_.end() ||
        command_buffer_entries_[args.commandBuffer].state != CommandBufferLifeCycle::Recording)
    {
        return;
    }

    command_buffer_recording[args.commandBuffer].bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    command_buffer_submit_recordings[args.commandBuffer].emplace_back(command_buffer_recording[args.commandBuffer]);
    resetRecording(args.commandBuffer);
}

void VulkanSpirvTrackModifier::process_SubmitInfo(uint64_t                             submitIndex,
                                                  const std::vector<format::HandleId>& commandBuffers,
                                                  const std::vector<format::HandleId>& waitSems,
                                                  const std::vector<format::HandleId>& signalSems,
                                                  const std::vector<uint64_t>&         waitValues,
                                                  const std::vector<uint64_t>&         signalValues)
{
    bool allWaitSatisfied = true;
    // whether all the wait semaphores are signaled. Set all the semaphores to null by default.
    for (size_t i = 0; i < waitSems.size(); i++)
    {
        if (semaphore_state.find(waitSems[i]) == semaphore_state.end())
        {
            continue;
        }

        SemaphoreState& wait_state = semaphore_state.at(waitSems[i]);
        if (wait_state.type == VK_SEMAPHORE_TYPE_TIMELINE)
        {
            GFXRECON_ASSERT(waitValues.size() == waitSems.size());
            GFXRECON_ASSERT(std::holds_alternative<TimelineSemaphoreState>(wait_state.state));

            uint64_t required = waitValues[i];
            auto&    timeline = std::get<TimelineSemaphoreState>(wait_state.state);
            if (timeline.currentValue < required)
            {
                allWaitSatisfied = false;
                timeline.waitingSubmits.emplace_back(submitIndex, required);
            }
        }
        else
        {
            GFXRECON_ASSERT(std::holds_alternative<BinarySemaphoreState>(wait_state.state));

            auto& binary = std::get<BinarySemaphoreState>(wait_state.state);
            if (!binary.signaled)
            {
                allWaitSatisfied = false;
                binary.waitingSubmits.push_back(submitIndex);
            }
        }
    }

    SubmitInfo info;
    info.submit_index      = submitIndex;
    info.command_buffers   = commandBuffers;
    info.wait_semaphores   = waitSems;
    info.signal_semaphores = signalSems;
    info.wait_values       = waitValues;
    info.signal_values     = signalValues;
    info.ready             = allWaitSatisfied;

    submit_entries_[submitIndex] = info;
    if (allWaitSatisfied)
    {
        ready_submits.push_back(submitIndex);
    }
}

void VulkanSpirvTrackModifier::signalSemaphoresFrom(uint64_t submitIndex)
{
    SubmitInfo& signaled_submit_info = submit_entries_.at(submitIndex);
    GFXRECON_ASSERT(signaled_submit_info.submitted);

    // the submit has been executed, signal the depending semaphores
    for (size_t i = 0; i < signaled_submit_info.signal_semaphores.size(); i++)
    {
        SemaphoreState& signaled_state = semaphore_state.at(signaled_submit_info.signal_semaphores[i]);

        if (signaled_state.type == VK_SEMAPHORE_TYPE_TIMELINE)
        {
            auto& timeline = std::get<TimelineSemaphoreState>(signaled_state.state);
            // update timeline semaphore's value with the signaled value
            uint64_t signaledVal = signaled_submit_info.signal_values[i];
            if (signaledVal > timeline.currentValue)
            {
                timeline.currentValue = signaledVal;
            }

            std::deque<std::pair<uint64_t, uint64_t>> remained_waiting;
            // traversal the submits waiting on the signaled sem
            while (!timeline.waitingSubmits.empty())
            {
                auto [dependent_submit, requiredVal] = timeline.waitingSubmits.front();
                timeline.waitingSubmits.pop_front();
                if (timeline.currentValue > requiredVal)
                {
                    SubmitInfo& dependent_submit_info = submit_entries_[dependent_submit];
                    bool        allReady              = true;
                    // whether all the wait_sem are signaled
                    for (size_t i = 0; i < dependent_submit_info.wait_semaphores.size(); i++)
                    {
                        // dependent submit's waitSem also timeline
                        auto handle = dependent_submit_info.wait_semaphores[i];
                        if (std::get<TimelineSemaphoreState>(semaphore_state[handle].state).currentValue <
                            dependent_submit_info.wait_values[i])
                        {
                            allReady = false;
                            break;
                        }
                    }
                    if (allReady && !dependent_submit_info.ready)
                    {
                        dependent_submit_info.ready = true;
                        ready_submits.push_back(dependent_submit);
                    }
                }
                else
                {
                    remained_waiting.push_back({ dependent_submit, requiredVal });
                }
            }
            timeline.waitingSubmits = std::move(remained_waiting);
        }
        else
        {
            auto& binary            = std::get<BinarySemaphoreState>(signaled_state.state);
            binary.signaled         = true;
            binary.signaledBySubmit = submitIndex;
            // traversal the submits waiting on the signaled sem
            while (!binary.waitingSubmits.empty())
            {
                uint64_t dependent_submit = binary.waitingSubmits.front();
                binary.waitingSubmits.pop_front();
                SubmitInfo& dependent_submit_info = submit_entries_[dependent_submit];

                bool allReady = true;
                // whether all the wait_sem are signaled
                for (auto handle : dependent_submit_info.wait_semaphores)
                {
                    if (!std::get<BinarySemaphoreState>(semaphore_state[handle].state).signaled)
                    {
                        allReady = false;
                        break;
                    }
                }
                if (allReady && !dependent_submit_info.ready)
                {
                    dependent_submit_info.ready = true;
                    ready_submits.push_back(dependent_submit);
                }
            }
        }
    }
}

void VulkanSpirvTrackModifier::Process_vkQueueSubmit(const ApiCallInfo& call_info, args::QueueSubmit& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto* meta_submit_infos = args.pSubmits.GetMetaStructPointer();
    const auto* submit_infos      = args.pSubmits.GetPointer();

    std::vector<format::HandleId> command_buffer;
    for (uint32_t submit_index = 0; submit_index < args.submitCount; submit_index++)
    {
        for (uint32_t cmdBuffer_index = 0; cmdBuffer_index < submit_infos[submit_index].commandBufferCount;
             cmdBuffer_index++)
        {
            format::HandleId cmdBuf = meta_submit_infos[submit_index].pCommandBuffers.GetPointer()[cmdBuffer_index];
            command_buffer.push_back(cmdBuf);
            command_buffer_entries_.at(cmdBuf).state = CommandBufferLifeCycle::Pending;

            if (command_buffer_recording[cmdBuf].in_operation &&
                command_buffer_recording[cmdBuf].bind_point == VK_PIPELINE_BIND_POINT_MAX_ENUM)
            {
                command_buffer_submit_recordings[cmdBuf].emplace_back(command_buffer_recording[cmdBuf]);
            }
            command_buffer_recording.erase(cmdBuf);
        }

        process_SubmitInfo(global_submit_index++, command_buffer);
        command_buffer.clear();
    }

    while (!ready_submits.empty())
    {
        uint64_t submit_id = ready_submits.front();
        ready_submits.pop_front();

        SubmitInfo& submit_info = submit_entries_[submit_id];
        if (submit_info.submitted)
        {
            continue;
        }
        submit_info.submitted = true;

        // execute each command buffer in the submit
        for (auto& id : submit_info.command_buffers)
        {
            executeCommandBuffer(id);
        }

        // signalSemaphoresFrom(submit_id);
        submit_entries_.erase(submit_id);
    }
}

void VulkanSpirvTrackModifier::Process_vkQueueSubmit2(const ApiCallInfo& call_info, args::QueueSubmit2& args)
{
    if (IsModificationPass())
    {
        return;
    }
}

void VulkanSpirvTrackModifier::Process_vkQueueSubmit2KHR(const ApiCallInfo& call_info, args::QueueSubmit2KHR& args)
{
    if (IsModificationPass())
    {
        return;
    }
}

void VulkanSpirvTrackModifier::resetRecording(format::HandleId commandBuffer)
{
    command_buffer_recording[commandBuffer].bind_point   = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    command_buffer_recording[commandBuffer].in_operation = false;
    command_buffer_recording[commandBuffer].push_constants.clear();
    command_buffer_recording[commandBuffer].pipelines.clear();
    command_buffer_recording[commandBuffer].descriptor_state_commands.clear();
    command_buffer_recording[commandBuffer].action_command_list.clear();
}

void VulkanSpirvTrackModifier::ApplyActionCommands(const CommandBufferRecording& recording)
{
    // Replay write and BuildAS actions in original recording order so later
    // execute-time verification observes the correct buffer contents.
    for (const ActionCommand& action : recording.action_command_list)
    {
        if (const auto* event = std::get_if<BufferWriteEvent>(&action))
        {
            ApplyBufferWriteAction(*event);
        }
        else if (const auto* build_action = std::get_if<BuildAccelerationStructureAction>(&action))
        {
            ApplyBuildAccelerationStructureAction(*build_action);
        }
    }
}

void VulkanSpirvTrackModifier::ApplyBufferWriteAction(const BufferWriteEvent& event)
{
    BufferInfo& dst_info = buffer_entries_.at(event.buffer);
    switch (event.sourceType)
    {
        case SourceType::CopyBuffer:
        {
            const BufferInfo& src_info = buffer_entries_.at(event.srcBuffer);
            for (const auto& region : event.regions)
            {
                GFXRECON_ASSERT(dst_info.size >= region.dstOffset + region.size);
                GFXRECON_ASSERT(src_info.size >= region.srcOffset + region.size);
                std::memcpy(
                    dst_info.data.data() + region.dstOffset, src_info.data.data() + region.srcOffset, region.size);
            }

            // Normalize this copy command into propagated root writes
            auto writes =
                provenance_tracker_.BuildRootWritesFromCopyRegions(event.srcBuffer, event.buffer, event.regions);
            for (const auto& write : writes)
            {
                // flatten each root write into the destination buffer provenance state
                provenance_tracker_.FlattenBufferProvenanceForRootWrite(write);
            }
            break;
        }
        case SourceType::Update:
        {
            VkDeviceSize offset = event.regions[0].dstOffset;
            VkDeviceSize size   = event.regions[0].size;
            GFXRECON_ASSERT(dst_info.size >= offset + size);
            std::memcpy(dst_info.data.data() + offset, event.srcPointer.data() + event.regions[0].srcOffset, size);

            // CmdUpdateBuffer is itself a root source, so flatten the same
            // byte range into provenance state at its execute-time write point.
            RootWriteToBufferRange write = {
                event.buffer, offset, size, ProvenanceRootType::UpdateBuffer, ProvenanceRootKey{ event.call_index, 0 }
            };
            provenance_tracker_.FlattenBufferProvenanceForRootWrite(write);
            break;
        }
        case SourceType::Fill:
        {
            VkDeviceSize offset  = event.regions[0].dstOffset;
            VkDeviceSize size    = event.regions[0].size;
            uint8_t*     dst_ptr = dst_info.data.data() + offset;
            if (size == VK_WHOLE_SIZE)
            {
                size = dst_info.size - offset;
            }
            for (uint32_t i = 0; i < size / 4; i++)
            {
                std::memcpy(dst_ptr + i * 4, event.srcPointer.data(), sizeof(uint32_t));
            }

            // vkCmdFillBuffer overwrites bytes with an immediate pattern, not a rewriteable root payload.
            // Drop any old provenance covering the filled range so later resolves do not point back to stale roots.
            provenance_tracker_.ClearBufferProvenanceRange(event.buffer, offset, size);
            break;
        }
        default:
            break;
    }
}

void VulkanSpirvTrackModifier::ApplyBuildAccelerationStructureAction(const BuildAccelerationStructureAction& action)
{
    std::vector<VerifiedBuildAsUseSite> local_verified_use_sites;
    local_verified_use_sites.reserve(action.candidate_use_sites.size());

    for (const BufferUseSiteKey& candidate : action.candidate_use_sites)
    {
        VkDeviceAddress value       = 0;
        const auto      buffer_iter = buffer_entries_.find(candidate.buffer);
        if (buffer_iter == buffer_entries_.end() ||
            candidate.offset_in_buffer + sizeof(VkDeviceAddress) > buffer_iter->second.size)
        {
            RejectedBuildAsUseSite rejected = { action.call_index,
                                                action.info_index,
                                                action.dst_as,
                                                candidate,
                                                value,
                                                RejectedDeviceAddressUseSiteReason::UnknownDeviceAddress };
            rejected_build_as_use_sites_.push_back(rejected);
            if (m_verbose)
            {
                GFXRECON_LOG_DEBUG("Phase2 rejected BuildAS use-site: call=%llu info=%u dst_as=%llu buffer=%llu "
                                   "offset=%llu value=0x%llx reason=%s",
                                   rejected.build_call_index,
                                   rejected.info_index,
                                   rejected.dst_as,
                                   rejected.use_site.buffer,
                                   rejected.use_site.offset_in_buffer,
                                   static_cast<unsigned long long>(rejected.value),
                                   RejectedDeviceAddressUseSiteReasonToString(rejected.reason));
            }
            continue;
        }

        // Read the current accelerationStructureReference value from the
        // execute-time buffer shadow state at this candidate slot location.
        const BufferInfo& instance_buffer = buffer_iter->second;
        std::memcpy(&value, instance_buffer.data.data() + candidate.offset_in_buffer, sizeof(VkDeviceAddress));

        // verify if the value of current candidate is a live BLAS device address.
        format::HandleId                   referenced_as              = 0;
        VkDeviceAddress                    referenced_as_base_address = 0;
        VkDeviceSize                       referenced_as_size         = 0;
        RejectedDeviceAddressUseSiteReason reject_reason = RejectedDeviceAddressUseSiteReason::UnknownDeviceAddress;
        if (VerifyLiveAccelerationStructureDeviceAddress(value,
                                                         VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                                                         action.call_index,
                                                         referenced_as,
                                                         referenced_as_base_address,
                                                         referenced_as_size,
                                                         reject_reason))
        {
            VerifiedBuildAsUseSite verified = {
                action.call_index, action.info_index,          action.dst_as,     candidate, value,
                referenced_as,     referenced_as_base_address, referenced_as_size
            };
            local_verified_use_sites.push_back(verified);
        }
        else
        {
            RejectedBuildAsUseSite rejected = { action.call_index, action.info_index, action.dst_as, candidate, value,
                                                reject_reason };
            rejected_build_as_use_sites_.push_back(rejected);
            if (m_verbose)
            {
                GFXRECON_LOG_DEBUG("Phase2 rejected BuildAS use-site: call=%llu info=%u dst_as=%llu buffer=%llu "
                                   "offset=%llu value=0x%llx reason=%s",
                                   rejected.build_call_index,
                                   rejected.info_index,
                                   rejected.dst_as,
                                   rejected.use_site.buffer,
                                   rejected.use_site.offset_in_buffer,
                                   static_cast<unsigned long long>(rejected.value),
                                   RejectedDeviceAddressUseSiteReasonToString(rejected.reason));
            }
        }
    }

    // resolve only the verified use-sites produced by this action.
    for (const VerifiedBuildAsUseSite& verified : local_verified_use_sites)
    {
        ResolvedRootRange                    resolved_root = {};
        UnresolvedDeviceAddressUseSiteReason unresolved_reason =
            UnresolvedDeviceAddressUseSiteReason::MissingBufferProvenance;
        if (provenance_tracker_.ResolveBufferRangeToRoot(verified.use_site.buffer,
                                                         verified.use_site.offset_in_buffer,
                                                         sizeof(VkDeviceAddress),
                                                         resolved_root,
                                                         unresolved_reason))
        {
            resolved_build_as_use_sites_.push_back({ verified, resolved_root });
        }
        else
        {
            unresolved_build_as_use_sites_.push_back({ verified, unresolved_reason });
            if (m_verbose)
            {
                GFXRECON_LOG_DEBUG(
                    "unresolved BuildAS use-site: call=%llu info=%u dst_as=%llu inst_buffer=%llu offset=%llu reason=%s",
                    verified.build_call_index,
                    verified.info_index,
                    verified.dst_as,
                    verified.use_site.buffer,
                    verified.use_site.offset_in_buffer,
                    UnresolvedDeviceAddressUseSiteReasonToString(unresolved_reason));
            }
        }
    }

    verified_build_as_use_sites_.insert(
        verified_build_as_use_sites_.end(), local_verified_use_sites.begin(), local_verified_use_sites.end());
}

bool VulkanSpirvTrackModifier::IsPipelineLayoutCompatibleForSet(format::HandleId lhs,
                                                                format::HandleId rhs,
                                                                uint32_t         set) const
{
    if (lhs == rhs)
    {
        return true;
    }

    const auto lhs_iter = pipeline_layout_entries_.find(lhs);
    const auto rhs_iter = pipeline_layout_entries_.find(rhs);
    if (lhs_iter == pipeline_layout_entries_.end() || rhs_iter == pipeline_layout_entries_.end())
    {
        return false;
    }

    const auto& lhs_info = lhs_iter->second;
    const auto& rhs_info = rhs_iter->second;

    // Push constant compatibility is order-insensitive, but still requires identical ranges.
    if (lhs_info.pushConstantRanges.size() != rhs_info.pushConstantRanges.size())
    {
        return false;
    }

    auto lhs_push_constant_ranges    = lhs_info.pushConstantRanges;
    auto rhs_push_constant_ranges    = rhs_info.pushConstantRanges;
    auto compare_push_constant_range = [](const VkPushConstantRange& lhs_range, const VkPushConstantRange& rhs_range) {
        if (lhs_range.offset != rhs_range.offset)
        {
            return lhs_range.offset < rhs_range.offset;
        }
        if (lhs_range.size != rhs_range.size)
        {
            return lhs_range.size < rhs_range.size;
        }
        return lhs_range.stageFlags < rhs_range.stageFlags;
    };
    std::sort(lhs_push_constant_ranges.begin(), lhs_push_constant_ranges.end(), compare_push_constant_range);
    std::sort(rhs_push_constant_ranges.begin(), rhs_push_constant_ranges.end(), compare_push_constant_range);

    for (size_t i = 0; i < lhs_push_constant_ranges.size(); ++i)
    {
        const VkPushConstantRange& lhs_range = lhs_push_constant_ranges[i];
        const VkPushConstantRange& rhs_range = rhs_push_constant_ranges[i];
        if (lhs_range.stageFlags != rhs_range.stageFlags || lhs_range.offset != rhs_range.offset ||
            lhs_range.size != rhs_range.size)
        {
            return false;
        }
    }

    // create independent_sets flag compatible
    const VkPipelineLayoutCreateFlags independent_sets_flag = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;
    if ((lhs_info.flags & independent_sets_flag) != (rhs_info.flags & independent_sets_flag))
    {
        return false;
    }

    // setlayout compatible
    if (lhs_info.setLayouts.size() <= set || rhs_info.setLayouts.size() <= set)
    {
        return false;
    }

    for (uint32_t index = 0; index <= set; ++index)
    {
        const auto lhs_set_layout_iter = set_layout_entries_.find(lhs_info.setLayouts[index]);
        const auto rhs_set_layout_iter = set_layout_entries_.find(rhs_info.setLayouts[index]);
        if (lhs_set_layout_iter == set_layout_entries_.end() || rhs_set_layout_iter == set_layout_entries_.end())
        {
            return false;
        }

        const auto& lhs_set_layout_info = lhs_set_layout_iter->second;
        const auto& rhs_set_layout_info = rhs_set_layout_iter->second;
        if (lhs_set_layout_info.flags != rhs_set_layout_info.flags ||
            lhs_set_layout_info.bindings.size() != rhs_set_layout_info.bindings.size())
        {
            return false;
        }

        auto lhs_binding_iter = lhs_set_layout_info.bindings.begin();
        auto rhs_binding_iter = rhs_set_layout_info.bindings.begin();
        while (lhs_binding_iter != lhs_set_layout_info.bindings.end())
        {
            const Binding& lhs_binding = lhs_binding_iter->second;
            const Binding& rhs_binding = rhs_binding_iter->second;
            if (lhs_binding.binding != rhs_binding.binding || lhs_binding.type != rhs_binding.type ||
                lhs_binding.mutable_descriptor_types != rhs_binding.mutable_descriptor_types ||
                lhs_binding.descriptorCount != rhs_binding.descriptorCount ||
                lhs_binding.stageFlags != rhs_binding.stageFlags ||
                lhs_binding.binding_flags != rhs_binding.binding_flags)
            {
                return false;
            }

            ++lhs_binding_iter;
            ++rhs_binding_iter;
        }
    }

    return true;
}

void VulkanSpirvTrackModifier::ApplyDescriptorStateCommands(format::HandleId              commandBuffer,
                                                            const CommandBufferRecording& recording)
{
    CommandBufferState& current_command_buffer_state = command_buffer_state[commandBuffer];

    auto clear_descriptor_set_backend_state = [](BindPointState& state) {
        state.descriptor_set_map.clear();
        state.dynamic_offsets.clear();
        state.dynamic_offsets_perSet.clear();
        state.dynamic_offsets_count.clear();
    };

    auto clear_descriptor_buffer_backend_state = [](BindPointState& state) {
        state.descriptor_buffer_set_offset_map.clear();
        // TODO: clear descriptor heap backend state when it is introduced.
    };

    auto erase_descriptor_set_binding = [](BindPointState& state, uint32_t set) {
        state.descriptor_set_map.erase(set);
        state.dynamic_offsets_perSet.erase(set);
        state.dynamic_offsets_count.erase(set);
    };

    auto erase_descriptor_buffer_set_offset = [](BindPointState& state, uint32_t set) {
        state.descriptor_buffer_set_offset_map.erase(set);
    };

    for (const DescriptorStateCommand& descriptor_command : recording.descriptor_state_commands)
    {
        switch (descriptor_command.type)
        {
            case DescriptorStateCommand::Type::BindDescriptorSets:
            {
                auto& current_bind_point_state =
                    current_command_buffer_state.bind_point_state[descriptor_command.bind_point];

                if (current_bind_point_state.descriptor_backend_mode != DescriptorBackendMode::DescriptorSet)
                {
                    clear_descriptor_buffer_backend_state(current_bind_point_state);
                    // TODO: clear descriptor heap backend state when it is introduced.
                }
                current_bind_point_state.descriptor_backend_mode = DescriptorBackendMode::DescriptorSet;

                for (const auto& map : descriptor_command.descriptor_sets)
                {
                    // check compatible for each set M < map.set
                    std::vector<uint32_t> disturbed_lower_sets;
                    for (const auto& bound_set_iter : current_bind_point_state.descriptor_set_map)
                    {
                        if (bound_set_iter.first < map.set &&
                            !IsPipelineLayoutCompatibleForSet(
                                bound_set_iter.second.layout, map.layout, bound_set_iter.first))
                        {
                            disturbed_lower_sets.push_back(bound_set_iter.first);
                        }
                    }
                    for (uint32_t disturbed_set : disturbed_lower_sets)
                    {
                        erase_descriptor_set_binding(current_bind_point_state, disturbed_set);
                    }

                    // check compatible for map.set
                    const auto current_set_iter = current_bind_point_state.descriptor_set_map.find(map.set);
                    if (current_set_iter != current_bind_point_state.descriptor_set_map.end() &&
                        !IsPipelineLayoutCompatibleForSet(current_set_iter->second.layout, map.layout, map.set))
                    {
                        std::vector<uint32_t> disturbed_higher_sets;
                        for (const auto& bound_set_iter : current_bind_point_state.descriptor_set_map)
                        {
                            if (bound_set_iter.first > map.set)
                            {
                                disturbed_higher_sets.push_back(bound_set_iter.first);
                            }
                        }
                        for (uint32_t disturbed_set : disturbed_higher_sets)
                        {
                            erase_descriptor_set_binding(current_bind_point_state, disturbed_set);
                        }
                    }

                    // set current map.set
                    erase_descriptor_set_binding(current_bind_point_state, map.set);
                    current_bind_point_state.descriptor_set_map[map.set] = { map.descriptor_set, map.layout };

                    const auto dynamic_offset_iter = descriptor_command.dynamic_offsets.find(map.set);
                    if (dynamic_offset_iter != descriptor_command.dynamic_offsets.end())
                    {
                        std::vector<uint32_t> offsets;
                        for (const auto& offset_iter : dynamic_offset_iter->second.binding_offsets)
                        {
                            auto pos = offsets.end();
                            offsets.insert(pos, offset_iter.second.begin(), offset_iter.second.end());
                            current_bind_point_state.dynamic_offsets_count[map.set][offset_iter.first] =
                                offset_iter.second.size();
                        }
                        current_bind_point_state.dynamic_offsets_perSet[map.set] = std::move(offsets);
                    }
                }

                current_bind_point_state.dynamic_offsets.clear();
                for (const auto& it : current_bind_point_state.dynamic_offsets_perSet)
                {
                    auto pos = current_bind_point_state.dynamic_offsets.end();
                    current_bind_point_state.dynamic_offsets.insert(pos, it.second.begin(), it.second.end());
                }
                break;
            }
            case DescriptorStateCommand::Type::BindDescriptorBuffers:
            {
                current_command_buffer_state.descriptor_buffers = descriptor_command.descriptor_buffers;

                for (auto& bind_point_state_iter : current_command_buffer_state.bind_point_state)
                {
                    bind_point_state_iter.second.descriptor_buffer_set_offset_map.clear();
                }
                break;
            }
            case DescriptorStateCommand::Type::SetDescriptorBufferOffsets:
            {
                auto& current_bind_point_state =
                    current_command_buffer_state.bind_point_state[descriptor_command.bind_point];

                if (current_bind_point_state.descriptor_backend_mode != DescriptorBackendMode::DescriptorBuffer)
                {
                    clear_descriptor_set_backend_state(current_bind_point_state);
                    // TODO: clear descriptor heap backend state when it is introduced.
                }
                current_bind_point_state.descriptor_backend_mode = DescriptorBackendMode::DescriptorBuffer;

                for (const DescriptorBufferOffsetMap& map : descriptor_command.descriptor_buffer_offsets)
                {
                    // check compatible for each set M < map.set
                    std::vector<uint32_t> disturbed_lower_sets;
                    for (const auto& bound_set_iter : current_bind_point_state.descriptor_buffer_set_offset_map)
                    {
                        if (bound_set_iter.first < map.set &&
                            !IsPipelineLayoutCompatibleForSet(
                                bound_set_iter.second.layout, map.layout, bound_set_iter.first))
                        {
                            disturbed_lower_sets.push_back(bound_set_iter.first);
                        }
                    }
                    for (uint32_t disturbed_set : disturbed_lower_sets)
                    {
                        erase_descriptor_buffer_set_offset(current_bind_point_state, disturbed_set);
                    }

                    // check compatible for map.set
                    const auto current_set_iter =
                        current_bind_point_state.descriptor_buffer_set_offset_map.find(map.set);
                    if (current_set_iter != current_bind_point_state.descriptor_buffer_set_offset_map.end() &&
                        !IsPipelineLayoutCompatibleForSet(current_set_iter->second.layout, map.layout, map.set))
                    {
                        std::vector<uint32_t> disturbed_higher_sets;
                        for (const auto& bound_set_iter : current_bind_point_state.descriptor_buffer_set_offset_map)
                        {
                            if (bound_set_iter.first > map.set)
                            {
                                disturbed_higher_sets.push_back(bound_set_iter.first);
                            }
                        }
                        for (uint32_t disturbed_set : disturbed_higher_sets)
                        {
                            erase_descriptor_buffer_set_offset(current_bind_point_state, disturbed_set);
                        }
                    }

                    GFXRECON_ASSERT(map.buffer_index < current_command_buffer_state.descriptor_buffers.size());

                    // set current map.set
                    const auto& buffer_info = current_command_buffer_state.descriptor_buffers[map.buffer_index];
                    erase_descriptor_buffer_set_offset(current_bind_point_state, map.set);
                    if (!buffer_info.resolved)
                    {
                        continue;
                    }
                    current_bind_point_state.descriptor_buffer_set_offset_map[map.set] = {
                        buffer_info.handle, buffer_info.base_address, buffer_info.address, map.offset, map.layout
                    };
                }
                break;
            }
            default:
                break;
        }
    }
}

void VulkanSpirvTrackModifier::executeCommandBuffer(format::HandleId commandBuffer)
{
    if (command_buffer_submit_recordings.find(commandBuffer) == command_buffer_submit_recordings.end())
    {
        command_buffer_recording.erase(commandBuffer);
        command_buffer_entries_.at(commandBuffer).state = CommandBufferLifeCycle::Executable;
        return;
    }

    for (const CommandBufferRecording& recording : command_buffer_submit_recordings[commandBuffer])
    {
        ApplyActionCommands(recording);

        // update commandBuffer state
        CommandBufferState& current_command_buffer_state = command_buffer_state[commandBuffer];
        current_command_buffer_state.command_buffer      = commandBuffer;

        for (const PushConstantData& pushconstant : recording.push_constants)
        {
            std::memcpy(current_command_buffer_state.push_constant.data() + pushconstant.offset,
                        pushconstant.pValues.data(),
                        pushconstant.size);

            // Mirror the same byte write into tracker-owned push-constant provenance: the
            // CommandBufferState::push_constant range[pushconstant.offset, pushconstant.offset + pushconstant.size)
            // now comes from this vkCmdPushConstants call's pValues [0, pushconstant.size).
            provenance_tracker_.FlattenPushConstantProvenanceForWrite(
                commandBuffer, pushconstant.offset, pushconstant.size, ProvenanceRootKey{ pushconstant.call_index, 0 });
        }

        for (const auto& pipeline_iter : recording.pipelines)
        {
            current_command_buffer_state.bind_point_state[pipeline_iter.first].pipeline = pipeline_iter.second;
            current_command_buffer_state.bind_point_state[pipeline_iter.first].pipeline_layout =
                pipeline_entries_[pipeline_iter.second].layout;
        }

        ApplyDescriptorStateCommands(commandBuffer, recording);

        // execute dispatch,draw
        if (recording.bind_point == VK_PIPELINE_BIND_POINT_COMPUTE ||
            recording.bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS)
        {
            executeDispatchDraw(commandBuffer, recording.bind_point);
        }
    }
    command_buffer_submit_recordings.erase(commandBuffer);
    command_buffer_entries_.at(commandBuffer).state = CommandBufferLifeCycle::Executable;
}

void VulkanSpirvTrackModifier::executeDispatchDraw(format::HandleId commandBuffer, VkPipelineBindPoint bindPoint)
{
    const BindPointState& bind_point_state = command_buffer_state[commandBuffer].bind_point_state.at(bindPoint);

    auto is_dynamic_buffer_descriptor_type = [](VkDescriptorType descriptor_type) {
        return descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
               descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    };

    auto is_static_buffer_descriptor_type = [](VkDescriptorType descriptor_type) {
        return descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
               descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    };

    auto get_active_descriptor_type = [](const DescriptorArray& descriptor_array, const DescriptorEntry& desc_entry) {
        return (descriptor_array.type == VK_DESCRIPTOR_TYPE_MUTABLE_EXT) ? desc_entry.type : descriptor_array.type;
    };

    auto is_buffer_back_descriptor_binding = [&](const DescriptorArray& descriptor_array) {
        return is_static_buffer_descriptor_type(descriptor_array.type) ||
               is_dynamic_buffer_descriptor_type(descriptor_array.type) ||
               descriptor_array.type == VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
    };

    // At the sim_data.bindings boundary, gfxr currently collapses several "no usable payload pointer"
    // cases to nullptr, including unbound / never-written slots, null descriptors and tracked resources
    // that cannot be resolved to host shadow data.
    // The current simulator integration does not distinguish these cases at the sim_data.bindings boundary.
    // A nullptr is treated as "payload unavailable" and is intentionally conservative for the current data-flow
    // behavior.
    auto resolve_bound_buffer_descriptor_block = [&](uint32_t               set,
                                                     const DescriptorArray& descriptor_array,
                                                     uint32_t               array_index,
                                                     const DescriptorEntry& desc_entry,
                                                     uint32_t dynamic_offset_base) -> ResolvedDescriptorPayloadBlock {
        ResolvedDescriptorPayloadBlock resolved_block = { nullptr, format::kNullHandleId, 0, 0 };

        const VkDescriptorType active_type = get_active_descriptor_type(descriptor_array, desc_entry);
        if (!is_static_buffer_descriptor_type(active_type) && !is_dynamic_buffer_descriptor_type(active_type))
        {
            return resolved_block;
        }
        if (desc_entry.buffer.handle == format::kNullHandleId)
        {
            return resolved_block;
        }

        const auto buffer_iter = buffer_entries_.find(desc_entry.buffer.handle);
        if (buffer_iter == buffer_entries_.end())
        {
            return resolved_block;
        }

        auto&        buffer_info             = buffer_iter->second;
        VkDeviceSize resolved_dynamic_offset = 0;
        if (is_dynamic_buffer_descriptor_type(descriptor_array.type))
        {
            const auto dynamic_offset_iter = bind_point_state.dynamic_offsets_perSet.find(set);
            if (dynamic_offset_iter == bind_point_state.dynamic_offsets_perSet.end() ||
                dynamic_offset_base + array_index >= dynamic_offset_iter->second.size())
            {
                return resolved_block;
            }
            resolved_dynamic_offset = dynamic_offset_iter->second[dynamic_offset_base + array_index];
        }

        if (desc_entry.buffer.offset > buffer_info.size ||
            resolved_dynamic_offset > (buffer_info.size - desc_entry.buffer.offset))
        {
            return resolved_block;
        }

        resolved_block.block_base_offset_in_buffer = desc_entry.buffer.offset + resolved_dynamic_offset;
        if (resolved_block.block_base_offset_in_buffer >= buffer_info.size)
        {
            return resolved_block;
        }

        const VkDeviceSize remaining_size = buffer_info.size - resolved_block.block_base_offset_in_buffer;
        resolved_block.block_size         = (desc_entry.buffer.range == VK_WHOLE_SIZE)
                                                ? remaining_size
                                                : std::min<VkDeviceSize>(desc_entry.buffer.range, remaining_size);
        if (resolved_block.block_size == 0)
        {
            return resolved_block;
        }

        resolved_block.binding_ptr = buffer_info.data.data() + resolved_block.block_base_offset_in_buffer;
        resolved_block.buffer      = desc_entry.buffer.handle;
        return resolved_block;
    };

    std::string str = "Not support bindPoint";
    if (bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        str = "CmdDispatch";
    }
    else if (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
        str = "CmdDraw";
    }
    if (m_verbose)
        GFXRECON_LOG_INFO(
            " ---- %llu: Pipeline %llu Execute %s ----", global_draw_index++, bind_point_state.pipeline, str.c_str());

    const auto& pipeline_info        = pipeline_entries_.at(bind_point_state.pipeline);
    const auto  pipeline_layout_iter = pipeline_layout_entries_.find(bind_point_state.pipeline_layout);

    auto is_set_slot_accessible = [&](uint32_t set, format::HandleId bound_layout) {
        return pipeline_layout_iter != pipeline_layout_entries_.end() &&
               IsPipelineLayoutCompatibleForSet(bound_layout, bind_point_state.pipeline_layout, set);
    };

    SPIRVSimulator::SimulationData sim_data;
    DispatchBlockInfos             block_infos;

    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::vector<void*>>> sim_binding_pointer_tables;

    if (bind_point_state.descriptor_backend_mode == DescriptorBackendMode::DescriptorSet)
    {
        // set up descriptors through descriptorSet
        for (const auto& set_map_iter : bind_point_state.descriptor_set_map)
        {
            uint32_t set = set_map_iter.first;
            if (!is_set_slot_accessible(set, set_map_iter.second.layout))
            {
                if (m_verbose)
                {
                    GFXRECON_LOG_INFO("Set[%u]: skipped, not compatible with current pipeline layout.", set);
                }
                continue;
            }

            const auto& descriptor_set_info  = descriptor_set_entries_.at(set_map_iter.second.descriptor_set);
            uint32_t    dynamic_offset_index = 0;

            for (const auto& binding_iter : descriptor_set_info.binding_descriptor_array)
            {
                uint64_t    binding          = binding_iter.first;
                const auto& descriptor_array = binding_iter.second;

                uint32_t alloc_descriptor_count = descriptor_array.descriptorCount;
                if (is_buffer_back_descriptor_binding(descriptor_array))
                {
                    const uint32_t dynamic_offset_base = dynamic_offset_index;
                    if (is_dynamic_buffer_descriptor_type(descriptor_array.type))
                    {
                        dynamic_offset_index += alloc_descriptor_count;
                    }

                    if (alloc_descriptor_count == 1)
                    {
                        ResolvedDescriptorPayloadBlock resolved_block;
                        const auto                     desc_entry_iter = descriptor_array.descriptor_entries.find(0);
                        if (desc_entry_iter != descriptor_array.descriptor_entries.end())
                        {
                            resolved_block = resolve_bound_buffer_descriptor_block(
                                set, descriptor_array, 0, desc_entry_iter->second, dynamic_offset_base);
                        }
                        sim_data.bindings[set][binding] = resolved_block.binding_ptr;
                        if (resolved_block.binding_ptr != nullptr && resolved_block.block_size != 0)
                        {
                            block_infos[resolved_block.binding_ptr] = { resolved_block.buffer,
                                                                        resolved_block.block_base_offset_in_buffer,
                                                                        resolved_block.block_size };
                        }
                    }
                    else if (alloc_descriptor_count > 1)
                    {
                        auto& pointer_array = sim_binding_pointer_tables[set][binding];
                        pointer_array.assign(alloc_descriptor_count, nullptr);
                        for (const auto& entry_iter : descriptor_array.descriptor_entries)
                        {
                            if (entry_iter.first >= alloc_descriptor_count)
                            {
                                continue;
                            }

                            ResolvedDescriptorPayloadBlock resolved_block = resolve_bound_buffer_descriptor_block(
                                set, descriptor_array, entry_iter.first, entry_iter.second, dynamic_offset_base);
                            pointer_array[entry_iter.first] = resolved_block.binding_ptr;
                            if (resolved_block.binding_ptr != nullptr && resolved_block.block_size != 0)
                            {
                                block_infos[resolved_block.binding_ptr] = {
                                    resolved_block.buffer,
                                    resolved_block.block_base_offset_in_buffer,
                                    resolved_block.block_size,
                                };
                            }
                        }
                        sim_data.bindings[set][binding] = pointer_array.data();
                    }
                }
                else if (descriptor_array.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
                {
                    uint8_t* binding_ptr = nullptr;
                    if (!descriptor_array.inline_uniform_block_data.empty())
                    {
                        binding_ptr = const_cast<uint8_t*>(descriptor_array.inline_uniform_block_data.data());
                    }
                    sim_data.bindings[set][binding] = binding_ptr;
                }
                else if (descriptor_array.type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                {}
                else
                {}
            }
        }
    }

    if (bind_point_state.descriptor_backend_mode == DescriptorBackendMode::DescriptorBuffer)
    {
        // get binding offset within set offset
        for (const auto& map_iter : bind_point_state.descriptor_buffer_set_offset_map)
        {
            uint32_t   set        = map_iter.first;
            uint64_t   binding    = 0;
            const auto set_offset = map_iter.second;

            if (!is_set_slot_accessible(set, set_offset.layout))
            {
                if (m_verbose)
                {
                    GFXRECON_LOG_INFO("Set[%u]: skipped, not compatible with current pipeline layout.", set);
                }
                continue;
            }

            const auto& pipeline_layout_info = pipeline_layout_entries_.at(set_offset.layout);
            const auto& set_layout_info      = set_layout_entries_.at(pipeline_layout_info.setLayouts[set]);
            GFXRECON_ASSERT(set_layout_info.flags == VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

            for (const auto& binding_iter : set_layout_info.bindings)
            {
                binding = binding_iter.first;
                // TODO: handle arrayElement if the more elements in descriptor array needed.
                VkDeviceSize binding_offset =
                    (set_offset.address - set_offset.base_address) + set_offset.offset + binding_iter.second.offset;
                const auto& buffer_info = buffer_entries_.at(set_offset.handle);

                uint8_t* data_host_pointer      = (uint8_t*)buffer_info.data.data() + binding_offset;
                sim_data.bindings[set][binding] = data_host_pointer;
                // sim_data.descriptor_candidates[data_host_pointer].emplace_back({});
                if (binding_iter.second.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                    binding_iter.second.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                {}
                else if (binding_iter.second.type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
                {}
            }
        }
    }

    // physical_address_buffers
    for (const auto& it : buffer_device_addresses_)
    {
        const auto& buffer_info      = buffer_entries_.at(it.second);
        const auto* host_ptr         = buffer_info.data.data();
        auto*       mutable_host_ptr = const_cast<uint8_t*>(host_ptr);
        sim_data.physical_address_buffers[it.first] =
            std::make_pair(buffer_info.size, static_cast<void*>(mutable_host_ptr));
        if (host_ptr != nullptr && buffer_info.size != 0)
        {
            block_infos[host_ptr] = { it.second, 0, buffer_info.size };
        }
    }

    for (const ShaderStage& stage : pipeline_info.stages)
    {
        if (stage.stageFlagBit != VK_SHADER_STAGE_COMPUTE_BIT && stage.stageFlagBit != VK_SHADER_STAGE_FRAGMENT_BIT &&
            stage.stageFlagBit != VK_SHADER_STAGE_VERTEX_BIT)
        {
            continue;
        }

        sim_data.entry_point_op_name = stage.pName;

        // specialization constant
        sim_data.specialization_constants = stage.specialization_constant.data();
        sim_data.specialization_constant_offsets.clear();
        for (const auto& entry : stage.specialization_offset_entries)
        {
            sim_data.specialization_constant_offsets[entry.first] = entry.second;
        }

        // push constant
        sim_data.push_constants                                               = nullptr;
        std::optional<DispatchPushConstantBlockInfo> push_constant_block_info = std::nullopt;
        if (pipeline_layout_iter != pipeline_layout_entries_.end())
        {
            const auto& pipeline_layout_info = pipeline_layout_entries_.at(bind_point_state.pipeline_layout);
            const auto  merged_range_iter    = pipeline_layout_info.mergedRangePerStage.find(stage.stageFlagBit);
            if (merged_range_iter != pipeline_layout_info.mergedRangePerStage.end())
            {
                const auto& merged_range = merged_range_iter->second;
                sim_data.push_constants =
                    command_buffer_state[commandBuffer].push_constant.data() + merged_range.offset;
                if (merged_range.size != 0)
                {
                    push_constant_block_info = DispatchPushConstantBlockInfo{
                        commandBuffer,
                        sim_data.push_constants,
                        merged_range.offset,
                        merged_range.size,
                    };
                }
            }
        }

        const ShaderModuleInfo& module_info = shader_module_entries_.at(stage.module);

        if (m_verbose)
        {
            GFXRECON_LOG_INFO("------ run simulator for shader %llu: %s ------",
                              stage.module,
                              util::ToString<VkShaderStageFlagBits>(stage.stageFlagBit).c_str());
        }
        SPIRVSimulator::SimulationResults sim_results;
        SPIRVSimulator::SPIRVSimulator    simulator(
            module_info.pCode, &mem_flag_tracker, &sim_data, &sim_results, nullptr, m_verbose, m_flags);
        simulator.Run();
        ApplyDispatchPhysicalAddressResults(sim_results, block_infos, push_constant_block_info);
        outputSimulator(sim_results);
    }
}

void VulkanSpirvTrackModifier::ApplyDispatchPhysicalAddressResults(
    const SPIRVSimulator::SimulationResults&            results,
    const DispatchBlockInfos&                           block_infos,
    const std::optional<DispatchPushConstantBlockInfo>& push_constant_block_info)
{
    std::vector<VerifiedDispatchBufferAddressUseSite> local_verified_use_sites;
    local_verified_use_sites.reserve(results.physical_address_data.size());

    for (const auto& simulator_use_site : results.physical_address_data)
    {
        DispatchUseSiteKey                            use_site = {};
        UnsupportedDispatchBufferAddressUseSiteReason unsupported_reason =
            UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitComponentCount;
        if (!NormalizeDispatchUseSite(
                simulator_use_site, block_infos, push_constant_block_info, use_site, unsupported_reason))
        {
            unsupported_dispatch_buffer_address_use_sites_.push_back({ simulator_use_site, unsupported_reason });
            if (m_verbose)
            {
                GFXRECON_LOG_DEBUG("unsupported dispatch use-site: value=0x%llx reason=%s",
                                   static_cast<unsigned long long>(simulator_use_site.raw_pointer_value),
                                   UnsupportedDispatchBufferAddressUseSiteReasonToString(unsupported_reason));
            }
            continue;
        }

        // verify if the value of raw physical pointer is a buffer device address.
        format::HandleId                   referenced_buffer              = format::kNullHandleId;
        VkDeviceAddress                    referenced_buffer_base_address = 0;
        VkDeviceSize                       referenced_buffer_size         = 0;
        RejectedDeviceAddressUseSiteReason reject_reason = RejectedDeviceAddressUseSiteReason::UnknownDeviceAddress;
        if (!VerifyDispatchBufferDeviceAddress(simulator_use_site.raw_pointer_value,
                                               referenced_buffer,
                                               referenced_buffer_base_address,
                                               referenced_buffer_size,
                                               reject_reason))
        {
            rejected_dispatch_buffer_address_use_sites_.push_back(
                { use_site, simulator_use_site.raw_pointer_value, reject_reason });
            if (m_verbose)
            {
                GFXRECON_LOG_DEBUG("rejected dispatch use-site: value=0x%llx reason=%s",
                                   static_cast<unsigned long long>(simulator_use_site.raw_pointer_value),
                                   RejectedDeviceAddressUseSiteReasonToString(reject_reason));
            }
            continue;
        }

        local_verified_use_sites.push_back({ use_site,
                                             simulator_use_site.raw_pointer_value,
                                             referenced_buffer,
                                             referenced_buffer_base_address,
                                             referenced_buffer_size });
    }

    for (const auto& verified : local_verified_use_sites)
    {
        ResolvedRootRange                    resolved_root     = {};
        UnresolvedDeviceAddressUseSiteReason unresolved_reason = UnresolvedDeviceAddressUseSiteReason::UncoveredRange;

        if (ResolveDispatchUseSiteToRoot(verified.use_site, resolved_root, unresolved_reason))
        {
            resolved_dispatch_buffer_address_use_sites_.push_back({ verified, resolved_root });
        }
        else
        {
            unresolved_dispatch_buffer_address_use_sites_.push_back({ verified, unresolved_reason });
            if (m_verbose)
            {
                GFXRECON_LOG_DEBUG("unresolved dispatch use-site: value=0x%llx reason=%s",
                                   static_cast<unsigned long long>(verified.value),
                                   UnresolvedDeviceAddressUseSiteReasonToString(unresolved_reason));
            }
        }
    }

    verified_dispatch_buffer_address_use_sites_.insert(verified_dispatch_buffer_address_use_sites_.end(),
                                                       local_verified_use_sites.begin(),
                                                       local_verified_use_sites.end());
}

void VulkanSpirvTrackModifier::outputSimulator(const SPIRVSimulator::SimulationResults& results)
{
    auto physical_address_data = results.physical_address_data;

    if (physical_address_data.size() == 0)
        return;

    GFXRECON_LOG_INFO("     >>>>>>>>>>>>> Pointers to pbuffers: >>>>>>>>>>>>>");
    for (const auto& pointer_t : physical_address_data)
    {
        GFXRECON_LOG_INFO("  Found pointer with address: 0x%lx, made from input bit components: ",
                          pointer_t.raw_pointer_value);
        for (auto bit_component : pointer_t.bit_components)
        {
            if (bit_component.location == SPIRVSimulator::BitLocation::Constant)
            {
                GFXRECON_LOG_INFO("    From Constant in SPIRV input words, at Byte Offset: %llu",
                                  bit_component.byte_offset);
            }
            else if (bit_component.location == SPIRVSimulator::BitLocation::SpecConstant)
            {
                GFXRECON_LOG_INFO("    From SpecId: %llu, Byte Offset: %llu, Bitsize: %llu, to val Bit Offset: %llu.",
                                  bit_component.binding_id,
                                  bit_component.byte_offset,
                                  bit_component.bitcount,
                                  bit_component.val_bit_offset);
            }
            else // storageClass
            {
                GFXRECON_LOG_INFO("    From DescriptorSetID: %llu, Binding: %llu, in StorageClass %s, Byte Offset: "
                                  "%llu, Bitsize: %llu, to val Bit Offset: %llu.",
                                  bit_component.set_id,
                                  bit_component.binding_id,
                                  spv::StorageClassToString(bit_component.storage_class),
                                  bit_component.byte_offset,
                                  bit_component.bitcount,
                                  bit_component.val_bit_offset);
            }
        }
    }
    std::cout << "\n" << std::endl;
}

bool VulkanSpirvTrackModifier::NormalizeDispatchUseSite(
    const SPIRVSimulator::PhysicalAddressData&          simulator_use_site,
    const DispatchBlockInfos&                           block_infos,
    const std::optional<DispatchPushConstantBlockInfo>& push_constant_block_info,
    DispatchUseSiteKey&                                 out_use_site,
    UnsupportedDispatchBufferAddressUseSiteReason&      reason) const
{
    if (simulator_use_site.bit_components.size() != 1)
    {
        reason = UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitComponentCount;
        return false;
    }

    const SPIRVSimulator::DataSourceBits& bit_component = simulator_use_site.bit_components[0];
    if (bit_component.location != SPIRVSimulator::BitLocation::StorageClass)
    {
        reason = UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitLocation;
        return false;
    }

    if ((bit_component.bit_offset % 8) != 0 || bit_component.val_bit_offset != 0)
    {
        reason = UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitAlignment;
        return false;
    }

    if (bit_component.bitcount != (sizeof(VkDeviceAddress) * 8))
    {
        reason = UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedBitWidth;
        return false;
    }

    const uint64_t byte_offset_in_block = bit_component.byte_offset + (bit_component.bit_offset / 8);
    const uint64_t query_size           = sizeof(VkDeviceAddress);

    switch (bit_component.storage_class)
    {
        case spv::StorageClass::StorageClassUniform:
        case spv::StorageClass::StorageClassUniformConstant:
        case spv::StorageClass::StorageClassStorageBuffer:
        case spv::StorageClass::StorageClassPhysicalStorageBuffer:
        {
            const auto block_iter = block_infos.find(bit_component.source_ptr);
            if (block_iter == block_infos.end())
            {
                reason = UnsupportedDispatchBufferAddressUseSiteReason::MissingBlockInfo;
                return false;
            }

            const DispatchBlockInfo& block_info = block_iter->second;
            if (byte_offset_in_block > block_info.block_size ||
                query_size > (block_info.block_size - byte_offset_in_block))
            {
                reason = UnsupportedDispatchBufferAddressUseSiteReason::BlockRangeOutOfBounds;
                return false;
            }

            out_use_site =
                BufferUseSiteKey{ block_info.buffer, block_info.block_base_offset_in_buffer + byte_offset_in_block };
            return true;
        }
        case spv::StorageClass::StorageClassPushConstant:
        {
            if (!push_constant_block_info.has_value() || push_constant_block_info->block_ptr == nullptr ||
                push_constant_block_info->command_buffer == format::kNullHandleId ||
                push_constant_block_info->block_ptr != bit_component.source_ptr)
            {
                reason = UnsupportedDispatchBufferAddressUseSiteReason::MissingPushConstantBlockInfo;
                return false;
            }

            if (byte_offset_in_block > push_constant_block_info->block_size ||
                query_size > (push_constant_block_info->block_size - byte_offset_in_block))
            {
                reason = UnsupportedDispatchBufferAddressUseSiteReason::PushConstantRangeOutOfBounds;
                return false;
            }

            out_use_site = DispatchPushConstantUseSiteKey{
                push_constant_block_info->command_buffer,
                push_constant_block_info->block_base_offset_in_push_constants + byte_offset_in_block,
            };
            return true;
        }
        default:
            reason = UnsupportedDispatchBufferAddressUseSiteReason::UnsupportedStorageClass;
            return false;
    }
}

bool VulkanSpirvTrackModifier::VerifyDispatchBufferDeviceAddress(
    VkDeviceAddress                     raw_pointer_value,
    format::HandleId&                   out_referenced_buffer,
    VkDeviceAddress&                    out_referenced_buffer_base_address,
    VkDeviceSize&                       out_referenced_buffer_size,
    RejectedDeviceAddressUseSiteReason& reject_reason) const
{
    out_referenced_buffer              = format::kNullHandleId;
    out_referenced_buffer_base_address = 0;
    out_referenced_buffer_size         = 0;

    if (raw_pointer_value == 0)
    {
        reject_reason = RejectedDeviceAddressUseSiteReason::ZeroValue;
        return false;
    }

    const auto address_iter = findEntryFromBufferDeviceAddress(raw_pointer_value);
    if (address_iter == buffer_device_addresses_.end())
    {
        reject_reason = RejectedDeviceAddressUseSiteReason::UnknownDeviceAddress;
        return false;
    }

    const auto buffer_iter = buffer_entries_.find(address_iter->second);
    if (buffer_iter == buffer_entries_.end())
    {
        reject_reason = RejectedDeviceAddressUseSiteReason::UnknownDeviceAddress;
        return false;
    }

    out_referenced_buffer              = address_iter->second;
    out_referenced_buffer_base_address = address_iter->first;
    out_referenced_buffer_size         = buffer_iter->second.size;
    return true;
}

bool VulkanSpirvTrackModifier::VerifyLiveAccelerationStructureDeviceAddress(
    VkDeviceAddress                     address,
    VkAccelerationStructureTypeKHR      expected_type,
    uint64_t                            build_call_index,
    format::HandleId&                   referenced_as,
    VkDeviceAddress&                    referenced_as_base_address,
    VkDeviceSize&                       referenced_as_size,
    RejectedDeviceAddressUseSiteReason& reject_reason) const
{
    referenced_as              = 0;
    referenced_as_base_address = 0;
    referenced_as_size         = 0;
    if (address == 0)
    {
        reject_reason = RejectedDeviceAddressUseSiteReason::ZeroValue;
        return false;
    }

    const auto address_iter = acceleration_structure_device_addresses_.find(address);
    if (address_iter == acceleration_structure_device_addresses_.end())
    {
        reject_reason = RejectedDeviceAddressUseSiteReason::UnknownDeviceAddress;
        return false;
    }

    for (format::HandleId candidate_as : address_iter->second)
    {
        const auto as_iter = acceleration_structure_entries_.find(candidate_as);
        if (as_iter == acceleration_structure_entries_.end())
        {
            continue;
        }

        const auto& as_info = as_iter->second;
        if (as_info.type != expected_type)
        {
            continue;
        }

        if (!(as_info.creation_index <= build_call_index && build_call_index < as_info.destruction_index))
        {
            continue;
        }

        // AS device addresses are only considered live while the backing buffer is also live.
        const auto buffer_iter = buffer_entries_.find(as_info.buf_handle);
        if (buffer_iter == buffer_entries_.end())
        {
            continue;
        }

        const auto& buffer_info = buffer_iter->second;
        if (!(buffer_info.creation_index <= build_call_index && build_call_index < buffer_info.destruction_index))
        {
            continue;
        }

        referenced_as              = candidate_as;
        referenced_as_base_address = as_info.device_address;
        referenced_as_size         = as_info.size;
        return true;
    }

    reject_reason = RejectedDeviceAddressUseSiteReason::NotLiveAtUseTime;
    return false;
}

bool VulkanSpirvTrackModifier::ResolveDispatchUseSiteToRoot(const DispatchUseSiteKey&             use_site,
                                                            ResolvedRootRange&                    out,
                                                            UnresolvedDeviceAddressUseSiteReason& reason) const
{
    if (const auto* buffer_use_site = std::get_if<BufferUseSiteKey>(&use_site))
    {
        return provenance_tracker_.ResolveBufferRangeToRoot(
            buffer_use_site->buffer, buffer_use_site->offset_in_buffer, sizeof(VkDeviceAddress), out, reason);
    }

    if (const auto* push_constant_use_site = std::get_if<DispatchPushConstantUseSiteKey>(&use_site))
    {
        return provenance_tracker_.ResolvePushConstantRangeToRoot(push_constant_use_site->command_buffer,
                                                                  push_constant_use_site->offset_in_push_constants,
                                                                  sizeof(VkDeviceAddress),
                                                                  out,
                                                                  reason);
    }

    reason = UnresolvedDeviceAddressUseSiteReason::UncoveredRange;
    return false;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)