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

#include "dx12_file_optimizer_arm.h"

#include "format/format_util.h"
#include "format/format_arm.h"

#include <cstring>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

bool Dx12FileOptimizerARM::ProcessFunctionCall(decode::ParsedBlock& parsed_block)
{
    const auto& args = parsed_block.Get<decode::FunctionCallArgs>();

    // Exit early if the call is filtered out by FileOptimizer
    if (FilterFunctionCall(args))
    {
        return true;
    }

    if (!parsed_block.Decompress(GetCompressor(), working_uncompressed_store_))
    {
        return false;
    }

    // Separate buffer that holds call parameters to modify
    encode::ParameterBuffer buffer;
    buffer.Write(args.data, args.data_size);

    // Dispatch call while applying modifiers
    auto modifier_dispatch_visitor = [this, &parsed_block, &buffer](const auto& store) {
        if constexpr (std::is_same_v<std::decay_t<decltype(store)>, std::monostate>)
        {
            return true; // Passthrough unknown blocks.
        }
        else
        {
            return ModifierDispatch(*store, parsed_block, buffer);
        }
    };

    return std::visit(modifier_dispatch_visitor, parsed_block.GetArgs());
}

bool Dx12FileOptimizerARM::ProcessMethodCall(decode::ParsedBlock& parsed_block)
{
    const auto& args = parsed_block.Get<decode::MethodCallArgs>();

    // Exit early if the call is filtered out by FileOptimizer
    if (FilterMethodCall(args))
    {
        return true;
    }

    if (!parsed_block.Decompress(GetCompressor(), working_uncompressed_store_))
    {
        return false;
    }

    // Separate buffer that holds call parameters to modify
    encode::ParameterBuffer buffer;
    buffer.Write(args.data, args.data_size);

    // Dispatch call while applying modifiers
    auto modifier_dispatch_visitor = [this, &parsed_block, &buffer](const auto& store) {
        if constexpr (std::is_same_v<std::decay_t<decltype(store)>, std::monostate>)
        {
            return true; // Passthrough unknown blocks.
        }
        else
        {
            return ModifierDispatch(*store, parsed_block, buffer);
        }
    };

    return std::visit(modifier_dispatch_visitor, parsed_block.GetArgs());
}

bool Dx12FileOptimizerARM::ProcessMetaData(decode::ParsedBlock& parsed_block)
{
    // Exit early if the call is filtered out by FileOptimizer
    auto filter_visitor = [this](const auto& store) {
        if constexpr (std::is_same_v<std::decay_t<decltype(store)>, std::monostate>)
        {
            return VisitResult::kNeedsPassthrough; // Passthrough unknown blocks.
        }
        else
        {
            return FilterMetaData(*store);
        }
    };
    VisitResult result = std::visit(filter_visitor, parsed_block.GetArgs());
    if (result != kNeedsPassthrough)
    {
        return result == kSuccess;
    }

    if (!parsed_block.Decompress(GetCompressor(), working_uncompressed_store_))
    {
        return false;
    }

    // There is no "parameter" but the buffer is still needed to signal the modification pass to the modifiers
    encode::ParameterBuffer buffer;

    // Dispatch call while applying modifiers
    auto modifier_dispatch_visitor = [this, &parsed_block, &buffer](const auto& store) {
        if constexpr (std::is_same_v<std::decay_t<decltype(store)>, std::monostate>)
        {
            return true; // Passthrough unknown blocks.
        }
        else
        {
            return ModifierDispatch(*store, parsed_block, buffer);
        }
    };

    return std::visit(modifier_dispatch_visitor, parsed_block.GetArgs());
}

bool Dx12FileOptimizerARM::ProcessFrameEndMarker(decode::ParsedBlock& parsed_block)
{
    // There is no "parameter" but the buffer is still needed to signal the modification pass to the modifiers
    encode::ParameterBuffer buffer;

    // Dispatch call while applying modifiers
    auto modifier_dispatch_visitor = [this, &parsed_block, &buffer](const auto& store) {
        if constexpr (std::is_same_v<std::decay_t<decltype(store)>, std::monostate>)
        {
            return true; // Passthrough unknown blocks.
        }
        else
        {
            return ModifierDispatch(*store, parsed_block, buffer);
        }
    };

    return std::visit(modifier_dispatch_visitor, parsed_block.GetArgs());
}

void Dx12FileOptimizerARM::WriteMethodCall(format::ApiCallId               call_id,
                                           format::HandleId                call_object_id,
                                           format::ThreadId                thread_id,
                                           const util::MemoryOutputStream* parameter_buffer)
{
    assert(parameter_buffer != nullptr);

    bool                               not_compressed      = true;
    format::CompressedMethodCallHeader compressed_header   = {};
    format::MethodCallHeader           uncompressed_header = {};
    size_t                             uncompressed_size   = parameter_buffer->GetDataSize();
    size_t                             header_size         = 0;
    const void*                        header_pointer      = nullptr;
    size_t                             data_size           = 0;
    const void*                        data_pointer        = nullptr;

    util::Compressor*     compressor                  = GetCompressor();
    std::vector<uint8_t>& compressed_parameter_buffer = GetCompressedParameterBuffer();

    if (compressor != nullptr)
    {
        size_t packet_size = 0;
        size_t compressed_size =
            compressor->Compress(uncompressed_size, parameter_buffer->GetData(), &compressed_parameter_buffer, 0);

        if ((compressed_size > 0) && (compressed_size < uncompressed_size))
        {
            data_pointer   = reinterpret_cast<const void*>(compressed_parameter_buffer.data());
            data_size      = compressed_size;
            header_pointer = reinterpret_cast<const void*>(&compressed_header);
            header_size    = sizeof(format::CompressedMethodCallHeader);

            compressed_header.block_header.type = format::BlockType::kCompressedMethodCallBlock;
            compressed_header.api_call_id       = call_id;
            compressed_header.object_id         = call_object_id;
            compressed_header.thread_id         = thread_id;
            compressed_header.uncompressed_size = uncompressed_size;

            packet_size += sizeof(compressed_header.api_call_id) + sizeof(compressed_header.object_id) +
                           sizeof(compressed_header.uncompressed_size) + sizeof(compressed_header.thread_id) +
                           compressed_size;

            compressed_header.block_header.size = packet_size;
            not_compressed                      = false;
        }
    }

    if (not_compressed)
    {
        size_t packet_size = 0;
        data_pointer       = reinterpret_cast<const void*>(parameter_buffer->GetData());
        data_size          = uncompressed_size;
        header_pointer     = reinterpret_cast<const void*>(&uncompressed_header);
        header_size        = sizeof(format::MethodCallHeader);

        uncompressed_header.block_header.type = format::BlockType::kMethodCallBlock;
        uncompressed_header.api_call_id       = call_id;
        uncompressed_header.object_id         = call_object_id;
        uncompressed_header.thread_id         = thread_id;

        packet_size += sizeof(uncompressed_header.api_call_id) + sizeof(compressed_header.object_id) +
                       sizeof(uncompressed_header.thread_id) + data_size;

        uncompressed_header.block_header.size = packet_size;
    }

    // Write appropriate function call block header.
    WriteBytes(header_pointer, header_size);

    // Write parameter data.
    WriteBytes(data_pointer, data_size);
}

void Dx12FileOptimizerARM::WriteFunctionCall(format::ApiCallId               call_id,
                                             format::ThreadId                thread_id,
                                             const util::MemoryOutputStream* parameter_buffer)
{
    assert(parameter_buffer != nullptr);

    bool                                 not_compressed      = true;
    format::CompressedFunctionCallHeader compressed_header   = {};
    format::FunctionCallHeader           uncompressed_header = {};
    size_t                               uncompressed_size   = parameter_buffer->GetDataSize();
    size_t                               header_size         = 0;
    const void*                          header_pointer      = nullptr;
    size_t                               data_size           = 0;
    const void*                          data_pointer        = nullptr;

    util::Compressor*     compressor                  = GetCompressor();
    std::vector<uint8_t>& compressed_parameter_buffer = GetCompressedParameterBuffer();

    if (compressor != nullptr)
    {
        size_t packet_size = 0;
        size_t compressed_size =
            compressor->Compress(uncompressed_size, parameter_buffer->GetData(), &compressed_parameter_buffer, 0);

        if ((0 < compressed_size) && (compressed_size < uncompressed_size))
        {
            data_pointer   = reinterpret_cast<const void*>(compressed_parameter_buffer.data());
            data_size      = compressed_size;
            header_pointer = reinterpret_cast<const void*>(&compressed_header);
            header_size    = sizeof(format::CompressedFunctionCallHeader);

            compressed_header.block_header.type = format::BlockType::kCompressedFunctionCallBlock;
            compressed_header.api_call_id       = call_id;
            compressed_header.thread_id         = thread_id;
            compressed_header.uncompressed_size = uncompressed_size;

            packet_size += sizeof(compressed_header.api_call_id) + sizeof(compressed_header.uncompressed_size) +
                           sizeof(compressed_header.thread_id) + compressed_size;

            compressed_header.block_header.size = packet_size;
            not_compressed                      = false;
        }
    }

    if (not_compressed)
    {
        size_t packet_size = 0;
        data_pointer       = reinterpret_cast<const void*>(parameter_buffer->GetData());
        data_size          = uncompressed_size;
        header_pointer     = reinterpret_cast<const void*>(&uncompressed_header);
        header_size        = sizeof(format::FunctionCallHeader);

        uncompressed_header.block_header.type = format::BlockType::kFunctionCallBlock;
        uncompressed_header.api_call_id       = call_id;
        uncompressed_header.thread_id         = thread_id;

        packet_size += sizeof(uncompressed_header.api_call_id) + sizeof(uncompressed_header.thread_id) + data_size;

        uncompressed_header.block_header.size = packet_size;
    }

    // Write appropriate function call block header.
    WriteBytes(header_pointer, header_size);

    // Write parameter data.
    WriteBytes(data_pointer, data_size);
}

void Dx12FileOptimizerARM::WriteMetaCommand(const util::MemoryOutputStream* parameter_buffer)
{
    assert(parameter_buffer != nullptr);

    const uint8_t* data_pointer = parameter_buffer->GetData();
    const size_t   data_size    = parameter_buffer->GetDataSize();

    if ((data_size >= sizeof(format::MetaDataHeader)) && (GetCompressor() != nullptr))
    {
        format::MetaDataHeader meta_header{};
        std::memcpy(&meta_header, data_pointer, sizeof(meta_header));

        const auto meta_data_type = format::GetMetaDataType(meta_header.meta_data_id);
        size_t     header_size    = 0;
        switch (meta_data_type)
        {
            case format::arm::MetaDataType::kFillMemoryResourceAddressCommand:
                header_size = sizeof(format::arm::FillMemoryResourceAddressCommandHeader);
                break;
            case format::arm::MetaDataType::kGetDx12AccelerationStructureSizeCommand:
                header_size = sizeof(format::arm::GetDx12AccelerationStructureSizeCommandHeader);
                break;
            case format::arm::MetaDataType::kDx12ResourceAliasingCommand:
                header_size = sizeof(format::arm::Dx12ResourceAliasingCommandHeader);
                break;
            default:
                break;
        }

        if ((header_size > 0) && (data_size >= header_size))
        {
            const size_t         payload_size = data_size - header_size;
            std::vector<uint8_t> compressed_payload;
            const size_t         compressed_size =
                GetCompressor()->Compress(payload_size, data_pointer + header_size, &compressed_payload, 0);

            if ((compressed_size > 0) && (compressed_size < payload_size))
            {
                std::vector<uint8_t> compressed_header(data_pointer, data_pointer + header_size);
                meta_header.block_header.type = format::BlockType::kCompressedMetaDataBlock;
                meta_header.block_header.size = header_size - sizeof(format::BlockHeader) + compressed_size;
                std::memcpy(compressed_header.data(), &meta_header, sizeof(meta_header));

                WriteBytes(compressed_header.data(), compressed_header.size());
                WriteBytes(compressed_payload.data(), compressed_size);
                return;
            }
        }
    }

    // Write Custom Metacommand Struct + Extra data the metacommand may use.
    WriteBytes(data_pointer, data_size);
}

GFXRECON_END_NAMESPACE(gfxrecon)
