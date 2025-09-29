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

#ifndef GFXRECON_DX12_REDUNDANCY_MODIFIER_H
#define GFXRECON_DX12_REDUNDANCY_MODIFIER_H

#include "util/dx12_modifier_base.h"
#include <unordered_set>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

struct FenceCallInfo
{
    ApiCallInfo      call_info;
    format::HandleId object_id;
    UINT64           completed_value;
};

class Dx12RedundancyModifier : public util::Dx12ModifierBase
{
  public:
    Dx12RedundancyModifier() = default;
    virtual ~Dx12RedundancyModifier(){};

    virtual bool CanOptimize() override;

    virtual void Process_ID3D12Fence_GetCompletedValue(const ApiCallInfo& call_info,
                                                       format::HandleId   object_id,
                                                       UINT64             return_value) override;

    virtual void Process_ID3D12Device_GetDeviceRemovedReason(const ApiCallInfo& call_info,
                                                             format::HandleId   object_id,
                                                             HRESULT            return_value) override;

    virtual void
    Process_IUnknown_Release(const ApiCallInfo& call_info, format::HandleId object_id, ULONG return_value) override;

  private:
    bool IsRedundantFenceCall(const ApiCallInfo& current_call_info, format::HandleId object_id, UINT64 return_value);
    void ReleaseFenceCall(const ApiCallInfo& current_call_info, format::HandleId object_id, UINT64 return_value);

    std::unordered_set<uint64_t>
        redundant_fence_calls_; // Set of block indices for redundant ID3D12Fence::GetCompletedValue calls.

    std::unordered_set<uint64_t>
        redundant_device_calls_; // Set of block indices for redundant ID3D12Device::GetDeviceRemovedReason calls.

    std::unordered_map<format::HandleId, FenceCallInfo>
        fence_call_info_map_; // Map of fence call info indexed by object ID.
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DX12_REDUNDANCY_MODIFIER_H
