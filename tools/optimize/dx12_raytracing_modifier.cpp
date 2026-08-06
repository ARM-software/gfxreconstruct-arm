
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

#include "dx12_raytracing_modifier.h"
#include "encode/struct_pointer_encoder.h"
#include "encode/parameter_encoder.h"
#include "encode/custom_dx12_struct_encoders.h"
#include "generated/generated_dx12_api_call_encoders.h"
#include "format/format.h"
#include "format/format_arm.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

void Dx12RayTracingModifier::Process_ID3D12Resource_GetGPUVirtualAddress(const ApiCallInfo&        call_info,
                                                                         format::HandleId          object_id,
                                                                         D3D12_GPU_VIRTUAL_ADDRESS return_value)
{
    if (IsModificationPass())
    {
        return;
    }

    if ((return_value == 0) || (return_value == UINT64_MAX))
    {
        // If the GPU virtual address is 0 or UINT64_MAX, it indicates that the resource is not valid or not
        // allocated, so we do not track it.
        return;
    }

    auto iter = resource_entries_.find(object_id);
    if (iter != resource_entries_.end())
    {
        if (iter->second.desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            // This is not a buffer resource, so we do not track its GPU virtual address.
            return;
        }

        iter->second.start_virtual_address = return_value;
        iter->second.end_virtual_address   = return_value + iter->second.desc.Width;

        min_gpu_va_ = std::min(min_gpu_va_, return_value);
        max_gpu_va_ = std::max(max_gpu_va_, return_value + iter->second.desc.Width);

        gpu_virtual_address_resource_[return_value] = iter->second;
        if (((iter->second.initial_state & D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) ==
             D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) ||
            ((iter->second.desc.Flags & D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE) ==
             D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE))
        {
            accel_struct_address_resource_[return_value] = iter->second;
        }

        GFXRECON_LOG_DEBUG("*** GPU virtual start address: 0x%" PRIx64 " end address: 0x%" PRIx64
                           " for resource ID: %" PRIu64 " GetCurrentBlockIndex(%" PRIu64 ")",
                           return_value,
                           iter->second.end_virtual_address,
                           object_id,
                           call_info.index);
    }
    else
    {
        GFXRECON_LOG_ERROR("Failed to find resource object id %" PRIu64 " in resource_entries_ map.", object_id);
    }
}

void Dx12RayTracingModifier::Process_ID3D12StateObjectProperties_GetShaderIdentifier(
    const ApiCallInfo&       call_info,
    format::HandleId         object_id,
    PointerDecoder<uint8_t>* return_value,
    WStringDecoder*          pExportName)
{
    if (IsModificationPass())
    {
        return;
    }

    if ((return_value == nullptr) || (return_value->IsNull()))
    {
        return;
    }

    std::vector<uint8_t> shader_id(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, 0);
    util::platform::MemoryCopy(shader_id.data(),
                               D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                               (uint8_t*)return_value->GetPointer(),
                               D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    auto [iter, inserted] = shader_id_to_properties_id_.try_emplace(shader_id, object_id);
    if ((!inserted) && (iter->second != object_id))
    {
        GFXRECON_LOG_DEBUG(
            "Shader identifier already exists for object ID: %" PRIu64 " and %" PRIu64, object_id, iter->second);
    }
}

void Dx12RayTracingModifier::Process_ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(
    const ApiCallInfo& call_info, format::HandleId object_id, Decoded_D3D12_GPU_DESCRIPTOR_HANDLE return_value)
{
    if (IsModificationPass())
    {
        return;
    }

    if ((return_value.decoded_value->ptr == 0) || (return_value.decoded_value->ptr == UINT64_MAX))
    {
        // If the GPU descriptor handle is 0 or UINT64_MAX, it indicates that the descriptor heap is not valid or not
        // allocated, so we do not track it.
        return;
    }

    if (descriptor_heap_infos_.find(object_id) == descriptor_heap_infos_.end())
    {
        GFXRECON_LOG_ERROR("Failed to find descriptor heap object id %" PRIu64 " in descriptor_heap_infos_ map.",
                           object_id);
        return;
    }

    auto& heap_info = descriptor_heap_infos_[object_id];
    if (heap_info.descriptor_type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV ||
        heap_info.descriptor_type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    {
        // For RTV and DSV heaps, do not track the GPU descriptor address.
        return;
    }

    uint64_t increment      = 0;
    auto     increment_iter = device_descriptor_increment_sizes_.find(heap_info.object_id);
    if (increment_iter != device_descriptor_increment_sizes_.end())
    {
        auto descriptor_type_iter = increment_iter->second.find(heap_info.descriptor_type);
        if (descriptor_type_iter != increment_iter->second.end())
        {
            increment = descriptor_type_iter->second;
        }
    }

    if (increment == 0)
    {
        GFXRECON_LOG_WARNING("Failed to find increment size for descriptor heap object id %" PRIu64 " in "
                             "device_descriptor_increment_sizes_ map. Using default increment size of %" PRIu64 ".",
                             object_id,
                             min_gpu_descriptor_increment_);
        increment = min_gpu_descriptor_increment_;
    }
    heap_info.capture_increment = increment;

    uint64_t descriptor_size = static_cast<uint64_t>(heap_info.descriptor_count) * increment;
    if (heap_info.capture_gpu_addr_begin == kNullGpuAddress)
    {
        heap_info.capture_gpu_addr_begin = return_value.decoded_value->ptr;
        heap_info.capture_gpu_addr_end   = return_value.decoded_value->ptr + descriptor_size;
    }

    descriptor_start_address_info_[return_value.decoded_value->ptr] = heap_info;
    min_gpu_descriptor_ = std::min(min_gpu_descriptor_, (*return_value.decoded_value).ptr);
    max_gpu_descriptor_ = std::max(max_gpu_descriptor_, (*return_value.decoded_value).ptr + descriptor_size);
}

void Dx12RayTracingModifier::Process_ID3D12Device_CreateCommittedResource(
    const ApiCallInfo&                                   call_info,
    format::HandleId                                     object_id,
    HRESULT                                              return_value,
    StructPointerDecoder<Decoded_D3D12_HEAP_PROPERTIES>* pHeapProperties,
    D3D12_HEAP_FLAGS                                     HeapFlags,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>*   pDesc,
    D3D12_RESOURCE_STATES                                InitialResourceState,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*     pOptimizedClearValue,
    Decoded_GUID                                         riidResource,
    HandlePointerDecoder<void*>*                         ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                 = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id     = handle;
    resource_entries_[handle].object_id     = object_id;
    resource_entries_[handle].heap_props    = *pHeapProperties->GetPointer();
    resource_entries_[handle].heap_flags    = HeapFlags;
    resource_entries_[handle].initial_state = InitialResourceState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
    util::platform::MemoryCopy(
        &resource_entries_[handle].desc, sizeof(D3D12_RESOURCE_DESC), pDesc->GetPointer(), sizeof(D3D12_RESOURCE_DESC));
}

void Dx12RayTracingModifier::Process_ID3D12Device4_CreateCommittedResource1(
    const ApiCallInfo&                                   call_info,
    format::HandleId                                     object_id,
    HRESULT                                              return_value,
    StructPointerDecoder<Decoded_D3D12_HEAP_PROPERTIES>* pHeapProperties,
    D3D12_HEAP_FLAGS                                     HeapFlags,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>*   pDesc,
    D3D12_RESOURCE_STATES                                InitialResourceState,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*     pOptimizedClearValue,
    format::HandleId                                     pProtectedSession,
    Decoded_GUID                                         riidResource,
    HandlePointerDecoder<void*>*                         ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                 = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id     = handle;
    resource_entries_[handle].object_id     = object_id;
    resource_entries_[handle].heap_props    = *pHeapProperties->GetPointer();
    resource_entries_[handle].heap_flags    = HeapFlags;
    resource_entries_[handle].initial_state = InitialResourceState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
    util::platform::MemoryCopy(
        &resource_entries_[handle].desc, sizeof(D3D12_RESOURCE_DESC), pDesc->GetPointer(), sizeof(D3D12_RESOURCE_DESC));
}

void Dx12RayTracingModifier::Process_ID3D12Device8_CreateCommittedResource2(
    const ApiCallInfo&                                   call_info,
    format::HandleId                                     object_id,
    HRESULT                                              return_value,
    StructPointerDecoder<Decoded_D3D12_HEAP_PROPERTIES>* pHeapProperties,
    D3D12_HEAP_FLAGS                                     HeapFlags,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC1>*  pDesc,
    D3D12_RESOURCE_STATES                                InitialResourceState,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*     pOptimizedClearValue,
    format::HandleId                                     pProtectedSession,
    Decoded_GUID                                         riidResource,
    HandlePointerDecoder<void*>*                         ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                 = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id     = handle;
    resource_entries_[handle].object_id     = object_id;
    resource_entries_[handle].heap_props    = *pHeapProperties->GetPointer();
    resource_entries_[handle].heap_flags    = HeapFlags;
    resource_entries_[handle].initial_state = InitialResourceState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
    util::platform::MemoryCopy(&resource_entries_[handle].desc,
                               sizeof(D3D12_RESOURCE_DESC1),
                               pDesc->GetPointer(),
                               sizeof(D3D12_RESOURCE_DESC1));
}

void Dx12RayTracingModifier::Process_ID3D12Device10_CreateCommittedResource3(
    const ApiCallInfo&                                   call_info,
    format::HandleId                                     object_id,
    HRESULT                                              return_value,
    StructPointerDecoder<Decoded_D3D12_HEAP_PROPERTIES>* pHeapProperties,
    D3D12_HEAP_FLAGS                                     HeapFlags,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC1>*  pDesc,
    D3D12_BARRIER_LAYOUT                                 InitialLayout,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*     pOptimizedClearValue,
    format::HandleId                                     pProtectedSession,
    UINT32                                               NumCastableFormats,
    PointerDecoder<DXGI_FORMAT>*                         pCastableFormats,
    Decoded_GUID                                         riidResource,
    HandlePointerDecoder<void*>*                         ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                  = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id      = handle;
    resource_entries_[handle].object_id      = object_id;
    resource_entries_[handle].heap_props     = *pHeapProperties->GetPointer();
    resource_entries_[handle].heap_flags     = HeapFlags;
    resource_entries_[handle].initial_layout = InitialLayout;
    resource_entries_[handle].block_index    = GetCurrentBlockIndex();
    util::platform::MemoryCopy(&resource_entries_[handle].desc,
                               sizeof(D3D12_RESOURCE_DESC1),
                               pDesc->GetPointer(),
                               sizeof(D3D12_RESOURCE_DESC1));
}

void Dx12RayTracingModifier::Process_ID3D12Device_CreatePlacedResource(
    const ApiCallInfo&                                 call_info,
    format::HandleId                                   object_id,
    HRESULT                                            return_value,
    format::HandleId                                   pHeap,
    UINT64                                             HeapOffset,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>* pDesc,
    D3D12_RESOURCE_STATES                              InitialState,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*   pOptimizedClearValue,
    Decoded_GUID                                       riid,
    HandlePointerDecoder<void*>*                       ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                 = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id     = handle;
    resource_entries_[handle].object_id     = object_id;
    resource_entries_[handle].heap_id       = pHeap;
    resource_entries_[handle].initial_state = InitialState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
    util::platform::MemoryCopy(
        &resource_entries_[handle].desc, sizeof(D3D12_RESOURCE_DESC), pDesc->GetPointer(), sizeof(D3D12_RESOURCE_DESC));
}

void Dx12RayTracingModifier::Process_ID3D12Device8_CreatePlacedResource1(
    const ApiCallInfo&                                  call_info,
    format::HandleId                                    object_id,
    HRESULT                                             return_value,
    format::HandleId                                    pHeap,
    UINT64                                              HeapOffset,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC1>* pDesc,
    D3D12_RESOURCE_STATES                               InitialState,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*    pOptimizedClearValue,
    Decoded_GUID                                        riid,
    HandlePointerDecoder<void*>*                        ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                 = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id     = handle;
    resource_entries_[handle].object_id     = object_id;
    resource_entries_[handle].heap_id       = pHeap;
    resource_entries_[handle].initial_state = InitialState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
    util::platform::MemoryCopy(&resource_entries_[handle].desc,
                               sizeof(D3D12_RESOURCE_DESC1),
                               pDesc->GetPointer(),
                               sizeof(D3D12_RESOURCE_DESC1));
}

void Dx12RayTracingModifier::Process_ID3D12Device10_CreatePlacedResource2(
    const ApiCallInfo&                                  call_info,
    format::HandleId                                    object_id,
    HRESULT                                             return_value,
    format::HandleId                                    pHeap,
    UINT64                                              HeapOffset,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC1>* pDesc,
    D3D12_BARRIER_LAYOUT                                InitialLayout,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*    pOptimizedClearValue,
    UINT32                                              NumCastableFormats,
    PointerDecoder<DXGI_FORMAT>*                        pCastableFormats,
    Decoded_GUID                                        riid,
    HandlePointerDecoder<void*>*                        ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                  = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id      = handle;
    resource_entries_[handle].object_id      = object_id;
    resource_entries_[handle].heap_id        = pHeap;
    resource_entries_[handle].initial_layout = InitialLayout;
    resource_entries_[handle].block_index    = GetCurrentBlockIndex();
    util::platform::MemoryCopy(&resource_entries_[handle].desc,
                               sizeof(D3D12_RESOURCE_DESC1),
                               pDesc->GetPointer(),
                               sizeof(D3D12_RESOURCE_DESC1));
}

void Dx12RayTracingModifier::Process_ID3D12Device_CreateReservedResource(
    const ApiCallInfo&                                 call_info,
    format::HandleId                                   object_id,
    HRESULT                                            return_value,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>* pDesc,
    D3D12_RESOURCE_STATES                              InitialState,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*   pOptimizedClearValue,
    Decoded_GUID                                       riid,
    HandlePointerDecoder<void*>*                       ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                 = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id     = handle;
    resource_entries_[handle].object_id     = object_id;
    resource_entries_[handle].initial_state = InitialState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
    util::platform::MemoryCopy(
        &resource_entries_[handle].desc, sizeof(D3D12_RESOURCE_DESC), pDesc->GetPointer(), sizeof(D3D12_RESOURCE_DESC));
}

void Dx12RayTracingModifier::Process_ID3D12Device4_CreateReservedResource1(
    const ApiCallInfo&                                 call_info,
    format::HandleId                                   object_id,
    HRESULT                                            return_value,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>* pDesc,
    D3D12_RESOURCE_STATES                              InitialState,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*   pOptimizedClearValue,
    format::HandleId                                   pProtectedSession,
    Decoded_GUID                                       riid,
    HandlePointerDecoder<void*>*                       ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                 = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id     = handle;
    resource_entries_[handle].object_id     = object_id;
    resource_entries_[handle].initial_state = InitialState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
    util::platform::MemoryCopy(
        &resource_entries_[handle].desc, sizeof(D3D12_RESOURCE_DESC), pDesc->GetPointer(), sizeof(D3D12_RESOURCE_DESC));
}

void Dx12RayTracingModifier::Process_ID3D12Device10_CreateReservedResource2(
    const ApiCallInfo&                                 call_info,
    format::HandleId                                   object_id,
    HRESULT                                            return_value,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>* pDesc,
    D3D12_BARRIER_LAYOUT                               InitialLayout,
    StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*   pOptimizedClearValue,
    format::HandleId                                   pProtectedSession,
    UINT32                                             NumCastableFormats,
    PointerDecoder<DXGI_FORMAT>*                       pCastableFormats,
    Decoded_GUID                                       riid,
    HandlePointerDecoder<void*>*                       ppvResource)
{
    if (IsModificationPass())
    {
        AddPrebuildInfoResourceValueCommand();
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle                  = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id      = handle;
    resource_entries_[handle].object_id      = object_id;
    resource_entries_[handle].initial_layout = InitialLayout;
    resource_entries_[handle].block_index    = GetCurrentBlockIndex();
    util::platform::MemoryCopy(
        &resource_entries_[handle].desc, sizeof(D3D12_RESOURCE_DESC), pDesc->GetPointer(), sizeof(D3D12_RESOURCE_DESC));
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList4_BuildRaytracingAccelerationStructure(
    const ApiCallInfo&                                                                         call_info,
    format::HandleId                                                                           object_id,
    StructPointerDecoder<Decoded_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC>*          pDesc,
    UINT                                                                                       NumPostbuildInfoDescs,
    StructPointerDecoder<Decoded_D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC>* pPostbuildInfoDescs)
{
    if (IsModificationPass())
    {
        return;
    }

    Process_BuildRaytracingAccelerationStructure(
        call_info, object_id, pDesc->GetPointer(), NumPostbuildInfoDescs, pPostbuildInfoDescs->GetPointer());
    opt_fillmem_ = true;
}

void Dx12RayTracingModifier::Process_BuildRaytracingAccelerationStructure(
    const ApiCallInfo&                                                 call_info,
    format::HandleId                                                   object_id,
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC*          desc,
    UINT                                                               num_post_build_descs,
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* post_build_descs)
{
    format::HandleId src_id = format::kNullHandleId;
    format::HandleId dst_id = format::kNullHandleId;

    if (desc->Inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
    {
        format::HandleId instance_id = FindBaseResourceFromGPUAddress(desc->Inputs.InstanceDescs);
        if (instance_id != format::kNullHandleId)
        {
            auto offset = desc->Inputs.InstanceDescs - resource_entries_[instance_id].start_virtual_address;

            ResourceValueInfo resource_value;
            resource_value.offset = offset;
            resource_value.size   = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * desc->Inputs.NumDescs;
            resource_value.type   = ResourceValueType::kRaytracingInstanceDescPointer;
            command_list_related_infos_[object_id].related_resource_values[instance_id] = resource_value;
        }
    }

    auto src_address = desc->SourceAccelerationStructureData;
    auto dst_address = desc->DestAccelerationStructureData;
    if (src_address != 0)
    {
        FindAccelerationStructureResourceFromGPUAddress(src_address);
    }
    if (dst_address != 0)
    {
        FindAccelerationStructureResourceFromGPUAddress(dst_address);
    }

    if (accel_struct_address_resource_.find(src_address) != accel_struct_address_resource_.end())
    {
        src_id = accel_struct_address_resource_[src_address].handle_id;
    }

    if (accel_struct_address_resource_.find(dst_address) != accel_struct_address_resource_.end())
    {
        dst_id = accel_struct_address_resource_[dst_address].handle_id;
    }

    if (real_device5_ == nullptr)
    {
        CreateDeviceAndCheckRayTracingSupport();
    }

    if (dst_id != format::kNullHandleId)
    {
        AccelerationStructureBuildDesc build_desc = {};
        build_desc.handle_id                      = dst_id;
        build_desc.object_id                      = resource_entries_[dst_id].object_id;
        build_desc.is_first_built                 = true;
        build_desc.is_meta_copy                   = false;
        build_desc.source_of_compaction           = 0;
        build_desc.build_blas_inputs.Type         = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        build_desc.build_tlas_inputs.Type         = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        build_desc.build_omm_inputs.Type          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_OPACITY_MICROMAP_ARRAY;

        if (real_device5_ != nullptr)
        {
            real_device5_->GetRaytracingAccelerationStructurePrebuildInfo(&(desc->Inputs),
                                                                          &(build_desc.real_prebuild_info));
        }

        if (build_desc.real_prebuild_info.ResultDataMaxSizeInBytes == 0)
        {
            GFXRECON_LOG_ERROR("Failed to get real prebuild info for dest address 0x%" PRIx64, dst_address);
        }

        for (UINT i = 0; i < num_post_build_descs; i++)
        {
            if (post_build_descs[i].InfoType == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE)
            {
                build_desc.source_of_compaction = dst_address;
                build_desc.postbuild_info       = post_build_descs[i];
            }
        }

        // In order for GetAccelerationStructureInputsBufferEntries to correctly process inputs buffer entries, a
        // non-zero GPU VA must be set for values that will be used.
        const D3D12_GPU_VIRTUAL_ADDRESS kDefaultGpuVa = 1;

        auto& acceleration_structure_inputs = desc->Inputs;
        if (acceleration_structure_inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
        {
            build_desc.build_blas_inputs             = desc->Inputs;
            build_desc.build_blas_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            build_desc.geometry_descs.resize(desc->Inputs.NumDescs);

            for (UINT i = 0; i < acceleration_structure_inputs.NumDescs; i++)
            {
                if (acceleration_structure_inputs.DescsLayout == D3D12_ELEMENTS_LAYOUT_ARRAY)
                {
                    auto& geometry_desc =
                        const_cast<D3D12_RAYTRACING_GEOMETRY_DESC&>(acceleration_structure_inputs.pGeometryDescs[i]);
                    if (geometry_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES)
                    {
                        geometry_desc.Triangles.VertexBuffer.StartAddress = kDefaultGpuVa;
                        geometry_desc.Triangles.Transform3x4              = kDefaultGpuVa;
                        geometry_desc.Triangles.IndexBuffer =
                            (geometry_desc.Triangles.IndexCount > 0) ? kDefaultGpuVa : 0;
                    }
                    else if (geometry_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS)
                    {
                        geometry_desc.AABBs.AABBs.StartAddress = kDefaultGpuVa;
                    }
                    else if (geometry_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES)
                    {
                        if (geometry_desc.OmmTriangles.pTriangles != nullptr)
                        {
                            auto triangles_desc                      = *geometry_desc.OmmTriangles.pTriangles;
                            triangles_desc.VertexBuffer.StartAddress = kDefaultGpuVa;
                            triangles_desc.Transform3x4              = kDefaultGpuVa;
                            triangles_desc.IndexBuffer = (triangles_desc.IndexCount > 0) ? kDefaultGpuVa : 0;

                            build_desc.omm_triangles_geometry_descs[i] = triangles_desc;
                        }

                        if (geometry_desc.OmmTriangles.pOmmLinkage != nullptr)
                        {
                            auto linkage_desc = *geometry_desc.OmmTriangles.pOmmLinkage;
                            linkage_desc.OpacityMicromapIndexBuffer.StartAddress = kDefaultGpuVa;
                            linkage_desc.OpacityMicromapArray                    = kDefaultGpuVa;

                            build_desc.omm_linkage_geometry_descs[i] = linkage_desc;
                        }
                    }
                    else
                    {
                        GFXRECON_ASSERT(false && "Invalid D3D12_RAYTRACING_GEOMETRY_TYPE.");
                    }

                    build_desc.geometry_descs[i] = geometry_desc;
                }
                else
                {
                    auto geometry_desc =
                        const_cast<D3D12_RAYTRACING_GEOMETRY_DESC*>(acceleration_structure_inputs.ppGeometryDescs[i]);
                    if (geometry_desc->Type == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES)
                    {
                        geometry_desc->Triangles.VertexBuffer.StartAddress = kDefaultGpuVa;
                        geometry_desc->Triangles.Transform3x4              = kDefaultGpuVa;
                        geometry_desc->Triangles.IndexBuffer =
                            (geometry_desc->Triangles.IndexCount > 0) ? kDefaultGpuVa : 0;
                    }
                    else if (geometry_desc->Type == D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS)
                    {
                        geometry_desc->AABBs.AABBs.StartAddress = kDefaultGpuVa;
                    }
                    else if (geometry_desc->Type == D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES)
                    {
                        if (geometry_desc->OmmTriangles.pTriangles != nullptr)
                        {
                            auto triangles_desc                      = *geometry_desc->OmmTriangles.pTriangles;
                            triangles_desc.VertexBuffer.StartAddress = kDefaultGpuVa;
                            triangles_desc.Transform3x4              = kDefaultGpuVa;
                            triangles_desc.IndexBuffer = (triangles_desc.IndexCount > 0) ? kDefaultGpuVa : 0;

                            build_desc.omm_triangles_geometry_descs[i] = triangles_desc;
                        }

                        if (geometry_desc->OmmTriangles.pOmmLinkage != nullptr)
                        {
                            auto linkage_desc = *geometry_desc->OmmTriangles.pOmmLinkage;
                            linkage_desc.OpacityMicromapIndexBuffer.StartAddress = kDefaultGpuVa;
                            linkage_desc.OpacityMicromapArray                    = kDefaultGpuVa;

                            build_desc.omm_linkage_geometry_descs[i] = linkage_desc;
                        }
                    }
                    else
                    {
                        GFXRECON_ASSERT(false && "Invalid D3D12_RAYTRACING_GEOMETRY_TYPE.");
                    }

                    build_desc.geometry_descs[i] = *geometry_desc;
                }
            }
        }
        else if (acceleration_structure_inputs.Type ==
                 D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_OPACITY_MICROMAP_ARRAY)
        {
            build_desc.build_omm_inputs = desc->Inputs;
            build_desc.omm_array_descs.resize(desc->Inputs.NumDescs);

            for (UINT i = 0; i < acceleration_structure_inputs.NumDescs; i++)
            {
                auto& omm_array_desc = const_cast<D3D12_RAYTRACING_OPACITY_MICROMAP_ARRAY_DESC&>(
                    acceleration_structure_inputs.pOpacityMicromapArrayDesc[i]);

                omm_array_desc.InputBuffer              = kDefaultGpuVa;
                omm_array_desc.PerOmmDescs.StartAddress = kDefaultGpuVa;
                for (UINT j = 0; j < omm_array_desc.NumOmmHistogramEntries; ++j)
                {
                    build_desc.omm_array_histograms[i].push_back(omm_array_desc.pOmmHistogram[j]);
                }

                build_desc.omm_array_descs[i] = omm_array_desc;
            }
        }
        else if (acceleration_structure_inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
        {
            const_cast<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS&>(acceleration_structure_inputs)
                .InstanceDescs                       = (desc->Inputs.NumDescs > 0) ? kDefaultGpuVa : 0;
            build_desc.build_tlas_inputs             = desc->Inputs;
            build_desc.build_tlas_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        }

        bool need_insert_prebuild_info = false;
        auto build_desc_iter           = acceleration_structure_build_desc_.find(dst_address);
        if (build_desc_iter == acceleration_structure_build_desc_.end())
        {
            need_insert_prebuild_info = true;
        }
        else
        {
            if (build_desc.build_blas_inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
            {
                if (build_desc_iter->second.geometry_descs.empty())
                {
                    need_insert_prebuild_info = true;
                }
                else
                {
                    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> new_geometry_descs = build_desc.geometry_descs;
                    for (UINT index = 0; index < build_desc_iter->second.geometry_descs.size(); ++index)
                    {
                        bool        found     = false;
                        const auto& geom_desc = build_desc_iter->second.geometry_descs[index];
                        if (geom_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES)
                        {
                            const_cast<D3D12_RAYTRACING_GEOMETRY_DESC&>(geom_desc).OmmTriangles.pTriangles  = nullptr;
                            const_cast<D3D12_RAYTRACING_GEOMETRY_DESC&>(geom_desc).OmmTriangles.pOmmLinkage = nullptr;
                        }

                        for (auto& new_geom_desc : new_geometry_descs)
                        {
                            if (new_geom_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES)
                            {
                                new_geom_desc.OmmTriangles.pTriangles  = nullptr;
                                new_geom_desc.OmmTriangles.pOmmLinkage = nullptr;
                            }
                            if (memcmp(&geom_desc, &new_geom_desc, sizeof(D3D12_RAYTRACING_GEOMETRY_DESC)) == 0)
                            {
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                        {
                            build_desc.geometry_descs.push_back(geom_desc);
                            if (geom_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES)
                            {
                                UINT current_pos = build_desc.geometry_descs.size() - 1;
                                if (build_desc_iter->second.omm_triangles_geometry_descs.contains(index))
                                {
                                    build_desc.omm_triangles_geometry_descs[current_pos] =
                                        build_desc_iter->second.omm_triangles_geometry_descs[index];
                                }
                                if (build_desc_iter->second.omm_linkage_geometry_descs.contains(index))
                                {
                                    build_desc.omm_linkage_geometry_descs[current_pos] =
                                        build_desc_iter->second.omm_linkage_geometry_descs[index];
                                }
                            }
                            build_desc.build_blas_inputs.NumDescs = build_desc.geometry_descs.size();
                            need_insert_prebuild_info             = true;
                        }
                    }
                }
            }

            if (build_desc.build_omm_inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_OPACITY_MICROMAP_ARRAY)
            {
                if (build_desc_iter->second.omm_array_descs.empty())
                {
                    need_insert_prebuild_info = true;
                }
                else
                {
                    if (build_desc.omm_array_descs.size() < build_desc_iter->second.omm_array_descs.size())
                    {
                        build_desc.omm_array_descs           = build_desc_iter->second.omm_array_descs;
                        build_desc.omm_array_histograms      = build_desc_iter->second.omm_array_histograms;
                        build_desc.build_omm_inputs.NumDescs = build_desc.omm_array_descs.size();
                        need_insert_prebuild_info            = true;
                    }
                }
            }

            if (build_desc.build_tlas_inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
            {
                build_desc.build_tlas_inputs.NumDescs =
                    std::max(build_desc.build_tlas_inputs.NumDescs, build_desc_iter->second.build_tlas_inputs.NumDescs);

                if (build_desc.build_tlas_inputs.NumDescs != 0)
                {
                    need_insert_prebuild_info = true;
                }
            }

            if (need_insert_prebuild_info)
            {
                acceleration_structure_build_desc_.erase(dst_address);
                prebuild_info_insert_values_[resource_entries_[dst_id].block_index].erase(dst_address);
            }
        }

        if (need_insert_prebuild_info)
        {
            AccelerationStructurePreBuildDesc prebuild_desc;
            prebuild_desc.handle_id          = dst_id;
            prebuild_desc.object_id          = resource_entries_[dst_id].object_id;
            prebuild_desc.num_instance_descs = build_desc.build_tlas_inputs.NumDescs;
            prebuild_desc.get_prebuild_info.Clear();
            if (gpu_virtual_address_resource_.find(dst_address) == gpu_virtual_address_resource_.end())
            {
                prebuild_desc.acceleration_structure_address = dst_address;
            }
            else
            {
                prebuild_desc.acceleration_structure_address = 0;
            }

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_inputs = {};
            if (build_desc.build_blas_inputs.NumDescs > 0)
            {
                build_inputs                = build_desc.build_blas_inputs;
                build_inputs.pGeometryDescs = build_desc.geometry_descs.data();
                for (UINT i = 0; i < build_inputs.NumDescs; ++i)
                {
                    auto& geometry_desc = const_cast<D3D12_RAYTRACING_GEOMETRY_DESC&>(build_inputs.pGeometryDescs[i]);
                    if (geometry_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES)
                    {
                        if (build_desc.omm_triangles_geometry_descs.contains(i))
                        {
                            geometry_desc.OmmTriangles.pTriangles = &build_desc.omm_triangles_geometry_descs[i];
                        }
                        else
                        {
                            geometry_desc.OmmTriangles.pTriangles = nullptr;
                        }

                        if (build_desc.omm_linkage_geometry_descs.contains(i))
                        {
                            geometry_desc.OmmTriangles.pOmmLinkage = &build_desc.omm_linkage_geometry_descs[i];
                        }
                        else
                        {
                            geometry_desc.OmmTriangles.pOmmLinkage = nullptr;
                        }
                    }
                }
            }
            else if (build_desc.build_omm_inputs.NumDescs > 0)
            {
                build_inputs                           = build_desc.build_omm_inputs;
                build_inputs.pOpacityMicromapArrayDesc = build_desc.omm_array_descs.data();
                for (UINT i = 0; i < build_inputs.NumDescs; ++i)
                {
                    D3D12_RAYTRACING_OPACITY_MICROMAP_ARRAY_DESC* omm_array_desc = &build_desc.omm_array_descs[i];
                    if (omm_array_desc->NumOmmHistogramEntries > 0)
                    {
                        omm_array_desc->pOmmHistogram = build_desc.omm_array_histograms[i].data();
                    }
                }
            }

            if (real_device5_ != nullptr)
            {
                real_device5_->GetRaytracingAccelerationStructurePrebuildInfo(&(build_inputs),
                                                                              &(build_desc.real_prebuild_info));
                if (build_desc.real_prebuild_info.ResultDataMaxSizeInBytes == 0)
                {
                    GFXRECON_LOG_ERROR("Failed to get real prebuild info for dest address 0x%" PRIx64, dst_address);
                }
            }

            gfxrecon::encode::ParameterEncoder encoder(&prebuild_desc.get_prebuild_info);
            encode::EncodeStructPtr(&encoder, &(build_inputs));

            acceleration_structure_build_desc_.emplace(dst_address, build_desc);
            prebuild_info_insert_values_[resource_entries_[dst_id].block_index].emplace(dst_address, prebuild_desc);
        }
    }
    else
    {
        GFXRECON_LOG_ERROR("Failed to find resource id for acceleration structure destination address 0x%" PRIx64,
                           dst_address);
    }
}

void Dx12RayTracingModifier::ProcessInitDx12AccelerationStructureCommand(
    const format::InitDx12AccelerationStructureCommandHeader&                           command_header,
    const std::vector<format::InitDx12AccelerationStructureGeometryDesc>&               geometry_descs,
    StructPointerDecoder<Decoded_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>* build_inputs,
    const uint8_t*                                                                      build_inputs_data)
{
    if (IsModificationPass())
    {
        return;
    }

    const ApiCallInfo      call_info   = { GetCurrentBlockIndex(), command_header.thread_id };
    const format::HandleId object_id   = format::kNullHandleId;
    const auto             src_address = command_header.copy_source_gpu_va;
    const auto             dst_address = command_header.dest_acceleration_structure_data;

    bool build = true;
    bool copy  = false;
    if (src_address != 0)
    {
        copy = true;
        if (src_address != dst_address)
        {
            build = false;
        }
    }

    if (build)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc           = {};
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>        build_geometry_descs = {};

        // Reconstruct acceleration structure build descs.
        build_desc.DestAccelerationStructureData    = dst_address;
        build_desc.SourceAccelerationStructureData  = 0;
        build_desc.ScratchAccelerationStructureData = 0;
        build_desc.Inputs.Type = static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE>(command_header.inputs_type);
        build_desc.Inputs.Flags =
            static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS>(command_header.inputs_flags);

        if ((build_inputs != nullptr) && (build_inputs->GetPointer() != nullptr))
        {
            build_desc.Inputs = *(build_inputs->GetPointer());
        }
        else
        {
            build_desc.Inputs.DescsLayout     = D3D12_ELEMENTS_LAYOUT_ARRAY;
            build_desc.Inputs.InstanceDescs   = 0;
            build_desc.Inputs.pGeometryDescs  = nullptr;
            build_desc.Inputs.ppGeometryDescs = nullptr;

            if (build_desc.Inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
            {
                build_geometry_descs.resize(command_header.inputs_geometry_descs_size);

                build_desc.Inputs.NumDescs = command_header.inputs_geometry_descs_size;
                GFXRECON_ASSERT(command_header.inputs_geometry_descs_size == geometry_descs.size());
                for (UINT i = 0; i < geometry_descs.size(); ++i)
                {
                    const auto&                     init_geom_desc = geometry_descs[i];
                    D3D12_RAYTRACING_GEOMETRY_DESC& geom_desc      = build_geometry_descs[i];

                    geom_desc.Type  = static_cast<D3D12_RAYTRACING_GEOMETRY_TYPE>(init_geom_desc.geometry_type);
                    geom_desc.Flags = static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(init_geom_desc.geometry_flags);
                    if (geom_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES)
                    {
                        auto& tris_desc        = geom_desc.Triangles;
                        tris_desc.Transform3x4 = 0;
                        tris_desc.IndexFormat  = static_cast<DXGI_FORMAT>(init_geom_desc.triangles_index_format);
                        tris_desc.VertexFormat = static_cast<DXGI_FORMAT>(init_geom_desc.triangles_vertex_format);
                        tris_desc.IndexCount   = init_geom_desc.triangles_index_count;
                        tris_desc.VertexCount  = init_geom_desc.triangles_vertex_count;
                        tris_desc.IndexBuffer  = 0;
                        tris_desc.VertexBuffer.StartAddress  = 0;
                        tris_desc.VertexBuffer.StrideInBytes = init_geom_desc.triangles_vertex_stride;
                    }
                    else if (geom_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS)
                    {
                        geom_desc.AABBs.AABBCount           = init_geom_desc.aabbs_count;
                        geom_desc.AABBs.AABBs.StartAddress  = 0;
                        geom_desc.AABBs.AABBs.StrideInBytes = init_geom_desc.aabbs_stride;
                    }
                    else if (geom_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES)
                    {
                        GFXRECON_LOG_ERROR("OMM_TRIANGLES geometry type is not supported.");
                    }
                    else
                    {
                        GFXRECON_ASSERT(false && "Invalid D3D12_RAYTRACING_GEOMETRY_TYPE.");
                    }
                }

                build_desc.Inputs.pGeometryDescs = build_geometry_descs.data();
            }
            else if (build_desc.Inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL)
            {
                build_desc.Inputs.NumDescs      = command_header.inputs_num_instance_descs;
                build_desc.Inputs.InstanceDescs = 0;
            }
            else if (build_desc.Inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_OPACITY_MICROMAP_ARRAY)
            {
                GFXRECON_LOG_ERROR("Raytracing acceleration structure with OMM array type isn't supported yet.");
            }
            else
            {
                GFXRECON_ASSERT(false && "Invalid D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE.");
            }
        }

        Process_BuildRaytracingAccelerationStructure(call_info, object_id, &build_desc, 0, nullptr);
    }

    if (copy)
    {
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE mode =
            static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE>(command_header.copy_mode);

        Process_CopyRaytracingAccelerationStructure(call_info, object_id, dst_address, src_address, mode);
    }

    opt_fillmem_ = true;
}

void Dx12RayTracingModifier::ProcessGetDx12AccelerationStructureSizeCommand(
    const format::arm::GetDx12AccelerationStructureSizeCommandHeader&                   command_header,
    StructPointerDecoder<Decoded_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>* input_descs)
{
    if (IsModificationPass())
    {
        // All old GetDx12AccelerationStructureSizeCommand Will be deleted
        SetDeleteCurrentCall();
        return;
    }

    opt_fillmem_ = true;
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList4_CopyRaytracingAccelerationStructure(
    const ApiCallInfo&                                call_info,
    format::HandleId                                  object_id,
    D3D12_GPU_VIRTUAL_ADDRESS                         DestAccelerationStructureData,
    D3D12_GPU_VIRTUAL_ADDRESS                         SourceAccelerationStructureData,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode)
{
    if (IsModificationPass())
    {
        return;
    }

    Process_CopyRaytracingAccelerationStructure(
        call_info, object_id, DestAccelerationStructureData, SourceAccelerationStructureData, Mode);
}

void Dx12RayTracingModifier::Process_CopyRaytracingAccelerationStructure(
    const ApiCallInfo&                                call_info,
    format::HandleId                                  object_id,
    D3D12_GPU_VIRTUAL_ADDRESS                         dest_acceleration_structure_data,
    D3D12_GPU_VIRTUAL_ADDRESS                         source_acceleration_structure_data,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE mode)
{
    format::HandleId src_id = format::kNullHandleId;
    format::HandleId dst_id = format::kNullHandleId;

    auto src_address = source_acceleration_structure_data;
    auto dst_address = dest_acceleration_structure_data;
    if (src_address != 0)
    {
        FindAccelerationStructureResourceFromGPUAddress(src_address);
    }
    if (dst_address != 0)
    {
        FindAccelerationStructureResourceFromGPUAddress(dst_address);
    }

    if (accel_struct_address_resource_.find(src_address) != accel_struct_address_resource_.end())
    {
        src_id = accel_struct_address_resource_[src_address].handle_id;
    }

    if (accel_struct_address_resource_.find(dst_address) != accel_struct_address_resource_.end())
    {
        dst_id = accel_struct_address_resource_[dst_address].handle_id;
    }

    if ((dst_id != format::kNullHandleId) && (src_id != format::kNullHandleId))
    {
        bool need_insert_prebuild_info = false;
        if ((acceleration_structure_build_desc_.find(src_address) != acceleration_structure_build_desc_.end()) &&
            (acceleration_structure_build_desc_.find(dst_address) == acceleration_structure_build_desc_.end()))
        {
            need_insert_prebuild_info = true;
        }
        else if ((acceleration_structure_build_desc_.find(src_address) != acceleration_structure_build_desc_.end()) &&
                 (acceleration_structure_build_desc_.find(dst_address) != acceleration_structure_build_desc_.end()))
        {
            prebuild_info_insert_values_[resource_entries_[dst_id].block_index].erase(dst_address);
            need_insert_prebuild_info = true;
        }
        else
        {
            GFXRECON_LOG_ERROR("Failed to find build desc for src address 0x%" PRIx64, src_address);
            return;
        }

        if (need_insert_prebuild_info)
        {
            AccelerationStructureBuildDesc build_desc;
            build_desc.handle_id            = dst_id;
            build_desc.object_id            = resource_entries_[dst_id].object_id;
            build_desc.is_first_built       = false;
            build_desc.is_meta_copy         = true;
            build_desc.source_of_compaction = 0;
            build_desc.build_blas_inputs    = acceleration_structure_build_desc_[src_address].build_blas_inputs;
            build_desc.build_tlas_inputs    = acceleration_structure_build_desc_[src_address].build_tlas_inputs;
            build_desc.geometry_descs       = acceleration_structure_build_desc_[src_address].geometry_descs;
            build_desc.real_prebuild_info   = acceleration_structure_build_desc_[src_address].real_prebuild_info;
            build_desc.postbuild_info       = {};

            if (acceleration_structure_build_desc_.find(dst_address) != acceleration_structure_build_desc_.end())
            {
                build_desc.build_tlas_inputs.NumDescs =
                    std::max(build_desc.build_tlas_inputs.NumDescs,
                             acceleration_structure_build_desc_[dst_address].build_tlas_inputs.NumDescs);

                UINT current_descs_size = build_desc.geometry_descs.size();
                for (UINT i = 0; i < acceleration_structure_build_desc_[dst_address].geometry_descs.size(); i++)
                {
                    build_desc.geometry_descs.push_back(
                        acceleration_structure_build_desc_[dst_address].geometry_descs[i]);

                    if (acceleration_structure_build_desc_[dst_address].omm_triangles_geometry_descs.contains(i))
                    {
                        build_desc.omm_triangles_geometry_descs[current_descs_size + i] =
                            acceleration_structure_build_desc_[dst_address].omm_triangles_geometry_descs[i];
                    }

                    if (acceleration_structure_build_desc_[dst_address].omm_linkage_geometry_descs.contains(i))
                    {
                        build_desc.omm_linkage_geometry_descs[current_descs_size + i] =
                            acceleration_structure_build_desc_[dst_address].omm_linkage_geometry_descs[i];
                    }
                }

                build_desc.build_blas_inputs.NumDescs = build_desc.geometry_descs.size();
                acceleration_structure_build_desc_.erase(dst_address);
            }

            // TODO: current insert prebuild info of source VA
            // The compacted resource size should be obtained from the post-build info
            if (mode == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT)
            {
                build_desc.source_of_compaction = src_address;
            }

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_inputs = {};
            if (build_desc.build_blas_inputs.NumDescs > 0)
            {
                build_inputs                = build_desc.build_blas_inputs;
                build_inputs.pGeometryDescs = build_desc.geometry_descs.data();
                for (UINT i = 0; i < build_inputs.NumDescs; ++i)
                {
                    auto& geometry_desc = const_cast<D3D12_RAYTRACING_GEOMETRY_DESC&>(build_inputs.pGeometryDescs[i]);
                    if (geometry_desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_OMM_TRIANGLES)
                    {
                        if (build_desc.omm_triangles_geometry_descs.contains(i))
                        {
                            geometry_desc.OmmTriangles.pTriangles = &build_desc.omm_triangles_geometry_descs[i];
                        }
                        else
                        {
                            geometry_desc.OmmTriangles.pTriangles = nullptr;
                        }

                        if (build_desc.omm_linkage_geometry_descs.contains(i))
                        {
                            geometry_desc.OmmTriangles.pOmmLinkage = &build_desc.omm_linkage_geometry_descs[i];
                        }
                        else
                        {
                            geometry_desc.OmmTriangles.pOmmLinkage = nullptr;
                        }
                    }
                }
            }
            else if (build_desc.build_omm_inputs.NumDescs > 0)
            {
                build_inputs                           = build_desc.build_omm_inputs;
                build_inputs.pOpacityMicromapArrayDesc = build_desc.omm_array_descs.data();
                for (UINT i = 0; i < build_inputs.NumDescs; ++i)
                {
                    D3D12_RAYTRACING_OPACITY_MICROMAP_ARRAY_DESC* omm_array_desc = &build_desc.omm_array_descs[i];
                    if (omm_array_desc->NumOmmHistogramEntries > 0)
                    {
                        omm_array_desc->pOmmHistogram = build_desc.omm_array_histograms[i].data();
                    }
                }
            }

            AccelerationStructurePreBuildDesc prebuild_desc;
            prebuild_desc.handle_id          = dst_id;
            prebuild_desc.object_id          = resource_entries_[dst_id].object_id;
            prebuild_desc.num_instance_descs = build_desc.build_tlas_inputs.NumDescs;
            prebuild_desc.get_prebuild_info.Clear();
            if (gpu_virtual_address_resource_.find(dst_address) == gpu_virtual_address_resource_.end())
            {
                prebuild_desc.acceleration_structure_address = dst_address;
            }
            else
            {
                prebuild_desc.acceleration_structure_address = 0;
            }

            if (real_device5_ != nullptr)
            {
                real_device5_->GetRaytracingAccelerationStructurePrebuildInfo(&(build_inputs),
                                                                              &(build_desc.real_prebuild_info));
                if (build_desc.real_prebuild_info.ResultDataMaxSizeInBytes == 0)
                {
                    GFXRECON_LOG_ERROR("Failed to get real prebuild info for dest address 0x%" PRIx64, dst_address);
                }
            }

            gfxrecon::encode::ParameterEncoder encoder(&prebuild_desc.get_prebuild_info);
            encode::EncodeStructPtr(&encoder, &(build_inputs));

            acceleration_structure_build_desc_.emplace(dst_address, build_desc);
            prebuild_info_insert_values_[resource_entries_[dst_id].block_index].emplace(dst_address, prebuild_desc);
        }
    }
    else
    {
        GFXRECON_LOG_ERROR("Failed to find src and dst address 0x%" PRIx64 " and 0x%" PRIx64, src_address, dst_address);
    }
}

void Dx12RayTracingModifier::Process_ID3D12Device5_CreateStateObject(
    const ApiCallInfo&                                     call_info,
    format::HandleId                                       object_id,
    HRESULT                                                return_value,
    StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* pDesc,
    Decoded_GUID                                           riid,
    HandlePointerDecoder<void*>*                           ppStateObject)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }
}

void Dx12RayTracingModifier::Process_ID3D12Device7_AddToStateObject(
    const ApiCallInfo&                                     call_info,
    format::HandleId                                       object_id,
    HRESULT                                                return_value,
    StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* pAddition,
    format::HandleId                                       pStateObjectToGrowFrom,
    Decoded_GUID                                           riid,
    HandlePointerDecoder<void*>*                           ppNewStateObject)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }
}

void Dx12RayTracingModifier::Process_D3D12SerializeRootSignature(
    const ApiCallInfo&                                       call_info,
    HRESULT                                                  return_value,
    StructPointerDecoder<Decoded_D3D12_ROOT_SIGNATURE_DESC>* pRootSignature,
    D3D_ROOT_SIGNATURE_VERSION                               Version,
    HandlePointerDecoder<ID3D10Blob*>*                       ppBlob,
    HandlePointerDecoder<ID3D10Blob*>*                       ppErrorBlob)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId blob_id = *ppBlob->GetPointer();
    auto             desc    = pRootSignature->GetPointer();
    if (desc != nullptr && desc->NumParameters > 0 && desc->pParameters != nullptr)
    {
        for (UINT i = 0; i < desc->NumParameters; ++i)
        {
            const auto& parameter_desc = desc->pParameters[i];
            if ((parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) ||
                (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV) ||
                (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV) ||
                (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV))
            {
                latest_blob_related_types_[blob_id].emplace_back(parameter_desc.ParameterType);
            }
        }
    }
}

void Dx12RayTracingModifier::Process_D3D12SerializeVersionedRootSignature(
    const ApiCallInfo&                                                 call_info,
    HRESULT                                                            return_value,
    StructPointerDecoder<Decoded_D3D12_VERSIONED_ROOT_SIGNATURE_DESC>* pRootSignature,
    HandlePointerDecoder<ID3D10Blob*>*                                 ppBlob,
    HandlePointerDecoder<ID3D10Blob*>*                                 ppErrorBlob)
{
    if (IsModificationPass())
    {
        return;
    }

    if ((return_value != S_OK) || (pRootSignature == nullptr) || (pRootSignature->GetPointer() == nullptr))
    {
        return;
    }

    format::HandleId blob_id = *ppBlob->GetPointer();
    auto             desc    = pRootSignature->GetPointer();

    if (desc->Version == D3D_ROOT_SIGNATURE_VERSION_1_0)
    {
        if (desc->Desc_1_0.NumParameters > 0 && desc->Desc_1_0.pParameters != nullptr)
        {
            for (UINT i = 0; i < desc->Desc_1_0.NumParameters; ++i)
            {
                const auto& parameter_desc = desc->Desc_1_0.pParameters[i];
                if ((parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV))
                {
                    latest_blob_related_types_[blob_id].emplace_back(parameter_desc.ParameterType);
                }
            }
        }
    }
    else if (desc->Version == D3D_ROOT_SIGNATURE_VERSION_1_1)
    {
        if (desc->Desc_1_1.NumParameters > 0 && desc->Desc_1_1.pParameters != nullptr)
        {
            for (UINT i = 0; i < desc->Desc_1_1.NumParameters; ++i)
            {
                const auto& parameter_desc = desc->Desc_1_1.pParameters[i];
                if ((parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV))
                {
                    latest_blob_related_types_[blob_id].emplace_back(parameter_desc.ParameterType);
                }
            }
        }
    }
    else if (desc->Version == D3D_ROOT_SIGNATURE_VERSION_1_2)
    {
        if (desc->Desc_1_2.NumParameters > 0 && desc->Desc_1_2.pParameters != nullptr)
        {
            for (UINT i = 0; i < desc->Desc_1_2.NumParameters; ++i)
            {
                const auto& parameter_desc = desc->Desc_1_2.pParameters[i];
                if ((parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV) ||
                    (parameter_desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV))
                {
                    latest_blob_related_types_[blob_id].emplace_back(parameter_desc.ParameterType);
                }
            }
        }
    }
}

void Dx12RayTracingModifier::Process_ID3D12Device_CreateRootSignature(const ApiCallInfo&       call_info,
                                                                      format::HandleId         object_id,
                                                                      HRESULT                  return_value,
                                                                      UINT                     nodeMask,
                                                                      PointerDecoder<uint8_t>* pBlobWithRootSignature,
                                                                      SIZE_T                   blobLengthInBytes,
                                                                      Decoded_GUID             riid,
                                                                      HandlePointerDecoder<void*>* ppvRootSignature)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    if (latest_blob_related_types_.size() == 1)
    {
        format::HandleId root_signature_id = *ppvRootSignature->GetPointer();
        const auto&      types             = latest_blob_related_types_.begin()->second;

        root_signature_related_types_[root_signature_id].insert(
            root_signature_related_types_[root_signature_id].end(), types.begin(), types.end());
        latest_blob_related_types_.clear();
    }
}

void Dx12RayTracingModifier::Process_ID3D12Device14_CreateRootSignatureFromSubobjectInLibrary(
    const ApiCallInfo&           call_info,
    format::HandleId             object_id,
    HRESULT                      return_value,
    UINT                         nodeMask,
    PointerDecoder<uint8_t>*     pLibraryBlob,
    SIZE_T                       blobLengthInBytes,
    WStringDecoder*              subobjectName,
    Decoded_GUID                 riid,
    HandlePointerDecoder<void*>* ppvRootSignature)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }
}

void Dx12RayTracingModifier::Process_ID3D12Device_CreateCommandSignature(
    const ApiCallInfo&                                          call_info,
    format::HandleId                                            object_id,
    HRESULT                                                     return_value,
    StructPointerDecoder<Decoded_D3D12_COMMAND_SIGNATURE_DESC>* pDesc,
    format::HandleId                                            pRootSignature,
    Decoded_GUID                                                riid,
    HandlePointerDecoder<void*>*                                ppvCommandSignature)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    auto desc = pDesc->GetPointer();
    if (desc && desc->NumArgumentDescs > 0 && desc->pArgumentDescs)
    {
        format::HandleId command_signature_id = *ppvCommandSignature->GetPointer();

        for (UINT i = 0; i < desc->NumArgumentDescs; ++i)
        {
            const auto& arg_desc = desc->pArgumentDescs[i];
            if ((arg_desc.Type == D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS) ||
                (arg_desc.Type == D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW) ||
                (arg_desc.Type == D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW) ||
                (arg_desc.Type == D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW) ||
                (arg_desc.Type == D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW) ||
                (arg_desc.Type == D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW))
            {
                command_signature_related_types_[command_signature_id].emplace_back(arg_desc.Type);
            }
        }
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_ExecuteIndirect(const ApiCallInfo& call_info,
                                                                               format::HandleId   object_id,
                                                                               format::HandleId   pCommandSignature,
                                                                               UINT               MaxCommandCount,
                                                                               format::HandleId   pArgumentBuffer,
                                                                               UINT64             ArgumentBufferOffset,
                                                                               format::HandleId   pCountBuffer,
                                                                               UINT64             CountBufferOffset)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_signature_related_types_.find(pCommandSignature) != command_signature_related_types_.end())
    {
        ResourceValueInfo resource_value;
        resource_value.offset = ArgumentBufferOffset;
        resource_value.size   = MaxCommandCount;
        resource_value.type   = ResourceValueType::kGpuVirtualAddress;

        const auto iter = resource_entries_.find(pArgumentBuffer);
        if (iter != resource_entries_.end())
        {
            resource_value.size = iter->second.desc.Width;
        }

        command_list_related_infos_[object_id].related_resource_values[pArgumentBuffer] = resource_value;
        opt_fillmem_                                                                    = true;
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList4_SetPipelineState1(const ApiCallInfo& call_info,
                                                                                  format::HandleId   object_id,
                                                                                  format::HandleId   pStateObject)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_list_related_infos_.find(object_id) != command_list_related_infos_.end())
    {
        command_list_related_infos_[object_id].state_object_id = pStateObject;
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList4_DispatchRays(
    const ApiCallInfo&                                      call_info,
    format::HandleId                                        object_id,
    StructPointerDecoder<Decoded_D3D12_DISPATCH_RAYS_DESC>* pDesc)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto pDesc_struct    = pDesc->GetPointer();
    auto       ray_gen_id      = FindBaseResourceFromGPUAddress(pDesc_struct->RayGenerationShaderRecord.StartAddress);
    auto       miss_table_id   = FindBaseResourceFromGPUAddress(pDesc_struct->MissShaderTable.StartAddress);
    auto       hit_table_id    = FindBaseResourceFromGPUAddress(pDesc_struct->HitGroupTable.StartAddress);
    auto       caller_table_id = FindBaseResourceFromGPUAddress(pDesc_struct->CallableShaderTable.StartAddress);

    if (ray_gen_id != format::kNullHandleId)
    {
        auto offset =
            pDesc_struct->RayGenerationShaderRecord.StartAddress - resource_entries_[ray_gen_id].start_virtual_address;

        ResourceValueInfo resource_value;
        resource_value.offset = offset;
        resource_value.size   = pDesc_struct->RayGenerationShaderRecord.SizeInBytes;
        resource_value.type   = ResourceValueType::kShaderIdentifier;
        command_list_related_infos_[object_id].related_resource_values[ray_gen_id] = resource_value;
    }

    if (miss_table_id != format::kNullHandleId)
    {
        auto offset =
            pDesc_struct->MissShaderTable.StartAddress - resource_entries_[miss_table_id].start_virtual_address;

        ResourceValueInfo resource_value;
        resource_value.offset = offset;
        resource_value.size   = pDesc_struct->MissShaderTable.SizeInBytes;
        resource_value.type   = ResourceValueType::kShaderIdentifier;
        command_list_related_infos_[object_id].related_resource_values[miss_table_id] = resource_value;
    }

    if (hit_table_id != format::kNullHandleId)
    {
        auto offset = pDesc_struct->HitGroupTable.StartAddress - resource_entries_[hit_table_id].start_virtual_address;

        ResourceValueInfo resource_value;
        resource_value.offset = offset;
        resource_value.size   = pDesc_struct->HitGroupTable.SizeInBytes;
        resource_value.type   = ResourceValueType::kShaderIdentifier;
        command_list_related_infos_[object_id].related_resource_values[hit_table_id] = resource_value;
    }

    if (caller_table_id != format::kNullHandleId)
    {
        auto offset =
            pDesc_struct->CallableShaderTable.StartAddress - resource_entries_[caller_table_id].start_virtual_address;

        ResourceValueInfo resource_value;
        resource_value.offset = offset;
        resource_value.size   = pDesc_struct->CallableShaderTable.SizeInBytes;
        resource_value.type   = ResourceValueType::kShaderIdentifier;
        command_list_related_infos_[object_id].related_resource_values[caller_table_id] = resource_value;
    }

    opt_fillmem_ = true;
}

void Dx12RayTracingModifier::Process_ID3D12Device_CreateCommandList(const ApiCallInfo&           call_info,
                                                                    format::HandleId             object_id,
                                                                    HRESULT                      return_value,
                                                                    UINT                         nodeMask,
                                                                    D3D12_COMMAND_LIST_TYPE      type,
                                                                    format::HandleId             pCommandAllocator,
                                                                    format::HandleId             pInitialState,
                                                                    Decoded_GUID                 riid,
                                                                    HandlePointerDecoder<void*>* ppCommandList)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    if (type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_COMPUTE ||
        type == D3D12_COMMAND_LIST_TYPE_COPY)
    {
        CommandListInfo cmd_list_info{};
        auto            cmd_list_id = *ppCommandList->GetPointer();

        command_list_related_infos_[cmd_list_id] = cmd_list_info;
    }
}

void Dx12RayTracingModifier::Process_ID3D12Device4_CreateCommandList1(const ApiCallInfo&           call_info,
                                                                      format::HandleId             object_id,
                                                                      HRESULT                      return_value,
                                                                      UINT                         nodeMask,
                                                                      D3D12_COMMAND_LIST_TYPE      type,
                                                                      D3D12_COMMAND_LIST_FLAGS     flags,
                                                                      Decoded_GUID                 riid,
                                                                      HandlePointerDecoder<void*>* ppCommandList)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    if (type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_COMPUTE ||
        type == D3D12_COMMAND_LIST_TYPE_COPY)
    {
        CommandListInfo cmd_list_info{};
        auto            cmd_list_id = *ppCommandList->GetPointer();

        command_list_related_infos_[cmd_list_id] = cmd_list_info;
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_Dispatch(const ApiCallInfo& call_info,
                                                                        format::HandleId   object_id,
                                                                        UINT               ThreadGroupCountX,
                                                                        UINT               ThreadGroupCountY,
                                                                        UINT               ThreadGroupCountZ)
{
    if (IsModificationPass())
    {
        return;
    }

    return;
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_CopyBufferRegion(const ApiCallInfo& call_info,
                                                                                format::HandleId   object_id,
                                                                                format::HandleId   pDstBuffer,
                                                                                UINT64             DstOffset,
                                                                                format::HandleId   pSrcBuffer,
                                                                                UINT64             SrcOffset,
                                                                                UINT64             NumBytes)
{
    if (IsModificationPass())
    {
        return;
    }

    ResourceCopyInfo copy_info;
    copy_info.dst_resource_id = pDstBuffer;
    copy_info.dst_offset      = DstOffset;
    copy_info.src_resource_id = pSrcBuffer;
    copy_info.src_offset      = SrcOffset;
    copy_info.num_bytes       = NumBytes;

    command_list_related_infos_[object_id].resource_copies.emplace_back(copy_info);
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_CopyResource(const ApiCallInfo& call_info,
                                                                            format::HandleId   object_id,
                                                                            format::HandleId   pDstResource,
                                                                            format::HandleId   pSrcResource)
{
    if (IsModificationPass())
    {
        return;
    }

    const auto iter = resource_entries_.find(pDstResource);
    if (iter != resource_entries_.end())
    {
        if (iter->second.desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            ResourceCopyInfo copy_info;
            copy_info.dst_resource_id = pDstResource;
            copy_info.dst_offset      = 0;
            copy_info.src_resource_id = pSrcResource;
            copy_info.src_offset      = 0;
            copy_info.num_bytes       = 0;

            command_list_related_infos_[object_id].resource_copies.emplace_back(copy_info);
        }
    }
}

void Dx12RayTracingModifier::Process_ID3D12CommandQueue_ExecuteCommandLists(
    const ApiCallInfo&                        call_info,
    format::HandleId                          object_id,
    UINT                                      NumCommandLists,
    HandlePointerDecoder<ID3D12CommandList*>* ppCommandLists)
{
    if (IsModificationPass())
    {
        return;
    }

    auto command_lists = ppCommandLists->GetPointer();
    for (UINT i = 0; i < NumCommandLists; ++i)
    {
        format::HandleId command_list_id = command_lists[i];
        if (command_list_related_infos_.find(command_list_id) != command_list_related_infos_.end())
        {
            command_list_related_infos_[command_list_id].related_resource_values.clear();
            command_list_related_infos_[command_list_id].resource_copies.clear();
            command_list_related_infos_[command_list_id].state_object_id = format::kNullHandleId;
        }
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_Reset(const ApiCallInfo& call_info,
                                                                     format::HandleId   object_id,
                                                                     HRESULT            return_value,
                                                                     format::HandleId   pAllocator,
                                                                     format::HandleId   pInitialState)
{
    if (IsModificationPass())
    {
        return;
    }

    if (command_list_related_infos_.find(object_id) != command_list_related_infos_.end())
    {
        command_list_related_infos_[object_id].related_resource_values.clear();
        command_list_related_infos_[object_id].resource_copies.clear();
        command_list_related_infos_[object_id].state_object_id = format::kNullHandleId;
    }
}

void Dx12RayTracingModifier::Process_ID3D12Device_CreateDescriptorHeap(
    const ApiCallInfo&                                        call_info,
    format::HandleId                                          object_id,
    HRESULT                                                   return_value,
    StructPointerDecoder<Decoded_D3D12_DESCRIPTOR_HEAP_DESC>* pDescriptorHeapDesc,
    Decoded_GUID                                              riid,
    HandlePointerDecoder<void*>*                              ppvHeap)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId heap_id = *ppvHeap->GetPointer();
    if (descriptor_heap_infos_.find(heap_id) == descriptor_heap_infos_.end())
    {
        DescriptorHeapDescInfo heap_info = {};
        descriptor_heap_infos_.emplace(heap_id, heap_info);
    }

    auto& heap_info            = descriptor_heap_infos_[heap_id];
    heap_info.handle_id        = heap_id;
    heap_info.object_id        = object_id;
    heap_info.descriptor_type  = pDescriptorHeapDesc->GetPointer()->Type;
    heap_info.descriptor_count = pDescriptorHeapDesc->GetPointer()->NumDescriptors;
}

void Dx12RayTracingModifier::Process_ID3D12Device_GetDescriptorHandleIncrementSize(
    const ApiCallInfo&         call_info,
    format::HandleId           object_id,
    UINT                       return_value,
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value == 0)
    {
        GFXRECON_LOG_WARNING("GetDescriptorHandleIncrementSize returned 0 for object ID: 0x%" PRIx64
                             ", descriptor heap type: %d",
                             object_id,
                             DescriptorHeapType);
        return;
    }

    auto iter = device_descriptor_increment_sizes_.find(object_id);
    if (iter == device_descriptor_increment_sizes_.end())
    {
        device_descriptor_increment_sizes_[object_id][DescriptorHeapType] = return_value;
    }
    else
    {
        auto increments = iter->second;
        if (increments.find(DescriptorHeapType) == increments.end())
        {
            device_descriptor_increment_sizes_[object_id][DescriptorHeapType] = return_value;
        }
        else
        {
            if (increments[DescriptorHeapType] != return_value)
            {
                GFXRECON_LOG_WARNING(
                    "GetDescriptorHandleIncrementSize returned different values for object ID: 0x%" PRIx64
                    ", descriptor heap type: %d, previous value: %u, new value: %u",
                    object_id,
                    DescriptorHeapType,
                    increments[DescriptorHeapType],
                    return_value);
            }
        }
    }
}

void Dx12RayTracingModifier::FindAccelerationStructureResourceFromGPUAddress(const D3D12_GPU_VIRTUAL_ADDRESS address)
{
    if (IsModificationPass())
    {
        return;
    }

    if ((address == 0) || (address < min_gpu_va_) || (address >= max_gpu_va_))
    {
        return;
    }

    format::HandleId accel_struct_id = FindBaseResourceFromGPUAddress(address);
    if (accel_struct_id == format::kNullHandleId)
    {
        return;
    }

    auto resource_iter = resource_entries_.find(accel_struct_id);
    if (resource_iter == resource_entries_.end())
    {
        return;
    }

    if (address % D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT != 0)
    {
        return;
    }

    if (((resource_iter->second.initial_state & D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) ==
         D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) ||
        ((resource_iter->second.desc.Flags & D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE) ==
         D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE))
    {
        accel_struct_address_resource_[address] = resource_iter->second;
    }
}

format::HandleId Dx12RayTracingModifier::FindBaseResourceFromGPUAddress(const D3D12_GPU_VIRTUAL_ADDRESS address)
{
    if (address == 0 || address < min_gpu_va_ || address >= max_gpu_va_)
    {
        return format::kNullHandleId;
    }

    auto entry = std::find_if(
        gpu_virtual_address_resource_.begin(), gpu_virtual_address_resource_.end(), [address](auto& entry) {
            {
                return ((entry.first == entry.second.start_virtual_address) &&
                        (address >= entry.second.start_virtual_address) &&
                        (address < entry.second.end_virtual_address));
            }
        });

    if (entry != gpu_virtual_address_resource_.end())
    {
        return entry->second.handle_id;
    }

    return format::kNullHandleId;
}

void Dx12RayTracingModifier::FindResourceRemapValues(
    const format::HandleId                       mapped_resource_id,
    const uint8_t*                               data,
    const uint64_t                               data_offset,
    const uint64_t                               data_size,
    std::vector<Dx12FillCommandResourceAddress>* found_resource_addresses)
{
    found_resource_addresses->clear();

    const uint64_t kDescSize      = sizeof(D3D12_GPU_DESCRIPTOR_HANDLE::ptr);
    const uint64_t kAddrSize      = sizeof(uint64_t);
    const uint64_t kIdSize        = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const uint64_t kMinDataStride = 1;

    // Since the index buffer format can be DXGI_FORMAT_R16_UINT, the GPU VA should be aligned to at least 2 bytes.
    const uint64_t kGpuVaAlignment = 2;

    // TODO: Before checking for GPU VA match, ensure that the data is valid.
    bool check_gpu_va = true;
    // TODO: Before checking for GPU descriptor handle match, ensure that the data is valid.
    bool check_gpu_descriptor = true;

    std::vector<uint8_t> zero_shader_id(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, 0);

    for (uint64_t i = 0; (i + kMinDataStride) < data_size; i += kMinDataStride)
    {
        Dx12FillCommandResourceAddress fill_cmd_resource_address;

        // First check for a shader id match.
        if ((i + kIdSize) <= data_size)
        {
            uint8_t* shader_id_ptr = const_cast<uint8_t*>(data) + i;

            if (0 == std::memcmp(shader_id_ptr, zero_shader_id.data(), kIdSize))
            {
                continue;
            }

            std::vector<uint8_t> shader_id(shader_id_ptr, shader_id_ptr + kIdSize);
            auto                 shader_id_iter = shader_id_to_properties_id_.find(shader_id);
            if (shader_id_iter != shader_id_to_properties_id_.end() && shader_id_iter->second != format::kNullHandleId)
            {
                auto properties_id = shader_id_iter->second;
                GFXRECON_LOG_DEBUG("Found shader identifier : 0x%" PRIx64 " offset %" PRIu64 " in resource ID: %" PRIu64
                                   ", data_offset %" PRIu64 " data_size %" PRIu64 " GetCurrentBlockIndex(%" PRIu64 ")",
                                   (uint64_t*)shader_id_ptr,
                                   i,
                                   mapped_resource_id,
                                   data_offset,
                                   data_size,
                                   GetCurrentBlockIndex());

                fill_cmd_resource_address.offset    = i;
                fill_cmd_resource_address.type      = format::ResourceValueType::kShaderIdentifier;
                fill_cmd_resource_address.object_id = properties_id;
                util::platform::MemoryCopy(fill_cmd_resource_address.shader_id,
                                           D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                                           shader_id_ptr,
                                           D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

                found_resource_addresses->emplace_back(fill_cmd_resource_address);
                i += kIdSize - kMinDataStride;
                continue;
            }
        }

        // Next check for GPU descriptor match.
        if (check_gpu_descriptor && ((i + kDescSize) <= data_size))
        {
            D3D12_GPU_DESCRIPTOR_HANDLE descriptor_handle = {};
            util::platform::MemoryCopy(&descriptor_handle.ptr,
                                       sizeof(D3D12_GPU_DESCRIPTOR_HANDLE::ptr),
                                       const_cast<uint8_t*>(data) + i,
                                       sizeof(D3D12_GPU_DESCRIPTOR_HANDLE::ptr));

            uint64_t handle_value = descriptor_handle.ptr;
            if ((handle_value != 0) && (handle_value >= min_gpu_descriptor_) && (handle_value < max_gpu_descriptor_))
            {
                auto entry = std::find_if(descriptor_start_address_info_.begin(),
                                          descriptor_start_address_info_.end(),
                                          [descriptor_handle](auto& entry) {
                                              return (descriptor_handle.ptr >= entry.second.capture_gpu_addr_begin) &&
                                                     (descriptor_handle.ptr < entry.second.capture_gpu_addr_end) &&
                                                     (((descriptor_handle.ptr - entry.second.capture_gpu_addr_begin) %
                                                       entry.second.capture_increment) == 0);
                                          });

                if (entry != descriptor_start_address_info_.end())
                {
                    GFXRECON_LOG_DEBUG("Found GPU descriptor handle: 0x%" PRIx64 " offset %" PRIu64
                                       " in resource ID: %" PRIu64 ", data_offset %" PRIu64 " data_size %" PRIu64
                                       " start handle: 0x%" PRIx64 ", end handle: 0x%" PRIx64
                                       " GetCurrentBlockIndex(%" PRIu64 ")",
                                       descriptor_handle.ptr,
                                       i,
                                       mapped_resource_id,
                                       data_offset,
                                       data_size,
                                       entry->second.capture_gpu_addr_begin,
                                       entry->second.capture_gpu_addr_end,
                                       GetCurrentBlockIndex());

                    fill_cmd_resource_address.offset         = i;
                    fill_cmd_resource_address.type           = format::ResourceValueType::kGpuDescriptorHandle;
                    fill_cmd_resource_address.object_id      = entry->second.handle_id;
                    fill_cmd_resource_address.start_value    = entry->second.capture_gpu_addr_begin;
                    fill_cmd_resource_address.adjusted_value = descriptor_handle.ptr;

                    found_resource_addresses->emplace_back(fill_cmd_resource_address);
                    i += kDescSize - kMinDataStride;
                    continue;
                }
            }
        }

        // Finally check for GPU VA match.
        if (check_gpu_va && ((i + kAddrSize) <= data_size))
        {
            D3D12_GPU_VIRTUAL_ADDRESS gpu_address = {};
            util::platform::MemoryCopy(&gpu_address,
                                       sizeof(D3D12_GPU_VIRTUAL_ADDRESS),
                                       const_cast<uint8_t*>(data) + i,
                                       sizeof(D3D12_GPU_VIRTUAL_ADDRESS));

            if ((gpu_address != 0) && (gpu_address >= min_gpu_va_) && (gpu_address < max_gpu_va_) &&
                ((gpu_address % kGpuVaAlignment) == 0))
            {
                // First check if the GPU VA is a raytracing acceleration structure.
                auto accel_struct_iter = accel_struct_address_resource_.find(gpu_address);
                if (accel_struct_iter != accel_struct_address_resource_.end())
                {
                    GFXRECON_LOG_DEBUG("Found acceleration structure address: 0x%" PRIx64 " offset %" PRIu64
                                       " in resource ID: %" PRIu64 ", data_offset %" PRIu64 " data_size %" PRIu64
                                       "  start address: 0x%" PRIx64 ", end address: 0x%" PRIx64
                                       " GetCurrentBlockIndex(%" PRIu64 ")",
                                       gpu_address,
                                       i,
                                       mapped_resource_id,
                                       data_offset,
                                       data_size,
                                       accel_struct_iter->second.start_virtual_address,
                                       accel_struct_iter->second.end_virtual_address,
                                       GetCurrentBlockIndex());

                    fill_cmd_resource_address.offset         = i;
                    fill_cmd_resource_address.type           = format::ResourceValueType::kGpuVirtualAddress;
                    fill_cmd_resource_address.object_id      = accel_struct_iter->second.handle_id;
                    fill_cmd_resource_address.start_value    = accel_struct_iter->second.start_virtual_address;
                    fill_cmd_resource_address.adjusted_value = gpu_address;

                    found_resource_addresses->emplace_back(fill_cmd_resource_address);
                    i += kAddrSize - kMinDataStride;
                    continue;
                }
                else
                {
                    auto entry = std::find_if(gpu_virtual_address_resource_.begin(),
                                              gpu_virtual_address_resource_.end(),
                                              [gpu_address](auto& entry) {
                                                  return (gpu_address >= entry.second.start_virtual_address) &&
                                                         (gpu_address < entry.second.end_virtual_address);
                                              });

                    if (entry != gpu_virtual_address_resource_.end())
                    {
                        GFXRECON_LOG_DEBUG("Found GPU virtual address: 0x%" PRIx64 " offset %" PRIu64
                                           " in resource ID: %" PRIu64 ", data_offset %" PRIu64 " data_size %" PRIu64
                                           "  start address: 0x%" PRIx64 ", end address: 0x%" PRIx64
                                           " GetCurrentBlockIndex(%" PRIu64 ")",
                                           gpu_address,
                                           i,
                                           mapped_resource_id,
                                           data_offset,
                                           data_size,
                                           entry->second.start_virtual_address,
                                           entry->second.end_virtual_address,
                                           GetCurrentBlockIndex());

                        fill_cmd_resource_address.offset         = i;
                        fill_cmd_resource_address.type           = format::ResourceValueType::kGpuVirtualAddress;
                        fill_cmd_resource_address.object_id      = entry->second.handle_id;
                        fill_cmd_resource_address.start_value    = entry->second.start_virtual_address;
                        fill_cmd_resource_address.adjusted_value = gpu_address;

                        found_resource_addresses->emplace_back(fill_cmd_resource_address);
                        i += kAddrSize - kMinDataStride;
                        continue;
                    }
                }
            }
        }
    }
}

void Dx12RayTracingModifier::Process_ID3D12Resource_Map(const ApiCallInfo&                         call_info,
                                                        format::HandleId                           object_id,
                                                        HRESULT                                    return_value,
                                                        UINT                                       Subresource,
                                                        StructPointerDecoder<Decoded_D3D12_RANGE>* pReadRange,
                                                        PointerDecoder<uint64_t, void*>*           ppData)
{
    if (IsModificationPass())
    {
        return;
    }

    if ((return_value != S_OK) || (ppData == nullptr) || (ppData->GetPointer() == nullptr))
    {
        return;
    }

    mapped_memory_resource_id_[*ppData->GetPointer()] = object_id;
    auto& memory_info                                 = mapped_memory_info_[object_id][Subresource];
    memory_info.memory_id                             = *ppData->GetPointer();
    ++(memory_info.count);
}

void Dx12RayTracingModifier::Process_ID3D12Resource_Unmap(const ApiCallInfo&                         call_info,
                                                          format::HandleId                           object_id,
                                                          UINT                                       Subresource,
                                                          StructPointerDecoder<Decoded_D3D12_RANGE>* pWrittenRange)
{
    if (IsModificationPass())
    {
        return;
    }

    if (mapped_memory_info_.find(object_id) != mapped_memory_info_.end())
    {
        auto entry = mapped_memory_info_[object_id].find(Subresource);
        if (entry != mapped_memory_info_[object_id].end())
        {
            auto& memory_info = entry->second;
            assert(memory_info.count > 0);
            --(memory_info.count);
            if (memory_info.count == 0)
            {
                mapped_memory_resource_id_.erase(memory_info.memory_id);
                mapped_memory_info_[object_id].erase(entry);
            }
        }
    }
}

void Dx12RayTracingModifier::Process_IUnknown_QueryInterface(const ApiCallInfo&           call_info,
                                                             format::HandleId             object_id,
                                                             HRESULT                      return_value,
                                                             Decoded_GUID                 riid,
                                                             HandlePointerDecoder<void*>* ppvObject)
{
    format::HandleId handle_id = *ppvObject->GetPointer();
    if (IsModificationPass())
    {
        if (*riid.decoded_value == __uuidof(ID3D12Resource) || *riid.decoded_value == __uuidof(ID3D12Resource1) ||
            *riid.decoded_value == __uuidof(ID3D12Resource2))
        {
            AddPrebuildInfoResourceValueCommand();
        }
        return;
    }

    if (return_value != S_OK)
    {
        return;
    }

    if (*riid.decoded_value == __uuidof(ID3D12Resource) || *riid.decoded_value == __uuidof(ID3D12Resource1) ||
        *riid.decoded_value == __uuidof(ID3D12Resource2))
    {
        if (resource_entries_.find(handle_id) == resource_entries_.end())
        {
            if (resource_entries_.find(object_id) != resource_entries_.end())
            {
                resource_entries_[handle_id]             = resource_entries_[object_id];
                resource_entries_[handle_id].handle_id   = handle_id;
                resource_entries_[handle_id].block_index = GetCurrentBlockIndex();
            }
        }
    }
    else if (*riid.decoded_value == __uuidof(ID3D12DescriptorHeap))
    {
        if (descriptor_heap_infos_.find(handle_id) == descriptor_heap_infos_.end())
        {
            if (descriptor_heap_infos_.find(object_id) != descriptor_heap_infos_.end())
            {
                descriptor_heap_infos_[handle_id] = descriptor_heap_infos_[object_id];
            }
        }
    }
    else if (*riid.decoded_value == __uuidof(ID3D12StateObjectProperties) ||
             *riid.decoded_value == __uuidof(ID3D12StateObjectProperties1))
    {
        state_object_properties_[object_id] = handle_id;
    }
}

void Dx12RayTracingModifier::Process_IUnknown_Release(const ApiCallInfo& call_info,
                                                      format::HandleId   object_id,
                                                      ULONG              return_value)
{
    if (IsModificationPass())
    {
        return;
    }

    if (return_value == 0)
    {
        auto resource_iter = resource_entries_.find(object_id);
        if (resource_iter != resource_entries_.end())
        {
            auto address   = resource_iter->second.start_virtual_address;
            auto addr_iter = gpu_virtual_address_resource_.find(address);
            if (addr_iter != gpu_virtual_address_resource_.end())
            {
                gpu_virtual_address_resource_.erase(addr_iter);
            }

            auto iter = accel_struct_address_resource_.begin();
            for (; iter != accel_struct_address_resource_.end();)
            {
                if (iter->second.handle_id == object_id)
                {
                    iter = accel_struct_address_resource_.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }

            auto desc_iter = acceleration_structure_build_desc_.begin();
            for (; desc_iter != acceleration_structure_build_desc_.end();)
            {
                if (desc_iter->second.handle_id == object_id)
                {
                    desc_iter = acceleration_structure_build_desc_.erase(desc_iter);
                }
                else
                {
                    ++desc_iter;
                }
            }

            resource_entries_.erase(resource_iter);
        }

        auto descriptor_iter = device_descriptor_increment_sizes_.find(object_id);
        if (descriptor_iter != device_descriptor_increment_sizes_.end())
        {
            device_descriptor_increment_sizes_.erase(descriptor_iter);
        }

        auto descriptor_heap_iter = descriptor_heap_infos_.find(object_id);
        if (descriptor_heap_iter != descriptor_heap_infos_.end())
        {
            descriptor_start_address_info_.erase(descriptor_heap_iter->second.capture_gpu_addr_begin);
            descriptor_heap_infos_.erase(descriptor_heap_iter);
        }

        if (command_list_related_infos_.find(object_id) != command_list_related_infos_.end())
        {
            command_list_related_infos_.erase(object_id);
        }

        auto state_object_iter = state_object_properties_.find(object_id);
        if (state_object_iter != state_object_properties_.end())
        {
            state_object_properties_.erase(state_object_iter);
        }
    }
}

void Dx12RayTracingModifier::ProcessFillMemoryResourceValueCommand(
    const format::FillMemoryResourceValueCommandHeader& command_header, const uint8_t* data)
{
    if (IsModificationPass())
    {
        // All old FillMemoryResourceValueCommand Will be deleted
        SetDeleteCurrentCall();
        return;
    }
}

void Dx12RayTracingModifier::ProcessInitSubresourceCommand(const format::InitSubresourceCommandHeader& command_header,
                                                           const uint8_t*                              data)
{
    if (IsModificationPass())
    {
        AddFillMemoryResourceAddressCommand(command_header.resource_id);
        return;
    }

    std::vector<Dx12FillCommandResourceAddress> found_resource_addresses;

    if (resource_entries_.find(command_header.resource_id) != resource_entries_.end())
    {
        auto& resource_info = resource_entries_[command_header.resource_id];
        if (resource_info.desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            return;
        }

        auto resource_id = resource_info.handle_id;
        auto data_size   = command_header.data_size;
        FindResourceRemapValues(resource_id, data, 0, data_size, &found_resource_addresses);
    }

    if (!found_resource_addresses.empty())
    {
        auto& resource_addresses = fill_cmd_resource_addresses_[GetCurrentBlockIndex()];
        for (const auto& found_resource_address : found_resource_addresses)
        {
            resource_addresses.push_back(found_resource_address);
        }
    }
}

void Dx12RayTracingModifier::ProcessFillMemoryResourceAddressCommand(
    const format::arm::FillMemoryResourceAddressCommandHeader& command_header, const uint8_t* data)
{
    if (IsModificationPass())
    {
        // All old FillMemoryResourceAddressCommand Will be deleted
        SetDeleteCurrentCall();
        return;
    }
}

void Dx12RayTracingModifier::ProcessFillMemoryCommand(uint64_t       memory_id,
                                                      uint64_t       offset,
                                                      uint64_t       size,
                                                      const uint8_t* data)
{
    if (IsModificationPass())
    {
        AddFillMemoryResourceAddressCommand(memory_id);
        return;
    }

    std::vector<Dx12FillCommandResourceAddress> found_resource_addresses;

    if (mapped_memory_resource_id_.find(memory_id) != mapped_memory_resource_id_.end())
    {
        const auto& mapped_resource_id = mapped_memory_resource_id_[memory_id];
        if (resource_entries_.find(mapped_resource_id) != resource_entries_.end())
        {
            const auto& resource_info = resource_entries_[mapped_resource_id];
            if (resource_info.desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
            {
                GFXRECON_LOG_ERROR("Mapped resource ID: %" PRIu64 " is not a buffer resource.", mapped_resource_id);
                return;
            }
        }

        FindResourceRemapValues(mapped_resource_id, data, offset, size, &found_resource_addresses);
    }

    if (!found_resource_addresses.empty())
    {
        auto& resource_addresses = fill_cmd_resource_addresses_[GetCurrentBlockIndex()];
        for (auto& found_resource_address : found_resource_addresses)
        {
            found_resource_address.offset += offset;
            resource_addresses.push_back(found_resource_address);
        }
    }
}

void Dx12RayTracingModifier::AddPrebuildInfoResourceValueCommand()
{
    auto prebuild_iter = prebuild_info_insert_values_.find(GetCurrentBlockIndex());
    if (prebuild_iter != prebuild_info_insert_values_.end() && !prebuild_iter->second.empty())
    {
        auto& build_descs = prebuild_iter->second;
        for (auto iter = build_descs.begin(); iter != build_descs.end(); ++iter)
        {
            const auto& prebuild_info = iter->second.get_prebuild_info;

            auto new_call       = CreatePreCall();
            new_call->type      = NewCallDataType::MetaDataCall;
            new_call->object_id = iter->second.object_id;
            new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
            new_call->thread_id = 1;

            format::arm::GetDx12AccelerationStructureSizeCommandHeader input_header;
            input_header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
            input_header.meta_header.block_header.size =
                format::GetMetaDataBlockBaseSize(input_header) + prebuild_info.GetDataSize();
            input_header.meta_header.meta_data_id =
                format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_D3D12,
                                       format::arm::MetaDataType::kGetDx12AccelerationStructureSizeCommand);
            input_header.thread_id                      = 1;
            input_header.device_id                      = iter->second.object_id;
            input_header.resource_id                    = iter->second.handle_id;
            input_header.acceleration_structure_address = iter->second.acceleration_structure_address;
            input_header.num_instance_descs             = iter->second.num_instance_descs;
            input_header.data_size                      = prebuild_info.GetDataSize();

            new_call->parameter_buffer.Write(&input_header,
                                             sizeof(format::arm::GetDx12AccelerationStructureSizeCommandHeader));
            new_call->parameter_buffer.Write(prebuild_info.GetData(), prebuild_info.GetDataSize());
        }

        prebuild_iter->second.clear();
    }
}

void Dx12RayTracingModifier::AddFillMemoryResourceAddressCommand(const uint64_t object_id)
{
    if (opt_fillmem_ == false)
    {
        return;
    }

    auto resource_addresses_iter = fill_cmd_resource_addresses_.find(GetCurrentBlockIndex());
    if (resource_addresses_iter != fill_cmd_resource_addresses_.end() && !resource_addresses_iter->second.empty())
    {
        auto&    resource_addresses     = resource_addresses_iter->second;
        uint64_t resource_address_count = resource_addresses.size();
        if (resource_address_count == 0)
        {
            return;
        }

        auto new_call       = CreatePreCall();
        new_call->type      = NewCallDataType::MetaDataCall;
        new_call->object_id = object_id;
        new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_Unknown;
        new_call->thread_id = 1;

        format::arm::FillMemoryResourceAddressCommandHeader ra_header;
        ra_header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
        ra_header.meta_header.block_header.size =
            format::GetMetaDataBlockBaseSize(ra_header) +
            (resource_address_count * sizeof(decode::Dx12FillCommandResourceAddress));
        ra_header.meta_header.meta_data_id = format::MakeMetaDataId(
            format::ApiFamilyId::ApiFamily_D3D12, format::arm::MetaDataType::kFillMemoryResourceAddressCommand);
        ra_header.thread_id              = 1;
        ra_header.resource_address_count = resource_address_count;
        size_t       header_size         = sizeof(format::arm::FillMemoryResourceAddressCommandHeader);
        const size_t uncompressed_size   = resource_address_count * sizeof(decode::Dx12FillCommandResourceAddress);

        new_call->parameter_buffer.Write(&ra_header, header_size);
        new_call->parameter_buffer.Write(resource_addresses.data(), uncompressed_size);

        resource_addresses_iter->second.clear();
    }
}

void Dx12RayTracingModifier::CreateDeviceAndCheckRayTracingSupport()
{
    graphics::dx12::IDXGIFactory4ComPtr factory = nullptr;
    HRESULT                             result  = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(result))
    {
        GFXRECON_LOG_WARNING("Failed to create DXGI factory for DXR offline device selection, HRESULT 0x%08x.",
                             static_cast<unsigned int>(result));
        return;
    }

    bool selected_device = false;

    auto inspect_adapter = [this, &factory, &selected_device](UINT index, bool should_select) -> bool {
        graphics::dx12::IDXGIAdapter1ComPtr adapter     = nullptr;
        HRESULT                             enum_result = factory->EnumAdapters1(index, &adapter.GetInterfacePtr());

        if (enum_result == DXGI_ERROR_NOT_FOUND)
        {
            return false;
        }

        if (FAILED(enum_result) || adapter == nullptr)
        {
            GFXRECON_LOG_WARNING("Failed to enumerate DXGI adapter index %u for DXR offline, HRESULT 0x%08x.",
                                 index,
                                 static_cast<unsigned int>(enum_result));
            return true;
        }

        DXGI_ADAPTER_DESC1 desc        = {};
        HRESULT            desc_result = adapter->GetDesc1(&desc);
        if (FAILED(desc_result))
        {
            GFXRECON_LOG_WARNING("Failed to query DXGI adapter desc for DXR offline GPU index %u, HRESULT 0x%08x.",
                                 index,
                                 static_cast<unsigned int>(desc_result));
            return true;
        }

        const bool is_software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;

        GFXRECON_LOG_INFO("DXR offline adapter: [Adapter %u] [%ls] [DeviceID 0x%04x] [VendorId 0x%04x] [Software %s]",
                          index,
                          desc.Description,
                          desc.DeviceId,
                          desc.VendorId,
                          is_software ? "true" : "false");

        if (is_software)
        {
            GFXRECON_LOG_INFO("DXR offline adapter %u skipped because it is a software adapter.", index);
            return true;
        }

        graphics::dx12::ID3D12Device5ComPtr device = nullptr;
        HRESULT                             create_result =
            D3D12CreateDevice(adapter.GetInterfacePtr(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

        if (FAILED(create_result) || device == nullptr)
        {
            GFXRECON_LOG_INFO("DXR offline adapter %u cannot create an ID3D12Device5, HRESULT 0x%08x.",
                              index,
                              static_cast<unsigned int>(create_result));
            return true;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature_data = {};
        HRESULT                           feature_result =
            device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &feature_data, sizeof(feature_data));

        if (FAILED(feature_result))
        {
            GFXRECON_LOG_INFO("DXR offline adapter %u failed D3D12_OPTIONS5 query, HRESULT 0x%08x.",
                              index,
                              static_cast<unsigned int>(feature_result));
            return true;
        }

        GFXRECON_LOG_INFO(
            "DXR offline adapter %u raytracing tier: %d.", index, static_cast<int>(feature_data.RaytracingTier));

        if (feature_data.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
        {
            GFXRECON_LOG_INFO("DXR offline adapter %u skipped because raytracing is not supported.", index);
            return true;
        }

        if (should_select)
        {
            HRESULT query_result = device->QueryInterface(IID_PPV_ARGS(&real_device5_));
            if (SUCCEEDED(query_result) && real_device5_ != nullptr)
            {
                selected_device = true;
                GFXRECON_LOG_INFO(
                    "DXR offline selected raytracing adapter: [Adapter %u] [%ls] [DeviceID 0x%04x] [VendorId 0x%04x]",
                    index,
                    desc.Description,
                    desc.DeviceId,
                    desc.VendorId);
            }
            else
            {
                GFXRECON_LOG_WARNING("Failed to query ID3D12Device5 for DXR offline adapter %u, HRESULT 0x%08x.",
                                     index,
                                     static_cast<unsigned int>(query_result));
            }
        }

        return true;
    };

    if (override_gpu_index_ >= 0)
    {
        GFXRECON_LOG_INFO("DXR offline using user-specified GPU index %d for raytracing prebuild info query.",
                          override_gpu_index_);
    }
    else
    {
        GFXRECON_LOG_INFO("DXR offline GPU index was not specified. Searching for the first hardware adapter with ray "
                          "tracing support.");
    }

    for (UINT index = 0;; ++index)
    {
        const bool should_select =
            (override_gpu_index_ >= 0) ? (index == static_cast<UINT>(override_gpu_index_)) : !selected_device;

        if (!inspect_adapter(index, should_select))
        {
            break;
        }
    }

    if (override_gpu_index_ >= 0 && !selected_device)
    {
        GFXRECON_LOG_WARNING(
            "The specified GPU index %d could not be used for DXR offline raytracing prebuild info query.",
            override_gpu_index_);
    }
    else if (override_gpu_index_ < 0 && !selected_device)
    {
        GFXRECON_LOG_WARNING(
            "DXR offline could not find a hardware adapter with raytracing support for prebuild info query.");
    }
}
bool Dx12RayTracingModifier::CanOptimize()
{
    if (opt_fillmem_ && (fill_cmd_resource_addresses_.size() > 0))
    {
        GFXRECON_WRITE_CONSOLE("Optimizing %zu FillMemoryCommand blocks for DXR/EI replay.",
                               fill_cmd_resource_addresses_.size());
    }

    return true;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
