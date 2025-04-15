/*
** Copyright (c) 2022 LunarG, Inc.
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

#ifndef GFXRECON_DECODE_DX12_ACCELERATION_STRUCTURE_BUILDER_H
#define GFXRECON_DECODE_DX12_ACCELERATION_STRUCTURE_BUILDER_H

#include "graphics/dx12_gpu_va_map.h"
#include "graphics/dx12_util.h"
#include "format/format.h"
#include "util/defines.h"

#include <d3d12.h>
#include <vector>
#include <unordered_map>
#include <algorithm>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class Dx12AccelerationStructureBuilder
{
  public:
    Dx12AccelerationStructureBuilder(graphics::dx12::ID3D12Device5ComPtr device5);

    virtual ~Dx12AccelerationStructureBuilder() {}

    void Build(const graphics::Dx12GpuVaMap&                                         gpu_va_map,
               const format::InitDx12AccelerationStructureCommandHeader&             command_header,
               const std::vector<format::InitDx12AccelerationStructureGeometryDesc>& init_geometry_descs,
               const uint8_t*                                                        build_inputs_data);

    void PrebuildInfo(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* pInfo) { prebuild_info_ = *pInfo; }

    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO GetLastPrebuildInfo();

    void PreBuildRaytracingAccelerationStructure(const format::HandleId                                    command_list,
                                                 const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc);

    void ReleaseScratchBuffer(const format::HandleId command_list);

    void PostExecuteCommandLists(const format::HandleId  queue,
                                 const UINT              num_command_lists,
                                 const format::HandleId* command_lists);

    void PostCommandQueueSignal(const format::HandleId queue, const format::HandleId fence, const UINT64 value);

    void PostGetCompletedValue(const format::HandleId fence, const UINT64 value);

    void PostCommandQueueWait(const format::HandleId queue, const format::HandleId fence, const UINT64 value);

    ID3D12Device5* GetDevice5() { return device5_; }

  private:
    void SetupBuild(const graphics::Dx12GpuVaMap&                                         gpu_va_map,
                    const format::InitDx12AccelerationStructureCommandHeader&             command_header,
                    const std::vector<format::InitDx12AccelerationStructureGeometryDesc>& init_geometry_descs,
                    const uint8_t*                                                        build_inputs_data,
                    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC&                   build_desc,
                    bool                                                                  use_temp_dest_buffer);

    void ExecuteBuild(const graphics::Dx12GpuVaMap&                       gpu_va_map,
                      D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& build_desc);

    void ExecuteCopy(D3D12_GPU_VIRTUAL_ADDRESS                         dest_gpu_va,
                     D3D12_GPU_VIRTUAL_ADDRESS                         source_gpu_va,
                     D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE mode);

  private:
    graphics::dx12::ID3D12Device5ComPtr              device5_;
    graphics::dx12::ID3D12CommandQueueComPtr         command_queue_;
    graphics::dx12::ID3D12CommandAllocatorComPtr     command_allocator_;
    graphics::dx12::ID3D12GraphicsCommandList4ComPtr command_list4_;
    graphics::dx12::ID3D12ResourceComPtr             inputs_buffer_;
    graphics::dx12::ID3D12FenceComPtr                fence_;
    uint64_t                                         inputs_buffer_size_;
    graphics::dx12::ID3D12ResourceComPtr             scratch_buffer_;
    uint64_t                                         scratch_buffer_size_;
    graphics::dx12::ID3D12ResourceComPtr             temp_dest_buffer_;
    uint64_t                                         temp_dest_buffer_size_;
    uint64_t                                         fence_value_;

    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> temp_geometry_descs_;
    std::vector<uint8_t>                        temp_instance_desc_input_data_;

    struct ScratchBufferData
    {
        graphics::dx12::ID3D12ResourceComPtr scratch_buffer;
        UINT64                               build_size;
        D3D12_GPU_VIRTUAL_ADDRESS            capture_scratch_address;
        D3D12_GPU_VIRTUAL_ADDRESS            replay_scratch_address;

        ScratchBufferData(graphics::dx12::ID3D12ResourceComPtr scratch_buffer,
                          UINT64                               build_size,
                          D3D12_GPU_VIRTUAL_ADDRESS            capture_scratch_address,
                          D3D12_GPU_VIRTUAL_ADDRESS            replay_scratch_address) :
            scratch_buffer(std::move(scratch_buffer)),
            build_size(build_size), capture_scratch_address(capture_scratch_address),
            replay_scratch_address(replay_scratch_address)
        {}

        ScratchBufferData(const ScratchBufferData&)            = delete;
        ScratchBufferData& operator=(const ScratchBufferData&) = delete;

        ScratchBufferData(ScratchBufferData&& other) noexcept :
            scratch_buffer(std::move(other.scratch_buffer)), build_size(other.build_size),
            capture_scratch_address(other.capture_scratch_address), replay_scratch_address(other.replay_scratch_address)
        {}

        ScratchBufferData& operator=(ScratchBufferData&& other) noexcept
        {
            if (this != &other)
            {
                scratch_buffer          = std::move(other.scratch_buffer);
                build_size              = other.build_size;
                capture_scratch_address = other.capture_scratch_address;
                replay_scratch_address  = other.replay_scratch_address;
            }
            return *this;
        }
    };

    struct CommandQueueData
    {
        std::vector<format::HandleId>                command_lists;
        std::unordered_map<format::HandleId, UINT64> wait_fences_value;

        CommandQueueData& operator=(CommandQueueData&& other) noexcept
        {
            if (this != &other)
            {
                command_lists     = std::move(other.command_lists);
                wait_fences_value = std::move(other.wait_fences_value);
            }
            return *this;
        }
    };

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO                prebuild_info_{};
    std::unordered_map<format::HandleId, std::vector<ScratchBufferData>> command_lis_recorded_scratches_;
    std::unordered_map<format::HandleId, CommandQueueData>               command_queue_data_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_DX12_ACCELERATION_STRUCTURE_BUILDER_H
