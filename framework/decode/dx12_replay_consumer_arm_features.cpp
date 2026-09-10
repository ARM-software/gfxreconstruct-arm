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

#include "dx12_replay_consumer_arm_features.h"
#include "dx12_replay_consumer_base.h"
#include "decode/dx12_enum_util.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

Dx12ReplayConsumerArmFeatures::Dx12ReplayConsumerArmFeatures(Dx12ReplayConsumerBase* consumer) : consumer_(consumer) {}

void Dx12ReplayConsumerArmFeatures::TrackPlaceholderSharedResource(format::HandleId resource_id)
{
    placeholder_shared_resources_.insert(resource_id);
}

void Dx12ReplayConsumerArmFeatures::ForgetPlaceholder(format::HandleId object_id)
{
    placeholder_shared_resources_.erase(object_id);
    outgoing_shared_fences_.erase(object_id);
}

void Dx12ReplayConsumerArmFeatures::TrackOutgoingSharedFence(format::HandleId fence_id)
{
    outgoing_shared_fences_.insert(fence_id);
}

bool Dx12ReplayConsumerArmFeatures::IsOutgoingSharedFence(format::HandleId fence_id) const
{
    return outgoing_shared_fences_.count(fence_id) > 0;
}

void Dx12ReplayConsumerArmFeatures::OnSharedResourceGetDesc(DxObjectInfo*              resource_object_info,
                                                            const D3D12_RESOURCE_DESC* captured_desc)
{
    if ((resource_object_info == nullptr) || (captured_desc == nullptr))
    {
        return;
    }

    if (placeholder_shared_resources_.count(resource_object_info->capture_id) == 0)
    {
        return;
    }

    // The supported imported-resource trace calls GetDesc before creating any descriptor or GPU use.
    if (RecreateSharedResourcePlaceholder(resource_object_info, *captured_desc))
    {
        placeholder_shared_resources_.erase(resource_object_info->capture_id);
    }
}

bool Dx12ReplayConsumerArmFeatures::RecreateSharedResourcePlaceholder(DxObjectInfo*              resource_object_info,
                                                                      const D3D12_RESOURCE_DESC& captured_desc)
{
    auto resource_info = GetExtraInfo<D3D12ResourceInfo>(resource_object_info);
    if ((resource_object_info->object == nullptr) || (resource_info == nullptr))
    {
        return false;
    }

    auto                               placeholder = static_cast<ID3D12Resource*>(resource_object_info->object);
    graphics::dx12::ID3D12DeviceComPtr device      = nullptr;
    if (FAILED(placeholder->GetDevice(IID_PPV_ARGS(&device))) || (device == nullptr))
    {
        return false;
    }

    D3D12_HEAP_PROPERTIES heap_properties = {};
    heap_properties.Type                  = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = captured_desc;
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    }

    ID3D12Resource* sized_resource = nullptr;
    HRESULT         create_result  = device->CreateCommittedResource(&heap_properties,
                                                            D3D12_HEAP_FLAG_NONE,
                                                            &desc,
                                                            D3D12_RESOURCE_STATE_COMMON,
                                                            nullptr,
                                                            IID_PPV_ARGS(&sized_resource));
    if (FAILED(create_result) || (sized_resource == nullptr))
    {
        GFXRECON_LOG_WARNING("Unable to recreate placeholder shared resource %" PRIu64
                             " at its captured description (%s); keeping the minimal placeholder.",
                             resource_object_info->capture_id,
                             enumutil::GetResultValueString(create_result).c_str());
        return false;
    }

    for (uint64_t i = 1; i < resource_object_info->ref_count; ++i)
    {
        sized_resource->AddRef();
    }

    if (resource_object_info->object != nullptr)
    {
        const uint64_t release_count = (resource_object_info->ref_count == 0) ? 1 : resource_object_info->ref_count;
        for (uint64_t i = 0; i < release_count; ++i)
        {
            static_cast<IUnknown*>(resource_object_info->object)->Release();
        }
    }
    resource_object_info->object = sized_resource;

    resource_info->desc                  = {};
    resource_info->desc.Dimension        = desc.Dimension;
    resource_info->desc.Alignment        = desc.Alignment;
    resource_info->desc.Width            = desc.Width;
    resource_info->desc.Height           = desc.Height;
    resource_info->desc.DepthOrArraySize = desc.DepthOrArraySize;
    resource_info->desc.MipLevels        = desc.MipLevels;
    resource_info->desc.Format           = desc.Format;
    resource_info->desc.SampleDesc       = desc.SampleDesc;
    resource_info->desc.Layout           = desc.Layout;
    resource_info->desc.Flags            = desc.Flags;

    resource_info->subresource_count = graphics::Dx12ResourceDataUtil::GetSubresourceCount(sized_resource);
    resource_info->resource_state_infos.assign(
        resource_info->subresource_count,
        graphics::dx12::ResourceStateInfo{ D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_BARRIER_FLAG_NONE });

    return true;
}

void Dx12ReplayConsumerArmFeatures::CheckReplayResult(const char* call_name,
                                                      HRESULT     capture_result,
                                                      HRESULT     replay_result)
{
    if ((consumer_->options_.enable_debug_device_lost) && (replay_result == DXGI_ERROR_DEVICE_REMOVED))
    {
        // Build a replay-pointer -> capture-id reverse lookup so DRED breadcrumb nodes (which only carry raw
        // ID3D12 object pointers) can be reported with the capture handle ids that match the trace (the same
        // ids exposed as "handle" fields in a gfxrecon JSONL export).
        std::unordered_map<const void*, uint64_t> object_to_capture_id;
        for (const auto& [object_id, info] : consumer_->GetObjectInfoTable())
        {
            if (info.object != nullptr)
            {
                object_to_capture_id[info.object] = object_id;
            }
        }
        auto resolve_capture_id = [&object_to_capture_id](const void* object) -> uint64_t {
            auto it = object_to_capture_id.find(object);
            return (it != object_to_capture_id.end()) ? it->second : 0;
        };

        for (const auto& [id, device] : consumer_->active_devices_)
        {
            auto device_ptr = reinterpret_cast<ID3D12Device*>(const_cast<void*>(device));

            HRESULT reason = device_ptr->GetDeviceRemovedReason();
            if (reason != S_OK)
            {
                gfxrecon::graphics::dx12::AnalyzeDeviceRemoved(device_ptr, resolve_capture_id);
            }
        }
    }

    if ((capture_result == S_OK) &&
        ((replay_result == DXGI_ERROR_NOT_FOUND) || (replay_result == DXGI_ERROR_MORE_DATA)))
    {
        GFXRECON_LOG_DEBUG("%s returned %s, which does not match the value returned at capture %s.",
                           call_name,
                           enumutil::GetResultValueString(replay_result).c_str(),
                           enumutil::GetResultValueString(capture_result).c_str());
        return;
    }

    if (capture_result != replay_result)
    {
        if ((replay_result == DXGI_ERROR_DEVICE_REMOVED) || (replay_result == E_OUTOFMEMORY))
        {
            graphics::dx12::IDXGIFactory1ComPtr factory  = nullptr;
            graphics::dx12::IDXGIAdapter1ComPtr adapter1 = nullptr;
            graphics::dx12::IDXGIAdapter3ComPtr adapter3 = nullptr;

            HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
            GFXRECON_ASSERT(SUCCEEDED(result) && (factory != nullptr));

            int32_t adapter_index =
                consumer_->options_.override_gpu_index >= 0 ? consumer_->options_.override_gpu_index : 0;
            result = factory->EnumAdapters1(adapter_index, &adapter1.GetInterfacePtr());
            GFXRECON_ASSERT(SUCCEEDED(result) && (adapter1 != nullptr));
            adapter1->QueryInterface(IID_PPV_ARGS(&adapter3));
            if (adapter3 != nullptr)
            {
                DXGI_QUERY_VIDEO_MEMORY_INFO memInfo = {};
                adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo);
                GFXRECON_LOG_INFO("GPU Memory Usage: %" PRIu64 " KB / %" PRIu64 " KB ",
                                  memInfo.CurrentUsage / 1024,
                                  memInfo.Budget / 1024);
            }
        }

        if ((replay_result == DXGI_ERROR_DEVICE_REMOVED) || (replay_result == D3D12_ERROR_INVALID_REDIST) ||
            (replay_result == DXGI_ERROR_DEVICE_RESET) || (replay_result == DXGI_ERROR_DEVICE_HUNG))
        {
            GFXRECON_LOG_FATAL(
                "%s returned %s, which does not match the value returned at capture %s. Replay cannot continue.",
                call_name,
                enumutil::GetResultValueString(replay_result).c_str(),
                enumutil::GetResultValueString(capture_result).c_str());
        }
        else
        {
            GFXRECON_LOG_WARNING("%s returned %s, which does not match the value returned at capture %s.",
                                 call_name,
                                 enumutil::GetResultValueString(replay_result).c_str(),
                                 enumutil::GetResultValueString(capture_result).c_str());
        }
    }
}

void Dx12ReplayConsumerArmFeatures::LogFrameDebugInfo()
{
    if (util::Log::WillOutputMessage(util::LoggingSeverity::kDebug))
    {
        graphics::dx12::IDXGIAdapter3ComPtr adapter3 = nullptr;
        graphics::dx12::IDXGIAdapterComPtr  adapter  = consumer_->GetAdapter();
        if (adapter == nullptr)
        {
            GFXRECON_LOG_DEBUG("Completed frame %d", consumer_->application_->GetCurrentFrameNumber() + 1);
            return;
        }

        adapter->QueryInterface(IID_PPV_ARGS(&adapter3));
        if (adapter3 != nullptr)
        {
            DXGI_QUERY_VIDEO_MEMORY_INFO local_mem_info     = {};
            DXGI_QUERY_VIDEO_MEMORY_INFO non_local_mem_info = {};

            adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local_mem_info);
            adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &non_local_mem_info);

            const double f = 1024.0 * 1024.0;
            GFXRECON_LOG_DEBUG("Frame %d local memory (mb): %.02f Budget, %.02f current Usage, %.02f current "
                               "Reservation, %.02f available Reservation",
                               consumer_->application_->GetCurrentFrameNumber() + 1,
                               (double)local_mem_info.Budget / f,
                               (double)local_mem_info.CurrentUsage / f,
                               (double)local_mem_info.CurrentReservation / f,
                               (double)local_mem_info.AvailableForReservation / f);
            GFXRECON_LOG_DEBUG("Frame %d non local memory (mb): %.02f Budget, %.02f current Usage, %.02f current "
                               "Reservation, %.02f available Reservation",
                               consumer_->application_->GetCurrentFrameNumber() + 1,
                               (double)non_local_mem_info.Budget / f,
                               (double)non_local_mem_info.CurrentUsage / f,
                               (double)non_local_mem_info.CurrentReservation / f,
                               (double)non_local_mem_info.AvailableForReservation / f);
        }
        else
        {
            GFXRECON_LOG_DEBUG("Completed frame %d", consumer_->application_->GetCurrentFrameNumber() + 1);
        }
    }
}

HRESULT Dx12ReplayConsumerArmFeatures::CreateSwapChainForComposition(DxObjectInfo*         replay_object_info,
                                                                     HRESULT               original_result,
                                                                     DxObjectInfo*         device_info,
                                                                     DXGI_SWAP_CHAIN_DESC* desc,
                                                                     HandlePointerDecoder<IDXGISwapChain*>* swapchain)
{
    GFXRECON_ASSERT((replay_object_info != nullptr) && (replay_object_info->object != nullptr) &&
                    (swapchain != nullptr));

    HRESULT result = E_FAIL;
    Window* window = nullptr;

    auto         replay_object      = static_cast<IDXGIFactory2*>(replay_object_info->object);
    IUnknown*    device             = nullptr;
    IDXGIOutput* restrict_to_output = nullptr;

    if (device_info != nullptr)
    {
        device = device_info->object;
    }

    // convert DXGI_SWAP_CHAIN_DESC to DXGI_SWAP_CHAIN_DESC1
    DXGI_SWAP_CHAIN_DESC1* pDesc = new DXGI_SWAP_CHAIN_DESC1();
    pDesc->Width                 = desc->BufferDesc.Width;
    pDesc->Height                = desc->BufferDesc.Height;
    pDesc->Format                = desc->BufferDesc.Format;
    pDesc->Stereo                = FALSE;
    pDesc->SampleDesc            = desc->SampleDesc;
    pDesc->BufferUsage           = desc->BufferUsage;
    pDesc->BufferCount           = desc->BufferCount;
    pDesc->Scaling               = DXGI_SCALING_STRETCH;
    pDesc->SwapEffect            = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    pDesc->Flags                 = desc->Flags;

    if (pDesc->Width == 0 || pDesc->Height == 0)
    {
        auto resolve_dim = [](UINT cur, uint32_t opt, int metric, UINT fallback, const char*& src) -> UINT {
            if (cur > 0)
            {
                src = "original";
                return cur;
            }
            if (opt > 0)
            {
                src = "consumer";
                return opt;
            }
            int v = GetSystemMetrics(metric);
            if (v > 0)
            {
                src = "system";
                return static_cast<UINT>(v);
            }
            src = "default";
            return fallback;
        };

        const char* width_src  = "";
        const char* height_src = "";
        UINT new_width  = resolve_dim(pDesc->Width, consumer_->options_.windowed_width, SM_CXSCREEN, 320, width_src);
        UINT new_height = resolve_dim(pDesc->Height, consumer_->options_.windowed_height, SM_CYSCREEN, 240, height_src);

        pDesc->Width  = new_width;
        pDesc->Height = new_height;
        GFXRECON_LOG_WARNING_ONCE(("Swapchain zero dimension resolved: Width=" + std::to_string(new_width) + " (" +
                                   width_src + "), Height=" + std::to_string(new_height) + " (" + height_src + ").")
                                      .c_str());
    }

    IDXGISwapChain1* pSwapchain1 = nullptr;

    result = replay_object->CreateSwapChainForComposition(device, pDesc, restrict_to_output, &pSwapchain1);
    *(swapchain->GetHandlePointer()) = pSwapchain1;

    if (SUCCEEDED(result))
    {
        auto object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
        consumer_->SetSwapchainInfo(object_info, window, 0, 0, desc->BufferCount, device, desc->Windowed, true);
    }

    delete pDesc;

    return result;
}

HRESULT Dx12ReplayConsumerArmFeatures::CreateSwapChainForComposition(DxObjectInfo*          replay_object_info,
                                                                     HRESULT                original_result,
                                                                     DxObjectInfo*          device_info,
                                                                     uint64_t               hwnd_id,
                                                                     DXGI_SWAP_CHAIN_DESC1* desc,
                                                                     DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full_screen_desc,
                                                                     DxObjectInfo* restrict_to_output_info,
                                                                     HandlePointerDecoder<IDXGISwapChain1*>* swapchain)
{
    GFXRECON_ASSERT((replay_object_info != nullptr) && (replay_object_info->object != nullptr) &&
                    (swapchain != nullptr));

    HRESULT result = E_FAIL;
    Window* window = nullptr;

    auto         replay_object      = static_cast<IDXGIFactory2*>(replay_object_info->object);
    IUnknown*    device             = nullptr;
    IDXGIOutput* restrict_to_output = nullptr;

    if (device_info != nullptr)
    {
        device = device_info->object;
    }

    if (restrict_to_output_info != nullptr)
    {
        restrict_to_output = static_cast<IDXGIOutput*>(restrict_to_output_info->object);
    }

    if ((consumer_->options_.force_windowed) || (consumer_->options_.force_windowed_origin))
    {
        full_screen_desc = nullptr;
    }

    if (desc->Width == 0 || desc->Height == 0)
    {
        auto resolve_dim = [](UINT cur, uint32_t opt, int metric, UINT fallback, const char*& src) -> UINT {
            if (cur > 0)
            {
                src = "original";
                return cur;
            }
            if (opt > 0)
            {
                src = "consumer";
                return opt;
            }
            int v = GetSystemMetrics(metric);
            if (v > 0)
            {
                src = "system";
                return static_cast<UINT>(v);
            }
            src = "default";
            return fallback;
        };

        const char* width_src  = "";
        const char* height_src = "";
        UINT new_width  = resolve_dim(desc->Width, consumer_->options_.windowed_width, SM_CXSCREEN, 320, width_src);
        UINT new_height = resolve_dim(desc->Height, consumer_->options_.windowed_height, SM_CYSCREEN, 240, height_src);

        desc->Width  = new_width;
        desc->Height = new_height;
        GFXRECON_LOG_WARNING_ONCE(("Swapchain zero dimension resolved: Width=" + std::to_string(new_width) + " (" +
                                   width_src + "), Height=" + std::to_string(new_height) + " (" + height_src + ").")
                                      .c_str());
    }

    desc->Scaling    = DXGI_SCALING_STRETCH;
    desc->SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    result =
        replay_object->CreateSwapChainForComposition(device, desc, restrict_to_output, swapchain->GetHandlePointer());

    if (SUCCEEDED(result))
    {
        auto object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
        consumer_->SetSwapchainInfo(
            object_info, window, hwnd_id, 0, desc->BufferCount, device, (full_screen_desc == nullptr), true);
    }

    return result;
}

// Create offscreen swapchain for IDXGIFactory::CreateSwapChain
HRESULT Dx12ReplayConsumerArmFeatures::CreateSwapChainForOffscreen(DxObjectInfo*         replay_object_info,
                                                                   HRESULT               original_result,
                                                                   DxObjectInfo*         device_info,
                                                                   DXGI_SWAP_CHAIN_DESC* desc,
                                                                   HandlePointerDecoder<IDXGISwapChain*>* swapchain)
{
    GFXRECON_ASSERT((replay_object_info != nullptr) && (replay_object_info->object != nullptr) &&
                    (swapchain != nullptr));

    HRESULT result = E_FAIL;

    auto      replay_object = static_cast<IDXGIFactory2*>(replay_object_info->object);
    IUnknown* device        = nullptr;

    if (device_info != nullptr)
    {
        device = device_info->object;
    }

    graphics::dx12::ID3D12CommandQueueComPtr d3d12_command_queue;
    HRESULT                                  hr = device->QueryInterface(IID_PPV_ARGS(&d3d12_command_queue));
    if (FAILED(hr))
    {
        GFXRECON_LOG_ERROR("Failed to cast IUnknown to ID3D12CommandQueue while creating offscreen swapchain.");
        return result;
    }

    graphics::dx12::ID3D12DeviceComPtr d3d12_device;
    hr = d3d12_command_queue->GetDevice(IID_PPV_ARGS(&d3d12_device));
    if (FAILED(hr))
    {
        GFXRECON_LOG_ERROR("Failed to retrieve device from command queue while creating offscreen swapchain.");
        return result;
    }

    if (desc->BufferDesc.Width == 0 || desc->BufferDesc.Height == 0)
    {
        auto resolve_dim = [](UINT cur, uint32_t opt, int metric, UINT fallback, const char*& src) -> UINT {
            if (cur > 0)
            {
                src = "original";
                return cur;
            }
            if (opt > 0)
            {
                src = "consumer";
                return opt;
            }
            int v = GetSystemMetrics(metric);
            if (v > 0)
            {
                src = "system";
                return static_cast<UINT>(v);
            }
            src = "default";
            return fallback;
        };

        const char* width_src  = "";
        const char* height_src = "";
        UINT        new_width =
            resolve_dim(desc->BufferDesc.Width, consumer_->options_.windowed_width, SM_CXSCREEN, 320, width_src);
        UINT new_height =
            resolve_dim(desc->BufferDesc.Height, consumer_->options_.windowed_height, SM_CYSCREEN, 240, height_src);

        desc->BufferDesc.Width  = new_width;
        desc->BufferDesc.Height = new_height;
        GFXRECON_LOG_WARNING_ONCE(("Swapchain zero dimension resolved: Width=" + std::to_string(new_width) + " (" +
                                   width_src + "), Height=" + std::to_string(new_height) + " (" + height_src + ").")
                                      .c_str());
    }

    Microsoft::WRL::ComPtr<Dx12OffscreenSwapchain> offscreen_swapchain =
        Dx12OffscreenSwapchain::Create(d3d12_device.GetInterfacePtr(), desc);
    if (offscreen_swapchain == nullptr)
    {
        GFXRECON_LOG_ERROR("Failed to create offscreen swapchain.");
        return result;
    }

    *(swapchain->GetHandlePointer()) = offscreen_swapchain.Detach();

    auto object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
    consumer_->SetSwapchainInfo(object_info, nullptr, 0, 0, desc->BufferCount, device, false, false, true);

    return S_OK;
}

// Create offscreen swapchain for IDXGIFactory2::CreateSwapChainForHwnd, IDXGIFactory2::CreateSwapChainForComposition
// and IDXGIFactory2::CreateSwapChainForCoreWindow
HRESULT Dx12ReplayConsumerArmFeatures::CreateSwapChainForOffscreen(DxObjectInfo*                    replay_object_info,
                                                                   HRESULT                          original_result,
                                                                   DxObjectInfo*                    device_info,
                                                                   uint64_t                         hwnd_id,
                                                                   DXGI_SWAP_CHAIN_DESC1*           desc,
                                                                   DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full_screen_desc,
                                                                   HandlePointerDecoder<IDXGISwapChain1*>* swapchain)
{
    GFXRECON_ASSERT((replay_object_info != nullptr) && (replay_object_info->object != nullptr) &&
                    (swapchain != nullptr));

    HRESULT result = E_FAIL;

    auto      replay_object = static_cast<IDXGIFactory2*>(replay_object_info->object);
    IUnknown* device        = nullptr;

    if (device_info != nullptr)
    {
        device = device_info->object;
    }

    graphics::dx12::ID3D12CommandQueueComPtr d3d12_command_queue;
    HRESULT                                  hr = device->QueryInterface(IID_PPV_ARGS(&d3d12_command_queue));
    if (FAILED(hr))
    {
        GFXRECON_LOG_ERROR("Failed to cast IUnknown to ID3D12CommandQueue while creating offscreen swapchain.");
        return result;
    }

    graphics::dx12::ID3D12DeviceComPtr d3d12_device;
    hr = d3d12_command_queue->GetDevice(IID_PPV_ARGS(&d3d12_device));
    if (FAILED(hr))
    {
        GFXRECON_LOG_ERROR("Failed to retrieve device from command queue while creating offscreen swapchain.");
        return result;
    }

    if (desc->Width == 0 || desc->Height == 0)
    {
        auto resolve_dim = [](UINT cur, uint32_t opt, int metric, UINT fallback, const char*& src) -> UINT {
            if (cur > 0)
            {
                src = "original";
                return cur;
            }
            if (opt > 0)
            {
                src = "consumer";
                return opt;
            }
            int v = GetSystemMetrics(metric);
            if (v > 0)
            {
                src = "system";
                return static_cast<UINT>(v);
            }
            src = "default";
            return fallback;
        };

        const char* width_src  = "";
        const char* height_src = "";
        UINT new_width  = resolve_dim(desc->Width, consumer_->options_.windowed_width, SM_CXSCREEN, 320, width_src);
        UINT new_height = resolve_dim(desc->Height, consumer_->options_.windowed_height, SM_CYSCREEN, 240, height_src);

        desc->Width  = new_width;
        desc->Height = new_height;
        GFXRECON_LOG_WARNING_ONCE(("Swapchain zero dimension resolved: Width=" + std::to_string(new_width) + " (" +
                                   width_src + "), Height=" + std::to_string(new_height) + " (" + height_src + ").")
                                      .c_str());
    }

    Microsoft::WRL::ComPtr<Dx12OffscreenSwapchain> offscreen_swapchain =
        Dx12OffscreenSwapchain::Create(d3d12_device.GetInterfacePtr(), hwnd_id, desc, full_screen_desc);
    if (offscreen_swapchain == nullptr)
    {
        GFXRECON_LOG_ERROR("Failed to create offscreen swapchain.");
        return result;
    }

    *(swapchain->GetHandlePointer()) = offscreen_swapchain.Detach();

    auto object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
    consumer_->SetSwapchainInfo(object_info, nullptr, 0, 0, desc->BufferCount, device, false, false, true);

    return S_OK;
}

void Dx12ReplayConsumerArmFeatures::ApplyFillMemoryResourceAddressCommand(uint64_t       offset,
                                                                          uint64_t       size,
                                                                          const uint8_t* data)
{
    if (consumer_->fill_memory_resource_address_info_.expected_block_index != 0)
    {
        if (consumer_->fill_memory_resource_address_info_.expected_block_index == consumer_->GetCurrentBlockIndex())
        {
            GFXRECON_ASSERT(consumer_->fill_memory_resource_address_info_.resource_addresses.size() > 0)

            for (size_t i = 0; i < consumer_->fill_memory_resource_address_info_.resource_addresses.size(); ++i)
            {
                auto value_type   = consumer_->fill_memory_resource_address_info_.resource_addresses[i].type;
                auto value_offset = consumer_->fill_memory_resource_address_info_.resource_addresses[i].offset;
                if (offset > 0)
                {
                    if (value_offset < offset)
                    {
                        GFXRECON_LOG_ERROR("Invalid resource value offset %" PRIu64
                                           " for FillMemory data range [%" PRIu64 ", %" PRIu64 "). Replay may fail.",
                                           value_offset,
                                           offset,
                                           offset + size);
                        continue;
                    }

                    // Adjust the offset if the fill memory command is not at the start of the resource.
                    value_offset -= offset;
                }

                if ((value_offset > size) || (value_offset + sizeof(UINT64) > size))
                {
                    GFXRECON_LOG_ERROR("Resource value offset %" PRIu64
                                       " with size exceeds FillMemory data size %" PRIu64 ". Replay may fail.",
                                       value_offset,
                                       size);
                    continue;
                }

                auto object_id   = consumer_->fill_memory_resource_address_info_.resource_addresses[i].object_id;
                auto start_value = consumer_->fill_memory_resource_address_info_.resource_addresses[i].start_value;
                auto adjusted_value =
                    consumer_->fill_memory_resource_address_info_.resource_addresses[i].adjusted_value;

                uint8_t* old_value_ptr = const_cast<uint8_t*>(data) + value_offset;

                switch (value_type)
                {
                    case format::ResourceValueType::kGpuVirtualAddress:
                    {
                        UINT64 current_value = 0;
                        util::platform::MemoryCopy(
                            &current_value, sizeof(current_value), old_value_ptr, sizeof(current_value));
                        if (current_value != adjusted_value)
                        {
                            GFXRECON_LOG_ERROR("Unexpected GPU VA value found in memory for object_id %" PRIu64
                                               ". Expected: 0x%016" PRIx64 ", Found: 0x%016" PRIx64
                                               ". Replay may fail.",
                                               object_id,
                                               adjusted_value,
                                               current_value);
                            break;
                        }

                        auto replay_base_address =
                            consumer_->gpu_va_map_.GetReplayAccelerationStructureAddress(adjusted_value);
                        if (replay_base_address != 0)
                        {
                            util::platform::MemoryCopy(old_value_ptr,
                                                       sizeof(replay_base_address),
                                                       &replay_base_address,
                                                       sizeof(replay_base_address));
                            break;
                        }

                        replay_base_address =
                            consumer_->gpu_va_map_.GetReplayGpuVirtualBaseAddress(object_id, start_value);
                        if (replay_base_address == 0)
                        {
                            GFXRECON_LOG_ERROR("Failed to find GPU VA base address for object_id %" PRIu64
                                               ". Replay may fail.",
                                               object_id);
                            break;
                        }

                        UINT64 replay_address = replay_base_address + (adjusted_value - start_value);
                        util::platform::MemoryCopy(
                            old_value_ptr, sizeof(replay_address), &replay_address, sizeof(replay_address));
                        break;
                    }
                    case format::ResourceValueType::kGpuDescriptorHandle:
                    {
                        UINT64 current_value = 0;
                        util::platform::MemoryCopy(
                            &current_value, sizeof(current_value), old_value_ptr, sizeof(current_value));
                        if (current_value != adjusted_value)
                        {
                            GFXRECON_LOG_ERROR("Unexpected GPU Descriptor Handle value found in memory for object_id "
                                               "%" PRIu64 ". Expected: 0x%016" PRIx64 ", Found: 0x%016" PRIx64
                                               ". Replay may fail.",
                                               object_id,
                                               adjusted_value,
                                               current_value);
                            break;
                        }

                        auto capture_offset = adjusted_value - start_value;
                        auto replay_offset_address =
                            consumer_->descriptor_map_.GetReplayGpuDescriptorBaseAddress(start_value, capture_offset);
                        if (replay_offset_address == 0)
                        {
                            GFXRECON_LOG_ERROR(
                                "Failed to find GPU Descriptor Handle base address for object_id %" PRIu64 ". "
                                "Replay may fail.",
                                object_id);
                            break;
                        }

                        util::platform::MemoryCopy(old_value_ptr,
                                                   sizeof(replay_offset_address),
                                                   &replay_offset_address,
                                                   sizeof(replay_offset_address));
                        break;
                    }
                    case format::ResourceValueType::kShaderIdentifier:
                    {
                        if (0 !=
                            std::memcmp(old_value_ptr,
                                        consumer_->fill_memory_resource_address_info_.resource_addresses[i].shader_id,
                                        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES))
                        {
                            GFXRECON_LOG_ERROR("Unexpected shader identifier found in memory for object_id %" PRIu64
                                               ". Replay may fail.",
                                               object_id);
                            break;
                        }

                        auto dst_value_ptr = old_value_ptr;
                        auto src_value_ptr = old_value_ptr;
                        if (!consumer_->shader_id_map_.Map(object_id, dst_value_ptr, src_value_ptr))
                        {
                            GFXRECON_LOG_WARNING_ONCE(
                                "Failed to map shader identifier for optimized DXR replay. Replay may fail.");
                        }
                        break;
                    }
                }
            }

            consumer_->fill_memory_resource_address_info_.Clear();
        }
        else
        {
            GFXRECON_LOG_ERROR("Unexpected state found for the data required for optimized replay of DXR and/or "
                               "ExecuteIndirect commands. Replay may fail.");
        }
    }
}

void Dx12ReplayConsumerArmFeatures::SetResourceReplayRequiredSize(
    DxObjectInfo*                                       replay_object_info,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>*  pDesc,
    StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC1>* pDesc1,
    D3D12_RESOURCE_STATES                               resource_state,
    format::HandleId                                    resource_id)
{
    auto accel_struct_builder = consumer_->GetAccelerationStructureBuilder(replay_object_info);
    if (consumer_->support_memory_allocator_ && (pDesc != nullptr))
    {
        auto desc_pointer = pDesc->GetPointer();
        if (desc_pointer->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            // Save original buffer size
            auto result = consumer_->resource_buffer_widths_.emplace(resource_id, desc_pointer->Width);
            if (!result.second)
            {
                result.first->second = desc_pointer->Width;
            }

            if (((resource_state & D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) ==
                 D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) ||
                ((desc_pointer->Flags & D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE) ==
                 D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE))
            {
                UINT64 accel_struct_size = 0;
                if (accel_struct_builder != nullptr)
                {
                    accel_struct_size = accel_struct_builder->GetLastPrebuildInfo().ResultDataMaxSizeInBytes;
                }

                if (accel_struct_size != 0)
                {
                    const_cast<D3D12_RESOURCE_DESC*>(desc_pointer)->Width = accel_struct_size;
                }
            }
        }
    }
    else if (consumer_->support_memory_allocator_ && (pDesc1 != nullptr))
    {
        auto desc_pointer = pDesc1->GetPointer();
        if (desc_pointer->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            // Save original buffer size
            auto result = consumer_->resource_buffer_widths_.emplace(resource_id, desc_pointer->Width);
            if (!result.second)
            {
                result.first->second = desc_pointer->Width;
            }

            if (((resource_state & D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) ==
                 D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE) ||
                ((desc_pointer->Flags & D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE) ==
                 D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE))
            {
                UINT64 accel_struct_size = 0;
                if (accel_struct_builder != nullptr)
                {
                    accel_struct_size = accel_struct_builder->GetLastPrebuildInfo().ResultDataMaxSizeInBytes;
                }

                if (accel_struct_size != 0)
                {
                    const_cast<D3D12_RESOURCE_DESC1*>(desc_pointer)->Width = accel_struct_size;
                }
            }
        }
    }
}

HRESULT Dx12ReplayConsumerArmFeatures::CreateSharedResourcePlaceholder(ID3D12Device* device, void** out_object)
{
    D3D12_HEAP_PROPERTIES heap_properties = {};
    heap_properties.Type                  = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width               = 1;
    desc.Height              = 1;
    desc.DepthOrArraySize    = 1;
    desc.MipLevels           = 1;
    desc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count    = 1;
    desc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags               = D3D12_RESOURCE_FLAG_NONE;

    return device->CreateCommittedResource(&heap_properties,
                                           D3D12_HEAP_FLAG_NONE,
                                           &desc,
                                           D3D12_RESOURCE_STATE_COMMON,
                                           nullptr,
                                           __uuidof(ID3D12Resource),
                                           out_object);
}

GFXRECON_END_NAMESPACE(gfxrecon)
GFXRECON_END_NAMESPACE(decode)
