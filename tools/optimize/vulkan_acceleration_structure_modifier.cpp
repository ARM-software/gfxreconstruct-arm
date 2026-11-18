/*
** Copyright (c) 2025-2026 LunarG, Inc.
** Copyright (c) 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "vulkan_acceleration_structure_modifier.h"

#include <cstdint>
#include <limits>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "format/format.h"
#include "format/format_arm.h"
#include "generated/generated_vulkan_struct_decoders.h"
#include "util/defines.h"
#include "util/logging.h"
#include "encode/parameter_buffer.h"
#include "encode/struct_pointer_encoder.h"
#include "graphics/vulkan_struct_get_pnext.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

void VulkanAccelerationStructureModifier::TrackBufferDeviceAddress(VkDeviceAddress address, format::HandleId buffer_id)
{
    auto buffer_entry = buffer_entries_.find(buffer_id);
    GFXRECON_ASSERT(buffer_entry != buffer_entries_.end());
    if (buffer_entry == buffer_entries_.end())
    {
        return;
    }

    const VkDeviceSize size = buffer_entry->second.size;
    GFXRECON_ASSERT(size <= std::numeric_limits<VkDeviceAddress>::max() - address);
    if ((size == 0) || (size > std::numeric_limits<VkDeviceAddress>::max() - address))
    {
        return;
    }

    buffer_entry->second.device_address = address;
}

void VulkanAccelerationStructureModifier::Process_vkGetBufferDeviceAddress(const ApiCallInfo&            call_info,
                                                                           args::GetBufferDeviceAddress& args)
{
    if (IsModificationPass())
    {
        return;
    }
    TrackBufferDeviceAddress(args.result, args.pInfo.GetMetaStructPointer()->buffer);
}

void VulkanAccelerationStructureModifier::Process_vkGetBufferDeviceAddressKHR(const ApiCallInfo& call_info,
                                                                              args::GetBufferDeviceAddressKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }
    TrackBufferDeviceAddress(args.result, args.pInfo.GetMetaStructPointer()->buffer);
}

void VulkanAccelerationStructureModifier::Process_vkGetBufferDeviceAddressEXT(const ApiCallInfo& call_info,
                                                                              args::GetBufferDeviceAddressEXT& args)
{
    if (IsModificationPass())
    {
        return;
    }
    TrackBufferDeviceAddress(args.result, args.pInfo.GetMetaStructPointer()->buffer);
}

void VulkanAccelerationStructureModifier::Process_vkGetAccelerationStructureDeviceAddressKHR(
    const ApiCallInfo& call_info, args::GetAccelerationStructureDeviceAddressKHR& args)
{
    const auto& as_id = args.pInfo.GetMetaStructPointer()->accelerationStructure;
    if (IsModificationPass())
    {

        if (acceleration_structure_build_infos_.count(as_id) &&
            acceleration_structure_build_infos_[as_id].is_meta_copy &&
            block_index_ < acceleration_structure_build_infos_[as_id].process_compacted_as_index)
        {
            // delete the compacted AS function,
            // it will be inserted new AS function before ProcessVulkanCopyAccelerationStructuresCommand
            SetDeleteCurrentCall();

            gfxrecon::encode::ParameterEncoder encoder(
                &acceleration_structure_build_infos_[as_id].get_address_parameter_buffer);
            encoder.EncodeHandleIdValue(args.device);
            encoder.EncodeStructPtrPreamble(args.pInfo.GetPointer());
            encoder.EncodeEnumValue(args.pInfo.GetPointer()->sType);
            encode::EncodePNextStruct(&encoder, args.pInfo.GetPointer()->pNext);
            encoder.EncodeHandleIdValue(as_id);
            encoder.EncodeHandleIdValue(args.result);
        }
        return;
    }

    acceleration_structure_entries_[as_id].device_address = args.result;
}

void VulkanAccelerationStructureModifier::ProcessAccelerationStructureCompactionDependencyCommand(
    format::HandleId parent, const std::vector<format::HandleId>& children)
{
    if (IsModificationPass())
    {
        // delete the old compaction dependency meta command, it will be inserted new fixed meta command
        SetDeleteCurrentCall();
        return;
    }
}

void VulkanAccelerationStructureModifier::ProcessSetOpaqueAddressCommand(format::HandleId device_id,
                                                                         format::HandleId object_id,
                                                                         uint64_t         address)
{
    if (IsModificationPass())
    {
        if (acceleration_structure_build_infos_.count(object_id) &&
            acceleration_structure_build_infos_[object_id].is_meta_copy &&
            block_index_ < acceleration_structure_build_infos_[object_id].process_compacted_as_index)
        {
            assert(acceleration_structure_entries_.count(object_id) > 0);
            acceleration_structure_entries_[object_id].device_address = address;
            SetDeleteCurrentCall();
        }
        return;
    }
}

void VulkanAccelerationStructureModifier::Process_vkCreateBuffer(const ApiCallInfo& call_info, args::CreateBuffer& args)
{
    if (IsModificationPass())
    {
        return;
    }

    format::HandleId handle                   = *args.pBuffer.GetPointer();
    buffer_entries_[handle].size              = args.pCreateInfo.GetPointer()->size;
    buffer_entries_[handle].creation_index    = call_info.index;
    buffer_entries_[handle].destruction_index = UINT64_MAX;
}

void VulkanAccelerationStructureModifier::Process_vkDestroyBuffer(const ApiCallInfo&   call_info,
                                                                  args::DestroyBuffer& args)
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

void VulkanAccelerationStructureModifier::Process_vkBindBufferMemory(const ApiCallInfo&      call_info,
                                                                     args::BindBufferMemory& args)
{
    if (IsModificationPass())
    {
        return;
    }

    auto buffer_entry = buffer_entries_.find(args.buffer);
    GFXRECON_ASSERT(buffer_entry != buffer_entries_.end());
    GFXRECON_ASSERT(buffer_entry->second.memory_handle_id == 0);
    buffer_entry->second.memory_handle_id = args.memory;
    buffer_entry->second.memory_offset    = args.memoryOffset;
}

void VulkanAccelerationStructureModifier::Process_vkBindBufferMemory2(const ApiCallInfo&       call_info,
                                                                      args::BindBufferMemory2& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkBindBufferMemoryInfo*         bind_infos      = args.pBindInfos.GetPointer();
    const Decoded_VkBindBufferMemoryInfo* bind_meta_infos = args.pBindInfos.GetMetaStructPointer();

    for (uint32_t i = 0; i < args.bindInfoCount; ++i)
    {
        auto buffer_entry = buffer_entries_.find(bind_meta_infos[i].buffer);
        GFXRECON_ASSERT(buffer_entry != buffer_entries_.end());
        GFXRECON_ASSERT(buffer_entry->second.memory_handle_id == 0);
        buffer_entry->second.memory_handle_id = bind_meta_infos[i].memory;
        buffer_entry->second.memory_offset    = bind_infos[i].memoryOffset;
    }
}

void VulkanAccelerationStructureModifier::Process_vkBindBufferMemory2KHR(const ApiCallInfo&          call_info,
                                                                         args::BindBufferMemory2KHR& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const VkBindBufferMemoryInfo*         bind_infos      = args.pBindInfos.GetPointer();
    const Decoded_VkBindBufferMemoryInfo* bind_meta_infos = args.pBindInfos.GetMetaStructPointer();

    for (uint32_t i = 0; i < args.bindInfoCount; ++i)
    {
        auto buffer_entry = buffer_entries_.find(bind_meta_infos[i].buffer);
        GFXRECON_ASSERT(buffer_entry != buffer_entries_.end());
        GFXRECON_ASSERT(buffer_entry->second.memory_handle_id == 0);
        buffer_entry->second.memory_handle_id = bind_meta_infos[i].memory;
        buffer_entry->second.memory_offset    = bind_infos[i].memoryOffset;
    }
}

void VulkanAccelerationStructureModifier::Process_vkCreateAccelerationStructureKHR(
    const ApiCallInfo& call_info, args::CreateAccelerationStructureKHR& args)
{
    ProcessCreateAccelerationStructure(call_info, args);
}

void VulkanAccelerationStructureModifier::Process_vkCreateAccelerationStructure2KHR(
    const ApiCallInfo& call_info, args::CreateAccelerationStructure2KHR& args)
{
    ProcessCreateAccelerationStructure(call_info, args);
}

template <typename CreateArgs>
void VulkanAccelerationStructureModifier::ProcessCreateAccelerationStructure(const ApiCallInfo& call_info,
                                                                             CreateArgs&        args)
{
    if (args.result != VK_SUCCESS)
    {
        return;
    }

    format::HandleId handle = *args.pAccelerationStructure.GetPointer();
    if (!IsModificationPass())
    {
        auto [it, inserted] = acceleration_structure_entries_.try_emplace(handle, AccelerationStructureInfo{});
        VkDeviceSize offset = 0;
        if constexpr (std::is_same_v<CreateArgs, args::CreateAccelerationStructure2KHR>)
        {
            const auto& address_range = args.pCreateInfo.GetPointer()->addressRange;
            auto        buffer_entry  = buffer_entries_.cend();
            for (auto candidate = buffer_entries_.cbegin(); candidate != buffer_entries_.cend(); ++candidate)
            {
                const auto& buffer = candidate->second;
                if (buffer.device_address == 0 || buffer.device_address > address_range.address ||
                    buffer.creation_index > call_info.index || call_info.index >= buffer.destruction_index)
                {
                    continue;
                }

                const VkDeviceAddress offset = address_range.address - buffer.device_address;
                if (offset >= buffer.size || address_range.size > buffer.size - offset)
                {
                    continue;
                }

                if (buffer_entry == buffer_entries_.cend() ||
                    buffer.device_address < buffer_entry->second.device_address ||
                    (buffer.device_address == buffer_entry->second.device_address &&
                     buffer.size < buffer_entry->second.size))
                {
                    buffer_entry = candidate;
                }
            }

            GFXRECON_ASSERT(buffer_entry != buffer_entries_.cend());
            if (buffer_entry == buffer_entries_.cend())
            {
                GFXRECON_LOG_ERROR("Cannot find storage buffer for vkCreateAccelerationStructure2KHR");
                acceleration_structure_entries_.erase(it);
                return;
            }
            it->second.buf_handle     = buffer_entry->first;
            offset                    = address_range.address - buffer_entry->second.device_address;
            it->second.device_address = address_range.address;
        }
        else
        {
            it->second.buf_handle = args.pCreateInfo.GetMetaStructPointer()->buffer;
            offset                = args.pCreateInfo.GetPointer()->offset;
        }
        it->second.type                        = args.pCreateInfo.GetPointer()->type;
        VkDeviceAddress storage_buffer_address = buffer_entries_[it->second.buf_handle].device_address;
        if (storage_buffer_address != 0)
        {
            it->second.device_address = storage_buffer_address + offset;
        }
        return;
    }

    GFXRECON_ASSERT(acceleration_structure_entries_.contains(handle));

    const AccelerationStructureInfo& created_object = acceleration_structure_entries_[handle];

    std::unordered_set<uint64_t> prim_counts_quieried;
    for (const auto& [id, donor_candidate_info] : acceleration_structure_entries_)
    {
        const BufferInfo& buffer_entry_current = buffer_entries_.find(created_object.buf_handle)->second;
        const BufferInfo& buffer_entry_donor   = buffer_entries_.find(donor_candidate_info.buf_handle)->second;

        if (buffer_entry_current.memory_handle_id != buffer_entry_donor.memory_handle_id)
        {
            continue;
        }

        if (created_object.device_address == 0 || donor_candidate_info.device_address == 0)
        {

            if (buffer_entry_current.memory_offset != buffer_entry_donor.memory_offset)
            {
                continue;
            }
        }
        else if (created_object.device_address != donor_candidate_info.device_address)
        {
            continue;
        }

        if (created_object.type != donor_candidate_info.type)
        {
            continue;
        }

        if (buffer_entry_current.creation_index >= buffer_entry_donor.destruction_index)
        {
            continue;
        }

        auto build_info_candidate = acceleration_structure_build_infos_.find(id);
        if (build_info_candidate == acceleration_structure_build_infos_.end())
        {
            continue;
        }

        const std::vector<uint32_t>& prim_counts = build_info_candidate->second.primitive_counts;
        uint32_t                     total       = std::accumulate(prim_counts.begin(), prim_counts.end(), 0);
        if (total > 0 && !prim_counts_quieried.contains(total))
        {
            prim_counts_quieried.insert(total);
            EncodeVkGetAccelerationStructureBuildSizesKHR(args.device, build_info_candidate->second);
        }
    }

    auto build_info = acceleration_structure_build_infos_.find(handle);
    if (build_info == acceleration_structure_build_infos_.end())
    {
        return;
    }

    if (build_info->second.is_first_built)
    {
        EncodeVkGetAccelerationStructureBuildSizesKHR(args.device, build_info->second);
        return;
    }

    if (build_info->second.is_meta_copy && block_index_ < build_info->second.process_compacted_as_index)
    {
        // delete the compacted AS function,
        // it will be inserted new AS function before ProcessVulkanCopyAccelerationStructuresCommand
        SetDeleteCurrentCall();

        auto* pCreateInfoDec = args.pCreateInfo.GetPointer();

        gfxrecon::encode::ParameterEncoder encoder(
            &acceleration_structure_build_infos_[handle].create_parameter_buffer);
        encoder.EncodeHandleIdValue(args.device);
        if constexpr (std::is_same_v<CreateArgs, args::CreateAccelerationStructure2KHR>)
        {
            encode::EncodeStructPtr(&encoder, pCreateInfoDec);
            build_info->second.create_call_id = format::ApiCallId::ApiCall_vkCreateAccelerationStructure2KHR;
        }
        else
        {
            encoder.EncodeStructPtrPreamble(pCreateInfoDec);
            encoder.EncodeEnumValue(pCreateInfoDec->sType);
            encode::EncodePNextStruct(&encoder, pCreateInfoDec->pNext);
            encoder.EncodeFlagsValue(pCreateInfoDec->createFlags);
            encoder.EncodeHandleIdValue(args.pCreateInfo.GetMetaStructPointer()->buffer);
            encoder.EncodeUInt64Value(pCreateInfoDec->offset);
            encoder.EncodeUInt64Value(pCreateInfoDec->size);
            encoder.EncodeEnumValue(pCreateInfoDec->type);
            encoder.EncodeUInt64Value(pCreateInfoDec->deviceAddress);
        }
        encode::EncodeStructPtr(&encoder, args.pAllocator.GetPointer());
        encoder.EncodeHandleIdPtr(args.pAccelerationStructure.GetPointer());
        encoder.EncodeEnumValue(args.result);
        return;
    }

    if (build_info->second.source_of_compaction != format::kNullHandleId)
    {
        format::ParentToChildDependencyHeader header;

        header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
        header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(header) + sizeof(format::HandleId);
        header.meta_header.meta_data_id      = format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_Vulkan,
                                                                 format::MetaDataType::kParentToChildDependency);
        header.thread_id                     = 1;
        header.dependency_type = format::ParentToChildDependencyType::kAccelerationStructureCompactionDependency;
        header.parent_id       = build_info->second.source_of_compaction;
        header.child_count     = 1;

        auto new_call     = CreatePreCall();
        new_call->type    = util::CallModifierBase::NewCallDataType::MetaDataCall;
        new_call->call_id = gfxrecon::format::ApiCallId::ApiCall_Unknown;
        new_call->parameter_buffer.Write(&header, sizeof(header));
        new_call->parameter_buffer.Write(&handle, sizeof(format::HandleId));
        return;
    }
}

void VulkanAccelerationStructureModifier::Process_vkGetAccelerationStructureBuildSizesKHR(
    const ApiCallInfo& call_info, args::GetAccelerationStructureBuildSizesKHR& args)
{
    if (IsModificationPass())
    {
        SetDeleteCurrentCall();
    }
}

void VulkanAccelerationStructureModifier::ProcessBuildAccelerationStructures(
    uint32_t                                                                   infoCount,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   ppBuildRangeInfos)
{
    for (uint32_t info_index = 0; info_index < infoCount; ++info_index)
    {
        const VkAccelerationStructureBuildGeometryInfoKHR& geometry_info = pInfos->GetPointer()[info_index];
        const VkAccelerationStructureBuildRangeInfoKHR*    build_range_info =
            ppBuildRangeInfos->GetPointer() != nullptr ? ppBuildRangeInfos->GetPointer()[info_index] : nullptr;
        const auto&             geometry_meta_info = pInfos->GetMetaStructPointer()[info_index];
        const format::HandleId& dst_as_id          = geometry_meta_info.dstAccelerationStructure;

        auto [build_info, inserted] =
            acceleration_structure_build_infos_.try_emplace(dst_as_id, AccelerationStructureBuildInfo{});

        // Sanity check for OMM KHR
        if (geometry_info.type == VK_ACCELERATION_STRUCTURE_TYPE_OPACITY_MICROMAP_KHR && build_range_info != nullptr)
        {
            GFXRECON_LOG_WARNING_ONCE("Unexpected ppBuildRangeInfos non-null for AS type OPACITY_MICROMAP_KHR");
        }

        // If the Acceleration Structure has already been build, we need update the build info on the need-to basis
        // Also, this approach makes much more sense for TLAS then for BLAS, so we restrict it to what we know should
        // work
        if (!inserted && geometry_info.type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
        {
            uint32_t total_cached = std::accumulate(
                build_info->second.primitive_counts.begin(), build_info->second.primitive_counts.end(), 0);
            uint32_t total_candidate = 0;
            for (uint32_t g = 0; g < geometry_info.geometryCount; ++g)
            {
                total_candidate += build_range_info[g].primitiveCount;
            }

            if (total_candidate < total_cached)
            {
                continue;
            }
        }

        build_info->second                   = {};
        build_info->second.is_first_built    = true;
        build_info->second.info              = geometry_info;
        build_info->second.info.pGeometries  = nullptr;
        build_info->second.info.ppGeometries = nullptr;

        build_info->second.geometries.resize(geometry_info.geometryCount);
        build_info->second.primitive_counts.resize(build_range_info != nullptr ? geometry_info.geometryCount : 0);

        // Keep micromap structures and their usage counts alive until the modification pass.
        for (uint32_t g = 0; g < geometry_info.geometryCount; ++g)
        {
            const auto& geometry_data =
                geometry_info.pGeometries != nullptr ? geometry_info.pGeometries[g] : *geometry_info.ppGeometries[g];
            const auto& geometry_metadata = geometry_info.pGeometries != nullptr
                                                ? geometry_meta_info.pGeometries->GetMetaStructPointer()[g]
                                                : *geometry_meta_info.ppGeometries->GetMetaStructPointer()[g];
            if (geometry_data.sType != VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR)
            {
                continue;
            }

            build_info->second.geometries[g]       = geometry_data;
            build_info->second.geometries[g].pNext = nullptr;
            if (build_range_info != nullptr)
            {
                build_info->second.primitive_counts[g] = build_range_info[g].primitiveCount;
            }

            switch (geometry_data.geometryType)
            {
                case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                {
                    const auto& triangles                                     = geometry_data.geometry.triangles;
                    build_info->second.geometries[g].geometry.triangles.pNext = nullptr;

                    // Sanity check that the trace doesn't use both KHR and EXT micromaps at the same time
                    GFXRECON_ASSERT(
                        (graphics::vulkan_struct_get_pnext<VkAccelerationStructureTrianglesOpacityMicromapKHR>(
                             &triangles) &&
                         graphics::vulkan_struct_get_pnext<VkAccelerationStructureTrianglesOpacityMicromapEXT>(
                             &triangles)) == 0);

                    if (const auto* omm_info_khr =
                            graphics::vulkan_struct_get_pnext<VkAccelerationStructureTrianglesOpacityMicromapKHR>(
                                &triangles))
                    {
                        const auto* meta_omm_info =
                            GetPNextMetaStruct<Decoded_VkAccelerationStructureTrianglesOpacityMicromapKHR>(
                                geometry_metadata.geometry->triangles->pNext);
                        auto& cached_omm_info                     = build_info->second.omm_infos_khr[g];
                        cached_omm_info                           = *omm_info_khr;
                        cached_omm_info.pNext                     = nullptr;
                        build_info->second.geometry_omm_id_map[g] = meta_omm_info->micromap;
                    }
                    else if (const auto* omm_info_ext =
                                 graphics::vulkan_struct_get_pnext<VkAccelerationStructureTrianglesOpacityMicromapEXT>(
                                     &triangles))
                    {
                        const auto* meta_omm_info_ext =
                            GetPNextMetaStruct<Decoded_VkAccelerationStructureTrianglesOpacityMicromapEXT>(
                                geometry_metadata.geometry->triangles->pNext);
                        auto& cached_omm_info                     = build_info->second.omm_infos_ext[g];
                        cached_omm_info                           = *omm_info_ext;
                        cached_omm_info.pNext                     = nullptr;
                        cached_omm_info.pUsageCounts              = nullptr;
                        cached_omm_info.ppUsageCounts             = nullptr;
                        build_info->second.geometry_omm_id_map[g] = meta_omm_info_ext->micromap;

                        auto& usage_infos = build_info->second.usage_infos_ext[g];
                        for (uint32_t usage_index = 0; usage_index < omm_info_ext->usageCountsCount; ++usage_index)
                        {
                            usage_infos.push_back(omm_info_ext->pUsageCounts != nullptr
                                                      ? omm_info_ext->pUsageCounts[usage_index]
                                                      : *omm_info_ext->ppUsageCounts[usage_index]);
                        }
                    }
                    break;
                }
                case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                {
                    break;
                }
                case VK_GEOMETRY_TYPE_MICROMAP_KHR:
                {
                    const auto* micromap_info =
                        graphics::vulkan_struct_get_pnext<VkAccelerationStructureGeometryMicromapDataKHR>(
                            &geometry_data);
                    GFXRECON_ASSERT(micromap_info != nullptr);
                    auto& cached_micromap_info         = build_info->second.micromap_infos_khr[g];
                    cached_micromap_info               = *micromap_info;
                    cached_micromap_info.pNext         = nullptr;
                    cached_micromap_info.pUsageCounts  = nullptr;
                    cached_micromap_info.ppUsageCounts = nullptr;

                    auto& usage_infos = build_info->second.usage_infos_khr[g];
                    for (uint32_t usage_index = 0; usage_index < micromap_info->usageCountsCount; ++usage_index)
                    {
                        usage_infos.push_back(micromap_info->pUsageCounts != nullptr
                                                  ? micromap_info->pUsageCounts[usage_index]
                                                  : *micromap_info->ppUsageCounts[usage_index]);
                    }
                    break;
                }
                case VK_GEOMETRY_TYPE_AABBS_KHR:
                {
                    break;
                }
                default:
                {
                    GFXRECON_LOG_ERROR("Unexpected geometry type");
                    break;
                }
            }
        }
    }
}

void VulkanAccelerationStructureModifier::Process_vkCmdBuildAccelerationStructuresKHR(
    const ApiCallInfo& call_info, args::CmdBuildAccelerationStructuresKHR& args)
{
    GFXRECON_UNREFERENCED_PARAMETER(call_info);

    if (IsModificationPass())
    {
        return;
    }

    ProcessBuildAccelerationStructures(args.infoCount, &args.pInfos, &args.ppBuildRangeInfos);
}

void VulkanAccelerationStructureModifier::ProcessVulkanBuildAccelerationStructuresCommand(
    format::HandleId                                                           device_id,
    uint32_t                                                                   info_count,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* geometry_infos,
    StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   range_infos,
    std::vector<std::vector<VkAccelerationStructureInstanceKHR>>&              instance_buffers_data)
{
    GFXRECON_UNREFERENCED_PARAMETER(instance_buffers_data);

    if (IsModificationPass())
    {
        return;
    }

    ProcessBuildAccelerationStructures(info_count, geometry_infos, range_infos);
}

void VulkanAccelerationStructureModifier::Process_vkCmdCopyAccelerationStructureKHR(
    const ApiCallInfo& call_info, args::CmdCopyAccelerationStructureKHR& args)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto& info = args.pInfo.GetMetaStructPointer();
    if (acceleration_structure_build_infos_.count(info->src) != 0)
    {
        if (acceleration_structure_build_infos_.count(info->dst) == 0)
        {
            if (info->decoded_value->mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR)
            {
                acceleration_structure_build_infos_[info->dst]                      = {};
                acceleration_structure_build_infos_[info->dst].source_of_compaction = info->src;
            }
            else
            {
                acceleration_structure_build_infos_.emplace(
                    std::make_pair(info->dst, acceleration_structure_build_infos_[info->src]));
            }
        }
    }
}

void VulkanAccelerationStructureModifier::ProcessVulkanCopyAccelerationStructuresCommand(
    format::HandleId device_id, StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* copy_infos)
{
    if (IsModificationPass())
    {

        for (uint32_t index = 0; index < copy_infos->GetLength(); ++index)
        {
            format::HandleId dst_id = copy_infos->GetMetaStructPointer()[index].dst;
            if (acceleration_structure_build_infos_.count(dst_id) &&
                acceleration_structure_build_infos_[dst_id].is_meta_copy)
            {
                format::SetOpaqueAddressCommand opaque_address_cmd;
                opaque_address_cmd.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
                opaque_address_cmd.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(opaque_address_cmd);
                opaque_address_cmd.meta_header.meta_data_id      = format::MakeMetaDataId(
                    format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kSetOpaqueAddressCommand);
                opaque_address_cmd.thread_id = 1;
                opaque_address_cmd.device_id = device_id;
                opaque_address_cmd.object_id = dst_id;
                opaque_address_cmd.address   = acceleration_structure_entries_[dst_id].device_address;

                auto new_opaque_address_call       = CreatePreCall();
                new_opaque_address_call->type      = NewCallDataType::MetaDataCall;
                new_opaque_address_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
                new_opaque_address_call->thread_id = 1;
                new_opaque_address_call->parameter_buffer.Write(&opaque_address_cmd, sizeof(opaque_address_cmd));

                format::ParentToChildDependencyHeader dependency_header;
                dependency_header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
                dependency_header.meta_header.block_header.size =
                    format::GetMetaDataBlockBaseSize(dependency_header) + sizeof(format::HandleId);
                dependency_header.meta_header.meta_data_id = format::MakeMetaDataId(
                    format::ApiFamilyId::ApiFamily_Vulkan, format::MetaDataType::kParentToChildDependency);
                dependency_header.thread_id = 1;
                dependency_header.dependency_type =
                    format::ParentToChildDependencyType::kAccelerationStructureCompactionDependency;
                dependency_header.parent_id   = acceleration_structure_build_infos_[dst_id].source_of_compaction;
                dependency_header.child_count = 1;

                auto new_dependency_call       = CreatePreCall();
                new_dependency_call->type      = util::CallModifierBase::NewCallDataType::MetaDataCall;
                new_dependency_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
                new_dependency_call->thread_id = 1;
                new_dependency_call->parameter_buffer.Write(&dependency_header, sizeof(dependency_header));
                new_dependency_call->parameter_buffer.Write(&dst_id, sizeof(format::HandleId));

                auto new_create_as_call       = CreatePreCall();
                new_create_as_call->type      = NewCallDataType::ApiCall;
                new_create_as_call->call_id   = acceleration_structure_build_infos_[dst_id].create_call_id;
                new_create_as_call->thread_id = 1;

                auto& create_buffer = acceleration_structure_build_infos_[dst_id].create_parameter_buffer;
                new_create_as_call->parameter_buffer.Write(create_buffer.GetData(), create_buffer.GetDataSize());

                auto& address_buffer = acceleration_structure_build_infos_[dst_id].get_address_parameter_buffer;
                if (address_buffer.GetDataSize() != 0)
                {
                    auto new_get_address_call  = CreatePreCall();
                    new_get_address_call->type = NewCallDataType::ApiCall;
                    new_get_address_call->call_id =
                        gfxrecon::format::ApiCallId::ApiCall_vkGetAccelerationStructureDeviceAddressKHR;
                    new_get_address_call->thread_id = 1;

                    new_get_address_call->parameter_buffer.Write(address_buffer.GetData(),
                                                                 address_buffer.GetDataSize());
                }

                acceleration_structure_build_infos_[dst_id].is_meta_copy = false;
            }
        }
        return;
    }

    for (uint32_t index = 0; index < copy_infos->GetLength(); ++index)
    {
        format::HandleId                   src_id = copy_infos->GetMetaStructPointer()[index].src;
        format::HandleId                   dst_id = copy_infos->GetMetaStructPointer()[index].dst;
        VkCopyAccelerationStructureModeKHR mode   = copy_infos->GetMetaStructPointer()[index].decoded_value->mode;

        if (acceleration_structure_build_infos_.count(src_id) != 0)
        {
            if (acceleration_structure_build_infos_.count(dst_id) == 0)
            {
                if (mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR)
                {
                    acceleration_structure_build_infos_[dst_id]                            = {};
                    acceleration_structure_build_infos_[dst_id].is_meta_copy               = true;
                    acceleration_structure_build_infos_[dst_id].process_compacted_as_index = block_index_;
                    acceleration_structure_build_infos_[dst_id].source_of_compaction       = src_id;
                }
                else
                {
                    acceleration_structure_build_infos_.emplace(
                        std::make_pair(dst_id, acceleration_structure_build_infos_[src_id]));
                }
            }
        }
    }
}

void VulkanAccelerationStructureModifier::EncodeVkGetAccelerationStructureBuildSizesKHR(
    format::HandleId device, AccelerationStructureBuildInfo& build_info)
{
    VkAccelerationStructureBuildGeometryInfoKHR pBuildInfo = build_info.info;
    pBuildInfo.pGeometries                                 = build_info.geometries.data();
    pBuildInfo.ppGeometries                                = nullptr;

    // Rebind pointers to owned data, including when a build info was copied for an acceleration structure clone.
    for (auto& [geometry, omm_info_ext] : build_info.omm_infos_ext)
    {
        omm_info_ext.pUsageCounts =
            omm_info_ext.usageCountsCount > 0 ? build_info.usage_infos_ext[geometry].data() : nullptr;
        omm_info_ext.ppUsageCounts                               = nullptr;
        build_info.geometries[geometry].geometry.triangles.pNext = &omm_info_ext;
    }
    for (auto& [geometry, micromap_info] : build_info.micromap_infos_khr)
    {
        micromap_info.pUsageCounts =
            micromap_info.usageCountsCount > 0 ? build_info.usage_infos_khr[geometry].data() : nullptr;
        micromap_info.ppUsageCounts           = nullptr;
        build_info.geometries[geometry].pNext = &micromap_info;
    }

    uint32_t* max_primitive_counts = build_info.primitive_counts.empty() ? nullptr : build_info.primitive_counts.data();
    VkAccelerationStructureBuildSizesInfoKHR pSizeInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, nullptr, 0, 0, 0
    };

    auto new_call       = CreatePreCall();
    new_call->type      = NewCallDataType::ApiCall;
    new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_vkGetAccelerationStructureBuildSizesKHR;
    new_call->thread_id = 1;
    gfxrecon::encode::ParameterEncoder encoder(&new_call->parameter_buffer);
    encoder.EncodeHandleIdValue(device);
    encoder.EncodeEnumValue(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR); // TODO: hardcoding device build,
                                                                              // could be host or host_or_device

    // Manually encoding, identical to generated except for pnext of triangles
    encoder.EncodeStructPtrPreamble(&pBuildInfo, false, false);

    encoder.EncodeEnumValue(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
    EncodePNextStructIfValid(&encoder, nullptr);
    encoder.EncodeEnumValue(pBuildInfo.type);
    encoder.EncodeFlagsValue(pBuildInfo.flags);
    encoder.EncodeEnumValue(pBuildInfo.mode);
    encoder.EncodeHandleIdValue(format::kNullHandleId);
    encoder.EncodeHandleIdValue(format::kNullHandleId);
    encoder.EncodeUInt32Value(pBuildInfo.geometryCount);

    encoder.EncodeStructArrayPreamble(pBuildInfo.pGeometries, pBuildInfo.geometryCount, false, false);

    for (uint32_t g = 0; g < pBuildInfo.geometryCount; ++g)
    {
        encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].sType);
        EncodePNextStruct(&encoder, pBuildInfo.pGeometries[g].pNext);
        encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].geometryType);
        switch (pBuildInfo.pGeometries[g].geometryType)
        {
            case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
                encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].geometry.triangles.sType);
                if (auto it = build_info.omm_infos_khr.find(g); it != build_info.omm_infos_khr.end())
                {
                    const auto& omm_info = it->second;
                    encoder.EncodeStructPtrPreamble(&omm_info, false, false);
                    encoder.EncodeEnumValue(omm_info.sType);
                    EncodePNextStruct(&encoder, omm_info.pNext);
                    encoder.EncodeEnumValue(omm_info.indexType);
                    encoder.EncodeUInt64Value(omm_info.indexBuffer);
                    encoder.EncodeUInt64Value(omm_info.indexStride);
                    encoder.EncodeUInt32Value(omm_info.baseTriangle);
                    // TODO: check if this handle exists at this moment in time. If not move the micromap create call
                    // before this current call
                    encoder.EncodeHandleIdValue(build_info.geometry_omm_id_map[g]);
                }
                else if (build_info.omm_infos_ext.contains(g))
                {
                    encoder.EncodeStructPtrPreamble(&(build_info.omm_infos_ext[g]), false, false);

                    encoder.EncodeEnumValue(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT);
                    EncodePNextStruct(&encoder, build_info.omm_infos_ext[g].pNext);
                    encoder.EncodeEnumValue(build_info.omm_infos_ext[g].indexType);
                    EncodeStruct(&encoder, build_info.omm_infos_ext[g].indexBuffer);
                    encoder.EncodeUInt64Value(build_info.omm_infos_ext[g].indexStride);
                    encoder.EncodeUInt32Value(build_info.omm_infos_ext[g].baseTriangle);
                    encoder.EncodeUInt32Value(build_info.omm_infos_ext[g].usageCountsCount);
                    EncodeStructArray(&encoder,
                                      build_info.omm_infos_ext[g].pUsageCounts,
                                      build_info.omm_infos_ext[g].usageCountsCount);
                    EncodeStructArray2D(&encoder,
                                        build_info.omm_infos_ext[g].ppUsageCounts,
                                        build_info.omm_infos_ext[g].usageCountsCount,
                                        1);

                    // todo: All this monstrocity is done for this line
                    // TODO: check if this handle exists at this moment in time. If not move the micromap create call
                    // before this current call
                    encoder.EncodeHandleIdValue(build_info.geometry_omm_id_map[g]);
                }
                else
                {
                    EncodePNextStruct(&encoder, nullptr);
                }
                encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].geometry.triangles.vertexFormat);
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.triangles.vertexData);
                encoder.EncodeUInt64Value(pBuildInfo.pGeometries[g].geometry.triangles.vertexStride);
                encoder.EncodeUInt32Value(pBuildInfo.pGeometries[g].geometry.triangles.maxVertex);
                encoder.EncodeEnumValue(pBuildInfo.pGeometries[g].geometry.triangles.indexType);
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.triangles.indexData);
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.triangles.transformData);
                break;
            case VK_GEOMETRY_TYPE_AABBS_KHR:
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.aabbs);
                break;
            case VK_GEOMETRY_TYPE_INSTANCES_KHR:
                EncodeStruct(&encoder, pBuildInfo.pGeometries[g].geometry.instances);
                break;
            default:
                break;
        }
        encoder.EncodeFlagsValue(pBuildInfo.pGeometries[g].flags);
    }
    EncodeStructArray2D(&encoder, pBuildInfo.ppGeometries, pBuildInfo.geometryCount, 1);
    EncodeStruct(&encoder, pBuildInfo.scratchData);

    encoder.EncodeUInt32Array(max_primitive_counts, pBuildInfo.geometryCount);
    encode::EncodeStructPtr(&encoder, &pSizeInfo);
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
