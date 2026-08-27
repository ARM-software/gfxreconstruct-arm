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

#include "dx12_shader_replacement_modifier.h"

#include "encode/custom_dx12_struct_encoders.h"
#include "encode/parameter_encoder.h"
#include "generated/generated_dx12_api_call_encoders.h"
#include "encode/struct_pointer_encoder.h"
#include "util/file_path.h"
#include "util/logging.h"
#include "util/memory_output_stream.h"

#include <cinttypes>
#include <memory>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

namespace
{

void ClearCachedPipelineState(D3D12_CACHED_PIPELINE_STATE* cached_pso)
{
    if (cached_pso != nullptr)
    {
        cached_pso->pCachedBlob           = nullptr;
        cached_pso->CachedBlobSizeInBytes = 0;
    }
}

bool RepointSubobjectToExportsAssociation(D3D12_STATE_OBJECT_DESC*             state_object_desc,
                                          Decoded_D3D12_STATE_OBJECT_DESC*     decoded_state_object_desc,
                                          const Decoded_D3D12_STATE_SUBOBJECT& decoded_subobject,
                                          const D3D12_STATE_SUBOBJECT&         subobject)
{
    auto* association_decoder = decoded_subobject.subobject_to_exports_association;
    if ((association_decoder == nullptr) || (association_decoder->GetPointer() == nullptr) ||
        (association_decoder->GetMetaStructPointer() == nullptr))
    {
        GFXRECON_LOG_WARNING("Skipping state object shader replacement because a subobject association could not be "
                             "decoded.");
        return false;
    }

    auto* association = const_cast<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(
        reinterpret_cast<const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(subobject.pDesc));
    auto* decoded_association  = association_decoder->GetMetaStructPointer();
    auto* associated_subobject = decoded_association->pSubobjectToAssociate;

    if ((association == nullptr) || (associated_subobject == nullptr))
    {
        GFXRECON_LOG_WARNING("Skipping state object shader replacement because a subobject association is incomplete.");
        return false;
    }

    if (associated_subobject->IsNull())
    {
        association->pSubobjectToAssociate = nullptr;
        return true;
    }

    auto* subobjects_decoder = decoded_state_object_desc->pSubobjects;
    if ((state_object_desc->pSubobjects == nullptr) || (subobjects_decoder == nullptr) ||
        !subobjects_decoder->HasAddress() || !associated_subobject->HasAddress())
    {
        GFXRECON_LOG_WARNING("Skipping state object shader replacement because a subobject association has no "
                             "captured address.");
        return false;
    }

    const uint64_t associated_address = associated_subobject->GetAddress();
    const uint64_t subobjects_address = subobjects_decoder->GetAddress();
    for (UINT i = 0; i < state_object_desc->NumSubobjects; ++i)
    {
        if (associated_address == (subobjects_address + (decoded_state_object_desc->subobject_stride * i)))
        {
            association->pSubobjectToAssociate = &state_object_desc->pSubobjects[i];
            return true;
        }
    }

    GFXRECON_LOG_WARNING("Skipping state object shader replacement because a subobject association target was not "
                         "found in the state object descriptor.");
    return false;
}

bool EncodeStateObjectSubobject(encode::ParameterEncoder*            encoder,
                                D3D12_STATE_OBJECT_DESC*             state_object_desc,
                                Decoded_D3D12_STATE_OBJECT_DESC*     decoded_state_object_desc,
                                const D3D12_STATE_SUBOBJECT&         subobject,
                                const Decoded_D3D12_STATE_SUBOBJECT& decoded_subobject)
{
    encoder->EncodeEnumValue(subobject.Type);

    if (subobject.pDesc == nullptr)
    {
        return true;
    }

    switch (subobject.Type)
    {
        case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
        {
            auto* root_signature_decoder = decoded_subobject.global_root_signature;
            if ((root_signature_decoder == nullptr) || (root_signature_decoder->GetMetaStructPointer() == nullptr))
            {
                GFXRECON_LOG_WARNING("Skipping state object shader replacement because a global root signature "
                                     "subobject has no decoded HandleId.");
                return false;
            }

            encoder->EncodeStructPtrPreamble(subobject.pDesc);
            encoder->EncodeHandleIdValue(root_signature_decoder->GetMetaStructPointer()->pGlobalRootSignature);
            return true;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
        {
            auto* root_signature_decoder = decoded_subobject.local_root_signature;
            if ((root_signature_decoder == nullptr) || (root_signature_decoder->GetMetaStructPointer() == nullptr))
            {
                GFXRECON_LOG_WARNING("Skipping state object shader replacement because a local root signature "
                                     "subobject has no decoded HandleId.");
                return false;
            }

            encoder->EncodeStructPtrPreamble(subobject.pDesc);
            encoder->EncodeHandleIdValue(root_signature_decoder->GetMetaStructPointer()->pLocalRootSignature);
            return true;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
        {
            auto* collection_decoder = decoded_subobject.existing_collection_desc;
            if ((collection_decoder == nullptr) || (collection_decoder->GetMetaStructPointer() == nullptr))
            {
                GFXRECON_LOG_WARNING("Skipping state object shader replacement because an existing collection "
                                     "subobject has no decoded HandleId.");
                return false;
            }

            const auto* collection = reinterpret_cast<const D3D12_EXISTING_COLLECTION_DESC*>(subobject.pDesc);
            encoder->EncodeStructPtrPreamble(collection);
            encoder->EncodeHandleIdValue(collection_decoder->GetMetaStructPointer()->pExistingCollection);
            encoder->EncodeUInt32Value(collection->NumExports);
            encode::EncodeStructArray(encoder, collection->pExports, collection->NumExports);
            return true;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
            if (!RepointSubobjectToExportsAssociation(
                    state_object_desc, decoded_state_object_desc, decoded_subobject, subobject))
            {
                return false;
            }
            break;
        default:
            break;
    }

    return encode::EncodeStateSubobjectDescription(encoder, subobject);
}

bool EncodeStateObjectDesc(encode::ParameterEncoder*                              encoder,
                           StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* state_object_decoder)
{
    auto* state_object_desc         = state_object_decoder->GetPointer();
    auto* decoded_state_object_desc = state_object_decoder->GetMetaStructPointer();
    if ((state_object_desc == nullptr) || (decoded_state_object_desc == nullptr))
    {
        GFXRECON_LOG_WARNING("Skipping state object shader replacement because the state object descriptor metadata "
                             "is unavailable.");
        return false;
    }

    auto* subobjects_decoder = decoded_state_object_desc->pSubobjects;
    if ((state_object_desc->NumSubobjects > 0) &&
        ((state_object_desc->pSubobjects == nullptr) || (subobjects_decoder == nullptr) ||
         (subobjects_decoder->GetMetaStructPointer() == nullptr) ||
         (subobjects_decoder->GetLength() != state_object_desc->NumSubobjects)))
    {
        GFXRECON_LOG_WARNING("Skipping state object shader replacement because its subobject metadata is incomplete.");
        return false;
    }

    encoder->EncodeStructPtrPreamble(state_object_desc);
    encoder->EncodeEnumValue(state_object_desc->Type);
    encoder->EncodeUInt32Value(state_object_desc->NumSubobjects);
    encoder->EncodeSizeTValue(sizeof(D3D12_STATE_SUBOBJECT));
    encoder->EncodeStructArrayPreamble(state_object_desc->pSubobjects, state_object_desc->NumSubobjects);

    if (state_object_desc->NumSubobjects > 0)
    {
        auto* decoded_subobjects = subobjects_decoder->GetMetaStructPointer();
        for (UINT i = 0; i < state_object_desc->NumSubobjects; ++i)
        {
            if (!EncodeStateObjectSubobject(encoder,
                                            state_object_desc,
                                            decoded_state_object_desc,
                                            state_object_desc->pSubobjects[i],
                                            decoded_subobjects[i]))
            {
                return false;
            }
        }
    }

    return true;
}

void StoreReplacementCall(util::MemoryOutputStream&            parameter_buffer,
                          util::CallModifierBase::NewCallData* new_call,
                          format::ApiCallId                    call_id,
                          format::HandleId                     object_id,
                          format::ThreadId                     thread_id)
{
    new_call->type      = util::CallModifierBase::NewCallDataType::ApiCall;
    new_call->call_id   = call_id;
    new_call->object_id = object_id;
    new_call->thread_id = thread_id;
    new_call->parameter_buffer.Write(parameter_buffer.GetData(), parameter_buffer.GetDataSize());
}

} // namespace

Dx12ShaderReplacementModifier::Dx12ShaderReplacementModifier(const std::string& shader_dir) :
    shader_dir_(shader_dir), shaders_()
{}

void Dx12ShaderReplacementModifier::Process_ID3D12Device_CreateGraphicsPipelineState(
    const ApiCallInfo&                                                call_info,
    format::HandleId                                                  object_id,
    HRESULT                                                           return_value,
    StructPointerDecoder<Decoded_D3D12_GRAPHICS_PIPELINE_STATE_DESC>* pDesc,
    Decoded_GUID                                                      riid,
    HandlePointerDecoder<void*>*                                      ppPipelineState)
{
    if ((return_value != S_OK) || (pDesc == nullptr) || pDesc->IsNull() || (pDesc->GetPointer() == nullptr) ||
        (ppPipelineState == nullptr) || ppPipelineState->IsNull() || (ppPipelineState->GetPointer() == nullptr))
    {
        return;
    }

    const format::HandleId pipeline_id = *ppPipelineState->GetPointer();
    if (pipeline_id == format::kNullHandleId)
    {
        return;
    }

    auto* pipeline_desc = pDesc->GetPointer();
    auto* decoded_desc  = pDesc->GetMetaStructPointer();
    if (decoded_desc == nullptr)
    {
        GFXRECON_LOG_WARNING("Skipping shader replacement for CreateGraphicsPipelineState with missing decoded "
                             "pipeline state metadata.");
        return;
    }

    bool replace_call = false;
    replace_call |=
        TryLoadOrReplaceShader(pipeline_id, graphics::Dx12ShaderTool::ShaderType::kVertex, &pipeline_desc->VS);
    replace_call |=
        TryLoadOrReplaceShader(pipeline_id, graphics::Dx12ShaderTool::ShaderType::kPixel, &pipeline_desc->PS);
    replace_call |=
        TryLoadOrReplaceShader(pipeline_id, graphics::Dx12ShaderTool::ShaderType::kDomain, &pipeline_desc->DS);
    replace_call |=
        TryLoadOrReplaceShader(pipeline_id, graphics::Dx12ShaderTool::ShaderType::kHull, &pipeline_desc->HS);
    replace_call |=
        TryLoadOrReplaceShader(pipeline_id, graphics::Dx12ShaderTool::ShaderType::kGeometry, &pipeline_desc->GS);

    if (!IsModificationPass() || !replace_call || (riid.decoded_value == nullptr))
    {
        return;
    }

    ClearCachedPipelineState(&pipeline_desc->CachedPSO);

    NewCallData* new_call = CreatePreCall();
    new_call->type        = NewCallDataType::ApiCall;
    new_call->call_id     = format::ApiCallId::ApiCall_ID3D12Device_CreateGraphicsPipelineState;
    new_call->object_id   = object_id;
    new_call->thread_id   = call_info.thread_id;

    encode::ParameterEncoder encoder(&new_call->parameter_buffer);

    encode::EncodeStructPtr(&encoder, pipeline_desc, decoded_desc->pRootSignature);
    encode::EncodeStruct(&encoder, *riid.decoded_value);
    encoder.EncodeHandleIdPtr(ppPipelineState->GetPointer());
    encoder.EncodeInt32Value(return_value);

    SetDeleteCurrentCall();
}

void Dx12ShaderReplacementModifier::Process_ID3D12Device_CreateComputePipelineState(
    const ApiCallInfo&                                               call_info,
    format::HandleId                                                 object_id,
    HRESULT                                                          return_value,
    StructPointerDecoder<Decoded_D3D12_COMPUTE_PIPELINE_STATE_DESC>* pDesc,
    Decoded_GUID                                                     riid,
    HandlePointerDecoder<void*>*                                     ppPipelineState)
{
    if ((return_value != S_OK) || (pDesc == nullptr) || pDesc->IsNull() || (pDesc->GetPointer() == nullptr) ||
        (ppPipelineState == nullptr) || ppPipelineState->IsNull() || (ppPipelineState->GetPointer() == nullptr))
    {
        return;
    }

    const format::HandleId pipeline_id = *ppPipelineState->GetPointer();
    if (pipeline_id == format::kNullHandleId)
    {
        return;
    }

    auto* pipeline_desc = pDesc->GetPointer();
    auto* decoded_desc  = pDesc->GetMetaStructPointer();
    if (decoded_desc == nullptr)
    {
        GFXRECON_LOG_WARNING("Skipping shader replacement for CreateComputePipelineState with missing decoded "
                             "pipeline state metadata.");
        return;
    }

    const bool replace_call =
        TryLoadOrReplaceShader(pipeline_id, graphics::Dx12ShaderTool::ShaderType::kCompute, &pipeline_desc->CS);

    if (!IsModificationPass() || !replace_call || (riid.decoded_value == nullptr))
    {
        return;
    }

    ClearCachedPipelineState(&pipeline_desc->CachedPSO);

    NewCallData* new_call = CreatePreCall();
    new_call->type        = NewCallDataType::ApiCall;
    new_call->call_id     = format::ApiCallId::ApiCall_ID3D12Device_CreateComputePipelineState;
    new_call->object_id   = object_id;
    new_call->thread_id   = call_info.thread_id;

    encode::ParameterEncoder encoder(&new_call->parameter_buffer);

    encode::EncodeStructPtr(&encoder, pipeline_desc, decoded_desc->pRootSignature);
    encode::EncodeStruct(&encoder, *riid.decoded_value);
    encoder.EncodeHandleIdPtr(ppPipelineState->GetPointer());
    encoder.EncodeInt32Value(return_value);

    SetDeleteCurrentCall();
}

void Dx12ShaderReplacementModifier::Process_ID3D12Device2_CreatePipelineState(
    const ApiCallInfo&                                              call_info,
    format::HandleId                                                object_id,
    HRESULT                                                         return_value,
    StructPointerDecoder<Decoded_D3D12_PIPELINE_STATE_STREAM_DESC>* pDesc,
    Decoded_GUID                                                    riid,
    HandlePointerDecoder<void*>*                                    ppPipelineState)
{
    if ((return_value != S_OK) || (pDesc == nullptr) || pDesc->IsNull() || (pDesc->GetPointer() == nullptr) ||
        (ppPipelineState == nullptr) || ppPipelineState->IsNull() || (ppPipelineState->GetPointer() == nullptr))
    {
        return;
    }

    const format::HandleId pipeline_id = *ppPipelineState->GetPointer();
    if (pipeline_id == format::kNullHandleId)
    {
        return;
    }

    auto* stream_desc = pDesc->GetMetaStructPointer();
    if (stream_desc == nullptr)
    {
        return;
    }

    const bool replace_call = TryLoadOrReplaceStreamShaders(pipeline_id, stream_desc);
    if (!IsModificationPass() || !replace_call || (riid.decoded_value == nullptr))
    {
        return;
    }

    ClearCachedPipelineState(stream_desc->cached_pso.decoded_value);

    NewCallData* new_call = CreatePreCall();
    new_call->type        = NewCallDataType::ApiCall;
    new_call->call_id     = format::ApiCallId::ApiCall_ID3D12Device2_CreatePipelineState;
    new_call->object_id   = object_id;
    new_call->thread_id   = call_info.thread_id;

    encode::ParameterEncoder encoder(&new_call->parameter_buffer);

    encode::EncodeStructPtr(&encoder, pDesc->GetPointer(), stream_desc->root_signature);
    encode::EncodeStruct(&encoder, *riid.decoded_value);
    encoder.EncodeHandleIdPtr(ppPipelineState->GetPointer());
    encoder.EncodeInt32Value(return_value);

    SetDeleteCurrentCall();
}

void Dx12ShaderReplacementModifier::Process_ID3D12Device5_CreateStateObject(
    const ApiCallInfo&                                     call_info,
    format::HandleId                                       object_id,
    HRESULT                                                return_value,
    StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* pDesc,
    Decoded_GUID                                           riid,
    HandlePointerDecoder<void*>*                           ppStateObject)
{
    if ((return_value != S_OK) || (pDesc == nullptr) || pDesc->IsNull() || (pDesc->GetPointer() == nullptr) ||
        (ppStateObject == nullptr) || ppStateObject->IsNull() || (ppStateObject->GetPointer() == nullptr))
    {
        return;
    }

    const format::HandleId state_object_id   = *ppStateObject->GetPointer();
    uint32_t               replacement_count = 0;
    if ((state_object_id == format::kNullHandleId) ||
        !TryLoadOrReplaceStateObjectShaders(state_object_id, pDesc, "CreateStateObject", replacement_count) ||
        !IsModificationPass() || (riid.decoded_value == nullptr))
    {
        return;
    }

    util::MemoryOutputStream parameter_buffer;
    encode::ParameterEncoder encoder(&parameter_buffer);
    if (!EncodeStateObjectDesc(&encoder, pDesc))
    {
        return;
    }

    encode::EncodeStruct(&encoder, *riid.decoded_value);
    encoder.EncodeHandleIdPtr(ppStateObject->GetPointer());
    encoder.EncodeInt32Value(return_value);

    NewCallData* new_call = CreatePreCall();
    StoreReplacementCall(parameter_buffer,
                         new_call,
                         format::ApiCallId::ApiCall_ID3D12Device5_CreateStateObject,
                         object_id,
                         call_info.thread_id);
    GFXRECON_LOG_INFO("CreateStateObject modification: rewrote state object %" PRIu64 " with %u replacement DXIL "
                      "library subobject(s).",
                      state_object_id,
                      replacement_count);
    SetDeleteCurrentCall();
}

void Dx12ShaderReplacementModifier::Process_ID3D12Device7_AddToStateObject(
    const ApiCallInfo&                                     call_info,
    format::HandleId                                       object_id,
    HRESULT                                                return_value,
    StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* pAddition,
    format::HandleId                                       pStateObjectToGrowFrom,
    Decoded_GUID                                           riid,
    HandlePointerDecoder<void*>*                           ppNewStateObject)
{
    if ((return_value != S_OK) || (pAddition == nullptr) || pAddition->IsNull() ||
        (pAddition->GetPointer() == nullptr) || (ppNewStateObject == nullptr) || ppNewStateObject->IsNull() ||
        (ppNewStateObject->GetPointer() == nullptr))
    {
        return;
    }

    const format::HandleId state_object_id   = *ppNewStateObject->GetPointer();
    uint32_t               replacement_count = 0;
    if ((state_object_id == format::kNullHandleId) ||
        !TryLoadOrReplaceStateObjectShaders(state_object_id, pAddition, "AddToStateObject", replacement_count) ||
        !IsModificationPass() || (riid.decoded_value == nullptr))
    {
        return;
    }

    util::MemoryOutputStream parameter_buffer;
    encode::ParameterEncoder encoder(&parameter_buffer);
    if (!EncodeStateObjectDesc(&encoder, pAddition))
    {
        return;
    }

    encoder.EncodeHandleIdValue(pStateObjectToGrowFrom);
    encode::EncodeStruct(&encoder, *riid.decoded_value);
    encoder.EncodeHandleIdPtr(ppNewStateObject->GetPointer());
    encoder.EncodeInt32Value(return_value);

    NewCallData* new_call = CreatePreCall();
    StoreReplacementCall(parameter_buffer,
                         new_call,
                         format::ApiCallId::ApiCall_ID3D12Device7_AddToStateObject,
                         object_id,
                         call_info.thread_id);
    GFXRECON_LOG_INFO("AddToStateObject modification: rewrote state object %" PRIu64 " grown from %" PRIu64
                      " with %u replacement DXIL library subobject(s).",
                      state_object_id,
                      pStateObjectToGrowFrom,
                      replacement_count);
    SetDeleteCurrentCall();
}

bool Dx12ShaderReplacementModifier::TryLoadShader(format::HandleId                     pipeline_id,
                                                  graphics::Dx12ShaderTool::ShaderType shader_type)
{
    std::unique_ptr<char[]> code;
    size_t                  code_size = 0;

    if (!graphics::Dx12ShaderTool::LoadReplacementPipelineShaderFromDir(
            shader_dir_, pipeline_id, shader_type, code, code_size))
    {
        return false;
    }

    const std::string file_name = graphics::Dx12ShaderTool::MakePipelineShaderFileName(pipeline_id, shader_type);
    shaders_.emplace(file_name, std::vector<char>(code.get(), code.get() + code_size));
    GFXRECON_LOG_INFO("Replacement shader found: %s", util::filepath::Join(shader_dir_, file_name).c_str());

    return true;
}

bool Dx12ShaderReplacementModifier::TryLoadOrReplaceShader(format::HandleId                     pipeline_id,
                                                           graphics::Dx12ShaderTool::ShaderType shader_type,
                                                           D3D12_SHADER_BYTECODE*               shader_bytecode)
{
    if ((shader_bytecode == nullptr) || (shader_bytecode->pShaderBytecode == nullptr) ||
        (shader_bytecode->BytecodeLength == 0))
    {
        return false;
    }

    const std::string file_name = graphics::Dx12ShaderTool::MakePipelineShaderFileName(pipeline_id, shader_type);
    auto              it        = shaders_.find(file_name);

    if (!IsModificationPass())
    {
        return (it == shaders_.end()) && TryLoadShader(pipeline_id, shader_type);
    }

    if (it == shaders_.end())
    {
        return false;
    }

    shader_bytecode->pShaderBytecode = it->second.data();
    shader_bytecode->BytecodeLength  = it->second.size();

    return true;
}

bool Dx12ShaderReplacementModifier::TryLoadOrReplaceStreamShaders(format::HandleId                          pipeline_id,
                                                                  Decoded_D3D12_PIPELINE_STATE_STREAM_DESC* stream_desc)
{
    bool replace_call = false;
    replace_call |= TryLoadOrReplaceShader(
        pipeline_id, graphics::Dx12ShaderTool::ShaderType::kVertex, stream_desc->vs_bytecode.decoded_value);
    replace_call |= TryLoadOrReplaceShader(
        pipeline_id, graphics::Dx12ShaderTool::ShaderType::kPixel, stream_desc->ps_bytecode.decoded_value);
    replace_call |= TryLoadOrReplaceShader(
        pipeline_id, graphics::Dx12ShaderTool::ShaderType::kDomain, stream_desc->ds_bytecode.decoded_value);
    replace_call |= TryLoadOrReplaceShader(
        pipeline_id, graphics::Dx12ShaderTool::ShaderType::kHull, stream_desc->hs_bytecode.decoded_value);
    replace_call |= TryLoadOrReplaceShader(
        pipeline_id, graphics::Dx12ShaderTool::ShaderType::kGeometry, stream_desc->gs_bytecode.decoded_value);
    replace_call |= TryLoadOrReplaceShader(
        pipeline_id, graphics::Dx12ShaderTool::ShaderType::kCompute, stream_desc->cs_bytecode.decoded_value);
    replace_call |= TryLoadOrReplaceShader(
        pipeline_id, graphics::Dx12ShaderTool::ShaderType::kAmplification, stream_desc->as_bytecode.decoded_value);
    replace_call |= TryLoadOrReplaceShader(
        pipeline_id, graphics::Dx12ShaderTool::ShaderType::kMesh, stream_desc->ms_bytecode.decoded_value);

    return replace_call;
}

bool Dx12ShaderReplacementModifier::TryLoadStateObjectShader(const char*              api_name,
                                                             format::HandleId         state_object_id,
                                                             uint32_t                 subobject_index,
                                                             D3D12_DXIL_LIBRARY_DESC* dxil_library)
{
    std::unique_ptr<char[]> code;
    size_t                  code_size = 0;
    const std::string       file_name =
        graphics::Dx12ShaderTool::MakeStateObjectDxilLibraryFileName(state_object_id, subobject_index);
    const std::string file_path = util::filepath::Join(shader_dir_, file_name);

    if (!graphics::Dx12ShaderTool::LoadReplacementStateObjectDxilLibraryFromDir(
            shader_dir_, state_object_id, subobject_index, code, code_size))
    {
        if (util::filepath::Exists(file_path))
        {
            GFXRECON_LOG_WARNING("%s detection: rejected replacement DXIL library for state object %" PRIu64
                                 " subobject %u: %s",
                                 api_name,
                                 state_object_id,
                                 subobject_index,
                                 file_path.c_str());
        }
        else
        {
            GFXRECON_LOG_DEBUG("%s detection: no replacement DXIL library for state object %" PRIu64
                               " subobject %u: %s",
                               api_name,
                               state_object_id,
                               subobject_index,
                               file_path.c_str());
        }
        return false;
    }

    if (!graphics::Dx12ShaderTool::ValidateStateObjectDxilLibrary(*dxil_library, code.get(), code_size))
    {
        GFXRECON_LOG_WARNING("%s detection: replacement DXIL library for state object %" PRIu64
                             " subobject %u is incompatible with the original %zu-byte library: %s",
                             api_name,
                             state_object_id,
                             subobject_index,
                             dxil_library->DXILLibrary.BytecodeLength,
                             file_path.c_str());
        return false;
    }

    shaders_.emplace(file_name, std::vector<char>(code.get(), code.get() + code_size));
    GFXRECON_LOG_INFO("%s detection: found replacement DXIL library for state object %" PRIu64
                      " subobject %u (%zu bytes): %s",
                      api_name,
                      state_object_id,
                      subobject_index,
                      code_size,
                      file_path.c_str());
    return true;
}

bool Dx12ShaderReplacementModifier::TryLoadOrReplaceStateObjectShader(const char*              api_name,
                                                                      format::HandleId         state_object_id,
                                                                      uint32_t                 subobject_index,
                                                                      D3D12_DXIL_LIBRARY_DESC* dxil_library)
{
    if ((dxil_library == nullptr) || (dxil_library->DXILLibrary.pShaderBytecode == nullptr) ||
        (dxil_library->DXILLibrary.BytecodeLength == 0))
    {
        return false;
    }

    const std::string file_name =
        graphics::Dx12ShaderTool::MakeStateObjectDxilLibraryFileName(state_object_id, subobject_index);
    auto it = shaders_.find(file_name);

    if (!IsModificationPass())
    {
        return (it == shaders_.end()) &&
               TryLoadStateObjectShader(api_name, state_object_id, subobject_index, dxil_library);
    }

    if (it == shaders_.end())
    {
        return false;
    }

    const size_t original_size                = dxil_library->DXILLibrary.BytecodeLength;
    dxil_library->DXILLibrary.pShaderBytecode = it->second.data();
    dxil_library->DXILLibrary.BytecodeLength  = it->second.size();
    GFXRECON_LOG_INFO("%s modification: replaced DXIL library for state object %" PRIu64
                      " subobject %u (%zu bytes -> %zu bytes).",
                      api_name,
                      state_object_id,
                      subobject_index,
                      original_size,
                      it->second.size());

    return true;
}

bool Dx12ShaderReplacementModifier::TryLoadOrReplaceStateObjectShaders(
    format::HandleId                                       state_object_id,
    StructPointerDecoder<Decoded_D3D12_STATE_OBJECT_DESC>* state_object_desc,
    const char*                                            api_name,
    uint32_t&                                              replacement_count)
{
    auto* desc = state_object_desc->GetPointer();
    if ((desc == nullptr) || (desc->pSubobjects == nullptr))
    {
        return false;
    }

    replacement_count = 0;
    for (UINT i = 0; i < desc->NumSubobjects; ++i)
    {
        auto& subobject = desc->pSubobjects[i];
        if ((subobject.Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY) && (subobject.pDesc != nullptr))
        {
            auto* dxil_library =
                const_cast<D3D12_DXIL_LIBRARY_DESC*>(reinterpret_cast<const D3D12_DXIL_LIBRARY_DESC*>(subobject.pDesc));
            if (TryLoadOrReplaceStateObjectShader(api_name, state_object_id, i, dxil_library))
            {
                ++replacement_count;
            }
        }
    }

    return replacement_count > 0;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
