/*
** Copyright (c) 2022-2025 LunarG, Inc.
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

    void SetPrebuildInfo(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* capture_prebuild_info,
                         D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* replay_prebuild_info,
                         graphics::Dx12GpuVaMap&                                gpu_va_map);

    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO GetLastPrebuildInfo();

    void PreBuildRaytracingAccelerationStructure(const format::HandleId      command_list_id,
                                                 ID3D12GraphicsCommandList4* command_list_ptr,
                                                 const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* build_desc);

    void PreCmdResourceBarrier(ID3D12GraphicsCommandList* command_list_ptr, const format::HandleId resource_id);

    void ReleaseAccelerationStructureBuffer(const format::HandleId resource_id, graphics::Dx12GpuVaMap& gpu_va_map);

    void ReleaseScratchBuffer();

    void PostExecuteCommandLists(ID3D12CommandQueue*     command_queue,
                                 const format::HandleId  queue,
                                 const UINT              num_command_lists,
                                 const format::HandleId* command_lists);

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

    struct AccelerationStructureBufferData
    {
        graphics::dx12::ID3D12ResourceComPtr accel_struct_buffer{ nullptr };
        UINT64                               accel_struct_buffer_size{ 0 };
        D3D12_GPU_VIRTUAL_ADDRESS            capture_accel_struct_address{ 0 };
        D3D12_GPU_VIRTUAL_ADDRESS            replay_accel_struct_address{ 0 };

        AccelerationStructureBufferData() = default;

        AccelerationStructureBufferData(AccelerationStructureBufferData&& other) noexcept = default;

        AccelerationStructureBufferData& operator=(AccelerationStructureBufferData&& other) noexcept
        {
            if (this != &other)
            {
                accel_struct_buffer          = std::move(other.accel_struct_buffer);
                accel_struct_buffer_size     = other.accel_struct_buffer_size;
                capture_accel_struct_address = other.capture_accel_struct_address;
                replay_accel_struct_address  = other.replay_accel_struct_address;
            }
            return *this;
        }
    };

    struct ScratchBufferData
    {
        graphics::dx12::ID3D12ResourceComPtr scratch_buffer{ nullptr };
        UINT64                               build_size{ 0 };
        D3D12_GPU_VIRTUAL_ADDRESS            capture_scratch_address{ 0 };
        D3D12_GPU_VIRTUAL_ADDRESS            replay_scratch_address{ 0 };

        ScratchBufferData() = default;

        ScratchBufferData(ScratchBufferData&& other) noexcept = default;

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

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO                prebuild_info_ = {};
    std::unordered_map<format::HandleId, std::vector<ScratchBufferData>> command_list_recorded_scratches_;
    std::unordered_map<uint64_t, std::vector<ScratchBufferData>>         sync_fence_value_executed_scratches_;
    std::unordered_map<format::HandleId, std::vector<AccelerationStructureBufferData>>
        recreated_new_accel_struct_buffers_;
    // Map of accel_struct_buffer replay GPU VA to capture VA and its size.
    std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, std::pair<D3D12_GPU_VIRTUAL_ADDRESS, uint64_t>>
        recreated_accel_struct_va_size_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_DX12_ACCELERATION_STRUCTURE_BUILDER_H
