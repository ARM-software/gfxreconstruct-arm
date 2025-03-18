/*
** Copyright (c) 2020 LunarG, Inc.
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

#ifndef GFXRECON_DECODE_DX12_RESOURCE_ALLOCATOR_H
#define GFXRECON_DECODE_DX12_RESOURCE_ALLOCATOR_H

#include "format/format.h"
#include "util/defines.h"
#include <d3d12.h>
#include <dxgi.h>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class Dx12ResourceAllocator
{
  public:
    virtual ~Dx12ResourceAllocator() {}

    virtual HRESULT Initialize(const IUnknown* adapter, const void* pvDevice) = 0;

    virtual void Destroy() = 0;

    virtual HRESULT CreateHeap(_In_ const D3D12_HEAP_DESC* pDesc, REFIID riid, _COM_Outptr_opt_ void** ppvHeap) = 0;

    virtual HRESULT CreateCommittedResource(_In_ const D3D12_HEAP_PROPERTIES* pHeapProperties,
                                            D3D12_HEAP_FLAGS                  HeapFlags,
                                            _In_ const D3D12_RESOURCE_DESC*   pDesc,
                                            D3D12_RESOURCE_STATES             InitialResourceState,
                                            _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                            REFIID                            riidResource,
                                            _COM_Outptr_opt_ void**           ppvResource) = 0;

    virtual HRESULT CreatePlacedResource(_In_ ID3D12Heap*                  pHeap,
                                         UINT64                            HeapOffset,
                                         _In_ const D3D12_RESOURCE_DESC*   pDesc,
                                         D3D12_RESOURCE_STATES             InitialState,
                                         _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                         REFIID                            riid,
                                         _COM_Outptr_opt_ void**           ppvResource) = 0;

    virtual HRESULT CreateReservedResource(_In_ const D3D12_RESOURCE_DESC*   pDesc,
                                           D3D12_RESOURCE_STATES             InitialState,
                                           _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                           REFIID                            riid,
                                           _COM_Outptr_opt_ void**           ppvResource) = 0;

    virtual HRESULT CreateCommittedResource1(_In_ const D3D12_HEAP_PROPERTIES*        pHeapProperties,
                                             D3D12_HEAP_FLAGS                         HeapFlags,
                                             _In_ const D3D12_RESOURCE_DESC*          pDesc,
                                             D3D12_RESOURCE_STATES                    InitialResourceState,
                                             _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                             _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                             REFIID                                   riidResource,
                                             _COM_Outptr_opt_ void**                  ppvResource) = 0;

    virtual HRESULT CreatePlacedResource1(_In_ ID3D12Heap*                  pHeap,
                                          UINT64                            HeapOffset,
                                          _In_ const D3D12_RESOURCE_DESC1*  pDesc,
                                          D3D12_RESOURCE_STATES             InitialState,
                                          _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                          REFIID                            riid,
                                          _COM_Outptr_opt_ void**           ppvResource) = 0;

    virtual HRESULT CreateReservedResource1(_In_ const D3D12_RESOURCE_DESC*          pDesc,
                                            D3D12_RESOURCE_STATES                    InitialState,
                                            _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                            _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                            REFIID                                   riid,
                                            _COM_Outptr_opt_ void**                  ppvResource) = 0;

    virtual HRESULT CreateCommittedResource2(_In_ const D3D12_HEAP_PROPERTIES*        pHeapProperties,
                                             D3D12_HEAP_FLAGS                         HeapFlags,
                                             _In_ const D3D12_RESOURCE_DESC1*         pDesc,
                                             D3D12_RESOURCE_STATES                    InitialResourceState,
                                             _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                             _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                             REFIID                                   riidResource,
                                             _COM_Outptr_opt_ void**                  ppvResource) = 0;

    virtual HRESULT CreatePlacedResource2(_In_ ID3D12Heap*                                      pHeap,
                                          UINT64                                                HeapOffset,
                                          _In_ const D3D12_RESOURCE_DESC1*                      pDesc,
                                          D3D12_BARRIER_LAYOUT                                  InitialLayout,
                                          _In_opt_ const D3D12_CLEAR_VALUE*                     pOptimizedClearValue,
                                          UINT32                                                NumCastableFormats,
                                          _In_opt_count_(NumCastableFormats) const DXGI_FORMAT* pCastableFormats,
                                          REFIID                                                riid,
                                          _COM_Outptr_opt_ void**                               ppvResource) = 0;

    virtual HRESULT CreateReservedResource2(_In_ const D3D12_RESOURCE_DESC*                       pDesc,
                                            D3D12_BARRIER_LAYOUT                                  InitialLayout,
                                            _In_opt_ const D3D12_CLEAR_VALUE*                     pOptimizedClearValue,
                                            _In_opt_ ID3D12ProtectedResourceSession*              pProtectedSession,
                                            UINT32                                                NumCastableFormats,
                                            _In_opt_count_(NumCastableFormats) const DXGI_FORMAT* pCastableFormats,
                                            REFIID                                                riid,
                                            _COM_Outptr_opt_ void**                               ppvResource) = 0;

    virtual HRESULT CreateCommittedResource3(_In_ const D3D12_HEAP_PROPERTIES*                     pHeapProperties,
                                             D3D12_HEAP_FLAGS                                      HeapFlags,
                                             _In_ const D3D12_RESOURCE_DESC1*                      pDesc,
                                             D3D12_BARRIER_LAYOUT                                  InitialLayout,
                                             _In_opt_ const D3D12_CLEAR_VALUE*                     pOptimizedClearValue,
                                             _In_opt_ ID3D12ProtectedResourceSession*              pProtectedSession,
                                             UINT32                                                NumCastableFormats,
                                             _In_opt_count_(NumCastableFormats) const DXGI_FORMAT* pCastableFormats,
                                             REFIID                                                riidResource,
                                             _COM_Outptr_opt_ void**                               ppvResource) = 0;

    virtual void GetResourceTiling(_In_ ID3D12Resource*             pTiledResource,
                                   _Out_opt_ UINT*                  pNumTilesForEntireResource,
                                   _Out_opt_ D3D12_PACKED_MIP_INFO* pPackedMipDesc,
                                   _Out_opt_ D3D12_TILE_SHAPE*      pStandardTileShapeForNonPackedMips,
                                   _Inout_opt_ UINT*                pNumSubresourceTilings,
                                   _In_ UINT                        FirstSubresourceTilingToGet,
                                   _Out_ D3D12_SUBRESOURCE_TILING*  pSubresourceTilingsForNonPackedMips) = 0;

    virtual void UpdateTileMappings(ID3D12CommandQueue*  pQueue,
                                    format::HandleId     heap_capture_id,
                                    _In_ ID3D12Resource* pResource,
                                    UINT                 NumResourceRegions,
                                    _In_reads_opt_(NumResourceRegions)
                                        const D3D12_TILED_RESOURCE_COORDINATE* pResourceRegionStartCoordinates,
                                    _In_reads_opt_(NumResourceRegions)
                                        const D3D12_TILE_REGION_SIZE*                       pResourceRegionSizes,
                                    _In_opt_ ID3D12Heap*                                    pHeap,
                                    UINT                                                    NumRanges,
                                    _In_reads_opt_(NumRanges) const D3D12_TILE_RANGE_FLAGS* pRangeFlags,
                                    _In_reads_opt_(NumRanges) const UINT*                   pHeapRangeStartOffsets,
                                    _In_reads_opt_(NumRanges) const UINT*                   pRangeTileCounts,
                                    D3D12_TILE_MAPPING_FLAGS                                Flags) = 0;

    virtual bool SupportD3D12MemoryAllocator() = 0;

    virtual void Release(IUnknown* object) = 0;

    virtual void PostCreateHeap(format::HandleId            capture_id,
                                _In_ const D3D12_HEAP_DESC* pDesc,
                                REFIID                      riid,
                                _COM_Outptr_opt_ void**     ppvHeap) = 0;

    virtual void ReportResourceIncompatibility(const D3D12_RESOURCE_DESC* pResourceDesc) = 0;

    virtual void ReportResourceIncompatibility2(const D3D12_RESOURCE_DESC1* pResourceDesc) = 0;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_DX12_RESOURCE_ALLOCATOR_H
