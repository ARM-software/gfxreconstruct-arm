
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

#ifndef GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H

#include <unordered_map>
#include <map>
#include <vector>
#include <algorithm>

#include "decode/dx12_consumer_base.h"
#include "decode/dx12_resource_value_tracker.h"
#include "util/dx12_modifier_base.h"
#include "util/memory_output_stream.h"
#include "util/defines.h"
#include "util/hash.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

typedef uint64_t Dx12MethodCallBlockIndex;

#pragma pack(push)
#pragma pack(1)
// There will be many AccelerationStructurePreBuildDesc. Set struct packing to 1 to minimize memory used.
struct AccelerationStructurePreBuildDesc
{
    format::HandleId         handle_id{ format::kNullHandleId };
    format::HandleId         object_id{ format::kNullHandleId };
    uint64_t                 acceleration_structure_address{ 0 };
    uint64_t                 num_instance_descs{ 0 };
    util::MemoryOutputStream get_prebuild_info{};
};
#pragma pack(pop)
typedef std::map<UINT64, AccelerationStructurePreBuildDesc> AccelerationStructureVAToPreBuildDescs;
typedef std::map<Dx12MethodCallBlockIndex, AccelerationStructureVAToPreBuildDescs> Dx12PrebuildInfoResourceValueMap;

class Dx12RayTracingModifier : public util::Dx12ModifierBase
{
  public:
    Dx12RayTracingModifier() = default;

    explicit Dx12RayTracingModifier(int32_t override_gpu_index) : override_gpu_index_(override_gpu_index) {}

    virtual bool CanOptimize() override;

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
        const format::InitDx12AccelerationStructureCommandHeader&                           command_header,
        const std::vector<format::InitDx12AccelerationStructureGeometryDesc>&               geometry_descs,
        StructPointerDecoder<Decoded_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>* build_inputs,
        const uint8_t*                                                                      build_inputs_data) override;

    virtual void ProcessGetDx12AccelerationStructureSizeCommand(
        const format::arm::GetDx12AccelerationStructureSizeCommandHeader&                   command_header,
        StructPointerDecoder<Decoded_D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS>* input_descs) override;

    virtual void Process_ID3D12GraphicsCommandList4_CopyRaytracingAccelerationStructure(
        const ApiCallInfo&                                call_info,
        format::HandleId                                  object_id,
        D3D12_GPU_VIRTUAL_ADDRESS                         DestAccelerationStructureData,
        D3D12_GPU_VIRTUAL_ADDRESS                         SourceAccelerationStructureData,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) override;

    virtual void Process_ID3D12Device5_CreateStateObject(const ApiCallInfo& call_info,
                                                         format::HandleId   object_id,
                                                         HRESULT            return_value,
                                                         StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* pDesc,
                                                         Decoded_GUID                                           riid,
                                                         HandlePointerDecoder<void*>* ppStateObject) override;

    virtual void
    Process_ID3D12Device7_AddToStateObject(const ApiCallInfo&                                     call_info,
                                           format::HandleId                                       object_id,
                                           HRESULT                                                return_value,
                                           StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* pAddition,
                                           format::HandleId             pStateObjectToGrowFrom,
                                           Decoded_GUID                 riid,
                                           HandlePointerDecoder<void*>* ppNewStateObject) override;

    virtual void
    Process_D3D12SerializeRootSignature(const ApiCallInfo&                                       call_info,
                                        HRESULT                                                  return_value,
                                        StructPointerDecoder<Decoded_D3D12_ROOT_SIGNATURE_DESC>* pRootSignature,
                                        D3D_ROOT_SIGNATURE_VERSION                               Version,
                                        HandlePointerDecoder<ID3D10Blob*>*                       ppBlob,
                                        HandlePointerDecoder<ID3D10Blob*>*                       ppErrorBlob) override;

    virtual void Process_D3D12SerializeVersionedRootSignature(
        const ApiCallInfo&                                                 call_info,
        HRESULT                                                            return_value,
        StructPointerDecoder<Decoded_D3D12_VERSIONED_ROOT_SIGNATURE_DESC>* pRootSignature,
        HandlePointerDecoder<ID3D10Blob*>*                                 ppBlob,
        HandlePointerDecoder<ID3D10Blob*>*                                 ppErrorBlob) override;

    virtual void Process_ID3D12Device_CreateRootSignature(const ApiCallInfo&           call_info,
                                                          format::HandleId             object_id,
                                                          HRESULT                      return_value,
                                                          UINT                         nodeMask,
                                                          PointerDecoder<uint8_t>*     pBlobWithRootSignature,
                                                          SIZE_T                       blobLengthInBytes,
                                                          Decoded_GUID                 riid,
                                                          HandlePointerDecoder<void*>* ppvRootSignature) override;

    virtual void Process_ID3D12Device14_CreateRootSignatureFromSubobjectInLibrary(
        const ApiCallInfo&           call_info,
        format::HandleId             object_id,
        HRESULT                      return_value,
        UINT                         nodeMask,
        PointerDecoder<uint8_t>*     pLibraryBlob,
        SIZE_T                       blobLengthInBytes,
        WStringDecoder*              subobjectName,
        Decoded_GUID                 riid,
        HandlePointerDecoder<void*>* ppvRootSignature) override;

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
    ProcessFillMemoryResourceAddressCommand(const format::arm::FillMemoryResourceAddressCommandHeader& command_header,
                                            const uint8_t*                                             data) override;

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

  private:
    void FindAccelerationStructureResourceFromGPUAddress(const D3D12_GPU_VIRTUAL_ADDRESS address);

    format::HandleId FindBaseResourceFromGPUAddress(const D3D12_GPU_VIRTUAL_ADDRESS address);

    void FindResourceRemapValues(const format::HandleId                       mapped_resource_id,
                                 const uint8_t*                               data,
                                 const uint64_t                               data_offset,
                                 const uint64_t                               data_size,
                                 std::vector<Dx12FillCommandResourceAddress>* found_resource_addresses);

    void Process_BuildRaytracingAccelerationStructure(
        const ApiCallInfo&                                                 call_info,
        format::HandleId                                                   object_id,
        const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC*          desc,
        UINT                                                               num_post_build_descs,
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* post_build_descs);

    void Process_CopyRaytracingAccelerationStructure(const ApiCallInfo&        call_info,
                                                     format::HandleId          object_id,
                                                     D3D12_GPU_VIRTUAL_ADDRESS dest_acceleration_structure_data,
                                                     D3D12_GPU_VIRTUAL_ADDRESS source_acceleration_structure_data,
                                                     D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE mode);

    void AddPrebuildInfoResourceValueCommand();

    void AddFillMemoryResourceAddressCommand(const uint64_t object_id);

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
        D3D12_GPU_VIRTUAL_ADDRESS end_virtual_address{ 0 };
        uint64_t                  block_index{ 0 };
    };

    struct DescriptorHeapDescInfo
    {
        format::HandleId           handle_id{ format::kNullHandleId };
        format::HandleId           object_id{ format::kNullHandleId };
        D3D12_DESCRIPTOR_HEAP_TYPE descriptor_type{};
        uint32_t                   descriptor_count{ 0 };
        uint64_t                   capture_gpu_addr_begin{ kNullGpuAddress };
        uint64_t                   capture_gpu_addr_end{ kNullCpuAddress };
        uint64_t                   capture_cpu_addr_begin{ kNullGpuAddress };
        uint64_t                   capture_cpu_addr_end{ kNullCpuAddress };
        uint64_t                   capture_increment{ 0 };
    };

    struct ResourceCopyInfo
    {
        format::HandleId dst_resource_id{ format::kNullHandleId };
        uint64_t         dst_offset{ 0 };
        format::HandleId src_resource_id{ format::kNullHandleId };
        uint64_t         src_offset{ 0 };
        uint64_t         num_bytes{ 0 }; ///< 0 indicates copying the entire resource.
    };

    struct ResourceValueInfo
    {
        uint64_t          offset{ 0 };
        ResourceValueType type{ ResourceValueType::kUnknown };
        uint64_t          size{ 0 };
    };

    struct CommandListInfo
    {
        format::HandleId              state_object_id{ format::kNullHandleId };
        std::vector<ResourceCopyInfo> resource_copies{};

        // resource handle -> resource value info
        std::map<format::HandleId, ResourceValueInfo> related_resource_values{};
    };

    struct AccelerationStructureBuildDesc
    {
        format::HandleId                                               handle_id{ format::kNullHandleId };
        format::HandleId                                               object_id{ format::kNullHandleId };
        bool                                                           is_first_built{ false };
        bool                                                           is_meta_copy{ false };
        D3D12_GPU_VIRTUAL_ADDRESS                                      source_of_compaction{ 0 };
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS           build_blas_inputs{};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS           build_tlas_inputs{};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS           build_omm_inputs{};
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>                    geometry_descs{};
        std::map<uint32_t, D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC>   omm_triangles_geometry_descs{};
        std::map<uint32_t, D3D12_RAYTRACING_GEOMETRY_OMM_LINKAGE_DESC> omm_linkage_geometry_descs{};
        std::vector<D3D12_RAYTRACING_OPACITY_MICROMAP_ARRAY_DESC>      omm_array_descs;
        std::map<uint32_t, std::vector<D3D12_RAYTRACING_OPACITY_MICROMAP_HISTOGRAM_ENTRY>> omm_array_histograms;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO                              real_prebuild_info{};
        // Post-build info only recorded POSTBUILD_INFO_COMPACTED_SIZE.
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC postbuild_info{};
    };

    graphics::dx12::ID3D12Device5ComPtr real_device5_;
    void                                CreateDeviceAndCheckRayTracingSupport();

  private:
    uint64_t min_gpu_va_{ UINT64_MAX };
    uint64_t max_gpu_va_{ 0 };
    uint64_t min_gpu_descriptor_{ UINT64_MAX };
    uint64_t max_gpu_descriptor_{ 0 };

    // Minimum GPU descriptor increment size for D3D12_DESCRIPTOR_HEAP_TYPE
    const uint64_t min_gpu_descriptor_increment_ = 16;

    // GPU index to override for replay. If set to -1, the GPU index will not be overridden.
    int32_t override_gpu_index_ = -1;

    // -----state object-----state object properties-----
    std::unordered_map<format::HandleId, format::HandleId> state_object_properties_;

    // -----device handle-----D3D12_DESCRIPTOR_HEAP_TYPE------increment size
    std::unordered_map<format::HandleId, std::unordered_map<D3D12_DESCRIPTOR_HEAP_TYPE, uint64_t>>
        device_descriptor_increment_sizes_;

    // -----descriptor heap handle-----D3D12DescriptorHeapInfo
    std::unordered_map<format::HandleId, DescriptorHeapDescInfo> descriptor_heap_infos_;

    // -----descriptor heap address-----D3D12DescriptorHeapInfo
    std::unordered_map<uint64_t, DescriptorHeapDescInfo> descriptor_start_address_info_;

    // -----resource handle-----ResourceObject
    std::unordered_map<format::HandleId, ResourceObject> resource_entries_;

    // -----mapped pointer id-----resource handle
    std::unordered_map<uint64_t, format::HandleId> mapped_memory_resource_id_;

    // -----resource handle-----subresource index-----mapped memory info
    std::unordered_map<format::HandleId, std::unordered_map<uint32_t, MappedMemoryInfo>> mapped_memory_info_;

    // -----gpu virtual address-----ResourceObject
    std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, ResourceObject> gpu_virtual_address_resource_;

    // -----acceleration structure gpu virtual address-----ResourceObject
    std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, ResourceObject> accel_struct_address_resource_;

    // -----acceleration structure gpu virtual address-----AccelerationStructureBuildDesc
    std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, AccelerationStructureBuildDesc> acceleration_structure_build_desc_;

    // -----command list id-----CommandListInfo
    std::unordered_map<format::HandleId, CommandListInfo> command_list_related_infos_;

    // -----command signature id-----related D3D12_INDIRECT_ARGUMENT_TYPE
    std::unordered_map<format::HandleId, std::vector<D3D12_INDIRECT_ARGUMENT_TYPE>> command_signature_related_types_;

    // -----root signature id-----related D3D12_ROOT_PARAMETER_TYPE
    std::unordered_map<format::HandleId, std::vector<D3D12_ROOT_PARAMETER_TYPE>> root_signature_related_types_;

    // -----latest blob id-----related D3D12_ROOT_PARAMETER_TYPE
    std::unordered_map<format::HandleId, std::vector<D3D12_ROOT_PARAMETER_TYPE>> latest_blob_related_types_;

    struct VectorHash
    {
        std::size_t operator()(const std::vector<uint8_t>& v) const
        {
            return gfxrecon::util::hash::hash_range(v.begin(), v.end());
        }
    };
    // -----shader_id-----state object properties id
    std::unordered_map<std::vector<uint8_t>, format::HandleId, VectorHash> shader_id_to_properties_id_;

    Dx12PrebuildInfoResourceValueMap  prebuild_info_insert_values_;
    Dx12FillCommandResourceAddressMap fill_cmd_resource_addresses_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_DX12_RAYTRACING_MODIFIER_H
