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

#include "dx12_redundancy_modifier.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

bool Dx12RedundancyModifier::IsRedundantFenceCall(const ApiCallInfo& current_call_info,
                                                  format::HandleId   object_id,
                                                  UINT64             return_value)
{
    if (fence_call_info_map_.find(object_id) == fence_call_info_map_.end())
    {
        fence_call_info_map_.insert({ object_id, FenceCallInfo{ current_call_info, object_id, return_value } });
        return false;
    }
    else
    {
        auto& fence_call_info = fence_call_info_map_[object_id];
        if (fence_call_info.call_info.thread_id == current_call_info.thread_id &&
            fence_call_info.completed_value == return_value)
        {
            // If the call is from the same thread and completed value, it is redundant.
            return true;
        }
        else
        {
            // Update the completed value for the object ID.
            fence_call_info.call_info       = current_call_info;
            fence_call_info.completed_value = return_value;
        }
    }

    return false;
}

void Dx12RedundancyModifier::ReleaseFenceCall(const ApiCallInfo& current_call_info,
                                              format::HandleId   object_id,
                                              UINT64             return_value)
{
    if (fence_call_info_map_.find(object_id) != fence_call_info_map_.end())
    {
        fence_call_info_map_.erase(object_id);
    }
}

void Dx12RedundancyModifier::Process_ID3D12Fence_GetCompletedValue(const ApiCallInfo& call_info,
                                                                   format::HandleId   object_id,
                                                                   UINT64             return_value)
{
    if (IsModificationPass())
    {
        if (redundant_fence_calls_.find(call_info.index) != redundant_fence_calls_.end())
        {
            SetDeleteCurrentCall();
            redundant_fence_calls_.erase(call_info.index);
        }
        return;
    }

    if (IsRedundantFenceCall(call_info, object_id, return_value))
    {
        redundant_fence_calls_.insert(call_info.index);
    }
}

void Dx12RedundancyModifier::Process_ID3D12Device_GetDeviceRemovedReason(const ApiCallInfo& call_info,
                                                                         format::HandleId   object_id,
                                                                         HRESULT            return_value)
{
    if (IsModificationPass())
    {
        if (redundant_device_calls_.find(call_info.index) != redundant_device_calls_.end())
        {
            SetDeleteCurrentCall();
            redundant_device_calls_.erase(call_info.index);
        }
        return;
    }

    if (return_value == S_OK)
    {
        redundant_device_calls_.insert(call_info.index);
    }
}

void Dx12RedundancyModifier::Process_IUnknown_Release(const ApiCallInfo& call_info,
                                                      format::HandleId   object_id,
                                                      ULONG              return_value)
{
    if (IsModificationPass())
    {
        return;
    }

    ReleaseFenceCall(call_info, object_id, return_value);
}

bool Dx12RedundancyModifier::CanOptimize()
{
    GFXRECON_WRITE_CONSOLE("Removing %zu redundant fence calls and %zu redundant device calls.",
                           redundant_fence_calls_.size(),
                           redundant_device_calls_.size());

    return true;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
