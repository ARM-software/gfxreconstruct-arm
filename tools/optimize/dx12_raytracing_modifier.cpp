
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
#include "encode/custom_ags_api_call_encoders.h"
#include "encode/custom_dx12_struct_encoders.h"
#include "generated/generated_dx12_api_call_encoders.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

void Dx12RayTracingModifier::Process_ID3D12Resource_GetGPUVirtualAddress(const ApiCallInfo&        call_info,
                                                                         format::HandleId          object_id,
                                                                         D3D12_GPU_VIRTUAL_ADDRESS return_value)
{
    if (resource_entries_.find(object_id) != resource_entries_.end())
    {
        if (resource_entries_[object_id].start_virtual_address == 0)
        {
            resource_entries_[object_id].start_virtual_address = return_value;
        }

        UINT64 width = resource_entries_[object_id].desc.Width;
        min_gpu_va_  = std::min(min_gpu_va_, return_value);
        max_gpu_va_  = std::max(max_gpu_va_, return_value + width);

        gpu_virtual_address_resource_[return_value] = resource_entries_[object_id];
    }
    else
    {
        GFXRECON_LOG_ERROR("Failed to find resource object id %llu in resource_entries_ map.", object_id);
    }
}

void Dx12RayTracingModifier::Process_ID3D12StateObjectProperties_GetShaderIdentifier(
    const ApiCallInfo&       call_info,
    format::HandleId         object_id,
    PointerDecoder<uint8_t>* return_value,
    WStringDecoder*          pExportName)
{
    if ((return_value != nullptr) && !return_value->IsNull())
    {
        shader_id_map_.Add(return_value->GetPointer(), return_value->GetPointer());
    }
}

void Dx12RayTracingModifier::Process_ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(
    const ApiCallInfo& call_info, format::HandleId object_id, Decoded_D3D12_GPU_DESCRIPTOR_HANDLE return_value)
{
    if (descriptor_heap_infos_.find(object_id) == descriptor_heap_infos_.end())
    {
        GFXRECON_LOG_ERROR("Failed to find descriptor heap object id %llu in descriptor_heap_infos_ map.", object_id);
        return;
    }

    auto&    heap_info = descriptor_heap_infos_[object_id];
    uint64_t descriptor_size =
        static_cast<uint64_t>(heap_info.descriptor_count) * (*heap_info.capture_increments)[heap_info.descriptor_type];
    if (heap_info.capture_gpu_addr_begin == kNullGpuAddress)
    {
        heap_info.capture_gpu_addr_begin = return_value.decoded_value->ptr;
        heap_info.capture_gpu_addr_end   = return_value.decoded_value->ptr + descriptor_size;
    }
    descriptor_start_address_info_[return_value.decoded_value->ptr] = heap_info;

    min_gpu_descriptor_ = std::min(min_gpu_descriptor_, (*return_value.decoded_value).ptr);
    max_gpu_descriptor_ = std::max(max_gpu_descriptor_, (*return_value.decoded_value).ptr + descriptor_size);
    for (auto increment : *heap_info.capture_increments)
    {
        if (increment > 0)
        {
            min_gpu_descriptor_alignment_ = std::min(min_gpu_descriptor_alignment_, static_cast<uint64_t>(increment));
        }
    }
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
    const auto       pDesc_struct = pDesc->GetPointer();
    format::HandleId src_id       = format::kNullHandleId;
    format::HandleId dst_id       = format::kNullHandleId;

    if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
    {
        command_list_related_ids_[object_id].push_back(dst_id);
    }

    auto src_address = pDesc_struct->SourceAccelerationStructureData;
    auto dst_address = pDesc_struct->DestAccelerationStructureData;
    if (gpu_virtual_address_resource_.find(src_address) != gpu_virtual_address_resource_.end())
    {
        src_id = gpu_virtual_address_resource_[src_address].handle_id;
    }

    if (gpu_virtual_address_resource_.find(dst_address) != gpu_virtual_address_resource_.end())
    {
        dst_id = gpu_virtual_address_resource_[dst_address].handle_id;
    }

    if ((dst_id != format::kNullHandleId) && (src_id == format::kNullHandleId))
    {
        if ((acceleration_structure_build_desc_.find(dst_id) == acceleration_structure_build_desc_.end()) &&
            (resource_entries_.find(dst_id) != resource_entries_.end()))
        {
            AccelerationStructureBuildDesc build_desc;
            build_desc.handle_id            = dst_id;
            build_desc.object_id            = resource_entries_[dst_id].object_id;
            build_desc.is_first_built       = true;
            build_desc.is_meta_copy         = false;
            build_desc.source_of_compaction = format::kNullHandleId;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info{ 0, 0, 0 };
            gfxrecon::encode::ParameterEncoder                    encoder(&build_desc.get_prebuild_info);
            encode::EncodeStructPtr(&encoder, &(pDesc_struct->Inputs));
            encode::EncodeStructPtr(&encoder, &prebuild_info);

            acceleration_structure_build_desc_.emplace(std::make_pair(dst_id, build_desc));
            prebuild_info_insert_values_.emplace(std::make_pair(resource_entries_[dst_id].block_index, build_desc));
        }
    }
}

void Dx12RayTracingModifier::ProcessInitDx12AccelerationStructureCommand(
    const format::InitDx12AccelerationStructureCommandHeader&       command_header,
    std::vector<format::InitDx12AccelerationStructureGeometryDesc>& geometry_descs,
    const uint8_t*                                                  build_inputs_data)
{
    return;
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList4_CopyRaytracingAccelerationStructure(
    const ApiCallInfo&                                call_info,
    format::HandleId                                  object_id,
    D3D12_GPU_VIRTUAL_ADDRESS                         DestAccelerationStructureData,
    D3D12_GPU_VIRTUAL_ADDRESS                         SourceAccelerationStructureData,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode)
{
    format::HandleId src_id = format::kNullHandleId;
    format::HandleId dst_id = format::kNullHandleId;

    if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
    {
        command_list_related_ids_[object_id].push_back(dst_id);
    }

    auto src_address = SourceAccelerationStructureData;
    auto dst_address = DestAccelerationStructureData;
    if (gpu_virtual_address_resource_.find(src_address) != gpu_virtual_address_resource_.end())
    {
        src_id = gpu_virtual_address_resource_[src_address].handle_id;
    }

    if (gpu_virtual_address_resource_.find(dst_address) != gpu_virtual_address_resource_.end())
    {
        dst_id = gpu_virtual_address_resource_[dst_address].handle_id;
    }

    if ((dst_id != format::kNullHandleId) && (src_id != format::kNullHandleId))
    {
        if ((acceleration_structure_build_desc_.find(src_id) != acceleration_structure_build_desc_.end()) &&
            (acceleration_structure_build_desc_.find(dst_id) == acceleration_structure_build_desc_.end()))
        {
            if (Mode == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT)
            {
                AccelerationStructureBuildDesc build_desc;
                build_desc.handle_id            = dst_id;
                build_desc.object_id            = resource_entries_[dst_id].object_id;
                build_desc.is_first_built       = false;
                build_desc.is_meta_copy         = true;
                build_desc.source_of_compaction = src_id;

                acceleration_structure_build_desc_.emplace(std::make_pair(dst_id, build_desc));
            }
            else
            {
                acceleration_structure_build_desc_.emplace(
                    std::make_pair(dst_id, acceleration_structure_build_desc_[src_id]));
            }

            // TODO current only insert GetRaytracingAccelerationStructurePrebuildInfo
            auto& build_desc = acceleration_structure_build_desc_[src_id];
            prebuild_info_insert_values_.emplace(std::make_pair(resource_entries_[dst_id].block_index, build_desc));
        }
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
    auto desc = pDesc->GetPointer();
    if (desc && desc->NumArgumentDescs > 0 && desc->pArgumentDescs)
    {
        format::HandleId              command_signature_id = *ppvCommandSignature->GetPointer();
        std::vector<format::HandleId> related_ids;

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
                command_signature_related_ids_.emplace(command_signature_id, related_ids);
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
    if (command_signature_related_ids_.find(pCommandSignature) != command_signature_related_ids_.end())
    {
        if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
        {
            command_list_related_ids_[object_id].push_back(pArgumentBuffer);
        }
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList4_SetPipelineState1(const ApiCallInfo& call_info,
                                                                                  format::HandleId   object_id,
                                                                                  format::HandleId   pStateObject)
{
    if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
    {
        command_list_related_ids_[object_id].push_back(pStateObject);
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList4_DispatchRays(
    const ApiCallInfo&                                      call_info,
    format::HandleId                                        object_id,
    StructPointerDecoder<Decoded_D3D12_DISPATCH_RAYS_DESC>* pDesc)
{
    if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
    {
        command_list_related_ids_[object_id].push_back(object_id);
    }
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
    if (type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_COMPUTE ||
        type == D3D12_COMMAND_LIST_TYPE_COPY)
    {
        std::vector<format::HandleId> command_list_handles;
        auto                          cmd_list_id = *ppCommandList->GetPointer();
        command_list_related_ids_.emplace(cmd_list_id, command_list_handles);
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
    if (type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_COMPUTE ||
        type == D3D12_COMMAND_LIST_TYPE_COPY)
    {
        std::vector<format::HandleId> command_list_handles;
        auto                          cmd_list_id = *ppCommandList->GetPointer();
        command_list_related_ids_.emplace(cmd_list_id, command_list_handles);
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_Dispatch(const ApiCallInfo& call_info,
                                                                        format::HandleId   object_id,
                                                                        UINT               ThreadGroupCountX,
                                                                        UINT               ThreadGroupCountY,
                                                                        UINT               ThreadGroupCountZ)
{
    if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
    {
        command_list_related_ids_[object_id].push_back(object_id);
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_CopyBufferRegion(const ApiCallInfo& call_info,
                                                                                format::HandleId   object_id,
                                                                                format::HandleId   pDstBuffer,
                                                                                UINT64             DstOffset,
                                                                                format::HandleId   pSrcBuffer,
                                                                                UINT64             SrcOffset,
                                                                                UINT64             NumBytes)
{
    if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
    {
        command_list_related_ids_[object_id].push_back(pDstBuffer);
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_CopyResource(const ApiCallInfo& call_info,
                                                                            format::HandleId   object_id,
                                                                            format::HandleId   pDstResource,
                                                                            format::HandleId   pSrcResource)
{
    if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
    {
        command_list_related_ids_[object_id].push_back(pDstResource);
    }
}

void Dx12RayTracingModifier::Process_ID3D12CommandQueue_ExecuteCommandLists(
    const ApiCallInfo&                        call_info,
    format::HandleId                          object_id,
    UINT                                      NumCommandLists,
    HandlePointerDecoder<ID3D12CommandList*>* ppCommandLists)
{
    auto command_lists = ppCommandLists->GetPointer();
    for (UINT i = 0; i < NumCommandLists; ++i)
    {
        format::HandleId command_list_id = command_lists[i];
        if (command_list_related_ids_.find(command_list_id) != command_list_related_ids_.end())
        {
            command_list_related_ids_[command_list_id].clear();
        }
    }
}

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList_Reset(const ApiCallInfo& call_info,
                                                                     format::HandleId   object_id,
                                                                     HRESULT            return_value,
                                                                     format::HandleId   pAllocator,
                                                                     format::HandleId   pInitialState)
{
    if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
    {
        command_list_related_ids_[object_id].clear();
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
    format::HandleId heap_id = *ppvHeap->GetPointer();
    if (descriptor_heap_infos_.find(heap_id) == descriptor_heap_infos_.end())
    {
        DescriptorHeapDescInfo heap_info;
        descriptor_heap_infos_.emplace(heap_id, heap_info);
    }

    auto& heap_info            = descriptor_heap_infos_[heap_id];
    heap_info.descriptor_type  = pDescriptorHeapDesc->GetPointer()->Type;
    heap_info.descriptor_count = pDescriptorHeapDesc->GetPointer()->NumDescriptors;

    if (device_descriptor_increment_sizes_.find(object_id) != device_descriptor_increment_sizes_.end())
    {
        heap_info.capture_increments = device_descriptor_increment_sizes_[object_id];
    }
}

void Dx12RayTracingModifier::Process_ID3D12Device_GetDescriptorHandleIncrementSize(
    const ApiCallInfo&         call_info,
    format::HandleId           object_id,
    UINT                       return_value,
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
    if (device_descriptor_increment_sizes_.find(object_id) == device_descriptor_increment_sizes_.end())
    {
        auto increments                               = std::make_shared<DescriptorIncrements>();
        device_descriptor_increment_sizes_[object_id] = increments;
    }

    auto increments                   = device_descriptor_increment_sizes_[object_id];
    (*increments)[DescriptorHeapType] = return_value;
    for (auto& heap_infos : descriptor_heap_infos_)
    {
        if (heap_infos.second.capture_increments == nullptr)
        {
            if (heap_infos.second.descriptor_type == DescriptorHeapType)
            {
                heap_infos.second.capture_increments = increments;
            }
        }
    }
}

void Dx12RayTracingModifier::FindResourceRemapValues(
    const uint8_t*                                               data,
    uint64_t                                                     data_size,
    std::vector<std::pair<uint64_t, format::ResourceValueType>>* found_resource_values)
{
    found_resource_values->clear();

    const graphics::Dx12ShaderIdentifier zero_shader_id = { 0 };
    const uint64_t                       kDescSize      = sizeof(D3D12_GPU_DESCRIPTOR_HANDLE::ptr);
    const uint64_t                       kAddrSize      = sizeof(uint64_t);
    const uint64_t                       kIdSize        = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const uint64_t                       kMinDataStride = 4;

    // TODO: Before checking for GPU VA match, ensure that the data is valid.
    bool check_gpu_va = true;

    for (uint64_t i = 0; (i + kMinDataStride) <= data_size; i += kMinDataStride)
    {
        // First check for a shader id match.
        if ((i + kIdSize) <= data_size)
        {
            uint8_t*                       shader_id  = const_cast<uint8_t*>(data) + i;
            graphics::Dx12ShaderIdentifier current_id = graphics::PackDx12ShaderIdentifier(shader_id);
            if (current_id == zero_shader_id)
            {
                continue;
            }

            std::vector<uint8_t> shader_id_vec(shader_id, shader_id + kIdSize);
            if (shader_id_map_.Map(shader_id_vec.data()))
            {
                found_resource_values->emplace_back(i, format::ResourceValueType::kShaderIdentifier);
                i += kIdSize - kMinDataStride;
                continue;
            }
        }

        // Next check for GPU descriptor match.
        if ((i + kDescSize) <= data_size)
        {
            uint64_t* handle_value = (uint64_t*)((uint8_t*)data + i);
            if ((*handle_value >= min_gpu_descriptor_) && (*handle_value < max_gpu_descriptor_) &&
                (*handle_value % min_gpu_descriptor_alignment_ == 0))
            {
                D3D12_GPU_DESCRIPTOR_HANDLE old_descriptor;
                old_descriptor.ptr = *handle_value;

                auto entry = std::find_if(descriptor_start_address_info_.begin(),
                                          descriptor_start_address_info_.end(),
                                          [old_descriptor](auto& entry) {
                                              return (old_descriptor.ptr >= entry.second.capture_gpu_addr_begin) &&
                                                     (old_descriptor.ptr <= entry.second.capture_gpu_addr_end);
                                          });

                if (entry != descriptor_start_address_info_.end())
                {
                    found_resource_values->emplace_back(i, format::ResourceValueType::kGpuDescriptorHandle);
                    i += kDescSize - kMinDataStride;
                    continue;
                }
            }
        }

        // Finally check for GPU VA match.
        if (check_gpu_va && ((i + kAddrSize) <= data_size) && (i % sizeof(uint64_t) == 0))
        {
            uint64_t* address_value = (uint64_t*)((uint8_t*)data + i);
            if ((*address_value >= min_gpu_va_) && (*address_value < max_gpu_va_) &&
                (*address_value % kMinDataStride == 0))
            {
                uint64_t old_address = *address_value;

                auto entry = std::find_if(gpu_virtual_address_resource_.begin(),
                                          gpu_virtual_address_resource_.end(),
                                          [old_address](auto& entry) {
                                              return (old_address >= entry.first) &&
                                                     (old_address <= entry.first + entry.second.desc.Width);
                                          });

                if (entry != gpu_virtual_address_resource_.end())
                {
                    found_resource_values->emplace_back(i, format::ResourceValueType::kGpuVirtualAddress);
                    i += kAddrSize - kMinDataStride;
                    continue;
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
    if (return_value != S_OK)
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
    if (return_value != S_OK)
    {
        return;
    }

    format::HandleId handle_id = *ppvObject->GetPointer();
    if (*riid.decoded_value == __uuidof(ID3D12Resource) || *riid.decoded_value == __uuidof(ID3D12Resource1) ||
        *riid.decoded_value == __uuidof(ID3D12Resource2))
    {
        if (resource_entries_.find(object_id) != resource_entries_.end())
        {
            resource_entries_[handle_id]             = resource_entries_[object_id];
            resource_entries_[handle_id].handle_id   = handle_id;
            resource_entries_[handle_id].block_index = GetCurrentBlockIndex();
        }
    }
    else if (*riid.decoded_value == __uuidof(ID3D12DescriptorHeap))
    {
        if (descriptor_heap_infos_.find(object_id) != descriptor_heap_infos_.end())
        {
            descriptor_heap_infos_[handle_id] = descriptor_heap_infos_[object_id];
        }
    }
}

void Dx12RayTracingModifier::Process_IUnknown_Release(const ApiCallInfo& call_info,
                                                      format::HandleId   object_id,
                                                      ULONG              return_value)
{
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

            resource_entries_.erase(resource_iter);
        }

        auto acc_str_iter = acceleration_structure_build_desc_.find(object_id);
        if (acc_str_iter != acceleration_structure_build_desc_.end())
        {
            acceleration_structure_build_desc_.erase(acc_str_iter);
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

        if (command_list_related_ids_.find(object_id) != command_list_related_ids_.end())
        {
            command_list_related_ids_.erase(object_id);
        }
    }
}

void Dx12RayTracingModifier::ProcessFillMemoryResourceValueCommand(
    const format::FillMemoryResourceValueCommandHeader& command_header, const uint8_t* data)
{
    // All old FillMemoryResourceValueCommand Will be deleted
    return;
}

void Dx12RayTracingModifier::ProcessInitSubresourceCommand(const format::InitSubresourceCommandHeader& command_header,
                                                           const uint8_t*                              data)
{
    if (resource_entries_.find(command_header.resource_id) != resource_entries_.end())
    {
        auto& resource_info = resource_entries_[command_header.resource_id];
        if (resource_info.desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            return;
        }
    }

    std::vector<std::pair<uint64_t, format::ResourceValueType>> found_resource_values;
    FindResourceRemapValues(data, command_header.data_size, &found_resource_values);

    for (const auto& found_resource_value : found_resource_values)
    {
        auto& resource_values = fill_cmd_resource_values_[GetCurrentBlockIndex()];
        resource_values.push_back({ found_resource_value.first, found_resource_value.second });
    }
}

void Dx12RayTracingModifier::ProcessFillMemoryCommand(uint64_t       memory_id,
                                                      uint64_t       offset,
                                                      uint64_t       size,
                                                      const uint8_t* data)
{
    if (mapped_memory_resource_id_.find(memory_id) != mapped_memory_resource_id_.end())
    {
        if (resource_entries_.find(mapped_memory_resource_id_[memory_id]) != resource_entries_.end())
        {
            auto& resource_info = resource_entries_[mapped_memory_resource_id_[memory_id]];
            if (resource_info.desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
            {
                return;
            }
        }
    }

    std::vector<std::pair<uint64_t, format::ResourceValueType>> found_resource_values;
    FindResourceRemapValues(data, size, &found_resource_values);

    for (const auto& found_resource_value : found_resource_values)
    {
        auto& resource_values = fill_cmd_resource_values_[GetCurrentBlockIndex()];
        resource_values.push_back({ found_resource_value.first + offset, found_resource_value.second });
    }
}

void Dx12RayTracingModifier::GetTrackedResourceValues(Dx12PrebuildInfoResourceValueMap& prebuild_values,
                                                      Dx12FillCommandResourceValueMap&  resource_values)
{
    prebuild_values = std::move(prebuild_info_insert_values_);
    prebuild_info_insert_values_.clear();

    resource_values = std::move(fill_cmd_resource_values_);
    fill_cmd_resource_values_.clear();
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
