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

#ifndef GFXRECON_DX12_FILE_OPTIMIZER_ARM_H
#define GFXRECON_DX12_FILE_OPTIMIZER_ARM_H

#include "decode/dx12_object_scanning_consumer.h"
#include "decode/file_processor_visitors.h"
#include "generated/generated_dx12_decoder.h"
#include "util/dx12_modifier_base.h"
#include "file_optimizer.h"
#include "util/defines.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

class Dx12FileOptimizerARM : public FileOptimizer
{
  public:
    struct Dx12OptimizationData
    {
        std::unordered_set<format::HandleId> unreferenced_ids;
        std::unordered_set<uint64_t>         unreferenced_blocks;
        decode::UnreferencedPsoCreationCalls calls_info{};

        std::vector<std::unique_ptr<util::Dx12ModifierBase>> modifiers;
    };

    Dx12FileOptimizerARM(Dx12OptimizationData*                       optimization_data,
                         const std::unordered_set<format::ThreadId>& removed_threads_ids) :
        FileOptimizer(optimization_data->unreferenced_ids, optimization_data->unreferenced_blocks, removed_threads_ids),
        optimization_data_(optimization_data)
    {}

  private:
    bool ProcessFunctionCall(decode::ParsedBlock& parsed_block) override;
    bool ProcessMethodCall(decode::ParsedBlock& parsed_block) override;
    bool ProcessMetaData(decode::ParsedBlock& parsed_block) override;
    bool ProcessFrameEndMarker(decode::ParsedBlock& parsed_block) override;

    template <typename Args>
    bool ModifierDispatch(const Args& args, decode::ParsedBlock& parsed_block, encode::ParameterBuffer& buffer)
    {
        constexpr auto decode_method = decode::DispatchTraits<Args>::kDecoderMethod;
        if (decode::file_processor::DecoderSupportsDispatch(decoder_, args))
        {
            [[maybe_unused]] decode::DecoderAllocGuard<decode::DispatchTraits<Args>::kHasAllocGuard> alloc_guard{};
            decode::file_processor::SetDecoderApiCallId(decoder_, args);
            auto dispatch_call = [this, decode_method](auto&&... expanded_args) {
                (decoder_.*decode_method)(std::forward<decltype(expanded_args)>(expanded_args)...);
            };

            // Each modifier will get access to parameter buffer to read and modify
            // The same parameter buffer will be passed to next modifier in chain
            bool delete_current_call = false;

            // These vectors own new call data to be inserted before/after currently processed call
            std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
            std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;

            for (auto& modifier : optimization_data_->modifiers)
            {
                modifier->SetCurrentBlockIndex(GetCurrentBlockIndex());
                modifier->SetParameterBuffer(&buffer);
                decoder_.AddConsumer(modifier.get());
                std::apply(dispatch_call, args.GetTuple());
                decoder_.RemoveConsumer(modifier.get());
                delete_current_call |= modifier->GetDeleteCurrentCall();
                modifier->AppendPreCalls(new_pre_calls);
                modifier->AppendPostCalls(new_post_calls);
            }

            for (auto& new_call : new_pre_calls)
            {
                switch (new_call->type)
                {
                    case util::CallModifierBase::NewCallDataType::ApiCall:
                        if constexpr (std::is_same_v<Args, decode::FunctionCallArgs>)
                        {
                            WriteFunctionCall(new_call->call_id, new_call->thread_id, &(new_call->parameter_buffer));
                        }
                        else if constexpr (std::is_same_v<Args, decode::MethodCallArgs>)
                        {
                            WriteMethodCall(new_call->call_id,
                                            new_call->object_id,
                                            new_call->thread_id,
                                            &(new_call->parameter_buffer));
                        }
                        else
                        {
                            GFXRECON_LOG_ERROR("Attempting to write an API call before unsupported block type");
                        }
                        break;
                    case util::CallModifierBase::NewCallDataType::MetaDataCall:
                        WriteMetaCommand(&(new_call->parameter_buffer));
                        break;
                    default:
                        GFXRECON_LOG_ERROR("Unrecognized PreCall NewCallDataType %d", new_call->type);
                        exit(EXIT_FAILURE);
                }
            }

            if (!delete_current_call)
            {
                if constexpr (std::is_same_v<Args, decode::FunctionCallArgs>)
                {
                    WriteFunctionCall(args.call_id, args.call_info.thread_id, &buffer);
                }
                else if constexpr (std::is_same_v<Args, decode::MethodCallArgs>)
                {
                    WriteMethodCall(args.call_id, args.object_id, args.call_info.thread_id, &buffer);
                }
                else if constexpr (std::is_same_v<Args, decode::FrameEndMarkerArgs>)
                {
                    format::Marker marker;
                    marker.header.size  = sizeof(format::Marker) - sizeof(format::BlockHeader);
                    marker.header.type  = format::kFrameMarkerBlock;
                    marker.marker_type  = format::kEndMarker;
                    marker.frame_number = args.frame_number - frames_removed_;
                    if (!WriteBytes(&marker, sizeof(marker)))
                    {
                        HandleBlockWriteError(decode::kErrorWritingBlockData, "Failed to write frame marker data");
                        return false;
                    }
                }
                else
                {
                    if (!FileTransformer::WriteBytes(parsed_block))
                    {
                        return false;
                    }
                }
            }
            else
            {
                if constexpr (std::is_same_v<Args, decode::FrameEndMarkerArgs>)
                {
                    ++frames_removed_;
                }
            }

            for (auto& new_call : new_post_calls)
            {
                switch (new_call->type)
                {
                    case util::CallModifierBase::NewCallDataType::ApiCall:
                        if constexpr (std::is_same_v<Args, decode::FunctionCallArgs>)
                        {
                            WriteFunctionCall(new_call->call_id, new_call->thread_id, &(new_call->parameter_buffer));
                        }
                        else if constexpr (std::is_same_v<Args, decode::MethodCallArgs>)
                        {
                            WriteMethodCall(new_call->call_id,
                                            new_call->object_id,
                                            new_call->thread_id,
                                            &(new_call->parameter_buffer));
                        }
                        else
                        {
                            GFXRECON_LOG_ERROR("Attempting to write an API call before unsupported block type");
                        }
                        break;
                    case util::CallModifierBase::NewCallDataType::MetaDataCall:
                        WriteMetaCommand(&(new_call->parameter_buffer));
                        break;
                    default:
                        GFXRECON_LOG_ERROR("Unrecognized PostCall NewCallDataType %d", new_call->type);
                        exit(EXIT_FAILURE);
                }
            }
        }

        return true;
    }

    bool ModifierDispatch(const decode::AnnotationArgs& args,
                          decode::ParsedBlock&          parsed_block,
                          encode::ParameterBuffer&      buffer)
    {
        return FileOptimizer::ProcessMetaData(parsed_block);
    }

    bool ModifierDispatch(const decode::file_processor::ProcessBlocksResult& result,
                          decode::ParsedBlock&                               parsed_block,
                          encode::ParameterBuffer&                           buffer)
    {
        return result.state == decode::file_processor::ProcessBlockState::kContinue;
    }

    bool ModifierDispatch(const decode::CallbackArgs& args,
                          decode::ParsedBlock&        parsed_block,
                          encode::ParameterBuffer&    buffer)
    {
        args.callback();
        return true;
    }

    void WriteFunctionCall(format::ApiCallId               call_id,
                           format::ThreadId                thread_id,
                           const util::MemoryOutputStream* parameter_buffer);

    void WriteMethodCall(format::ApiCallId               call_id,
                         format::HandleId                call_object_id,
                         format::ThreadId                thread_id,
                         const util::MemoryOutputStream* parameter_buffer);

    void WriteMetaCommand(const util::MemoryOutputStream* parameter_buffer);

    Dx12OptimizationData* optimization_data_;
    decode::Dx12Decoder   decoder_;
    uint64_t              frames_removed_ = 0;
};

GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DX12_FILE_OPTIMIZER_ARM_H
