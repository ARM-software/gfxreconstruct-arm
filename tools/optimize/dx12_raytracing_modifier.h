#ifndef GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H

#include <unordered_map>
#include <map>
#include <algorithm>

#include "decode/dx12_consumer_base.h"
#include "decode/dx12_descriptor_map.h"
#include "decode/dx12_resource_value_tracker.h"
#include "graphics/dx12_gpu_va_map.h"
#include "graphics/dx12_shader_id_map.h"
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

    virtual void Process_ID3D12StateObjectProperties_GetShaderIdentifier(const ApiCallInfo&       call_info,
                                                                         format::HandleId         object_id,
                                                                         PointerDecoder<uint8_t>* return_value,
                                                                         WStringDecoder*          pExportName) override;

    virtual void Process_ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(
        const ApiCallInfo&                  call_info,
        format::HandleId                    object_id,
        Decoded_D3D12_GPU_DESCRIPTOR_HANDLE return_value) override;

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

    virtual void
    Process_ID3D12Device_CreatePlacedResource(const ApiCallInfo&                                 call_info,
                                              format::HandleId                                   object_id,
                                              HRESULT                                            return_value,
                                              format::HandleId                                   pHeap,
                                              UINT64                                             HeapOffset,
                                              StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>* pDesc,
                                              D3D12_RESOURCE_STATES                              InitialState,
                                              StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*   pOptimizedClearValue,
                                              Decoded_GUID                                       riid,
                                              HandlePointerDecoder<void*>*                       ppvResource) override;

    virtual void
    Process_ID3D12Device8_CreatePlacedResource1(const ApiCallInfo&                                  call_info,
                                                format::HandleId                                    object_id,
                                                HRESULT                                             return_value,
                                                format::HandleId                                    pHeap,
                                                UINT64                                              HeapOffset,
                                                StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC1>* pDesc,
                                                D3D12_RESOURCE_STATES                               InitialState,
                                                StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>* pOptimizedClearValue,
                                                Decoded_GUID                                     riid,
                                                HandlePointerDecoder<void*>*                     ppvResource) override;

    virtual void
    Process_ID3D12Device10_CreatePlacedResource2(const ApiCallInfo&                                  call_info,
                                                 format::HandleId                                    object_id,
                                                 HRESULT                                             return_value,
                                                 format::HandleId                                    pHeap,
                                                 UINT64                                              HeapOffset,
                                                 StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC1>* pDesc,
                                                 D3D12_BARRIER_LAYOUT                                InitialLayout,
                                                 StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>* pOptimizedClearValue,
                                                 UINT32                                           NumCastableFormats,
                                                 PointerDecoder<DXGI_FORMAT>*                     pCastableFormats,
                                                 Decoded_GUID                                     riid,
                                                 HandlePointerDecoder<void*>*                     ppvResource) override;

    virtual void
    Process_ID3D12Device_CreateReservedResource(const ApiCallInfo&                                 call_info,
                                                format::HandleId                                   object_id,
                                                HRESULT                                            return_value,
                                                StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>* pDesc,
                                                D3D12_RESOURCE_STATES                              InitialState,
                                                StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>*   pOptimizedClearValue,
                                                Decoded_GUID                                       riid,
                                                HandlePointerDecoder<void*>* ppvResource) override;

    virtual void
    Process_ID3D12Device4_CreateReservedResource1(const ApiCallInfo&                                 call_info,
                                                  format::HandleId                                   object_id,
                                                  HRESULT                                            return_value,
                                                  StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>* pDesc,
                                                  D3D12_RESOURCE_STATES                              InitialState,
                                                  StructPointerDecoder<Decoded_D3D12_CLEAR_VALUE>* pOptimizedClearValue,
                                                  format::HandleId                                 pProtectedSession,
                                                  Decoded_GUID                                     riid,
                                                  HandlePointerDecoder<void*>* ppvResource) override;

    virtual void Process_ID3D12Device10_CreateReservedResource2(
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
        HandlePointerDecoder<void*>*                       ppvResource) override;

    virtual void Process_ID3D12GraphicsCommandList4_BuildRaytracingAccelerationStructure(
        const ApiCallInfo&                                                                call_info,
        format::HandleId                                                                  object_id,
        StructPointerDecoder<Decoded_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC>* pDesc,
        UINT                                                                              NumPostbuildInfoDescs,
        StructPointerDecoder<Decoded_D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC>* pPostbuildInfoDescs)
        override;

    virtual void ProcessInitDx12AccelerationStructureCommand(
        const format::InitDx12AccelerationStructureCommandHeader&       command_header,
        std::vector<format::InitDx12AccelerationStructureGeometryDesc>& geometry_descs,
        const uint8_t*                                                  build_inputs_data) override;

    virtual void Process_ID3D12GraphicsCommandList4_CopyRaytracingAccelerationStructure(
        const ApiCallInfo&                                call_info,
        format::HandleId                                  object_id,
        D3D12_GPU_VIRTUAL_ADDRESS                         DestAccelerationStructureData,
        D3D12_GPU_VIRTUAL_ADDRESS                         SourceAccelerationStructureData,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) override;

    virtual void
    Process_ID3D12Device_CreateCommandSignature(const ApiCallInfo& call_info,
                                                format::HandleId   object_id,
                                                HRESULT            return_value,
                                                StructPointerDecoder<Decoded_D3D12_COMMAND_SIGNATURE_DESC>* pDesc,
                                                format::HandleId             pRootSignature,
                                                Decoded_GUID                 riid,
                                                HandlePointerDecoder<void*>* ppvCommandSignature) override;

    virtual void Process_ID3D12GraphicsCommandList_ExecuteIndirect(const ApiCallInfo& call_info,
                                                                   format::HandleId   object_id,
                                                                   format::HandleId   pCommandSignature,
                                                                   UINT               MaxCommandCount,
                                                                   format::HandleId   pArgumentBuffer,
                                                                   UINT64             ArgumentBufferOffset,
                                                                   format::HandleId   pCountBuffer,
                                                                   UINT64             CountBufferOffset) override;

    virtual void Process_ID3D12GraphicsCommandList4_SetPipelineState1(const ApiCallInfo& call_info,
                                                                      format::HandleId   object_id,
                                                                      format::HandleId   pStateObject) override;

    virtual void Process_ID3D12GraphicsCommandList4_DispatchRays(
        const ApiCallInfo&                                      call_info,
        format::HandleId                                        object_id,
        StructPointerDecoder<Decoded_D3D12_DISPATCH_RAYS_DESC>* pDesc) override;

    virtual void Process_ID3D12Device_CreateCommandList(const ApiCallInfo&           call_info,
                                                        format::HandleId             object_id,
                                                        HRESULT                      return_value,
                                                        UINT                         nodeMask,
                                                        D3D12_COMMAND_LIST_TYPE      type,
                                                        format::HandleId             pCommandAllocator,
                                                        format::HandleId             pInitialState,
                                                        Decoded_GUID                 riid,
                                                        HandlePointerDecoder<void*>* ppCommandList) override;

    virtual void Process_ID3D12Device4_CreateCommandList1(const ApiCallInfo&           call_info,
                                                          format::HandleId             object_id,
                                                          HRESULT                      return_value,
                                                          UINT                         nodeMask,
                                                          D3D12_COMMAND_LIST_TYPE      type,
                                                          D3D12_COMMAND_LIST_FLAGS     flags,
                                                          Decoded_GUID                 riid,
                                                          HandlePointerDecoder<void*>* ppCommandList) override;

    virtual void Process_ID3D12GraphicsCommandList_Dispatch(const ApiCallInfo& call_info,
                                                            format::HandleId   object_id,
                                                            UINT               ThreadGroupCountX,
                                                            UINT               ThreadGroupCountY,
                                                            UINT               ThreadGroupCountZ) override;

    virtual void Process_ID3D12GraphicsCommandList_CopyBufferRegion(const ApiCallInfo& call_info,
                                                                    format::HandleId   object_id,
                                                                    format::HandleId   pDstBuffer,
                                                                    UINT64             DstOffset,
                                                                    format::HandleId   pSrcBuffer,
                                                                    UINT64             SrcOffset,
                                                                    UINT64             NumBytes) override;

    virtual void Process_ID3D12GraphicsCommandList_CopyResource(const ApiCallInfo& call_info,
                                                                format::HandleId   object_id,
                                                                format::HandleId   pDstResource,
                                                                format::HandleId   pSrcResource) override;

    virtual void
    Process_ID3D12CommandQueue_ExecuteCommandLists(const ApiCallInfo&                        call_info,
                                                   format::HandleId                          object_id,
                                                   UINT                                      NumCommandLists,
                                                   HandlePointerDecoder<ID3D12CommandList*>* ppCommandLists) override;

    virtual void Process_ID3D12GraphicsCommandList_Reset(const ApiCallInfo& call_info,
                                                         format::HandleId   object_id,
                                                         HRESULT            return_value,
                                                         format::HandleId   pAllocator,
                                                         format::HandleId   pInitialState) override;

    virtual void Process_ID3D12Resource_Map(const ApiCallInfo&                         call_info,
                                            format::HandleId                           object_id,
                                            HRESULT                                    return_value,
                                            UINT                                       Subresource,
                                            StructPointerDecoder<Decoded_D3D12_RANGE>* pReadRange,
                                            PointerDecoder<uint64_t, void*>*           ppData) override;

    virtual void Process_ID3D12Resource_Unmap(const ApiCallInfo&                         call_info,
                                              format::HandleId                           object_id,
                                              UINT                                       Subresource,
                                              StructPointerDecoder<Decoded_D3D12_RANGE>* pWrittenRange) override;

    virtual void Process_IUnknown_QueryInterface(const ApiCallInfo&           call_info,
                                                 format::HandleId             object_id,
                                                 HRESULT                      return_value,
                                                 Decoded_GUID                 riid,
                                                 HandlePointerDecoder<void*>* ppvObject) override;

    virtual void
    Process_IUnknown_Release(const ApiCallInfo& call_info, format::HandleId object_id, ULONG return_value) override;

    virtual void
    ProcessFillMemoryResourceValueCommand(const format::FillMemoryResourceValueCommandHeader& command_header,
                                          const uint8_t*                                      data) override;

    virtual void ProcessInitSubresourceCommand(const format::InitSubresourceCommandHeader& command_header,
                                               const uint8_t*                              data) override;

    virtual void
    ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override;

    virtual void Process_ID3D12Device_CreateDescriptorHeap(
        const ApiCallInfo&                                        call_info,
        format::HandleId                                          object_id,
        HRESULT                                                   return_value,
        StructPointerDecoder<Decoded_D3D12_DESCRIPTOR_HEAP_DESC>* pDescriptorHeapDesc,
        Decoded_GUID                                              riid,
        HandlePointerDecoder<void*>*                              ppvHeap) override;

    virtual void
    Process_ID3D12Device_GetDescriptorHandleIncrementSize(const ApiCallInfo&         call_info,
                                                          format::HandleId           object_id,
                                                          UINT                       return_value,
                                                          D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) override;

    void GetTrackedResourceValues(Dx12PrebuildInfoResourceValueMap& prebuild_values,
                                  Dx12FillCommandResourceValueMap&  resource_values);

  private:
    void FindResourceRemapValues(const uint8_t*                                               data,
                                 uint64_t                                                     data_size,
                                 std::vector<std::pair<uint64_t, format::ResourceValueType>>* found_resource_values);

  private:
    struct ResourceObject
    {
        format::HandleId          handle_id{ format::kNullHandleId };
        format::HandleId          object_id{ format::kNullHandleId };
        D3D12_HEAP_PROPERTIES     heap_props{};
        D3D12_HEAP_FLAGS          heap_flags{};
        format::HandleId          heap_id{ format::kNullHandleId };
        D3D12_RESOURCE_DESC1      desc{};
        D3D12_RESOURCE_STATES     initial_state{};
        D3D12_BARRIER_LAYOUT      initial_layout{};
        D3D12_GPU_VIRTUAL_ADDRESS start_virtual_address{ 0 };
        uint64_t                  block_index{ 0 };
    };

    struct DescriptorHeapDescInfo
    {
        D3D12_DESCRIPTOR_HEAP_TYPE            descriptor_type{};
        uint32_t                              descriptor_count{ 0 };
        uint64_t                              capture_gpu_addr_begin{ kNullGpuAddress };
        uint64_t                              capture_gpu_addr_end{ kNullCpuAddress };
        uint64_t                              capture_cpu_addr_begin{ kNullGpuAddress };
        uint64_t                              capture_cpu_addr_end{ kNullCpuAddress };
        std::shared_ptr<DescriptorIncrements> capture_increments;
    };

  private:
    uint64_t min_gpu_va_{ UINT64_MAX };
    uint64_t max_gpu_va_{ 0 };
    uint64_t min_gpu_descriptor_{ UINT64_MAX };
    uint64_t max_gpu_descriptor_{ 0 };
    uint64_t min_gpu_descriptor_alignment_{ UINT64_MAX };

    // -----shader identifier-----shader identifier
    graphics::Dx12ShaderIdMap shader_id_map_;

    // -----device handle-----DescriptorIncrements
    std::unordered_map<format::HandleId, std::shared_ptr<DescriptorIncrements>> device_descriptor_increment_sizes_;

    // -----descriptor heap handle-----D3D12DescriptorHeapInfo
    std::unordered_map<format::HandleId, DescriptorHeapDescInfo> descriptor_heap_infos_;

    // -----descriptor heap address-----D3D12DescriptorHeapInfo
    std::unordered_map<uint64_t, DescriptorHeapDescInfo> descriptor_start_address_info_;

    // -----resource handle-----ResourceObject
    std::unordered_map<format::HandleId, ResourceObject> resource_entries_;

    // -----mpped pointer id-----resource handle
    std::unordered_map<uint64_t, format::HandleId> mapped_memory_resource_id_;

    // -----resource handle-----subresource index-----mapped memory info
    std::unordered_map<format::HandleId, std::unordered_map<uint32_t, MappedMemoryInfo>> mapped_memory_info_;

    // -----gpu virtual address-----ResourceObject
    std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, ResourceObject> gpu_virtual_address_resource_;

    // -----acceleration structure handle-----AccelerationStructureBuildDesc
    std::unordered_map<format::HandleId, AccelerationStructureBuildDesc> acceleration_structure_build_desc_;

    // -----command list id-----related id
    std::unordered_map<format::HandleId, std::vector<format::HandleId>> command_list_related_ids_;

    // -----command signature id-----related id
    std::unordered_map<format::HandleId, std::vector<format::HandleId>> command_signature_related_ids_;

    Dx12PrebuildInfoResourceValueMap prebuild_info_insert_values_;
    Dx12FillCommandResourceValueMap  fill_cmd_resource_values_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H
