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

// Define VMA_ASSERT for use in vk_mem_alloc.h
// For debug compiles, VMA_ASSERT failure is treated as a warning.
// For release compiles, VMA_ASSERT failure is a no-op.
// The expr_ parameter can be in the form of 'condition && "error string"'.
// The error string will be printed if condition is false.

#include "decode/dx12_rebind_allocator.h"
#include "graphics/dx12_util.h"
#include "util/logging.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

Dx12RebindAllocator::Dx12RebindAllocator() : allocator_(nullptr), device_(nullptr) {}

HRESULT Dx12RebindAllocator::Initialize(const IUnknown* adapter, const void* pvDevice)
{
    device_                      = reinterpret_cast<ID3D12Device*>(const_cast<void*>(pvDevice));
    D3D12MA::ALLOCATOR_DESC desc = {};
    desc.Flags                   = D3D12MA::ALLOCATOR_FLAG_NONE;
    desc.pDevice                 = reinterpret_cast<ID3D12Device*>(const_cast<void*>(pvDevice));
    desc.pAdapter                = reinterpret_cast<IDXGIAdapter*>(const_cast<IUnknown*>(adapter));

    HRESULT result = D3D12MA::CreateAllocator(&desc, &allocator_);
    return result;
}

void Dx12RebindAllocator::Destroy()
{
    device_ = nullptr;
    if (allocator_)
    {
        allocator_->Release();
        allocator_ = nullptr;
    }
}

void Dx12RebindAllocator::SetReplayResourceDescAlignment(const D3D12_RESOURCE_DESC* pResourceDesc)
{
    D3D12_RESOURCE_ALLOCATION_INFO  alloc_info  = {};
    D3D12_RESOURCE_ALLOCATION_INFO1 alloc_info1 = {};

    if (device_ != nullptr)
    {
        graphics::dx12::ID3D12Device4ComPtr device4;
        device_->QueryInterface(IID_PPV_ARGS(&device4));

        if (device4 != nullptr)
        {
            device4->GetResourceAllocationInfo1(0, 1, pResourceDesc, &alloc_info1);
            alloc_info.SizeInBytes = alloc_info1.SizeInBytes;
            alloc_info.Alignment   = alloc_info1.Alignment;
        }
        else
        {
            alloc_info = device_->GetResourceAllocationInfo(0, 1, pResourceDesc);
        }

        if (alloc_info.Alignment && pResourceDesc->Alignment && alloc_info.Alignment != pResourceDesc->Alignment)
        {
            const_cast<D3D12_RESOURCE_DESC*>(pResourceDesc)->Alignment = alloc_info.Alignment;
        }

        if (pResourceDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            if (alloc_info.SizeInBytes && alloc_info.SizeInBytes != pResourceDesc->Width)
            {
                const_cast<D3D12_RESOURCE_DESC*>(pResourceDesc)->Width = alloc_info.SizeInBytes;
            }
        }
    }
}

void Dx12RebindAllocator::SetReplayResourceDescAlignment1(const D3D12_RESOURCE_DESC1* pResourceDesc)
{
    D3D12_RESOURCE_ALLOCATION_INFO  alloc_info  = {};
    D3D12_RESOURCE_ALLOCATION_INFO1 alloc_info1 = {};

    if (device_ != nullptr)
    {
        graphics::dx12::ID3D12Device8ComPtr device8;
        device_->QueryInterface(IID_PPV_ARGS(&device8));

        if (device8 != nullptr)
        {
            device8->GetResourceAllocationInfo2(0, 1, pResourceDesc, &alloc_info1);
            alloc_info.SizeInBytes = alloc_info1.SizeInBytes;
            alloc_info.Alignment   = alloc_info1.Alignment;
        }
        else
        {
            D3D12_RESOURCE_DESC* desc =
                reinterpret_cast<D3D12_RESOURCE_DESC*>(const_cast<D3D12_RESOURCE_DESC1*>(pResourceDesc));
            alloc_info = device_->GetResourceAllocationInfo(0, 1, desc);
        }

        if (alloc_info.Alignment && pResourceDesc->Alignment && alloc_info.Alignment != pResourceDesc->Alignment)
        {
            const_cast<D3D12_RESOURCE_DESC1*>(pResourceDesc)->Alignment = alloc_info.Alignment;
        }

        if (pResourceDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            if (alloc_info.SizeInBytes && alloc_info.SizeInBytes != pResourceDesc->Width)
            {
                const_cast<D3D12_RESOURCE_DESC1*>(pResourceDesc)->Width = alloc_info.SizeInBytes;
            }
        }
    }
}

void Dx12RebindAllocator::Release(IUnknown* object)
{
    ID3D12Resource* pResource = reinterpret_cast<ID3D12Resource*>(object);
    if (resource_allocation_.find(pResource) != resource_allocation_.end())
    {
        resource_allocation_[pResource]->Release();
        resource_allocation_[pResource] = nullptr;
        resource_allocation_.erase(pResource);
    }

    if (resource_recreated_heap_.find(pResource) != resource_recreated_heap_.end())
    {
        for (auto& heap : resource_recreated_heap_[pResource])
        {
            heap->Release();
            heap = nullptr;
        }
        resource_recreated_heap_.erase(pResource);
    }
}

void Dx12RebindAllocator::AllRelease()
{
    for (auto& alloc : resource_allocation_)
    {
        alloc.second->Release();
        alloc.second = nullptr;
    }
    resource_allocation_.clear();

    for (auto& recreated_heap : resource_recreated_heap_)
    {
        for (auto& heap : recreated_heap.second)
        {
            heap->Release();
            heap = nullptr;
        }
    }
    resource_recreated_heap_.clear();
}

HRESULT Dx12RebindAllocator::CreateHeap(format::HandleId            capture_id,
                                        _In_ const D3D12_HEAP_DESC* pDesc,
                                        REFIID                      riid,
                                        _COM_Outptr_opt_ void**     ppvHeap)
{
    if (pDesc)
    {
        heap_id_desc_.emplace(capture_id, *pDesc);
    }

    const_cast<D3D12_HEAP_DESC*>(pDesc)->SizeInBytes = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    HRESULT result = device_->CreateHeap(pDesc, riid, ppvHeap);

    return result;
}

HRESULT Dx12RebindAllocator::CreateHeap1(format::HandleId                         capture_id,
                                         _In_ const D3D12_HEAP_DESC*              pDesc,
                                         _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                         REFIID                                   riid,
                                         _COM_Outptr_opt_ void**                  ppvHeap)
{
    if (pDesc)
    {
        heap_id_desc_.emplace(capture_id, *pDesc);
    }

    const_cast<D3D12_HEAP_DESC*>(pDesc)->SizeInBytes = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    graphics::dx12::ID3D12Device4ComPtr device4;
    device_->QueryInterface(IID_PPV_ARGS(&device4));
    HRESULT result = device4->CreateHeap1(pDesc, pProtectedSession, riid, ppvHeap);

    return result;
}

HRESULT Dx12RebindAllocator::CreateCommittedResource(_In_ const D3D12_HEAP_PROPERTIES* pHeapProperties,
                                                     D3D12_HEAP_FLAGS                  HeapFlags,
                                                     _In_ const D3D12_RESOURCE_DESC*   pDesc,
                                                     D3D12_RESOURCE_STATES             InitialResourceState,
                                                     _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                                     REFIID                            riidResource,
                                                     _COM_Outptr_opt_ void**           ppvResource)
{
    HRESULT                  result     = S_FALSE;
    D3D12MA::Allocation*     allocation = nullptr;
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType                 = pHeapProperties->Type;
    alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED;

    if (allocator_)
    {
        SetReplayResourceDescAlignment(pDesc);
        result = allocator_->CreateResource(
            &alloc_desc, pDesc, InitialResourceState, pOptimizedClearValue, &allocation, riidResource, ppvResource);
        if (!result)
        {
            ID3D12Resource* pResource = reinterpret_cast<ID3D12Resource*>(*ppvResource);
            resource_allocation_.emplace(pResource, allocation);
        }
    }
    return result;
}

HRESULT Dx12RebindAllocator::CreatePlacedResource(format::HandleId                  heap_capture_id,
                                                  _In_ ID3D12Heap*                  pHeap,
                                                  UINT64                            HeapOffset,
                                                  _In_ const D3D12_RESOURCE_DESC*   pDesc,
                                                  D3D12_RESOURCE_STATES             InitialState,
                                                  _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                                  REFIID                            riid,
                                                  _COM_Outptr_opt_ void**           ppvResource)
{
    HRESULT                  result     = S_FALSE;
    D3D12MA::Allocation*     allocation = nullptr;
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType                 = D3D12_HEAP_TYPE_DEFAULT;
    alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_CAN_ALIAS;

    if (heap_id_desc_.find(heap_capture_id) != heap_id_desc_.end())
    {
        alloc_desc.HeapType = heap_id_desc_[heap_capture_id].Properties.Type;
    }
    else
    {
        if ((pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER) == D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)
        {
            const_cast<D3D12_RESOURCE_DESC*>(pDesc)->Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
        }
    }

    if (allocator_)
    {
        SetReplayResourceDescAlignment(pDesc);
        result = allocator_->CreateResource(
            &alloc_desc, pDesc, InitialState, pOptimizedClearValue, &allocation, riid, ppvResource);
        if (!result)
        {
            ID3D12Resource* pResource = reinterpret_cast<ID3D12Resource*>(*ppvResource);
            resource_allocation_.emplace(pResource, allocation);
        }
    }
    return result;
}

HRESULT Dx12RebindAllocator::CreateReservedResource(_In_ const D3D12_RESOURCE_DESC*   pDesc,
                                                    D3D12_RESOURCE_STATES             InitialState,
                                                    _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                                    REFIID                            riid,
                                                    _COM_Outptr_opt_ void**           ppvResource)
{
    HRESULT result = S_FALSE;

    if (device_)
    {
        SetReplayResourceDescAlignment(pDesc);
        result = device_->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
    }
    return result;
}

HRESULT Dx12RebindAllocator::CreateCommittedResource1(_In_ const D3D12_HEAP_PROPERTIES*        pHeapProperties,
                                                      D3D12_HEAP_FLAGS                         HeapFlags,
                                                      _In_ const D3D12_RESOURCE_DESC*          pDesc,
                                                      D3D12_RESOURCE_STATES                    InitialResourceState,
                                                      _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                                      _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                                      REFIID                                   riidResource,
                                                      _COM_Outptr_opt_ void**                  ppvResource)
{
    HRESULT                  result     = S_FALSE;
    D3D12MA::Allocation*     allocation = nullptr;
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType                 = pHeapProperties->Type;
    alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED;

    if (allocator_)
    {
        SetReplayResourceDescAlignment(pDesc);
        result = allocator_->CreateResource(
            &alloc_desc, pDesc, InitialResourceState, pOptimizedClearValue, &allocation, riidResource, ppvResource);
        if (!result)
        {
            ID3D12Resource* pResource = reinterpret_cast<ID3D12Resource*>(*ppvResource);
            resource_allocation_.emplace(pResource, allocation);
        }
    }
    return result;
}

HRESULT Dx12RebindAllocator::CreatePlacedResource1(format::HandleId                  heap_capture_id,
                                                   _In_ ID3D12Heap*                  pHeap,
                                                   UINT64                            HeapOffset,
                                                   _In_ const D3D12_RESOURCE_DESC1*  pDesc,
                                                   D3D12_RESOURCE_STATES             InitialState,
                                                   _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                                   REFIID                            riid,
                                                   _COM_Outptr_opt_ void**           ppvResource)
{
    HRESULT                  result     = S_FALSE;
    D3D12MA::Allocation*     allocation = nullptr;
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType                 = D3D12_HEAP_TYPE_DEFAULT;
    alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_CAN_ALIAS;

    if (heap_id_desc_.find(heap_capture_id) != heap_id_desc_.end())
    {
        alloc_desc.HeapType = heap_id_desc_[heap_capture_id].Properties.Type;
    }
    else
    {
        if ((pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER) == D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)
        {
            const_cast<D3D12_RESOURCE_DESC1*>(pDesc)->Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
        }
    }

    if (allocator_)
    {
        SetReplayResourceDescAlignment1(pDesc);
        result = allocator_->CreateResource2(
            &alloc_desc, pDesc, InitialState, pOptimizedClearValue, &allocation, riid, ppvResource);
        if (!result)
        {
            ID3D12Resource* pResource = reinterpret_cast<ID3D12Resource*>(*ppvResource);
            resource_allocation_.emplace(pResource, allocation);
        }
    }

    return result;
}

HRESULT Dx12RebindAllocator::CreateReservedResource1(_In_ const D3D12_RESOURCE_DESC*          pDesc,
                                                     D3D12_RESOURCE_STATES                    InitialState,
                                                     _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                                     _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                                     REFIID                                   riid,
                                                     _COM_Outptr_opt_ void**                  ppvResource)
{
    HRESULT result = S_FALSE;

    if (device_)
    {
        SetReplayResourceDescAlignment(pDesc);

        graphics::dx12::ID3D12Device4ComPtr device4;
        device_->QueryInterface(IID_PPV_ARGS(&device4));

        result = device4->CreateReservedResource1(
            pDesc, InitialState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);
    }

    return result;
}

HRESULT Dx12RebindAllocator::CreateCommittedResource2(_In_ const D3D12_HEAP_PROPERTIES*        pHeapProperties,
                                                      D3D12_HEAP_FLAGS                         HeapFlags,
                                                      _In_ const D3D12_RESOURCE_DESC1*         pDesc,
                                                      D3D12_RESOURCE_STATES                    InitialResourceState,
                                                      _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                                      _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                                      REFIID                                   riidResource,
                                                      _COM_Outptr_opt_ void**                  ppvResource)
{
    HRESULT                  result     = S_FALSE;
    D3D12MA::Allocation*     allocation = nullptr;
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType                 = pHeapProperties->Type;
    alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED;

    if (allocator_)
    {
        SetReplayResourceDescAlignment1(pDesc);
        result = allocator_->CreateResource2(
            &alloc_desc, pDesc, InitialResourceState, pOptimizedClearValue, &allocation, riidResource, ppvResource);
        if (!result)
        {
            ID3D12Resource* pResource = reinterpret_cast<ID3D12Resource*>(*ppvResource);
            resource_allocation_.emplace(pResource, allocation);
        }
    }
    return result;
}

HRESULT Dx12RebindAllocator::CreatePlacedResource2(format::HandleId                  heap_capture_id,
                                                   _In_ ID3D12Heap*                  pHeap,
                                                   UINT64                            HeapOffset,
                                                   _In_ const D3D12_RESOURCE_DESC1*  pDesc,
                                                   D3D12_BARRIER_LAYOUT              InitialLayout,
                                                   _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                                   UINT32                            NumCastableFormats,
                                                   _In_opt_count_(NumCastableFormats)
                                                       const DXGI_FORMAT*  pCastableFormats,
                                                   REFIID                  riid,
                                                   _COM_Outptr_opt_ void** ppvResource)
{
    HRESULT                  result     = S_FALSE;
    D3D12MA::Allocation*     allocation = nullptr;
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType                 = D3D12_HEAP_TYPE_DEFAULT;
    alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_CAN_ALIAS;

    if (heap_id_desc_.find(heap_capture_id) != heap_id_desc_.end())
    {
        alloc_desc.HeapType = heap_id_desc_[heap_capture_id].Properties.Type;
    }
    else
    {
        if ((pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER) == D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)
        {
            const_cast<D3D12_RESOURCE_DESC1*>(pDesc)->Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
        }
    }

    if (allocator_)
    {
        SetReplayResourceDescAlignment1(pDesc);
        result = allocator_->CreateResource3(&alloc_desc,
                                             pDesc,
                                             InitialLayout,
                                             pOptimizedClearValue,
                                             NumCastableFormats,
                                             const_cast<DXGI_FORMAT*>(pCastableFormats),
                                             &allocation,
                                             riid,
                                             ppvResource);

        if (!result)
        {
            ID3D12Resource* pResource = reinterpret_cast<ID3D12Resource*>(*ppvResource);
            resource_allocation_.emplace(pResource, allocation);
        }
    }

    return result;
}

HRESULT Dx12RebindAllocator::CreateReservedResource2(_In_ const D3D12_RESOURCE_DESC*          pDesc,
                                                     D3D12_BARRIER_LAYOUT                     InitialLayout,
                                                     _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                                     _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                                     UINT32                                   NumCastableFormats,
                                                     _In_opt_count_(NumCastableFormats)
                                                         const DXGI_FORMAT*  pCastableFormats,
                                                     REFIID                  riid,
                                                     _COM_Outptr_opt_ void** ppvResource)
{
    HRESULT result = S_FALSE;

    if (device_)
    {
        SetReplayResourceDescAlignment(pDesc);

        graphics::dx12::ID3D12Device10ComPtr device10;
        device_->QueryInterface(IID_PPV_ARGS(&device10));

        result = device10->CreateReservedResource2(pDesc,
                                                   InitialLayout,
                                                   pOptimizedClearValue,
                                                   pProtectedSession,
                                                   NumCastableFormats,
                                                   pCastableFormats,
                                                   riid,
                                                   ppvResource);
    }

    return result;
}

HRESULT Dx12RebindAllocator::CreateCommittedResource3(_In_ const D3D12_HEAP_PROPERTIES*        pHeapProperties,
                                                      D3D12_HEAP_FLAGS                         HeapFlags,
                                                      _In_ const D3D12_RESOURCE_DESC1*         pDesc,
                                                      D3D12_BARRIER_LAYOUT                     InitialLayout,
                                                      _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                                      _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                                      UINT32                                   NumCastableFormats,
                                                      _In_opt_count_(NumCastableFormats)
                                                          const DXGI_FORMAT*  pCastableFormats,
                                                      REFIID                  riidResource,
                                                      _COM_Outptr_opt_ void** ppvResource)
{
    HRESULT                  result     = S_FALSE;
    D3D12MA::Allocation*     allocation = nullptr;
    D3D12MA::ALLOCATION_DESC alloc_desc = {};
    alloc_desc.HeapType                 = pHeapProperties->Type;
    alloc_desc.Flags                    = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED;

    if (allocator_)
    {
        SetReplayResourceDescAlignment1(pDesc);
        result = allocator_->CreateResource3(&alloc_desc,
                                             pDesc,
                                             InitialLayout,
                                             pOptimizedClearValue,
                                             NumCastableFormats,
                                             const_cast<DXGI_FORMAT*>(pCastableFormats),
                                             &allocation,
                                             riidResource,
                                             ppvResource);
        if (!result)
        {
            ID3D12Resource* pResource = reinterpret_cast<ID3D12Resource*>(*ppvResource);
            resource_allocation_.emplace(pResource, allocation);
        }
    }
    return result;
}

void Dx12RebindAllocator::GetResourceTiling(_In_ ID3D12Resource*             pTiledResource,
                                            _Out_opt_ UINT*                  pNumTilesForEntireResource,
                                            _Out_opt_ D3D12_PACKED_MIP_INFO* pPackedMipDesc,
                                            _Out_opt_ D3D12_TILE_SHAPE*      pStandardTileShapeForNonPackedMips,
                                            _Inout_opt_ UINT*                pNumSubresourceTilings,
                                            _In_ UINT                        FirstSubresourceTilingToGet,
                                            _Out_ D3D12_SUBRESOURCE_TILING*  pSubresourceTilingsForNonPackedMips)
{
    if (device_)
    {
        device_->GetResourceTiling(pTiledResource,
                                   pNumTilesForEntireResource,
                                   pPackedMipDesc,
                                   pStandardTileShapeForNonPackedMips,
                                   pNumSubresourceTilings,
                                   FirstSubresourceTilingToGet,
                                   pSubresourceTilingsForNonPackedMips);
    }
}

void Dx12RebindAllocator::UpdateTileMappings(ID3D12CommandQueue*                    pQueue,
                                             format::HandleId                       heap_capture_id,
                                             ID3D12Resource*                        pResource,
                                             UINT                                   NumResourceRegions,
                                             const D3D12_TILED_RESOURCE_COORDINATE* pResourceRegionStartCoordinates,
                                             const D3D12_TILE_REGION_SIZE*          pResourceRegionSizes,
                                             ID3D12Heap*                            pHeap,
                                             UINT                                   NumRanges,
                                             const D3D12_TILE_RANGE_FLAGS*          pRangeFlags,
                                             const UINT*                            pHeapRangeStartOffsets,
                                             const UINT*                            pRangeTileCounts,
                                             D3D12_TILE_MAPPING_FLAGS               Flags)
{
    if (pHeap == nullptr && pRangeTileCounts == nullptr)
    {
        pQueue->UpdateTileMappings(pResource,
                                   NumResourceRegions,
                                   pResourceRegionStartCoordinates,
                                   pResourceRegionSizes,
                                   pHeap,
                                   NumRanges,
                                   pRangeFlags,
                                   pHeapRangeStartOffsets,
                                   pRangeTileCounts,
                                   Flags);
    }
    else
    {
        // D3D12_RESOURCE_ALLOCATION_INFO alloc_info = device_->GetResourceAllocationInfo(0, 1, resource);
        // UINT                           tileSize   = alloc_info.SizeInBytes;

        // creat heap
        ID3D12Heap* pNewHeap = nullptr;
        if (heap_id_desc_.find(heap_capture_id) != heap_id_desc_.end())
        {
            if (heap_id_recreated_heap_.find(heap_capture_id) == heap_id_recreated_heap_.end())
            {
                HRESULT hr = device_->CreateHeap(&heap_id_desc_[heap_capture_id], IID_PPV_ARGS(&pNewHeap));
                if (hr == S_OK)
                {
                    heap_id_recreated_heap_.emplace(heap_capture_id, pNewHeap);
                    resource_recreated_heap_[pResource].push_back(pNewHeap);
                }
            }
            else
            {
                pNewHeap = heap_id_recreated_heap_[heap_capture_id];
            }

            pQueue->UpdateTileMappings(pResource,
                                       NumResourceRegions,
                                       pResourceRegionStartCoordinates,
                                       pResourceRegionSizes,
                                       pNewHeap,
                                       NumRanges,
                                       pRangeFlags,
                                       pHeapRangeStartOffsets,
                                       pRangeTileCounts,
                                       Flags);
        }
        else
        {
            pQueue->UpdateTileMappings(pResource,
                                       NumResourceRegions,
                                       pResourceRegionStartCoordinates,
                                       pResourceRegionSizes,
                                       pHeap,
                                       NumRanges,
                                       pRangeFlags,
                                       pHeapRangeStartOffsets,
                                       pRangeTileCounts,
                                       Flags);
        }
    }
}

void Dx12RebindAllocator::ReportResourceIncompatibility(const D3D12_RESOURCE_DESC* pResourceDesc)
{
    return;
}

void Dx12RebindAllocator::ReportResourceIncompatibility1(const D3D12_RESOURCE_DESC1* pResourceDesc)
{
    return;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
