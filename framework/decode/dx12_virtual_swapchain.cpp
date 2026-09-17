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

#include "decode/dx12_virtual_swapchain.h"

#include "util/logging.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

Dx12VirtualSwapchain::Dx12VirtualSwapchain(UINT buffer_count) : buffer_count_(buffer_count) {}

HRESULT Dx12VirtualSwapchain::Initialize(IDXGISwapChain* replay_swapchain)
{
    if (replay_swapchain == nullptr)
    {
        return E_POINTER;
    }

    replay_swapchain_    = replay_swapchain;
    const HRESULT result = replay_swapchain->QueryInterface(IID_PPV_ARGS(&replay_swapchain3_));
    return FAILED(result) ? result : Reset(buffer_count_);
}

HRESULT Dx12VirtualSwapchain::Reset(UINT buffer_count)
{
    buffer_count_           = buffer_count;
    index_offset_           = 0;
    buffers_exposed_        = false;
    const auto replay_index = QueryReplayCurrentIndex();
    if (!replay_index.has_value())
    {
        return DXGI_ERROR_INVALID_CALL;
    }

    return SetCurrentIndices(0, replay_index.value()) ? S_OK : DXGI_ERROR_INVALID_CALL;
}

HRESULT Dx12VirtualSwapchain::SetCaptureInitialIndex(UINT capture_index)
{
    const auto replay_index = QueryReplayCurrentIndex();
    if (!replay_index.has_value())
    {
        return DXGI_ERROR_INVALID_CALL;
    }

    return SetCurrentIndices(capture_index, replay_index.value()) ? S_OK : DXGI_ERROR_INVALID_CALL;
}

std::optional<UINT> Dx12VirtualSwapchain::MapCaptureToReplay(UINT capture_index) const
{
    if ((buffer_count_ == 0) || (capture_index >= buffer_count_))
    {
        return std::nullopt;
    }

    return (capture_index + index_offset_) % buffer_count_;
}

std::optional<UINT> Dx12VirtualSwapchain::MapReplayToCapture(UINT replay_index) const
{
    if ((buffer_count_ == 0) || (replay_index >= buffer_count_))
    {
        return std::nullopt;
    }

    return (replay_index + buffer_count_ - index_offset_) % buffer_count_;
}

HRESULT Dx12VirtualSwapchain::GetBuffer(UINT capture_index, REFIID riid, void** surface)
{
    if (surface == nullptr)
    {
        return E_POINTER;
    }

    const auto replay_index = MapCaptureToReplay(capture_index);
    if (!replay_index.has_value() || (replay_swapchain_ == nullptr))
    {
        return DXGI_ERROR_INVALID_CALL;
    }

    return replay_swapchain_->GetBuffer(replay_index.value(), riid, surface);
}

bool Dx12VirtualSwapchain::SetCurrentIndices(UINT capture_index, UINT replay_index)
{
    if ((buffer_count_ == 0) || (capture_index >= buffer_count_) || (replay_index >= buffer_count_))
    {
        return false;
    }

    const UINT new_offset = GetIndexOffset(capture_index, replay_index);
    if (buffers_exposed_ && (new_offset != index_offset_))
    {
        GFXRECON_LOG_ERROR("DX12 virtual swapchain rejected index offset change after buffers were exposed: "
                           "capture index %u, replay index %u, old offset %u, new offset %u",
                           capture_index,
                           replay_index,
                           index_offset_,
                           new_offset);
        return false;
    }

    replay_current_index_ = replay_index;
    index_offset_         = new_offset;
    return true;
}

std::optional<UINT> Dx12VirtualSwapchain::QueryReplayCurrentIndex() const
{
    if (replay_swapchain3_ == nullptr)
    {
        return std::nullopt;
    }

    const UINT replay_index = replay_swapchain3_->GetCurrentBackBufferIndex();
    return (replay_index < buffer_count_) ? std::optional<UINT>(replay_index) : std::nullopt;
}

UINT Dx12VirtualSwapchain::GetIndexOffset(UINT capture_index, UINT replay_index) const
{
    return (replay_index + buffer_count_ - capture_index) % buffer_count_;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
