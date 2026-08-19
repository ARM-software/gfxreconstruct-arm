/*
** Copyright (c) 2024 LunarG, Inc.
** Copyright (c) 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#ifndef GFXRECON_VULKAN_FILE_OPTIMIZER_H
#define GFXRECON_VULKAN_FILE_OPTIMIZER_H

#include "decode/file_processor_visitors.h"
#include "file_optimizer.h"
#include "util/defines.h"
#include "generated/generated_vulkan_decoder.h"
#include "util/vulkan_modifier_base.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

class VulkanFileOptimizer : public FileOptimizer
{
  public:
    struct VulkanOptimizationData
    {
        std::unordered_set<gfxrecon::format::HandleId>         unreferenced_ids;
        std::unordered_set<uint64_t>                           unreferenced_blocks;
        std::vector<std::unique_ptr<util::VulkanModifierBase>> modifiers;
    };

    VulkanFileOptimizer(VulkanOptimizationData*                     optimization_data,
                        const std::unordered_set<format::ThreadId>& removed_threads_ids);

  private:
    bool ProcessFunctionCall(decode::ParsedBlock& parsed_block) override;
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
                        WriteFunctionCall(new_call->call_id, new_call->thread_id, &(new_call->parameter_buffer));
                        break;
                    case util::CallModifierBase::NewCallDataType::MetaDataCall:
                        WriteMetaCommand(&(new_call->parameter_buffer));
                        break;
                    case util::CallModifierBase::NewCallDataType::FrameMarkerCall:
                        WriteFrameEndMarker(new_call->frame_number);
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
                else if constexpr (std::is_same_v<Args, decode::FrameEndMarkerArgs>)
                {
                    if (!FileTransformer::WriteBytes(parsed_block))
                    {
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

            for (auto& new_call : new_post_calls)
            {
                if (delete_current_call && (new_call->type == util::CallModifierBase::NewCallDataType::FrameMarkerCall))
                {
                    continue;
                }

                if (new_call->type == util::CallModifierBase::NewCallDataType::FrameMarkerCall)
                {
                    continue;
                }

                switch (new_call->type)
                {
                    case util::CallModifierBase::NewCallDataType::ApiCall:
                        WriteFunctionCall(new_call->call_id, new_call->thread_id, &(new_call->parameter_buffer));
                        break;
                    case util::CallModifierBase::NewCallDataType::MetaDataCall:
                        WriteMetaCommand(&(new_call->parameter_buffer));
                        break;
                    case util::CallModifierBase::NewCallDataType::FrameMarkerCall:
                        WriteFrameEndMarker(new_call->frame_number);
                        break;
                    default:
                        GFXRECON_LOG_ERROR("Unrecognized PostCall NewCallDataType %d", new_call->type);
                        exit(EXIT_FAILURE);
                }
            }

            for (auto& new_call : new_post_calls)
            {
                if (delete_current_call || (new_call->type != util::CallModifierBase::NewCallDataType::FrameMarkerCall))
                {
                    continue;
                }

                WriteFrameEndMarker(new_call->frame_number);
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
    bool WriteFrameEndMarker(uint64_t frame_number);
    void WriteMetaCommand(const util::MemoryOutputStream* parameter_buffer);

    VulkanOptimizationData* optimization_data_;
    decode::VulkanDecoder   decoder_;
};

GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_FILE_OPTIMIZER_H
