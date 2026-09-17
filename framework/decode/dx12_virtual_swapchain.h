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

#ifndef GFXRECON_DECODE_DX12_VIRTUAL_SWAPCHAIN_H
#define GFXRECON_DECODE_DX12_VIRTUAL_SWAPCHAIN_H

#include "util/defines.h"

#include <dxgi1_4.h>
#include <wrl/client.h>

#include <optional>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class Dx12VirtualSwapchain
{
  public:
    explicit Dx12VirtualSwapchain(UINT buffer_count);

    HRESULT Initialize(IDXGISwapChain* replay_swapchain);

    HRESULT Reset(UINT buffer_count);

    HRESULT SetCaptureInitialIndex(UINT capture_index);

    std::optional<UINT> MapCaptureToReplay(UINT capture_index) const;

    std::optional<UINT> MapReplayToCapture(UINT replay_index) const;

    HRESULT GetBuffer(UINT capture_index, REFIID riid, void** surface);

    UINT GetReplayCurrentIndex() const { return replay_current_index_; }

    UINT GetBufferCount() const { return buffer_count_; }

    void MarkBuffersExposed() { buffers_exposed_ = true; }

  private:
    bool SetCurrentIndices(UINT capture_index, UINT replay_index);

    std::optional<UINT> QueryReplayCurrentIndex() const;

    UINT GetIndexOffset(UINT capture_index, UINT replay_index) const;

    UINT buffer_count_{ 0 };
    UINT replay_current_index_{ 0 };
    UINT index_offset_{ 0 };
    bool buffers_exposed_{ false };

    Microsoft::WRL::ComPtr<IDXGISwapChain>  replay_swapchain_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> replay_swapchain3_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_DX12_VIRTUAL_SWAPCHAIN_H
