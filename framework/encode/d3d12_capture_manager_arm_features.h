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

#ifndef GFXRECON_ENCODE_D3D12_CAPTURE_MANAGER_ARM_FEATURES_H
#define GFXRECON_ENCODE_D3D12_CAPTURE_MANAGER_ARM_FEATURES_H

#include "format/format.h"
#include "graphics/dx12_util.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(graphics)

class Dx12ResourceDataUtil;

GFXRECON_END_NAMESPACE(graphics)
GFXRECON_BEGIN_NAMESPACE(encode)

class D3D12CaptureManager;
class ID3D12CommandList_Wrapper;
class ID3D12CommandQueue_Wrapper;
class ID3D12Device_Wrapper;
class ID3D12GraphicsCommandList_Wrapper;
class ID3D12Resource_Wrapper;

class D3D12CaptureManagerArmFeatures
{
  public:
    enum class SharedResourceSnapshotResult
    {
        kWritten,
        kUnavailable,
    };

    explicit D3D12CaptureManagerArmFeatures(D3D12CaptureManager* manager);
    ~D3D12CaptureManagerArmFeatures();

    void PostProcessCreateSharedHandle(ID3D12Device_Wrapper*      wrapper,
                                       HRESULT                    result,
                                       ID3D12DeviceChild*         object,
                                       const SECURITY_ATTRIBUTES* attributes,
                                       DWORD                      access,
                                       LPCWSTR                    name,
                                       HANDLE*                    handle);

    void PreProcessCopyTextureRegion(ID3D12GraphicsCommandList_Wrapper* wrapper,
                                     const D3D12_TEXTURE_COPY_LOCATION* dst,
                                     UINT                               dst_x,
                                     UINT                               dst_y,
                                     UINT                               dst_z,
                                     const D3D12_TEXTURE_COPY_LOCATION* src,
                                     const D3D12_BOX*                   src_box);

    void PostProcessCreateShaderResourceView(ID3D12Resource* resource);

    void PostProcessOpenSharedHandle(
        ID3D12Device_Wrapper* wrapper, HRESULT result, HANDLE nt_handle, REFIID riid, void** object);

    void RefreshShaderResourceSnapshots(ID3D12CommandQueue* consumer_queue);

    void WritePendingSharedResourceSnapshots(ID3D12CommandQueue_Wrapper* queue_wrapper,
                                             UINT                        num_lists,
                                             ID3D12CommandList* const*   lists);

    void ClearPendingSharedResourceSnapshots(format::HandleId command_list_id);

    void TrackWriteModeResourceBarriers(ID3D12CommandList_Wrapper*    list_wrapper,
                                        UINT                          num_barriers,
                                        const D3D12_RESOURCE_BARRIER* barriers,
                                        bool                          record_barriers);

    void TrackWriteModeExecuteCommandLists(UINT num_lists, ID3D12CommandList* const* lists);

    void DestroyDevice(ID3D12Device_Wrapper* wrapper);
    void DestroyResource(ID3D12Resource_Wrapper* wrapper);

  private:
    void                            TrackSharedResource(ID3D12DeviceChild* object, bool opened_shared_resource);
    void                            WarnFullCaptureRequired();
    graphics::Dx12ResourceDataUtil* GetResourceDataUtil(ID3D12Resource_Wrapper* resource_wrapper);
    void WriteSharedResourceSnapshots(const std::unordered_set<format::HandleId>& resource_ids,
                                      ID3D12CommandQueue*                         consumer_queue);

    SharedResourceSnapshotResult
    WriteSharedResourceContent(ID3D12Resource_Wrapper*                               resource_wrapper,
                               const std::vector<graphics::dx12::ResourceStateInfo>& resource_states,
                               ID3D12CommandQueue*                                   consumer_queue);

    D3D12CaptureManager* manager_;

    std::unordered_map<format::HandleId, ID3D12Resource_Wrapper*>              shared_resources_;
    std::unordered_map<format::HandleId, std::unordered_set<format::HandleId>> pending_snapshots_;
    std::unordered_set<format::HandleId>                                       opened_shared_resources_;
    std::unordered_set<format::HandleId>                                       pending_shader_resource_snapshots_;
    std::unordered_set<format::HandleId>                                       active_shader_resource_snapshots_;
    // output_mutex_ serializes use of each utility's command objects and staging buffers.
    std::unordered_map<format::HandleId, std::unique_ptr<graphics::Dx12ResourceDataUtil>> resource_data_utils_;
    std::mutex                                                                            shared_resources_mutex_;
    std::mutex                                                                            shared_resource_state_mutex_;
    std::mutex                                                                            output_mutex_;
    std::once_flag                                                                        full_capture_warning_once_;
};

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_ENCODE_D3D12_CAPTURE_MANAGER_ARM_FEATURES_H
