#ifndef GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H

#include <unordered_map>
#include <map>
#include <algorithm>

#include "decode/dx12_consumer_base.h"
#include "generated/generated_dx12_consumer.h"
#include "util/memory_output_stream.h"
#include "util/defines.h"
#include "format/format.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

typedef uint64_t Dx12MethodCallBlockIndex;

#pragma pack(push)
#pragma pack(1)
// There will be many AccelerationStructureBuildDesc. Set struct packing to 1 to minimize memory used.
struct AccelerationStructureBuildDesc
{
    format::HandleId         handle_id;
    format::HandleId         object_id;
    bool                     is_first_built;
    util::MemoryOutputStream get_prebuild_info;
    bool                     is_meta_copy;
    format::HandleId         source_of_compaction;
};
#pragma pack(pop)
typedef std::map<Dx12MethodCallBlockIndex, AccelerationStructureBuildDesc> Dx12PrebuildInfoResourceValueMap;

class Dx12RayTracingModifier : public decode::Dx12Consumer
{
  public:
    Dx12RayTracingModifier() = default;

    virtual void Process_ID3D12Resource_GetGPUVirtualAddress(const ApiCallInfo&        call_info,
                                                             format::HandleId          object_id,
                                                             D3D12_GPU_VIRTUAL_ADDRESS return_value) override;

    virtual void
    Process_ID3D12Device_CreateCommittedResource(const ApiCallInfo&                                   call_info,
                                                 format::HandleId                                     object_id,
                                                 HRESULT                                              return_value,
                                                 StructPointerDecoder<Decoded_D3D12_HEAP_PROPERTIES>* pHeapProperties,
                                                 D3D12_HEAP_FLAGS                                     HeapFlags,
                                                 StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>*   pDesc,
                                                 D3D12_RESOURCE_STATES                            InitialResourceState,
                                                 StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>* pOptimizedClearValue,
                                                 Decoded_GUID                                     riidResource,
                                                 HandlePointerDecoder<void*>*                     ppvResource) override;

    virtual void Process_ID3D12Device4_CreateCommittedResource1(
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
        HandlePointerDecoder<void*>*                         ppvResource) override;

    virtual void Process_ID3D12Device8_CreateCommittedResource2(
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
        HandlePointerDecoder<void*>*                         ppvResource) override;

    virtual void Process_ID3D12Device10_CreateCommittedResource3(
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
        HandlePointerDecoder<void*>*                         ppvResource) override;

    virtual void Process_ID3D12GraphicsCommandList4_BuildRaytracingAccelerationStructure(
        const ApiCallInfo&                                                                call_info,
        format::HandleId                                                                  object_id,
        StructPointerDecoder<Decoded_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC>* pDesc,
        UINT                                                                              NumPostbuildInfoDescs,
        StructPointerDecoder<Decoded_D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC>* pPostbuildInfoDescs)
        override;

    virtual void Process_ID3D12GraphicsCommandList4_CopyRaytracingAccelerationStructure(
        const ApiCallInfo&                                call_info,
        format::HandleId                                  object_id,
        D3D12_GPU_VIRTUAL_ADDRESS                         DestAccelerationStructureData,
        D3D12_GPU_VIRTUAL_ADDRESS                         SourceAccelerationStructureData,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) override;

    void GetTrackedResourceValues(Dx12PrebuildInfoResourceValueMap& values);

  private:
    struct ResourceObject
    {
        format::HandleId      handle_id;
        format::HandleId      object_id;
        D3D12_HEAP_PROPERTIES heap_props;
        D3D12_HEAP_FLAGS      heap_flags;
        D3D12_RESOURCE_DESC   desc;
        D3D12_RESOURCE_DESC1  desc1;
        D3D12_RESOURCE_STATES initial_state;
        uint64_t              block_index;
    };

  private:
    // -----resource handle-----ResourceObject
    std::unordered_map<format::HandleId, ResourceObject> resource_entries_;

    // -----gpu virtual address-----handle id
    std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, format::HandleId> virtual_address_resource_;

    // -----acceleration structure handle-----AccelerationStructureBuildDesc
    std::unordered_map<format::HandleId, AccelerationStructureBuildDesc> acceleration_structure_build_desc_;

    Dx12PrebuildInfoResourceValueMap prebuild_info_insert_values_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H
