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
    virtual_address_resource_[return_value] = object_id;
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
    resource_entries_[handle].desc          = *pDesc->GetPointer();
    resource_entries_[handle].initial_state = InitialResourceState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
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
    resource_entries_[handle].desc          = *pDesc->GetPointer();
    resource_entries_[handle].initial_state = InitialResourceState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
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
    resource_entries_[handle].desc1         = *pDesc->GetPointer();
    resource_entries_[handle].initial_state = InitialResourceState;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
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
    format::HandleId handle                 = *ppvResource->GetPointer();
    resource_entries_[handle].handle_id     = handle;
    resource_entries_[handle].object_id     = object_id;
    resource_entries_[handle].heap_props    = *pHeapProperties->GetPointer();
    resource_entries_[handle].heap_flags    = HeapFlags;
    resource_entries_[handle].desc1         = *pDesc->GetPointer();
    resource_entries_[handle].initial_state = D3D12_RESOURCE_STATE_COMMON;
    resource_entries_[handle].block_index   = GetCurrentBlockIndex();
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

    if (virtual_address_resource_.find(pDesc_struct->SourceAccelerationStructureData) !=
        virtual_address_resource_.end())
    {
        src_id = virtual_address_resource_[pDesc_struct->SourceAccelerationStructureData];
    }

    if (virtual_address_resource_.find(pDesc_struct->DestAccelerationStructureData) != virtual_address_resource_.end())
    {
        dst_id = virtual_address_resource_[pDesc_struct->DestAccelerationStructureData];
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

void Dx12RayTracingModifier::Process_ID3D12GraphicsCommandList4_CopyRaytracingAccelerationStructure(
    const ApiCallInfo&                                call_info,
    format::HandleId                                  object_id,
    D3D12_GPU_VIRTUAL_ADDRESS                         DestAccelerationStructureData,
    D3D12_GPU_VIRTUAL_ADDRESS                         SourceAccelerationStructureData,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode)
{
    format::HandleId src_id = format::kNullHandleId;
    format::HandleId dst_id = format::kNullHandleId;

    if (virtual_address_resource_.find(SourceAccelerationStructureData) != virtual_address_resource_.end())
    {
        src_id = virtual_address_resource_[SourceAccelerationStructureData];
    }

    if (virtual_address_resource_.find(DestAccelerationStructureData) != virtual_address_resource_.end())
    {
        dst_id = virtual_address_resource_[DestAccelerationStructureData];
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

void Dx12RayTracingModifier::GetTrackedResourceValues(Dx12PrebuildInfoResourceValueMap& values)
{
    values = std::move(prebuild_info_insert_values_);
    prebuild_info_insert_values_.clear();
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
