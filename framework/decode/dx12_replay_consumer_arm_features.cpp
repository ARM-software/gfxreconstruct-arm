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
#include "generated/generated_dx12_enum_to_string.h"

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
            DXGI_QUERY_VIDEO_MEMORY_INFO local_mem_info     = {};
            DXGI_QUERY_VIDEO_MEMORY_INFO non_local_mem_info = {};
            if (ReportVideoMemoryBudget(local_mem_info, non_local_mem_info))
            {
                const double f = 1024.0 * 1024.0;
                GFXRECON_LOG_INFO("GPU Memory Total Usage(mb)/Budget(mb) : %.02f / %.02f ",
                                  (double)(local_mem_info.CurrentUsage + non_local_mem_info.CurrentUsage) / f,
                                  (double)(local_mem_info.Budget + non_local_mem_info.Budget) / f);
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

bool Dx12ReplayConsumerArmFeatures::ReportVideoMemoryBudget(DXGI_QUERY_VIDEO_MEMORY_INFO& local_mem_info,
                                                            DXGI_QUERY_VIDEO_MEMORY_INFO& non_local_mem_info)
{
    graphics::dx12::IDXGIAdapter3ComPtr adapter3 = nullptr;
    graphics::dx12::IDXGIAdapterComPtr  adapter  = consumer_->GetAdapter();
    if (adapter == nullptr)
    {
        return false;
    }

    HRESULT hr = adapter->QueryInterface(IID_PPV_ARGS(&adapter3));
    if (FAILED(hr))
    {
        return false;
    }

    hr = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local_mem_info);
    if (FAILED(hr))
    {
        return false;
    }

    hr = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &non_local_mem_info);
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void Dx12ReplayConsumerArmFeatures::LogFrameDebugInfo()
{
    if (!util::Log::WillOutputMessage(util::LoggingSeverity::kDebug))
    {
        return;
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO local_mem_info     = {};
    DXGI_QUERY_VIDEO_MEMORY_INFO non_local_mem_info = {};
    if (!ReportVideoMemoryBudget(local_mem_info, non_local_mem_info))
    {
        GFXRECON_LOG_DEBUG("Completed frame %d", consumer_->application_->GetCurrentFrameNumber() + 1);
        return;
    }
    else
    {
        const double f = 1024.0 * 1024.0;
        GFXRECON_LOG_DEBUG("Frame %d local memory (mb): %.02f Budget, %.02f current Usage, %.02f current "
                           "Reservation, %.02f available Reservation",
                           consumer_->application_->GetCurrentFrameNumber() + 1,
                           (double)local_mem_info.Budget / f,
                           (double)local_mem_info.CurrentUsage / f,
                           (double)local_mem_info.CurrentReservation / f,
                           (double)local_mem_info.AvailableForReservation / f);
        if (non_local_mem_info.Budget && non_local_mem_info.CurrentUsage)
        {
            GFXRECON_LOG_DEBUG("Frame %d non local memory (mb): %.02f Budget, %.02f current Usage, %.02f current "
                               "Reservation, %.02f available Reservation",
                               consumer_->application_->GetCurrentFrameNumber() + 1,
                               (double)non_local_mem_info.Budget / f,
                               (double)non_local_mem_info.CurrentUsage / f,
                               (double)non_local_mem_info.CurrentReservation / f,
                               (double)non_local_mem_info.AvailableForReservation / f);
        }
    }
}

bool Dx12ReplayConsumerArmFeatures::SetVirtualSwapchainInitialIndex(DxObjectInfo* swapchain_info,
                                                                    uint32_t      capture_index)
{
    auto swapchain_extra_info = GetExtraInfo<DxgiSwapchainInfo>(swapchain_info);
    if ((swapchain_extra_info == nullptr) || (swapchain_extra_info->virtual_swapchain == nullptr))
    {
        return false;
    }

    auto*         virtual_swapchain = swapchain_extra_info->virtual_swapchain.get();
    const HRESULT result            = virtual_swapchain->SetCaptureInitialIndex(capture_index);
    if (FAILED(result))
    {
        GFXRECON_LOG_ERROR("DX12 virtual swapchain trim current-index mapping failed at block %" PRIu64
                           " for swapchain %" PRIu64 " (capture index %u, HRESULT: 0x%08X)",
                           consumer_->GetCurrentBlockIndex(),
                           swapchain_info->capture_id,
                           capture_index,
                           result);
        return true;
    }

    const UINT replay_index = virtual_swapchain->GetReplayCurrentIndex();
    if (capture_index != replay_index)
    {
        GFXRECON_LOG_INFO("Enabled DX12 virtual swapchain index mapping at block %" PRIu64 " for swapchain %" PRIu64
                          ": capture index %u, replay index %u, buffer count %u",
                          consumer_->GetCurrentBlockIndex(),
                          swapchain_info->capture_id,
                          capture_index,
                          replay_index,
                          virtual_swapchain->GetBufferCount());
    }

    return true;
}

UINT Dx12ReplayConsumerArmFeatures::GetVirtualSwapchainCurrentBackBufferIndex(DxObjectInfo* swapchain_info,
                                                                              UINT          capture_result)
{
    GFXRECON_ASSERT((swapchain_info != nullptr) && (swapchain_info->object != nullptr));

    const UINT replay_result = static_cast<IDXGISwapChain3*>(swapchain_info->object)->GetCurrentBackBufferIndex();
    auto       extra_info    = GetExtraInfo<DxgiSwapchainInfo>(swapchain_info);
    if ((extra_info == nullptr) || (extra_info->virtual_swapchain == nullptr))
    {
        return replay_result;
    }

    auto*      virtual_swapchain = extra_info->virtual_swapchain.get();
    const auto mapped_result     = virtual_swapchain->MapReplayToCapture(replay_result);
    if (!mapped_result.has_value())
    {
        GFXRECON_LOG_ERROR("DX12 virtual swapchain GetCurrentBackBufferIndex failed at block %" PRIu64
                           " for swapchain %" PRIu64 " (HRESULT: 0x%08X)",
                           consumer_->GetCurrentBlockIndex(),
                           swapchain_info->capture_id,
                           DXGI_ERROR_INVALID_CALL);
        return capture_result;
    }

    if (mapped_result.value() != capture_result)
    {
        GFXRECON_LOG_ERROR("DX12 virtual swapchain current-index mismatch at block %" PRIu64 " for swapchain %" PRIu64
                           ": capture returned %u, mapped replay result %u, replay returned %u, buffer count %u",
                           consumer_->GetCurrentBlockIndex(),
                           swapchain_info->capture_id,
                           capture_result,
                           mapped_result.value(),
                           replay_result,
                           virtual_swapchain->GetBufferCount());
    }

    return mapped_result.value();
}

HRESULT Dx12ReplayConsumerArmFeatures::CreateSwapChain(DxObjectInfo* replay_object_info,
                                                       HRESULT       original_result,
                                                       DxObjectInfo* device_info,
                                                       StructPointerDecoder<Decoded_DXGI_SWAP_CHAIN_DESC>* desc,
                                                       HandlePointerDecoder<IDXGISwapChain*>*              swapchain)
{
    auto    desc_pointer   = desc->GetPointer();
    HRESULT result         = E_FAIL;
    Window* window         = nullptr;
    auto    wsi_context    = consumer_->application_ ? consumer_->application_->GetWsiContext("", true) : nullptr;
    auto    window_factory = wsi_context ? wsi_context->GetWindowFactory() : nullptr;

    DXGI_FORMAT format = desc_pointer->BufferDesc.Format;
    if (format != DXGI_FORMAT_R8G8B8A8_UNORM && format != DXGI_FORMAT_R10G10B10A2_UNORM &&
        format != DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
        GFXRECON_LOG_WARNING(
            "SwapChain uses uncommon DXGI_FORMAT: %s. This may affect image capture or display fidelity.",
            util::ToString<DXGI_FORMAT>(format).c_str());
    }

    if ((window_factory != nullptr) && (desc_pointer != nullptr))
    {
        consumer_->ReplaceWindowedResolution(desc_pointer->BufferDesc.Width, desc_pointer->BufferDesc.Height);
        window =
            window_factory->Create(consumer_->options_.window_topleft_x,
                                   consumer_->options_.window_topleft_y,
                                   desc_pointer->BufferDesc.Width,
                                   desc_pointer->BufferDesc.Height,
                                   consumer_->options_.force_windowed || consumer_->options_.force_windowed_origin);
    }

    if (window == nullptr)
    {
        GFXRECON_LOG_FATAL("Failed to create a window.  Replay cannot continue.");
        return result;
    }

    HWND hwnd{};
    if (!window->GetNativeHandle(Window::kWin32HWnd, reinterpret_cast<void**>(&hwnd)))
    {
        GFXRECON_LOG_FATAL("Failed to retrieve handle from window");
        window_factory->Destroy(window);
        return result;
    }

    GFXRECON_ASSERT((replay_object_info != nullptr) && (replay_object_info->object != nullptr) &&
                    (swapchain != nullptr));

    auto      replay_object    = static_cast<IDXGIFactory*>(replay_object_info->object);
    IUnknown* device           = (device_info != nullptr) ? device_info->object : nullptr;
    desc_pointer->OutputWindow = hwnd;

    GFXRECON_UNREFERENCED_PARAMETER(original_result);

    const UINT buffer_count = desc_pointer->BufferCount;
    result                  = replay_object->CreateSwapChain(device, desc_pointer, swapchain->GetHandlePointer());

    if (SUCCEEDED(result))
    {
        auto     object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
        auto     meta_info   = desc->GetMetaStructPointer();
        uint64_t hwnd_id     = (meta_info != nullptr) ? meta_info->OutputWindow : 0;

        const format::HandleId capture_id =
            (swapchain->GetPointer() != nullptr) ? *swapchain->GetPointer() : format::kNullHandleId;
        result = SetSwapchainInfo(object_info,
                                  *swapchain->GetHandlePointer(),
                                  capture_id,
                                  window,
                                  hwnd_id,
                                  hwnd,
                                  buffer_count,
                                  device,
                                  desc_pointer->Windowed,
                                  false,
                                  false);
        if (FAILED(result))
        {
            (*swapchain->GetHandlePointer())->Release();
            *swapchain->GetHandlePointer() = nullptr;
            window_factory->Destroy(window);
        }
    }
    else
    {
        window_factory->Destroy(window);
    }

    return result;
}

HRESULT
Dx12ReplayConsumerArmFeatures::CreateSwapChainForHwnd(DxObjectInfo*                           replay_object_info,
                                                      HRESULT                                 original_result,
                                                      DxObjectInfo*                           device_info,
                                                      uint64_t                                hwnd_id,
                                                      DXGI_SWAP_CHAIN_DESC1*                  desc,
                                                      DXGI_SWAP_CHAIN_FULLSCREEN_DESC*        full_screen_desc,
                                                      DxObjectInfo*                           restrict_to_output_info,
                                                      HandlePointerDecoder<IDXGISwapChain1*>* swapchain)
{
    GFXRECON_ASSERT((device_info != nullptr) && (device_info->object != nullptr));

    HRESULT result         = E_FAIL;
    Window* window         = nullptr;
    auto    wsi_context    = consumer_->application_ ? consumer_->application_->GetWsiContext("", true) : nullptr;
    auto    window_factory = wsi_context ? wsi_context->GetWindowFactory() : nullptr;

    if ((window_factory != nullptr) && (desc != nullptr))
    {
        consumer_->ReplaceWindowedResolution(desc->Width, desc->Height);
        window =
            window_factory->Create(consumer_->options_.window_topleft_x,
                                   consumer_->options_.window_topleft_y,
                                   desc->Width,
                                   desc->Height,
                                   consumer_->options_.force_windowed || consumer_->options_.force_windowed_origin);
    }

    if (window == nullptr)
    {
        GFXRECON_LOG_FATAL("Failed to create a window.  Replay cannot continue.");
        return result;
    }

    HWND hwnd{};
    if (!window->GetNativeHandle(Window::kWin32HWnd, reinterpret_cast<void**>(&hwnd)))
    {
        GFXRECON_LOG_FATAL("Failed to retrieve handle from window");
        window_factory->Destroy(window);
        return result;
    }

    GFXRECON_ASSERT((replay_object_info != nullptr) && (replay_object_info->object != nullptr) &&
                    (swapchain != nullptr));

    auto         replay_object      = static_cast<IDXGIFactory2*>(replay_object_info->object);
    auto         device             = device_info->object;
    IDXGIOutput* restrict_to_output = nullptr;
    if (restrict_to_output_info != nullptr)
    {
        restrict_to_output = static_cast<IDXGIOutput*>(restrict_to_output_info->object);
    }

    if (consumer_->options_.force_windowed || consumer_->options_.force_windowed_origin)
    {
        full_screen_desc = nullptr;
    }

    GFXRECON_UNREFERENCED_PARAMETER(original_result);

    const UINT buffer_count = desc->BufferCount;
    result                  = replay_object->CreateSwapChainForHwnd(
        device, hwnd, desc, full_screen_desc, restrict_to_output, swapchain->GetHandlePointer());

    if (SUCCEEDED(result))
    {
        auto                   object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
        const format::HandleId capture_id =
            (swapchain->GetPointer() != nullptr) ? *swapchain->GetPointer() : format::kNullHandleId;
        result = SetSwapchainInfo(object_info,
                                  *swapchain->GetHandlePointer(),
                                  capture_id,
                                  window,
                                  hwnd_id,
                                  hwnd,
                                  buffer_count,
                                  device,
                                  (full_screen_desc == nullptr),
                                  false,
                                  false);
        if (FAILED(result))
        {
            (*swapchain->GetHandlePointer())->Release();
            *swapchain->GetHandlePointer() = nullptr;
            window_factory->Destroy(window);
        }
    }
    else
    {
        window_factory->Destroy(window);
    }

    return result;
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

    GFXRECON_UNREFERENCED_PARAMETER(original_result);

    const UINT buffer_count = desc->BufferCount;
    result = replay_object->CreateSwapChainForComposition(device, pDesc, restrict_to_output, &pSwapchain1);
    *(swapchain->GetHandlePointer()) = pSwapchain1;

    if (SUCCEEDED(result))
    {
        auto                   object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
        const format::HandleId capture_id =
            (swapchain->GetPointer() != nullptr) ? *swapchain->GetPointer() : format::kNullHandleId;
        result = SetSwapchainInfo(
            object_info, pSwapchain1, capture_id, window, 0, 0, buffer_count, device, desc->Windowed, true, false);
        if (FAILED(result))
        {
            pSwapchain1->Release();
            *(swapchain->GetHandlePointer()) = nullptr;
        }
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

    GFXRECON_UNREFERENCED_PARAMETER(original_result);

    const UINT buffer_count = desc->BufferCount;
    result =
        replay_object->CreateSwapChainForComposition(device, desc, restrict_to_output, swapchain->GetHandlePointer());

    if (SUCCEEDED(result))
    {
        auto                   object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
        const format::HandleId capture_id =
            (swapchain->GetPointer() != nullptr) ? *swapchain->GetPointer() : format::kNullHandleId;
        result = SetSwapchainInfo(object_info,
                                  *swapchain->GetHandlePointer(),
                                  capture_id,
                                  window,
                                  hwnd_id,
                                  0,
                                  buffer_count,
                                  device,
                                  (full_screen_desc == nullptr),
                                  true,
                                  false);
        if (FAILED(result))
        {
            (*swapchain->GetHandlePointer())->Release();
            *swapchain->GetHandlePointer() = nullptr;
        }
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

    auto                   object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
    const format::HandleId capture_id =
        (swapchain->GetPointer() != nullptr) ? *swapchain->GetPointer() : format::kNullHandleId;
    SetSwapchainInfo(object_info,
                     *swapchain->GetHandlePointer(),
                     capture_id,
                     nullptr,
                     0,
                     0,
                     desc->BufferCount,
                     device,
                     false,
                     false,
                     true);

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

    auto                   object_info = static_cast<DxObjectInfo*>(swapchain->GetConsumerData(0));
    const format::HandleId capture_id =
        (swapchain->GetPointer() != nullptr) ? *swapchain->GetPointer() : format::kNullHandleId;
    SetSwapchainInfo(object_info,
                     *swapchain->GetHandlePointer(),
                     capture_id,
                     nullptr,
                     0,
                     0,
                     desc->BufferCount,
                     device,
                     false,
                     false,
                     true);

    return S_OK;
}

HRESULT Dx12ReplayConsumerArmFeatures::SetSwapchainInfo(DxObjectInfo*    info,
                                                        IDXGISwapChain*  replay_swapchain,
                                                        format::HandleId capture_id,
                                                        Window*          window,
                                                        uint64_t         hwnd_id,
                                                        HWND             hwnd,
                                                        uint32_t         image_count,
                                                        IUnknown*        queue_iunknown,
                                                        bool             windowed,
                                                        bool             headless,
                                                        bool             offscreen)
{
    if ((window == nullptr) && !headless && !offscreen)
    {
        return S_OK;
    }

    if (info == nullptr)
    {
        if (window != nullptr)
        {
            consumer_->active_windows_.insert(window);
        }
        return S_OK;
    }

    GFXRECON_ASSERT(info->extra_info == nullptr);

    auto swapchain_info          = std::make_unique<DxgiSwapchainInfo>();
    swapchain_info->window       = window;
    swapchain_info->hwnd_id      = hwnd_id;
    swapchain_info->buffer_count = image_count;
    swapchain_info->image_ids.resize(image_count);
    swapchain_info->is_fullscreen = !windowed;
    swapchain_info->is_headless   = headless;
    swapchain_info->is_offscreen  = offscreen;
    std::fill(swapchain_info->image_ids.begin(), swapchain_info->image_ids.end(), format::kNullHandleId);

    HRESULT result = queue_iunknown->QueryInterface(IID_PPV_ARGS(&swapchain_info->command_queue));
    if (FAILED(result))
    {
        GFXRECON_LOG_WARNING("Failed to get the ID3D12CommandQueue interface from the IUnknown* device "
                             "argument to CreateSwapChain.");
    }

    if ((consumer_->options_.swapchain_option == util::SwapchainOption::kVirtual) && !offscreen)
    {
        auto virtual_swapchain = std::make_unique<Dx12VirtualSwapchain>(image_count);
        result                 = virtual_swapchain->Initialize(replay_swapchain);
        if (FAILED(result))
        {
            GFXRECON_LOG_ERROR("Failed to initialize DX12 virtual swapchain at block %" PRIu64 " for swapchain %" PRIu64
                               ": buffer count %u, HRESULT 0x%08X",
                               consumer_->GetCurrentBlockIndex(),
                               capture_id,
                               image_count,
                               result);
            return result;
        }

        swapchain_info->virtual_swapchain = std::move(virtual_swapchain);
    }

    info->extra_info = std::move(swapchain_info);

    if ((hwnd_id != 0) && !headless && !offscreen)
    {
        GFXRECON_ASSERT(hwnd != nullptr);
        consumer_->window_handles_[hwnd_id] = hwnd;
    }

    if (window != nullptr)
    {
        consumer_->active_windows_.insert(window);
    }

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
