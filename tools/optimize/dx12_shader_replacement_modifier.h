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

#ifndef GFXRECON_TOOLS_OPTIMIZE_DX12_SHADER_REPLACEMENT_MODIFIER_H
#define GFXRECON_TOOLS_OPTIMIZE_DX12_SHADER_REPLACEMENT_MODIFIER_H

#include "graphics/dx12_shader_tool.h"
#include "util/dx12_modifier_base.h"

#include <string>
#include <unordered_map>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class Dx12ShaderReplacementModifier : public util::Dx12ModifierBase
{
  public:
    explicit Dx12ShaderReplacementModifier(const std::string& shader_dir);

    bool CanOptimize() override { return !shaders_.empty(); }

    void Process_ID3D12Device_CreateGraphicsPipelineState(
        const ApiCallInfo&                                                call_info,
        format::HandleId                                                  object_id,
        HRESULT                                                           return_value,
        StructPointerDecoder<Decoded_D3D12_GRAPHICS_PIPELINE_STATE_DESC>* pDesc,
        Decoded_GUID                                                      riid,
        HandlePointerDecoder<void*>*                                      ppPipelineState) override;

    void Process_ID3D12Device_CreateComputePipelineState(
        const ApiCallInfo&                                               call_info,
        format::HandleId                                                 object_id,
        HRESULT                                                          return_value,
        StructPointerDecoder<Decoded_D3D12_COMPUTE_PIPELINE_STATE_DESC>* pDesc,
        Decoded_GUID                                                     riid,
        HandlePointerDecoder<void*>*                                     ppPipelineState) override;

    void
    Process_ID3D12Device2_CreatePipelineState(const ApiCallInfo& call_info,
                                              format::HandleId   object_id,
                                              HRESULT            return_value,
                                              StructPointerDecoder<Decoded_D3D12_PIPELINE_STATE_STREAM_DESC>* pDesc,
                                              Decoded_GUID                                                    riid,
                                              HandlePointerDecoder<void*>* ppPipelineState) override;

    void Process_ID3D12Device5_CreateStateObject(const ApiCallInfo&                                     call_info,
                                                 format::HandleId                                       object_id,
                                                 HRESULT                                                return_value,
                                                 StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* pDesc,
                                                 Decoded_GUID                                           riid,
                                                 HandlePointerDecoder<void*>* ppStateObject) override;

    void Process_ID3D12Device7_AddToStateObject(const ApiCallInfo&                                     call_info,
                                                format::HandleId                                       object_id,
                                                HRESULT                                                return_value,
                                                StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* pAddition,
                                                format::HandleId             pStateObjectToGrowFrom,
                                                Decoded_GUID                 riid,
                                                HandlePointerDecoder<void*>* ppNewStateObject) override;

  private:
    bool TryLoadShader(format::HandleId pipeline_id, graphics::Dx12ShaderTool::ShaderType shader_type);
    bool TryLoadOrReplaceShader(format::HandleId                     pipeline_id,
                                graphics::Dx12ShaderTool::ShaderType shader_type,
                                D3D12_SHADER_BYTECODE*               shader_bytecode);
    bool TryLoadOrReplaceStreamShaders(format::HandleId                          pipeline_id,
                                       Decoded_D3D12_PIPELINE_STATE_STREAM_DESC* stream_desc);
    bool TryLoadStateObjectShader(const char*              api_name,
                                  format::HandleId         state_object_id,
                                  uint32_t                 subobject_index,
                                  D3D12_DXIL_LIBRARY_DESC* dxil_library);
    bool TryLoadOrReplaceStateObjectShader(const char*              api_name,
                                           format::HandleId         state_object_id,
                                           uint32_t                 subobject_index,
                                           D3D12_DXIL_LIBRARY_DESC* dxil_library);
    bool TryLoadOrReplaceStateObjectShaders(format::HandleId                                       state_object_id,
                                            StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* state_object_desc,
                                            const char*                                            api_name,
                                            uint32_t&                                              replacement_count);

    std::string                                        shader_dir_;
    std::unordered_map<std::string, std::vector<char>> shaders_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_DX12_SHADER_REPLACEMENT_MODIFIER_H
