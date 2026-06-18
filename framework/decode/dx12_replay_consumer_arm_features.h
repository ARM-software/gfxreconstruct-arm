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

#ifndef GFXRECON_DECODE_DX12_REPLAY_CONSUMER_ARM_FEATURES_H
#define GFXRECON_DECODE_DX12_REPLAY_CONSUMER_ARM_FEATURES_H

#include "graphics/dx12_util.h"
#include "generated/generated_dx12_consumer.h"
#include "decode/custom_dx12_struct_decoders_forward.h"
#include "decode/dx12_object_info.h"

#include <unordered_set>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class Dx12ReplayConsumerBase;

class Dx12ReplayConsumerArmFeatures
{
  public:
    Dx12ReplayConsumerArmFeatures(Dx12ReplayConsumerBase* consumer);

    void CheckReplayResult(const char* call_name, HRESULT capture_result, HRESULT replay_result);

    void LogFrameDebugInfo();

    HRESULT
    CreateSwapChainForComposition(DxObjectInfo*                          replay_object_info,
                                  HRESULT                                original_result,
                                  DxObjectInfo*                          device_info,
                                  DXGI_SWAP_CHAIN_DESC*                  desc,
                                  HandlePointerDecoder<IDXGISwapChain*>* swapchain);

    HRESULT
    CreateSwapChainForComposition(DxObjectInfo*                           replay_object_info,
                                  HRESULT                                 original_result,
                                  DxObjectInfo*                           device_info,
                                  uint64_t                                hwnd_id,
                                  DXGI_SWAP_CHAIN_DESC1*                  desc,
                                  DXGI_SWAP_CHAIN_FULLSCREEN_DESC*        full_screen_desc,
                                  DxObjectInfo*                           restrict_to_output_info,
                                  HandlePointerDecoder<IDXGISwapChain1*>* swapchain);

    // Create offscreen swapchain for IDXGIFactory::CreateSwapChain
    HRESULT
    CreateSwapChainForOffscreen(DxObjectInfo*                          replay_object_info,
                                HRESULT                                original_result,
                                DxObjectInfo*                          device_info,
                                DXGI_SWAP_CHAIN_DESC*                  desc,
                                HandlePointerDecoder<IDXGISwapChain*>* swapchain);

    // Create offscreen swapchain for IDXGIFactory2::CreateSwapChainForHwnd,
    // IDXGIFactory2::CreateSwapChainForComposition and IDXGIFactory2::CreateSwapChainForCoreWindow
    HRESULT
    CreateSwapChainForOffscreen(DxObjectInfo*                           replay_object_info,
                                HRESULT                                 original_result,
                                DxObjectInfo*                           device_info,
                                uint64_t                                hwnd_id,
                                DXGI_SWAP_CHAIN_DESC1*                  desc,
                                DXGI_SWAP_CHAIN_FULLSCREEN_DESC*        full_screen_desc,
                                HandlePointerDecoder<IDXGISwapChain1*>* swapchain);

    void ApplyFillMemoryResourceAddressCommand(uint64_t offset, uint64_t size, const uint8_t* data);

    void SetResourceReplayRequiredSize(DxObjectInfo*                                       replay_object_info,
                                       StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC>*  pDesc,
                                       StructPointerDecoder<Decoded_D3D12_RESOURCE_DESC1>* pDesc1,
                                       D3D12_RESOURCE_STATES                               resource_state,
                                       format::HandleId                                    resource_id);

    HRESULT CreateSharedResourcePlaceholder(ID3D12Device* device, void** out_object);
    void    TrackPlaceholderSharedResource(format::HandleId resource_id);
    void    ForgetPlaceholder(format::HandleId object_id);
    void    TrackOutgoingSharedFence(format::HandleId fence_id);
    bool    IsOutgoingSharedFence(format::HandleId fence_id) const;
    void    OnSharedResourceGetDesc(DxObjectInfo* resource_object_info, const D3D12_RESOURCE_DESC* captured_desc);

  private:
    bool RecreateSharedResourcePlaceholder(DxObjectInfo*              resource_object_info,
                                           const D3D12_RESOURCE_DESC& captured_desc);

    Dx12ReplayConsumerBase*              consumer_;
    std::unordered_set<format::HandleId> placeholder_shared_resources_;
    std::unordered_set<format::HandleId> outgoing_shared_fences_;
};
GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_DX12_REPLAY_CONSUMER_ARM_FEATURES_H
