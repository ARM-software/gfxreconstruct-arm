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
    desc.pDevice                 = reinterpret_cast<ID3D12Device*>(const_cast<void*>(pvDevice));
    desc.pAdapter                = reinterpret_cast<IDXGIAdapter*>(const_cast<IUnknown*>(adapter));
    desc.Flags                   = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;

    GFXRECON_LOG_INFO_ONCE("Replay with D3D12 rebind memory translation.");
    HRESULT result = D3D12MA::CreateAllocator(&desc, &allocator_);

    return result;
}

void Dx12RebindAllocator::Destroy()
{
    // The order of release is Resource->allocation->custom pool
    for (auto& alloc : resource_id_allocation_)
    {
        alloc.second.Reset();
    }
    resource_id_allocation_.clear();

    for (auto& alloc : heap_id_aliasing_allocation_)
    {
        alloc.second.Reset();
    }
    heap_id_aliasing_allocation_.clear();

    for (auto& pool : heap_id_custom_pool_)
    {
        pool.second.Reset();
    }
    heap_id_custom_pool_.clear();

    for (auto& recreated_heap : resource_id_recreated_heap_)
    {
        for (auto& heap : recreated_heap.second)
        {
            heap.Reset();
        }
    }
    resource_id_recreated_heap_.clear();

    for (auto& heap : heap_id_recreated_heap_)
    {
        heap.second.Reset();
    }
    heap_id_recreated_heap_.clear();

    if (allocator_ != nullptr)
    {
        allocator_.Reset();
    }

    device_ = nullptr;
}

D3D12_HEAP_PROPERTIES
Dx12RebindAllocator::GetReplayCustomHeapProperties(const D3D12_CPU_PAGE_PROPERTY cpu_page_property)
{
    D3D12_HEAP_PROPERTIES heap_props;
    if (cpu_page_property == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK)
    {
        heap_props = device_->GetCustomHeapProperties(1, D3D12_HEAP_TYPE_READBACK);
    }
    else if (cpu_page_property == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE)
    {
        heap_props = device_->GetCustomHeapProperties(1, D3D12_HEAP_TYPE_UPLOAD);
    }
    else
    {
        heap_props = device_->GetCustomHeapProperties(1, D3D12_HEAP_TYPE_DEFAULT);
    }

    return heap_props;
}

void Dx12RebindAllocator::SetReplayResourceCompatibility(const format::HandleId    heap_capture_id,
                                                         const ID3D12Heap*         heap,
                                                         const UINT64              Heap_offset,
                                                         D3D12_RESOURCE_DESC1*     resource_desc,
                                                         D3D12MA::ALLOCATION_DESC& allocation_desc)
{
    assert(heap != nullptr);
    GetReplayResourceDescAllocationInfo1(resource_desc);

    assert(heap != nullptr);
    allocation_desc.Flags = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_NONE;

    if (heap_id_desc_.find(heap_capture_id) == heap_id_desc_.end())
    {
        D3D12_HEAP_DESC heap_desc = const_cast<ID3D12Heap*>(heap)->GetDesc();
        heap_id_desc_.insert_or_assign(heap_capture_id, heap_desc);
    }

    // remove SHARED and SHARED_CROSS_ADAPTER flags that are not allowed on real heaps
    heap_id_desc_[heap_capture_id].Flags &= ~(D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);
    resource_desc->Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;

    D3D12_FEATURE_DATA_D3D12_OPTIONS opts = {};
    if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts))))
    {
        if (opts.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_1)
        {
            if ((heap_id_desc_[heap_capture_id].Flags &
                 (D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES |
                  D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)) == 0)
            {
                GFXRECON_LOG_WARNING("Adding DENY_RT_DS_TEXTURES|DENY_NON_RT_DS_TEXTURES to OpenExistingHeap heap "
                                     "for tier 1 compatibility");
                heap_id_desc_[heap_capture_id].Flags |=
                    (D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES);
            }
        }
    }

    allocation_desc.HeapType       = heap_id_desc_[heap_capture_id].Properties.Type;
    allocation_desc.ExtraHeapFlags = heap_id_desc_[heap_capture_id].Flags;

    if (allocation_desc.HeapType == D3D12_HEAP_TYPE_CUSTOM)
    {
        auto                  cpu_page_property = heap_id_desc_[heap_capture_id].Properties.CPUPageProperty;
        D3D12_HEAP_PROPERTIES heap_properties   = GetReplayCustomHeapProperties(cpu_page_property);
        D3D12_HEAP_FLAGS      heap_flags        = heap_id_desc_[heap_capture_id].Flags;

        ComPtr<D3D12MA::Pool> custom_pool = nullptr;
        D3D12MA::CPOOL_DESC   pool_desc{ heap_properties, heap_flags };

        if (heap_id_custom_pool_.find(heap_capture_id) != heap_id_custom_pool_.end())
        {
            allocation_desc.CustomPool = heap_id_custom_pool_[heap_capture_id].Get();
        }
        else
        {
            if (S_OK == allocator_->CreatePool(&pool_desc, &custom_pool))
            {
                allocation_desc.CustomPool = custom_pool.Get();
                heap_id_custom_pool_.insert_or_assign(heap_capture_id, std::move(custom_pool));
            }
            else
            {
                GFXRECON_LOG_FATAL("Failed to create custom pool for for resource using D3D12_HEAP_TYPE_CUSTOM!");
            }
        }
    }

    // don't create resources non-resident
    allocation_desc.ExtraHeapFlags &= ~D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
}

D3D12_RESOURCE_ALLOCATION_INFO
Dx12RebindAllocator::GetReplayResourceDescAllocationInfo(const D3D12_RESOURCE_DESC* resource_desc)
{
    D3D12_RESOURCE_ALLOCATION_INFO  alloc_info  = {};
    D3D12_RESOURCE_ALLOCATION_INFO1 alloc_info1 = {};

    if (device_ != nullptr)
    {
        graphics::dx12::ID3D12Device4ComPtr device4;
        device_->QueryInterface(IID_PPV_ARGS(&device4));

        if (device4 != nullptr)
        {
            alloc_info = device4->GetResourceAllocationInfo1(0, 1, resource_desc, &alloc_info1);
        }
        else
        {
            alloc_info = device_->GetResourceAllocationInfo(0, 1, resource_desc);
        }

        // Alignment is set to 0, the runtime will set it to the correct value
        const_cast<D3D12_RESOURCE_DESC*>(resource_desc)->Alignment = 0;

        if (resource_desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            if (alloc_info.SizeInBytes && alloc_info.SizeInBytes != resource_desc->Width)
            {
                const_cast<D3D12_RESOURCE_DESC*>(resource_desc)->Width = alloc_info.SizeInBytes;
            }
        }
    }

    return alloc_info;
}

D3D12_RESOURCE_ALLOCATION_INFO
Dx12RebindAllocator::GetReplayResourceDescAllocationInfo1(const D3D12_RESOURCE_DESC1* resource_desc)
{
    D3D12_RESOURCE_ALLOCATION_INFO  alloc_info  = {};
    D3D12_RESOURCE_ALLOCATION_INFO1 alloc_info1 = {};

    if (device_ != nullptr)
    {
        graphics::dx12::ID3D12Device4ComPtr device4;
        device_->QueryInterface(IID_PPV_ARGS(&device4));

        graphics::dx12::ID3D12Device8ComPtr device8;
        device_->QueryInterface(IID_PPV_ARGS(&device8));

        if (device8 != nullptr)
        {
            alloc_info = device8->GetResourceAllocationInfo2(0, 1, resource_desc, &alloc_info1);
        }
        else if (device4 != nullptr)
        {
            D3D12_RESOURCE_DESC* desc =
                reinterpret_cast<D3D12_RESOURCE_DESC*>(const_cast<D3D12_RESOURCE_DESC1*>(resource_desc));
            alloc_info = device4->GetResourceAllocationInfo1(0, 1, desc, &alloc_info1);
        }
        else
        {
            D3D12_RESOURCE_DESC* desc =
                reinterpret_cast<D3D12_RESOURCE_DESC*>(const_cast<D3D12_RESOURCE_DESC1*>(resource_desc));
            alloc_info = device_->GetResourceAllocationInfo(0, 1, desc);
        }

        // Alignment is set to 0, the runtime will set it to the correct value
        const_cast<D3D12_RESOURCE_DESC1*>(resource_desc)->Alignment = 0;

        if (resource_desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            if (alloc_info.SizeInBytes && alloc_info.SizeInBytes != resource_desc->Width)
            {
                const_cast<D3D12_RESOURCE_DESC1*>(resource_desc)->Width = alloc_info.SizeInBytes;
            }
        }
    }

    return alloc_info;
}

ULONG Dx12RebindAllocator::Release(IUnknown* object, format::HandleId object_id)
{
    // The order of release is Resource->allocation->custom pool
    if (resource_id_allocation_.find(object_id) != resource_id_allocation_.end())
    {
        resource_id_allocation_[object_id] = nullptr;
        resource_id_allocation_.erase(object_id);
    }

    if (heap_id_aliasing_allocation_.find(object_id) != heap_id_aliasing_allocation_.end())
    {
        heap_id_aliasing_allocation_[object_id] = nullptr;
        heap_id_aliasing_allocation_.erase(object_id);
    }

    if (heap_id_custom_pool_.find(object_id) != heap_id_custom_pool_.end())
    {
        heap_id_custom_pool_[object_id] = nullptr;
        heap_id_custom_pool_.erase(object_id);
    }

    if (resource_id_recreated_heap_.find(object_id) != resource_id_recreated_heap_.end())
    {
        resource_id_recreated_heap_[object_id].clear();
        resource_id_recreated_heap_.erase(object_id);
    }

    if (heap_id_recreated_heap_.find(object_id) != heap_id_recreated_heap_.end())
    {
        heap_id_recreated_heap_[object_id] = nullptr;
        heap_id_recreated_heap_.erase(object_id);
    }

    return 0;
}

HRESULT Dx12RebindAllocator::CreateHeap(format::HandleId            capture_id,
                                        _In_ const D3D12_HEAP_DESC* pDesc,
                                        REFIID                      riid,
                                        _COM_Outptr_opt_ void**     ppvHeap)
{
    if (pDesc == nullptr)
    {
        return E_INVALIDARG;
    }

    if (pDesc->Properties.Type == D3D12_HEAP_TYPE_CUSTOM)
    {
        D3D12_HEAP_PROPERTIES heap_props = GetReplayCustomHeapProperties(pDesc->Properties.CPUPageProperty);
        const_cast<D3D12_HEAP_DESC*>(pDesc)->Properties = heap_props;
    }

    heap_id_desc_.insert_or_assign(capture_id, *pDesc);
    const_cast<D3D12_HEAP_DESC*>(pDesc)->SizeInBytes = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;

    HRESULT result = device_->CreateHeap(pDesc, riid, ppvHeap);

    return result;
}

HRESULT Dx12RebindAllocator::CreateHeap1(format::HandleId                         capture_id,
                                         _In_ const D3D12_HEAP_DESC*              pDesc,
                                         _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                         REFIID                                   riid,
                                         _COM_Outptr_opt_ void**                  ppvHeap)
{
    if (pDesc == nullptr)
    {
        return E_INVALIDARG;
    }

    if (pDesc->Properties.Type == D3D12_HEAP_TYPE_CUSTOM)
    {
        D3D12_HEAP_PROPERTIES heap_props = GetReplayCustomHeapProperties(pDesc->Properties.CPUPageProperty);
        const_cast<D3D12_HEAP_DESC*>(pDesc)->Properties = heap_props;
    }

    heap_id_desc_.insert_or_assign(capture_id, *pDesc);
    const_cast<D3D12_HEAP_DESC*>(pDesc)->SizeInBytes = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;

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
                                                     HandlePointerDecoder<void*>*      ppvResource)
{
    HRESULT                     result     = S_FALSE;
    ComPtr<D3D12MA::Allocation> allocation = nullptr;
    D3D12MA::ALLOCATION_DESC    alloc_desc = {};

    // don't create resources non-resident
    HeapFlags &= ~D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
    GetReplayResourceDescAllocationInfo(pDesc);

    if (pHeapProperties->Type == D3D12_HEAP_TYPE_CUSTOM)
    {
        if (device_ != nullptr)
        {
            D3D12_HEAP_PROPERTIES heap_props = GetReplayCustomHeapProperties(pHeapProperties->CPUPageProperty);

            result = device_->CreateCommittedResource(&heap_props,
                                                      HeapFlags,
                                                      pDesc,
                                                      InitialResourceState,
                                                      pOptimizedClearValue,
                                                      riidResource,
                                                      ppvResource->GetHandlePointer());
        }
    }
    else
    {
        if (allocator_ != nullptr)
        {
            alloc_desc.Flags          = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED;
            alloc_desc.HeapType       = pHeapProperties->Type;
            alloc_desc.ExtraHeapFlags = HeapFlags;

            result = allocator_->CreateResource(&alloc_desc,
                                                pDesc,
                                                InitialResourceState,
                                                pOptimizedClearValue,
                                                &allocation,
                                                riidResource,
                                                ppvResource->GetHandlePointer());

            if (SUCCEEDED(result))
            {
                auto resource_id = *(ppvResource->GetPointer());
                resource_id_allocation_.insert_or_assign(resource_id, std::move(allocation));
            }
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
                                                  HandlePointerDecoder<void*>*      ppvResource)
{
    HRESULT                     result     = S_FALSE;
    ComPtr<D3D12MA::Allocation> allocation = nullptr;
    D3D12MA::ALLOCATION_DESC    alloc_desc = {};

    D3D12_RESOURCE_DESC* resource_desc = const_cast<D3D12_RESOURCE_DESC*>(pDesc);
    SetReplayResourceCompatibility(heap_capture_id, pHeap, HeapOffset, resource_desc, alloc_desc);

    if (allocator_ != nullptr)
    {
        // If the HeapOffset is 0 and it is not a multi-sample resource, an aliasing resource needs be created.
        if ((HeapOffset != 0) || (pDesc->SampleDesc.Count > 1))
        {
            result = allocator_->CreateResource(&alloc_desc,
                                                pDesc,
                                                InitialState,
                                                pOptimizedClearValue,
                                                &allocation,
                                                riid,
                                                ppvResource->GetHandlePointer());
        }
        else
        {
            bool                 recreated_allocation = false;
            D3D12MA::Allocation* aliasing_alloc       = nullptr;

            if (heap_id_aliasing_allocation_.find(heap_capture_id) == heap_id_aliasing_allocation_.end())
            {
                recreated_allocation = true;
            }
            else
            {
                D3D12_RESOURCE_ALLOCATION_INFO replay_alloc_info = GetReplayResourceDescAllocationInfo(pDesc);
                if (replay_alloc_info.SizeInBytes != 0 && replay_alloc_info.SizeInBytes != UINT64_MAX)
                {
                    if (heap_id_aliasing_allocation_[heap_capture_id].Get()->GetSize() < replay_alloc_info.SizeInBytes)
                    {
                        recreated_allocation = true;
                    }
                }
            }

            if (recreated_allocation)
            {
                D3D12_RESOURCE_ALLOCATION_INFO alloc_info = {};
                alloc_info.Alignment                      = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

                D3D12_RESOURCE_ALLOCATION_INFO replay_alloc_info = GetReplayResourceDescAllocationInfo(pDesc);
                if (replay_alloc_info.SizeInBytes != 0 && replay_alloc_info.SizeInBytes != UINT64_MAX)
                {
                    alloc_info.SizeInBytes = replay_alloc_info.SizeInBytes;
                    alloc_info.Alignment   = replay_alloc_info.Alignment;
                }
                else
                {
                    alloc_info.SizeInBytes = heap_id_desc_[heap_capture_id].SizeInBytes;
                }

                result = allocator_->AllocateMemory(&alloc_desc, &alloc_info, &allocation);
                if (SUCCEEDED(result))
                {
                    // This may release old allocation and save new allocation
                    heap_id_aliasing_allocation_.insert_or_assign(heap_capture_id, allocation);
                    aliasing_alloc = allocation.Get();
                }
            }
            else
            {
                result         = S_OK;
                aliasing_alloc = heap_id_aliasing_allocation_[heap_capture_id].Get();
                allocation     = aliasing_alloc;
            }

            if (SUCCEEDED(result))
            {
                result = allocator_->CreateAliasingResource(aliasing_alloc,
                                                            0,
                                                            pDesc,
                                                            InitialState,
                                                            pOptimizedClearValue,
                                                            riid,
                                                            ppvResource->GetHandlePointer());
            }
        }

        if (SUCCEEDED(result))
        {
            auto resource_id = *(ppvResource->GetPointer());
            resource_id_allocation_.insert_or_assign(resource_id, std::move(allocation));
        }
    }

    return result;
}

HRESULT Dx12RebindAllocator::CreateReservedResource(_In_ const D3D12_RESOURCE_DESC*   pDesc,
                                                    D3D12_RESOURCE_STATES             InitialState,
                                                    _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                                                    REFIID                            riid,
                                                    HandlePointerDecoder<void*>*      ppvResource)
{
    HRESULT result = S_FALSE;

    if (device_ != nullptr)
    {
        GetReplayResourceDescAllocationInfo(pDesc);
        result = device_->CreateReservedResource(
            pDesc, InitialState, pOptimizedClearValue, riid, ppvResource->GetHandlePointer());
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
                                                      HandlePointerDecoder<void*>*             ppvResource)
{
    HRESULT                     result     = S_FALSE;
    ComPtr<D3D12MA::Allocation> allocation = nullptr;
    D3D12MA::ALLOCATION_DESC    alloc_desc = {};

    // don't create resources non-resident
    HeapFlags &= ~D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
    GetReplayResourceDescAllocationInfo(pDesc);

    if (pHeapProperties->Type == D3D12_HEAP_TYPE_CUSTOM || pProtectedSession != nullptr)
    {
        graphics::dx12::ID3D12Device4ComPtr device4;
        device_->QueryInterface(IID_PPV_ARGS(&device4));
        assert(device4 != nullptr);
        if (pHeapProperties->Type == D3D12_HEAP_TYPE_CUSTOM)
        {
            D3D12_HEAP_PROPERTIES heap_props = GetReplayCustomHeapProperties(pHeapProperties->CPUPageProperty);

            result = device4->CreateCommittedResource1(&heap_props,
                                                       HeapFlags,
                                                       pDesc,
                                                       InitialResourceState,
                                                       pOptimizedClearValue,
                                                       pProtectedSession,
                                                       riidResource,
                                                       ppvResource->GetHandlePointer());
        }
        else
        {
            result = device4->CreateCommittedResource1(pHeapProperties,
                                                       HeapFlags,
                                                       pDesc,
                                                       InitialResourceState,
                                                       pOptimizedClearValue,
                                                       pProtectedSession,
                                                       riidResource,
                                                       ppvResource->GetHandlePointer());
        }
    }
    else
    {
        if (allocator_ != nullptr)
        {
            alloc_desc.Flags          = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED;
            alloc_desc.HeapType       = pHeapProperties->Type;
            alloc_desc.ExtraHeapFlags = HeapFlags;

            result = allocator_->CreateResource(&alloc_desc,
                                                pDesc,
                                                InitialResourceState,
                                                pOptimizedClearValue,
                                                &allocation,
                                                riidResource,
                                                ppvResource->GetHandlePointer());

            if (SUCCEEDED(result))
            {
                auto resource_id = *(ppvResource->GetPointer());
                resource_id_allocation_.insert_or_assign(resource_id, std::move(allocation));
            }
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
                                                   HandlePointerDecoder<void*>*      ppvResource)
{
    HRESULT                     result     = S_FALSE;
    ComPtr<D3D12MA::Allocation> allocation = nullptr;
    D3D12MA::ALLOCATION_DESC    alloc_desc = {};

    D3D12_RESOURCE_DESC1* resource_desc = const_cast<D3D12_RESOURCE_DESC1*>(pDesc);
    SetReplayResourceCompatibility(heap_capture_id, pHeap, HeapOffset, resource_desc, alloc_desc);

    if (allocator_ != nullptr)
    {
        // If the HeapOffset is 0 and it is not a multi-sample resource, an aliasing resource needs be created.
        if ((HeapOffset != 0) || (pDesc->SampleDesc.Count > 1))
        {
            result = allocator_->CreateResource2(&alloc_desc,
                                                 pDesc,
                                                 InitialState,
                                                 pOptimizedClearValue,
                                                 &allocation,
                                                 riid,
                                                 ppvResource->GetHandlePointer());
        }
        else
        {
            bool                 recreated_allocation = false;
            D3D12MA::Allocation* aliasing_alloc       = nullptr;

            if (heap_id_aliasing_allocation_.find(heap_capture_id) == heap_id_aliasing_allocation_.end())
            {
                recreated_allocation = true;
            }
            else
            {
                D3D12_RESOURCE_ALLOCATION_INFO replay_alloc_info = GetReplayResourceDescAllocationInfo1(pDesc);
                if (replay_alloc_info.SizeInBytes != 0 && replay_alloc_info.SizeInBytes != UINT64_MAX)
                {
                    if (heap_id_aliasing_allocation_[heap_capture_id].Get()->GetSize() < replay_alloc_info.SizeInBytes)
                    {
                        recreated_allocation = true;
                    }
                }
            }

            if (recreated_allocation)
            {
                D3D12_RESOURCE_ALLOCATION_INFO alloc_info = {};
                alloc_info.Alignment                      = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

                D3D12_RESOURCE_ALLOCATION_INFO replay_alloc_info = GetReplayResourceDescAllocationInfo1(pDesc);
                if (replay_alloc_info.SizeInBytes != 0 && replay_alloc_info.SizeInBytes != UINT64_MAX)
                {
                    alloc_info.SizeInBytes = replay_alloc_info.SizeInBytes;
                    alloc_info.Alignment   = replay_alloc_info.Alignment;
                }
                else
                {
                    alloc_info.SizeInBytes = heap_id_desc_[heap_capture_id].SizeInBytes;
                }

                result = allocator_->AllocateMemory(&alloc_desc, &alloc_info, &allocation);
                if (SUCCEEDED(result))
                {
                    // This may release old allocation and save new allocation
                    heap_id_aliasing_allocation_.insert_or_assign(heap_capture_id, allocation);
                    aliasing_alloc = allocation.Get();
                }
            }
            else
            {
                result         = S_OK;
                aliasing_alloc = heap_id_aliasing_allocation_[heap_capture_id].Get();
                allocation     = aliasing_alloc;
            }

            if (SUCCEEDED(result))
            {
                result = allocator_->CreateAliasingResource1(aliasing_alloc,
                                                             0,
                                                             pDesc,
                                                             InitialState,
                                                             pOptimizedClearValue,
                                                             riid,
                                                             ppvResource->GetHandlePointer());
            }
        }

        if (SUCCEEDED(result))
        {
            auto resource_id = *(ppvResource->GetPointer());
            resource_id_allocation_.insert_or_assign(resource_id, std::move(allocation));
        }
    }

    return result;
}

HRESULT Dx12RebindAllocator::CreateReservedResource1(_In_ const D3D12_RESOURCE_DESC*          pDesc,
                                                     D3D12_RESOURCE_STATES                    InitialState,
                                                     _In_opt_ const D3D12_CLEAR_VALUE*        pOptimizedClearValue,
                                                     _In_opt_ ID3D12ProtectedResourceSession* pProtectedSession,
                                                     REFIID                                   riid,
                                                     HandlePointerDecoder<void*>*             ppvResource)
{
    HRESULT result = S_FALSE;

    if (device_ != nullptr)
    {
        GetReplayResourceDescAllocationInfo(pDesc);

        graphics::dx12::ID3D12Device4ComPtr device4;
        device_->QueryInterface(IID_PPV_ARGS(&device4));

        result = device4->CreateReservedResource1(
            pDesc, InitialState, pOptimizedClearValue, pProtectedSession, riid, ppvResource->GetHandlePointer());
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
                                                      HandlePointerDecoder<void*>*             ppvResource)
{
    HRESULT                     result     = S_FALSE;
    ComPtr<D3D12MA::Allocation> allocation = nullptr;
    D3D12MA::ALLOCATION_DESC    alloc_desc = {};

    // don't create resources non-resident
    HeapFlags &= ~D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
    GetReplayResourceDescAllocationInfo1(pDesc);

    if (pHeapProperties->Type == D3D12_HEAP_TYPE_CUSTOM || pProtectedSession != nullptr)
    {
        graphics::dx12::ID3D12Device8ComPtr device8;
        device_->QueryInterface(IID_PPV_ARGS(&device8));
        assert(device8 != nullptr);
        if (pHeapProperties->Type == D3D12_HEAP_TYPE_CUSTOM)
        {
            D3D12_HEAP_PROPERTIES heap_props = GetReplayCustomHeapProperties(pHeapProperties->CPUPageProperty);

            result = device8->CreateCommittedResource2(&heap_props,
                                                       HeapFlags,
                                                       pDesc,
                                                       InitialResourceState,
                                                       pOptimizedClearValue,
                                                       pProtectedSession,
                                                       riidResource,
                                                       ppvResource->GetHandlePointer());
        }
        else
        {
            result = device8->CreateCommittedResource2(pHeapProperties,
                                                       HeapFlags,
                                                       pDesc,
                                                       InitialResourceState,
                                                       pOptimizedClearValue,
                                                       pProtectedSession,
                                                       riidResource,
                                                       ppvResource->GetHandlePointer());
        }
    }
    else
    {
        if (allocator_ != nullptr)
        {
            alloc_desc.Flags          = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED;
            alloc_desc.HeapType       = pHeapProperties->Type;
            alloc_desc.ExtraHeapFlags = HeapFlags;

            result = allocator_->CreateResource2(&alloc_desc,
                                                 pDesc,
                                                 InitialResourceState,
                                                 pOptimizedClearValue,
                                                 &allocation,
                                                 riidResource,
                                                 ppvResource->GetHandlePointer());

            if (SUCCEEDED(result))
            {
                auto resource_id = *(ppvResource->GetPointer());
                resource_id_allocation_.insert_or_assign(resource_id, std::move(allocation));
            }
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
                                                       const DXGI_FORMAT*       pCastableFormats,
                                                   REFIID                       riid,
                                                   HandlePointerDecoder<void*>* ppvResource)
{
    HRESULT                     result     = S_FALSE;
    ComPtr<D3D12MA::Allocation> allocation = nullptr;
    D3D12MA::ALLOCATION_DESC    alloc_desc = {};

    D3D12_RESOURCE_DESC1* resource_desc = const_cast<D3D12_RESOURCE_DESC1*>(pDesc);
    SetReplayResourceCompatibility(heap_capture_id, pHeap, HeapOffset, resource_desc, alloc_desc);

    if (allocator_ != nullptr)
    {
        // If the HeapOffset is 0 and it is not a multi-sample resource, an aliasing resource needs be created.
        if ((HeapOffset != 0) || (pDesc->SampleDesc.Count > 1))
        {
            result = allocator_->CreateResource3(&alloc_desc,
                                                 pDesc,
                                                 InitialLayout,
                                                 pOptimizedClearValue,
                                                 NumCastableFormats,
                                                 const_cast<DXGI_FORMAT*>(pCastableFormats),
                                                 &allocation,
                                                 riid,
                                                 ppvResource->GetHandlePointer());
        }
        else
        {
            bool                 recreated_allocation = false;
            D3D12MA::Allocation* aliasing_alloc       = nullptr;

            if (heap_id_aliasing_allocation_.find(heap_capture_id) == heap_id_aliasing_allocation_.end())
            {
                recreated_allocation = true;
            }
            else
            {
                D3D12_RESOURCE_ALLOCATION_INFO replay_alloc_info = GetReplayResourceDescAllocationInfo1(pDesc);
                if (replay_alloc_info.SizeInBytes != 0 && replay_alloc_info.SizeInBytes != UINT64_MAX)
                {
                    if (heap_id_aliasing_allocation_[heap_capture_id].Get()->GetSize() < replay_alloc_info.SizeInBytes)
                    {
                        recreated_allocation = true;
                    }
                }
            }

            if (recreated_allocation)
            {
                D3D12_RESOURCE_ALLOCATION_INFO alloc_info = {};
                alloc_info.Alignment                      = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

                D3D12_RESOURCE_ALLOCATION_INFO replay_alloc_info = GetReplayResourceDescAllocationInfo1(pDesc);
                if (replay_alloc_info.SizeInBytes != 0 && replay_alloc_info.SizeInBytes != UINT64_MAX)
                {
                    alloc_info.SizeInBytes = replay_alloc_info.SizeInBytes;
                    alloc_info.Alignment   = replay_alloc_info.Alignment;
                }
                else
                {
                    alloc_info.SizeInBytes = heap_id_desc_[heap_capture_id].SizeInBytes;
                }

                result = allocator_->AllocateMemory(&alloc_desc, &alloc_info, &allocation);
                if (SUCCEEDED(result))
                {
                    // This may release old allocation and save new allocation
                    heap_id_aliasing_allocation_.insert_or_assign(heap_capture_id, allocation);
                    aliasing_alloc = allocation.Get();
                }
            }
            else
            {
                result         = S_OK;
                aliasing_alloc = heap_id_aliasing_allocation_[heap_capture_id].Get();
                allocation     = aliasing_alloc;
            }

            if (SUCCEEDED(result))
            {
                result = allocator_->CreateAliasingResource2(aliasing_alloc,
                                                             0,
                                                             pDesc,
                                                             InitialLayout,
                                                             pOptimizedClearValue,
                                                             NumCastableFormats,
                                                             const_cast<DXGI_FORMAT*>(pCastableFormats),
                                                             riid,
                                                             ppvResource->GetHandlePointer());
            }
        }

        if (SUCCEEDED(result))
        {
            auto resource_id = *(ppvResource->GetPointer());
            resource_id_allocation_.insert_or_assign(resource_id, std::move(allocation));
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
                                                         const DXGI_FORMAT*       pCastableFormats,
                                                     REFIID                       riid,
                                                     HandlePointerDecoder<void*>* ppvResource)
{
    HRESULT result = S_FALSE;

    if (device_ != nullptr)
    {
        GetReplayResourceDescAllocationInfo(pDesc);

        graphics::dx12::ID3D12Device10ComPtr device10;
        device_->QueryInterface(IID_PPV_ARGS(&device10));

        result = device10->CreateReservedResource2(pDesc,
                                                   InitialLayout,
                                                   pOptimizedClearValue,
                                                   pProtectedSession,
                                                   NumCastableFormats,
                                                   pCastableFormats,
                                                   riid,
                                                   ppvResource->GetHandlePointer());
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
                                                          const DXGI_FORMAT*       pCastableFormats,
                                                      REFIID                       riidResource,
                                                      HandlePointerDecoder<void*>* ppvResource)
{
    HRESULT                     result     = S_FALSE;
    ComPtr<D3D12MA::Allocation> allocation = nullptr;
    D3D12MA::ALLOCATION_DESC    alloc_desc = {};

    // don't create resources non-resident
    HeapFlags &= ~D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
    GetReplayResourceDescAllocationInfo1(pDesc);

    if (pHeapProperties->Type == D3D12_HEAP_TYPE_CUSTOM || pProtectedSession != nullptr)
    {
        graphics::dx12::ID3D12Device10ComPtr device10;
        device_->QueryInterface(IID_PPV_ARGS(&device10));
        assert(device10 != nullptr);
        if (pHeapProperties->Type == D3D12_HEAP_TYPE_CUSTOM)
        {
            D3D12_HEAP_PROPERTIES heap_props = GetReplayCustomHeapProperties(pHeapProperties->CPUPageProperty);

            result = device10->CreateCommittedResource3(&heap_props,
                                                        HeapFlags,
                                                        pDesc,
                                                        InitialLayout,
                                                        pOptimizedClearValue,
                                                        pProtectedSession,
                                                        NumCastableFormats,
                                                        pCastableFormats,
                                                        riidResource,
                                                        ppvResource->GetHandlePointer());
        }
        else
        {
            result = device10->CreateCommittedResource3(pHeapProperties,
                                                        HeapFlags,
                                                        pDesc,
                                                        InitialLayout,
                                                        pOptimizedClearValue,
                                                        pProtectedSession,
                                                        NumCastableFormats,
                                                        pCastableFormats,
                                                        riidResource,
                                                        ppvResource->GetHandlePointer());
        }
    }
    else
    {
        if (allocator_ != nullptr)
        {
            alloc_desc.Flags          = D3D12MA::ALLOCATION_FLAGS::ALLOCATION_FLAG_COMMITTED;
            alloc_desc.HeapType       = pHeapProperties->Type;
            alloc_desc.ExtraHeapFlags = HeapFlags;

            result = allocator_->CreateResource3(&alloc_desc,
                                                 pDesc,
                                                 InitialLayout,
                                                 pOptimizedClearValue,
                                                 NumCastableFormats,
                                                 const_cast<DXGI_FORMAT*>(pCastableFormats),
                                                 &allocation,
                                                 riidResource,
                                                 ppvResource->GetHandlePointer());

            if (SUCCEEDED(result))
            {
                auto resource_id = *(ppvResource->GetPointer());
                resource_id_allocation_.insert_or_assign(resource_id, std::move(allocation));
            }
        }
    }

    return result;
}

HRESULT Dx12RebindAllocator::SetResidencyPriority(UINT                                   NumObjects,
                                                  HandlePointerDecoder<ID3D12Pageable*>* ppObjects,
                                                  const D3D12_RESIDENCY_PRIORITY*        pPriorities)
{
    HRESULT result = S_FALSE;

    graphics::dx12::ID3D12Device1ComPtr device1;
    device_->QueryInterface(IID_PPV_ARGS(&device1));

    auto object_ids = ppObjects->GetPointer();
    auto objects    = ppObjects->GetHandlePointer();

    for (UINT i = 0; i < NumObjects; i++)
    {
        auto object = reinterpret_cast<ID3D12Resource*>(objects[i]);
        if (resource_id_allocation_.find(object_ids[i]) != resource_id_allocation_.end())
        {
            const auto allocation = resource_id_allocation_[object_ids[i]].Get();
            if (allocation->GetHeap() != nullptr)
            {
                objects[i] = reinterpret_cast<ID3D12Pageable*>(allocation->GetHeap());
            }
        }
    }

    if (device1 != nullptr)
    {
        result = device1->SetResidencyPriority(NumObjects, objects, pPriorities);
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
    if (device_ != nullptr)
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
                                             format::HandleId                       resource_capture_id,
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
    if (pHeap == nullptr)
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
        if (heap_id_desc_.find(heap_capture_id) == heap_id_desc_.end())
        {
            D3D12_HEAP_DESC heap_desc = pHeap->GetDesc();

            if (heap_desc.Properties.Type == D3D12_HEAP_TYPE_CUSTOM)
            {
                // remove SHARED and SHARED_CROSS_ADAPTER flags that are not allowed on real heaps
                heap_desc.Flags &= ~(D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

                D3D12_FEATURE_DATA_D3D12_OPTIONS opts = {};
                if (SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts))))
                {
                    if (opts.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_1)
                    {
                        if ((heap_desc.Flags & (D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES |
                                                D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)) == 0)
                        {
                            GFXRECON_LOG_WARNING(
                                "Adding DENY_RT_DS_TEXTURES|DENY_NON_RT_DS_TEXTURES to OpenExistingHeap heap "
                                "for tier 1 compatibility");
                            heap_desc.Flags |=
                                (D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES);
                        }
                    }
                }
            }

            heap_id_desc_.insert_or_assign(heap_capture_id, heap_desc);
        }

        // creat heap
        ID3D12Heap* pNewHeap = nullptr;
        if (heap_id_recreated_heap_.find(heap_capture_id) == heap_id_recreated_heap_.end())
        {
            ComPtr<ID3D12Heap> heap = nullptr;
            HRESULT            hr   = device_->CreateHeap(&heap_id_desc_[heap_capture_id], IID_PPV_ARGS(&heap));
            if (hr == S_OK)
            {
                pNewHeap = heap.Get();
                heap_id_recreated_heap_.insert_or_assign(heap_capture_id, heap);
                resource_id_recreated_heap_[resource_capture_id].push_back(std::move(heap));
            }
            else
            {
                pNewHeap = pHeap;
            }
        }
        else
        {
            pNewHeap = heap_id_recreated_heap_[heap_capture_id].Get();
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
}

void Dx12RebindAllocator::ReportResourceIncompatibility(const D3D12_RESOURCE_DESC* resource_desc)
{
    return;
}

void Dx12RebindAllocator::ReportResourceIncompatibility1(const D3D12_RESOURCE_DESC1* resource_desc)
{
    return;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
