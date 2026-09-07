/*
** Copyright (c) 2026 LunarG, Inc.
** Copyright (c) 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "encode/d3d12_capture_manager_arm_features.h"

#include "encode/d3d12_capture_manager.h"
#include "encode/dx12_object_wrapper_info.h"
#include "encode/dx12_object_wrapper_util.h"
#include "encode/dx12_state_writer.h"
#include "graphics/dx12_resource_data_util.h"
#include "util/memory_output_stream.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)

namespace
{
ID3D12Resource_Wrapper* GetResourceWrapper(IUnknown* object)
{
    IUnknown_Wrapper* wrapper = nullptr;
    if ((object == nullptr) || FAILED(object->QueryInterface(IID_IUnknown_Wrapper, reinterpret_cast<void**>(&wrapper))))
    {
        return nullptr;
    }

    auto resource_wrapper = ID3D12Resource_Wrapper::GetExistingWrapper(wrapper->GetWrappedObject());
    return resource_wrapper;
}
} // namespace

D3D12CaptureManagerArmFeatures::D3D12CaptureManagerArmFeatures(D3D12CaptureManager* manager) : manager_(manager) {}

D3D12CaptureManagerArmFeatures::~D3D12CaptureManagerArmFeatures() = default;

void D3D12CaptureManagerArmFeatures::WarnFullCaptureRequired()
{
    std::call_once(full_capture_warning_once_, []() {
        GFXRECON_LOG_WARNING(
            "External shared D3D12 resources and fences require full capture; start capture before creating or "
            "opening the shared object.");
    });
}

void D3D12CaptureManagerArmFeatures::DestroyDevice(ID3D12Device_Wrapper* wrapper)
{
    if (wrapper == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(output_mutex_);
    resource_data_utils_.erase(wrapper->GetCaptureId());
}

void D3D12CaptureManagerArmFeatures::DestroyResource(ID3D12Resource_Wrapper* wrapper)
{
    if (wrapper == nullptr)
    {
        return;
    }

    const auto resource_info = wrapper->GetObjectInfo();

    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
    const auto                  resource_id = wrapper->GetCaptureId();
    const auto                  erased      = shared_resources_.erase(resource_id);
    opened_shared_resources_.erase(resource_id);
    pending_shader_resource_snapshots_.erase(resource_id);
    active_shader_resource_snapshots_.erase(resource_id);
    for (auto& pending_snapshot : pending_snapshots_)
    {
        pending_snapshot.second.erase(resource_id);
    }
    if (resource_info != nullptr)
    {
        resource_info->is_explicit_shared_resource.store(false, std::memory_order_release);
    }
    if (erased != 0)
    {
        GFXRECON_LOG_DEBUG("Untracked released shared resource (id = %" PRIu64 ").", resource_id);
    }
}

void D3D12CaptureManagerArmFeatures::PostProcessCreateSharedHandle(ID3D12Device_Wrapper*      wrapper,
                                                                   HRESULT                    result,
                                                                   ID3D12DeviceChild*         pObject,
                                                                   const SECURITY_ATTRIBUTES* pAttributes,
                                                                   DWORD                      Access,
                                                                   LPCWSTR                    Name,
                                                                   HANDLE*                    pHandle)
{
    GFXRECON_UNREFERENCED_PARAMETER(wrapper);
    GFXRECON_UNREFERENCED_PARAMETER(pAttributes);
    GFXRECON_UNREFERENCED_PARAMETER(Access);
    GFXRECON_UNREFERENCED_PARAMETER(Name);

    if (SUCCEEDED(result) && (pHandle != nullptr) && (*pHandle != nullptr))
    {
        IUnknown_Wrapper* object_wrapper = nullptr;
        if ((pObject == nullptr) || FAILED(reinterpret_cast<IUnknown*>(pObject)->QueryInterface(
                                        IID_IUnknown_Wrapper, reinterpret_cast<void**>(&object_wrapper))))
        {
            return;
        }

        auto resource_wrapper = ID3D12Resource_Wrapper::GetExistingWrapper(object_wrapper->GetWrappedObject());
        auto fence_wrapper    = ID3D12Fence_Wrapper::GetExistingWrapper(object_wrapper->GetWrappedObject());
        if (manager_->IsCaptureModeWrite())
        {
            if (resource_wrapper != nullptr)
            {
                TrackSharedResource(pObject, false);
            }
        }
        else if (manager_->IsCaptureModeTrack() && ((resource_wrapper != nullptr) || (fence_wrapper != nullptr)))
        {
            WarnFullCaptureRequired();
        }
    }
}

void D3D12CaptureManagerArmFeatures::TrackSharedResource(ID3D12DeviceChild* object, bool opened_shared_resource)
{
    if ((object == nullptr) || !manager_->IsCaptureModeWrite())
    {
        return;
    }

    auto resource_wrapper = GetResourceWrapper(reinterpret_cast<IUnknown*>(object));
    if (resource_wrapper == nullptr)
    {
        return;
    }

    auto resource_info = resource_wrapper->GetObjectInfo();
    if (resource_info == nullptr)
    {
        return;
    }
    const auto                  resource_id = resource_wrapper->GetCaptureId();
    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
    shared_resources_[resource_id] = resource_wrapper;
    if (opened_shared_resource)
    {
        opened_shared_resources_.insert(resource_id);
    }
    // Publish after the map entry is visible to fast-path readers.
    resource_info->is_explicit_shared_resource.store(true, std::memory_order_release);
}

void D3D12CaptureManagerArmFeatures::PreProcessCopyTextureRegion(ID3D12GraphicsCommandList_Wrapper* wrapper,
                                                                 const D3D12_TEXTURE_COPY_LOCATION* pDst,
                                                                 UINT                               DstX,
                                                                 UINT                               DstY,
                                                                 UINT                               DstZ,
                                                                 const D3D12_TEXTURE_COPY_LOCATION* pSrc,
                                                                 const D3D12_BOX*                   pSrcBox)
{
    GFXRECON_UNREFERENCED_PARAMETER(pDst);
    GFXRECON_UNREFERENCED_PARAMETER(DstX);
    GFXRECON_UNREFERENCED_PARAMETER(DstY);
    GFXRECON_UNREFERENCED_PARAMETER(DstZ);
    GFXRECON_UNREFERENCED_PARAMETER(pSrcBox);

    if ((pSrc == nullptr) || (pSrc->pResource == nullptr) || !manager_->IsCaptureModeWrite())
    {
        return;
    }

    auto source_wrapper = GetResourceWrapper(reinterpret_cast<IUnknown*>(pSrc->pResource));
    if (source_wrapper == nullptr)
    {
        return;
    }

    if (wrapper == nullptr)
    {
        return;
    }

    const auto resource_info = source_wrapper->GetObjectInfo();
    if (resource_info == nullptr)
    {
        return;
    }

    if (!resource_info->is_explicit_shared_resource.load(std::memory_order_acquire))
    {
        return;
    }

    const auto                  resource_id     = source_wrapper->GetCaptureId();
    const auto                  command_list_id = wrapper->GetCaptureId();
    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
    if (shared_resources_.count(resource_id) != 0)
    {
        // Defer the readback until the command list is submitted so prior work on its application queue is complete.
        pending_snapshots_[command_list_id].insert(resource_id);
    }
}

void D3D12CaptureManagerArmFeatures::PostProcessCreateShaderResourceView(ID3D12Resource* resource)
{
    if ((resource == nullptr) || !manager_->IsCaptureModeWrite())
    {
        return;
    }

    auto resource_wrapper = GetResourceWrapper(reinterpret_cast<IUnknown*>(resource));
    if (resource_wrapper == nullptr)
    {
        return;
    }

    const auto resource_info = resource_wrapper->GetObjectInfo();
    if ((resource_info == nullptr) || !resource_info->is_explicit_shared_resource.load(std::memory_order_acquire))
    {
        return;
    }

    const auto                  resource_id = resource_wrapper->GetCaptureId();
    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
    if ((shared_resources_.count(resource_id) == 0) || (opened_shared_resources_.count(resource_id) == 0))
    {
        return;
    }

    const bool refresh_enabled            = manager_->GetCaptureOpenSharedResourceRefreshSetting();
    const bool first_shader_resource_view = active_shader_resource_snapshots_.insert(resource_id).second;
    if (first_shader_resource_view)
    {
        if (refresh_enabled)
        {
            GFXRECON_LOG_INFO("Tracking opened shared D3D12 resource for shader-resource snapshots (id = %" PRIu64 ").",
                              resource_id);
        }
        else
        {
            GFXRECON_LOG_INFO(
                "Capturing initial contents of opened shared D3D12 resource; per-present refresh is disabled "
                "(id = %" PRIu64 ").",
                resource_id);
        }
    }
    else if (!refresh_enabled)
    {
        return;
    }

    if (pending_shader_resource_snapshots_.insert(resource_id).second)
    {
        GFXRECON_LOG_DEBUG("Queued opened shared resource snapshot for shader-resource view (id = %" PRIu64 ").",
                           resource_id);
    }
}

void D3D12CaptureManagerArmFeatures::PostProcessOpenSharedHandle(
    ID3D12Device_Wrapper* wrapper, HRESULT result, HANDLE NTHandle, REFIID riid, void** ppvObj)
{
    GFXRECON_UNREFERENCED_PARAMETER(NTHandle);

    if (FAILED(result) || (wrapper == nullptr) || (ppvObj == nullptr) || ((*ppvObj) == nullptr))
    {
        return;
    }

    const bool is_resource = IsEqualGUID(riid, __uuidof(ID3D12Resource));
    const bool is_fence    = IsEqualGUID(riid, __uuidof(ID3D12Fence)) || IsEqualGUID(riid, __uuidof(ID3D12Fence1));
    if (!is_resource)
    {
        if (is_fence && manager_->IsCaptureModeTrack() && !manager_->IsCaptureModeWrite())
        {
            WarnFullCaptureRequired();
        }
        return;
    }

    if (!manager_->IsCaptureModeWrite())
    {
        if (manager_->IsCaptureModeTrack())
        {
            WarnFullCaptureRequired();
        }
        return;
    }

    auto resource_wrapper = GetResourceWrapper(reinterpret_cast<IUnknown*>(*ppvObj));
    if (resource_wrapper == nullptr)
    {
        GFXRECON_LOG_WARNING("Unable to resolve the wrapper for an opened shared D3D12 resource.");
        return;
    }

    auto resource = resource_wrapper->GetWrappedObjectAs<ID3D12Resource>();
    if (resource == nullptr)
    {
        GFXRECON_LOG_WARNING("Unable to resolve the native object for an opened shared D3D12 resource.");
        return;
    }

    D3D12_HEAP_PROPERTIES heap_properties{};
    D3D12_HEAP_FLAGS      heap_flags  = D3D12_HEAP_FLAG_NONE;
    HRESULT               heap_result = resource->GetHeapProperties(&heap_properties, &heap_flags);
    if (FAILED(heap_result))
    {
        GFXRECON_LOG_WARNING("GetHeapProperties failed for an opened shared D3D12 resource (error = %lx).",
                             heap_result);
    }

    auto desc = resource->GetDesc();
    manager_->InitializeID3D12ResourceInfo(
        wrapper,
        resource_wrapper,
        desc.Dimension,
        desc.Layout,
        desc.Width,
        heap_properties.Type,
        heap_properties.CPUPageProperty,
        heap_properties.MemoryPoolPreference,
        heap_flags,
        D3D12_RESOURCE_STATE_COMMON,
        SUCCEEDED(heap_result) &&
            manager_->UseWriteWatch(heap_properties.Type, heap_flags, heap_properties.CPUPageProperty),
        nullptr,
        0);

    // Its contents are deferred until a safe queue-submit boundary.
    TrackSharedResource(reinterpret_cast<ID3D12DeviceChild*>(resource_wrapper), true);
}

graphics::Dx12ResourceDataUtil*
D3D12CaptureManagerArmFeatures::GetResourceDataUtil(ID3D12Resource_Wrapper* resource_wrapper)
{
    if (resource_wrapper == nullptr)
    {
        return nullptr;
    }

    const auto resource_info = resource_wrapper->GetObjectInfo();
    if ((resource_info == nullptr) || (resource_info->device_wrapper == nullptr))
    {
        GFXRECON_LOG_ERROR("Failed to get a readback utility for shared resource (id = %" PRIu64
                           "): resource information is unavailable.",
                           resource_wrapper->GetCaptureId());
        return nullptr;
    }

    const auto device_id  = resource_info->device_wrapper->GetCaptureId();
    auto       utility_it = resource_data_utils_.find(device_id);
    if (utility_it != resource_data_utils_.end())
    {
        return utility_it->second.get();
    }

    auto device = resource_info->device_wrapper->GetWrappedObjectAs<ID3D12Device>();
    if (device == nullptr)
    {
        GFXRECON_LOG_ERROR("Failed to get a readback utility for shared resource (id = %" PRIu64
                           "): native device is null.",
                           resource_wrapper->GetCaptureId());
        return nullptr;
    }

    auto utility = std::make_unique<graphics::Dx12ResourceDataUtil>(device, 0);
    auto result  = utility.get();
    resource_data_utils_.emplace(device_id, std::move(utility));
    return result;
}

D3D12CaptureManagerArmFeatures::SharedResourceSnapshotResult D3D12CaptureManagerArmFeatures::WriteSharedResourceContent(
    ID3D12Resource_Wrapper*                               resource_wrapper,
    const std::vector<graphics::dx12::ResourceStateInfo>& resource_states,
    ID3D12CommandQueue*                                   consumer_queue)
{
    if (resource_wrapper == nullptr)
    {
        return SharedResourceSnapshotResult::kUnavailable;
    }

    auto thread_data = manager_->GetThreadData();
    GFXRECON_ASSERT(thread_data != nullptr);

    std::lock_guard<std::mutex> lock(output_mutex_);
    auto                        resource_data_util = GetResourceDataUtil(resource_wrapper);
    if (resource_data_util == nullptr)
    {
        return SharedResourceSnapshotResult::kUnavailable;
    }

    util::MemoryOutputStream memory_stream;
    Dx12StateWriter          state_writer(&memory_stream, manager_->GetCompressor(), thread_data->thread_id_);
    uint64_t                 block_count =
        state_writer.WriteSharedResourceContent(resource_wrapper, resource_states, consumer_queue, resource_data_util);
    if ((block_count == 0) || (memory_stream.GetDataSize() == 0))
    {
        return SharedResourceSnapshotResult::kUnavailable;
    }

    manager_->common_manager_->WriteToFile(memory_stream.GetData(), memory_stream.GetDataSize());
    if (block_count > 1)
    {
        manager_->common_manager_->IncrementBlockIndex(block_count - 1);
    }

    return SharedResourceSnapshotResult::kWritten;
}

void D3D12CaptureManagerArmFeatures::WriteSharedResourceSnapshots(
    const std::unordered_set<format::HandleId>& resource_ids, ID3D12CommandQueue* consumer_queue)
{
    if (resource_ids.empty())
    {
        return;
    }

    struct PendingResourceSnapshot
    {
        ID3D12Resource_Wrapper*                        resource_wrapper{ nullptr };
        std::vector<graphics::dx12::ResourceStateInfo> resource_states;
    };

    std::vector<PendingResourceSnapshot> resources;
    {
        std::lock_guard<std::mutex> lock(shared_resources_mutex_);
        for (auto resource_id : resource_ids)
        {
            auto resource_it = shared_resources_.find(resource_id);
            if (resource_it == shared_resources_.end())
            {
                continue;
            }

            auto resource_wrapper = resource_it->second;
            resource_wrapper->AddRef();

            auto resource_info = resource_wrapper->GetObjectInfo();
            if (resource_info == nullptr)
            {
                resource_wrapper->Release();
                continue;
            }

            std::lock_guard<std::mutex> state_lock(shared_resource_state_mutex_);
            auto                        resource_states = resource_info->subresource_transitions;
            if (resource_states.empty() && (opened_shared_resources_.count(resource_id) != 0) &&
                (resource_info->num_subresources != 0))
            {
                resource_states.assign(resource_info->num_subresources,
                                       graphics::dx12::ResourceStateInfo{ resource_info->initial_state,
                                                                          D3D12_RESOURCE_BARRIER_FLAG_NONE });
                GFXRECON_LOG_DEBUG("Using the initial resource state for opened shared resource snapshot (id = %" PRIu64
                                   ").",
                                   resource_id);
            }
            resources.push_back({ resource_wrapper, std::move(resource_states) });
        }
    }

    for (size_t resource_index = 0; resource_index < resources.size(); ++resource_index)
    {
        const auto& snapshot = resources[resource_index];
        const auto  result =
            WriteSharedResourceContent(snapshot.resource_wrapper, snapshot.resource_states, consumer_queue);
        const auto resource_id = snapshot.resource_wrapper->GetCaptureId();
        snapshot.resource_wrapper->Release();

        if (result != SharedResourceSnapshotResult::kWritten)
        {
            for (++resource_index; resource_index < resources.size(); ++resource_index)
            {
                resources[resource_index].resource_wrapper->Release();
            }

            GFXRECON_LOG_ERROR("Stopping capture: shared resource (id = %" PRIu64 ") could not be snapshotted.",
                               resource_id);
            manager_->WriteDisplayMessageCmd(
                "Capture stopped: shared-resource content could not be captured before replay use.");
            manager_->SetCaptureMode(CommonCaptureManager::kModeDisabled);
            return;
        }
    }
}

void D3D12CaptureManagerArmFeatures::WritePendingSharedResourceSnapshots(ID3D12CommandQueue_Wrapper* queue_wrapper,
                                                                         UINT                        num_lists,
                                                                         ID3D12CommandList* const*   lists)
{
    if (!manager_->IsCaptureModeWrite() || (queue_wrapper == nullptr) || (num_lists == 0) || (lists == nullptr))
    {
        return;
    }

    auto queue = queue_wrapper->GetWrappedObjectAs<ID3D12CommandQueue>();
    if (queue == nullptr)
    {
        return;
    }

    std::unordered_set<format::HandleId> resource_ids;
    {
        std::lock_guard<std::mutex> lock(shared_resources_mutex_);
        resource_ids.insert(pending_shader_resource_snapshots_.begin(), pending_shader_resource_snapshots_.end());
        pending_shader_resource_snapshots_.clear();

        for (UINT i = 0; i < num_lists; ++i)
        {
            if (lists[i] == nullptr)
            {
                continue;
            }

            auto list_wrapper = reinterpret_cast<ID3D12CommandList_Wrapper*>(lists[i]);
            auto pending_it   = pending_snapshots_.find(list_wrapper->GetCaptureId());
            if (pending_it == pending_snapshots_.end())
            {
                continue;
            }

            resource_ids.insert(pending_it->second.begin(), pending_it->second.end());
            pending_snapshots_.erase(pending_it);
        }
    }

    WriteSharedResourceSnapshots(resource_ids, queue);
}

void D3D12CaptureManagerArmFeatures::RefreshShaderResourceSnapshots(ID3D12CommandQueue* consumer_queue)
{
    if (!manager_->GetCaptureOpenSharedResourceRefreshSetting() || !manager_->IsCaptureModeWrite() ||
        (consumer_queue == nullptr))
    {
        return;
    }

    std::unordered_set<format::HandleId> resource_ids;
    {
        std::lock_guard<std::mutex> lock(shared_resources_mutex_);
        resource_ids = active_shader_resource_snapshots_;
    }

    WriteSharedResourceSnapshots(resource_ids, consumer_queue);
}

void D3D12CaptureManagerArmFeatures::ClearPendingSharedResourceSnapshots(format::HandleId command_list_id)
{
    std::lock_guard<std::mutex> lock(shared_resources_mutex_);
    pending_snapshots_.erase(command_list_id);
}

void D3D12CaptureManagerArmFeatures::TrackWriteModeResourceBarriers(ID3D12CommandList_Wrapper*    list_wrapper,
                                                                    UINT                          num_barriers,
                                                                    const D3D12_RESOURCE_BARRIER* barriers,
                                                                    bool                          record_barriers)
{
    if ((list_wrapper == nullptr) || (barriers == nullptr))
    {
        return;
    }

    auto list_info = list_wrapper->GetObjectInfo();
    if (list_info == nullptr)
    {
        return;
    }

    for (UINT i = 0; i < num_barriers; ++i)
    {
        const auto& barrier = barriers[i];
        if (barrier.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
        {
            continue;
        }

        auto resource_wrapper = GetResourceWrapper(reinterpret_cast<IUnknown*>(barrier.Transition.pResource));
        if (resource_wrapper == nullptr)
        {
            continue;
        }

        auto resource_info = resource_wrapper->GetObjectInfo();
        if (resource_info == nullptr)
        {
            continue;
        }
        if (!resource_info->is_explicit_shared_resource.load(std::memory_order_acquire))
        {
            continue;
        }

        std::lock_guard<std::mutex> state_lock(shared_resource_state_mutex_);
        const bool is_all_subresources = barrier.Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        if (resource_info->subresource_transitions.empty() && is_all_subresources &&
            (barrier.Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE))
        {
            // For an imported resource, StateBefore is the only authoritative state available before this list
            // executes. Do not infer a state when a transition only names a subset of subresources.
            resource_info->subresource_transitions.assign(
                resource_info->num_subresources,
                graphics::dx12::ResourceStateInfo{ barrier.Transition.StateBefore, D3D12_RESOURCE_BARRIER_FLAG_NONE });
        }

        if (record_barriers)
        {
            DxTransitionBarrier transition;
            transition.resource_wrapper = resource_wrapper;
            transition.subresource      = barrier.Transition.Subresource;
            transition.state_before     = barrier.Transition.StateBefore;
            transition.state_after      = barrier.Transition.StateAfter;
            transition.barrier_flags    = barrier.Flags;
            list_info->transition_barriers.push_back(transition);
        }
    }
}

void D3D12CaptureManagerArmFeatures::TrackWriteModeExecuteCommandLists(UINT num_lists, ID3D12CommandList* const* lists)
{
    if ((num_lists > 0) && (lists == nullptr))
    {
        return;
    }

    for (UINT i = 0; i < num_lists; ++i)
    {
        if (lists[i] == nullptr)
        {
            continue;
        }

        auto list_wrapper = reinterpret_cast<ID3D12CommandList_Wrapper*>(lists[i]);
        auto list_info    = list_wrapper->GetObjectInfo();
        if (list_info == nullptr)
        {
            continue;
        }

        for (const auto& transition : list_info->transition_barriers)
        {
            auto resource_info = transition.resource_wrapper->GetObjectInfo();
            if ((resource_info == nullptr) ||
                !resource_info->is_explicit_shared_resource.load(std::memory_order_acquire))
            {
                continue;
            }

            std::lock_guard<std::mutex> state_lock(shared_resource_state_mutex_);
            if ((resource_info->subresource_transitions.size() != resource_info->num_subresources) ||
                (transition.barrier_flags != D3D12_RESOURCE_BARRIER_FLAG_NONE))
            {
                resource_info->subresource_transitions.clear();
                continue;
            }

            const auto update_state = [&](UINT subresource) {
                auto& current_state = resource_info->subresource_transitions[subresource];
                if ((current_state.states != transition.state_before) ||
                    (current_state.barrier_flags != D3D12_RESOURCE_BARRIER_FLAG_NONE))
                {
                    GFXRECON_LOG_WARNING("Lost tracked resource state for resource (id = %" PRIu64
                                         ") after an unexpected resource barrier.",
                                         transition.resource_wrapper->GetCaptureId());
                    resource_info->subresource_transitions.clear();
                    return false;
                }

                current_state = { transition.state_after, transition.barrier_flags };
                return true;
            };

            if (transition.subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
            {
                for (UINT subresource = 0; subresource < resource_info->num_subresources; ++subresource)
                {
                    if (!update_state(subresource))
                    {
                        break;
                    }
                }
            }
            else if (transition.subresource < resource_info->num_subresources)
            {
                update_state(transition.subresource);
            }
            else
            {
                GFXRECON_LOG_WARNING("Lost tracked resource state for resource (id = %" PRIu64
                                     ") after an invalid subresource barrier.",
                                     transition.resource_wrapper->GetCaptureId());
                resource_info->subresource_transitions.clear();
            }
        }
    }
}

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)
