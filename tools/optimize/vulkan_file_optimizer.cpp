/*
** Copyright (c) 2020 LunarG, Inc.
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

#include "tools/optimize/vulkan_file_optimizer.h"
#include "generated/generated_vulkan_skiavk_modifier.h"
#include "framework/format/format_util.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

bool VulkanFileOptimizer::ProcessFunctionCall(const format::BlockHeader& block_header, format::ApiCallId call_id)
{
    size_t              parameter_buffer_size = static_cast<size_t>(block_header.size) - sizeof(call_id);
    uint64_t            uncompressed_size     = 0;
    decode::ApiCallInfo call_info{ GetCurrentBlockIndex() };
    bool                success = ReadBytes(&call_info.thread_id, sizeof(call_info.thread_id));

    parameter_buffer_size -= sizeof(call_info.thread_id);
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(GetCurrentBlockIndex());
    }
    if (format::IsBlockCompressed(block_header.type))
    {
        parameter_buffer_size -= sizeof(uncompressed_size);
        success = success && ReadBytes(&uncompressed_size, sizeof(uncompressed_size));

        if (success)
        {
            GFXRECON_CHECK_CONVERSION_DATA_LOSS(size_t, uncompressed_size);

            size_t actual_size = 0;
            success            = ReadCompressedParameterBuffer(
                parameter_buffer_size, static_cast<size_t>(uncompressed_size), &actual_size);

            if (success)
            {
                assert(actual_size == uncompressed_size);
                parameter_buffer_size = static_cast<size_t>(uncompressed_size);
            }
            else
            {
                HandleBlockReadError(kErrorReadingCompressedBlockData,
                                     "Failed to read compressed function call block data");
            }
        }
        else
        {
            HandleBlockReadError(kErrorReadingCompressedBlockHeader,
                                 "Failed to read compressed function call block header");
        }
    }
    else
    {
        success = success && ReadParameterBuffer(parameter_buffer_size);

        if (!success)
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read function call block data");
        }
    }

    // Separate buffer that holds call parameters to modify
    encode::ParameterBuffer buffer;

    // Initialize our modifiable parameter buffer with the initial data from trace
    buffer.Write(GetParameterBuffer().data(), parameter_buffer_size);

    // Each modifier will get access to parameter buffer to read and modify
    // The same parameter buffer will be passed to next modifier in chain
    bool delete_current_call = false;

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;

    // This vector owns new call data to be inserted after currently processed call
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;

    if (success)
    {
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decode::DecodeAllocator::Begin();
            decoder.DecodeFunctionCall(call_id, call_info, buffer.GetData(), buffer.GetDataSize());
            decode::DecodeAllocator::End();
            decoder.RemoveConsumer(modifier.get());
            delete_current_call |= modifier->GetDeleteCurrentCall();
            modifier->AppendPreCalls(new_pre_calls);
            modifier->AppendPostCalls(new_post_calls);
        }
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
            default:
                GFXRECON_LOG_ERROR("Unrecognized PreCall NewCallDataType %d", new_call->type);
                exit(EXIT_FAILURE);
        }
    }

    if (success)
    {
        if (!delete_current_call)
        {
            WriteFunctionCall(call_id, call_info.thread_id, &buffer);
        }
    }

    for (auto& new_call : new_post_calls)
    {
        switch (new_call->type)
        {
            case util::CallModifierBase::NewCallDataType::ApiCall:
                WriteFunctionCall(new_call->call_id, new_call->thread_id, &(new_call->parameter_buffer));
                break;
            case util::CallModifierBase::NewCallDataType::MetaDataCall:
                WriteMetaCommand(&(new_call->parameter_buffer));
                break;
            default:
                GFXRECON_LOG_ERROR("Unrecognized PostCall NewCallDataType %d", new_call->type);
                exit(EXIT_FAILURE);
        }
    }

    return success;
}

// TODO: This is the same code used by CaptureManager to write function call data. It could be moved to a format
// utility.
void VulkanFileOptimizer::WriteFunctionCall(format::ApiCallId               call_id,
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

void VulkanFileOptimizer::WriteMetaCommand(const util::MemoryOutputStream* parameter_buffer)
{
    // Since Metacommands use Custom Structs and are not compressed we do the whole encoding on the modifier side

    assert(parameter_buffer != nullptr);

    const void* data_pointer = reinterpret_cast<const void*>(parameter_buffer->GetData());
    size_t      data_size    = parameter_buffer->GetDataSize();

    // Write Custom Metacommand Struct + Extra data the metacommand may use.
    WriteBytes(data_pointer, data_size);
}

bool VulkanFileOptimizer::ProcessMetaData(const format::BlockHeader& block_header, format::MetaDataId meta_data_id)
{
    uint64_t                index                 = GetCurrentBlockIndex();
    uint64_t                parameter_buffer_size = 0;
    bool                    success               = false;
    format::MetaDataType    meta_data_type        = format::GetMetaDataType(meta_data_id);
    encode::ParameterBuffer buffer;
    bool                    delete_current_call = false;
    bool                    process_meta_data   = false;

    std::vector<uint8_t>                                              meta_data_header;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(GetCurrentBlockIndex());
    }

    if (meta_data_type == format::MetaDataType::kFillMemoryCommand)
    {
        format::FillMemoryCommandHeader header;
        header.meta_header.block_header.size = block_header.size;
        header.meta_header.block_header.type = block_header.type;
        header.meta_header.meta_data_id      = meta_data_id;

        success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
        success = success && ReadBytes(&header.memory_id, sizeof(header.memory_id));
        success = success && ReadBytes(&header.memory_offset, sizeof(header.memory_offset));
        success = success && ReadBytes(&header.memory_size, sizeof(header.memory_size));

        if (success)
        {
            if (format::IsBlockCompressed(block_header.type))
            {
                size_t uncompressed_size = 0;
                size_t compressed_size   = static_cast<size_t>(block_header.size) - sizeof(meta_data_id) -
                                         sizeof(header.thread_id) - sizeof(header.memory_id) -
                                         sizeof(header.memory_offset) - sizeof(header.memory_size);
                parameter_buffer_size = compressed_size;
                success               = ReadCompressedParameterBuffer(
                    compressed_size, static_cast<size_t>(header.memory_size), &uncompressed_size);
            }
            else
            {
                parameter_buffer_size = header.memory_size;
                success               = ReadParameterBuffer(static_cast<size_t>(header.memory_size));
            }

            if (success)
            {
                meta_data_header.resize(sizeof(header));
                memcpy(meta_data_header.data(), &header, sizeof(header));
                for (auto& modifier : optimization_data_->modifiers)
                {
                    modifier->SetParameterBuffer(&buffer);
                    process_meta_data = true;
                    decoder.AddConsumer(modifier.get());
                    decoder.DispatchFillMemoryCommand(header.thread_id,
                                                      header.memory_id,
                                                      header.memory_offset,
                                                      header.memory_size,
                                                      GetParameterBuffer().data());
                    decoder.RemoveConsumer(modifier.get());
                    modifier->AppendPreCalls(new_pre_calls);
                    modifier->AppendPostCalls(new_post_calls);
                }
            }
            else
            {
                parameter_buffer_size = 0;
                if (format::IsBlockCompressed(block_header.type))
                {
                    HandleBlockReadError(kErrorReadingCompressedBlockData,
                                         "Failed to read fill memory meta-data block");
                }
                else
                {
                    HandleBlockReadError(kErrorReadingBlockData, "Failed to read fill memory meta-data block");
                }
            }
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockHeader, "Failed to read fill memory meta-data block header");
        }
    }
    else if (meta_data_type == format::MetaDataType::kInitBufferCommand)
    {
        format::InitBufferCommandHeader header;
        header.meta_header.block_header.size = block_header.size;
        header.meta_header.block_header.type = block_header.type;
        header.meta_header.meta_data_id      = meta_data_id;

        success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
        success = success && ReadBytes(&header.device_id, sizeof(header.device_id));
        success = success && ReadBytes(&header.buffer_id, sizeof(header.buffer_id));
        success = success && ReadBytes(&header.data_size, sizeof(header.data_size));

        if (success)
        {
            if (format::IsBlockCompressed(block_header.type))
            {
                size_t uncompressed_size = 0;
                size_t compressed_size =
                    static_cast<size_t>(block_header.size) - (sizeof(header) - sizeof(header.meta_header.block_header));
                parameter_buffer_size = compressed_size;
                success               = ReadCompressedParameterBuffer(
                    compressed_size, static_cast<size_t>(header.data_size), &uncompressed_size);
            }
            else
            {
                parameter_buffer_size = header.data_size;
                success               = ReadParameterBuffer(static_cast<size_t>(header.data_size));
            }

            if (success)
            {
                meta_data_header.resize(sizeof(header));
                memcpy(meta_data_header.data(), &header, sizeof(header));
                for (auto& modifier : optimization_data_->modifiers)
                {
                    modifier->SetParameterBuffer(&buffer);
                    process_meta_data = true;
                    decoder.AddConsumer(modifier.get());
                    decoder.DispatchInitBufferCommand(header.thread_id,
                                                      header.device_id,
                                                      header.buffer_id,
                                                      header.data_size,
                                                      GetParameterBuffer().data());
                    decoder.RemoveConsumer(modifier.get());
                    modifier->AppendPreCalls(new_pre_calls);
                    modifier->AppendPostCalls(new_post_calls);
                }
            }
            else
            {
                if (format::IsBlockCompressed(block_header.type))
                {
                    HandleBlockReadError(kErrorReadingCompressedBlockData,
                                         "Failed to read init buffer data meta-data block");
                }
                else
                {
                    HandleBlockReadError(kErrorReadingBlockData, "Failed to read init buffer data meta-data block");
                }
            }
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockHeader, "Failed to read init buffer data meta-data block header");
        }
    }
    else if (meta_data_type == format::MetaDataType::kVulkanBuildAccelerationStructuresCommand)
    {
        format::VulkanMetaBuildAccelerationStructuresHeader header;
        header.meta_header.block_header.size = block_header.size;
        header.meta_header.block_header.type = block_header.type;
        header.meta_header.meta_data_id      = meta_data_id;

        size_t parameter_size = static_cast<size_t>(block_header.size) - sizeof(meta_data_id);
        success               = ReadParameterBuffer(parameter_size);

        if (success)
        {
            parameter_buffer_size = parameter_size;
            meta_data_header.resize(sizeof(header));
            memcpy(meta_data_header.data(), &header, sizeof(header));
            for (auto& modifier : optimization_data_->modifiers)
            {
                modifier->SetParameterBuffer(&buffer);
                process_meta_data = true;
                decoder.AddConsumer(modifier.get());
                decode::DecodeAllocator::Begin();
                decoder.DispatchVulkanAccelerationStructuresBuildMetaCommand(GetParameterBuffer().data(),
                                                                             parameter_size);
                decode::DecodeAllocator::End();
                decoder.RemoveConsumer(modifier.get());
                modifier->AppendPreCalls(new_pre_calls);
                modifier->AppendPostCalls(new_post_calls);
            }
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockHeader,
                                 "Failed to read acceleration structure init meta-data block header");
        }
    }
    else if (meta_data_type == format::MetaDataType::kVulkanCopyAccelerationStructuresCommand)
    {
        format::VulkanCopyAccelerationStructuresCommandHeader header;
        header.meta_header.block_header.size = block_header.size;
        header.meta_header.block_header.type = block_header.type;
        header.meta_header.meta_data_id      = meta_data_id;

        size_t parameter_size = static_cast<size_t>(block_header.size) - sizeof(meta_data_id);
        success               = ReadParameterBuffer(parameter_size);

        if (success)
        {
            parameter_buffer_size = parameter_size;
            meta_data_header.resize(sizeof(header));
            memcpy(meta_data_header.data(), &header, sizeof(header));
            for (auto& modifier : optimization_data_->modifiers)
            {
                modifier->SetParameterBuffer(&buffer);
                process_meta_data = true;
                decoder.AddConsumer(modifier.get());
                decode::DecodeAllocator::Begin();
                decoder.DispatchVulkanAccelerationStructuresCopyMetaCommand(GetParameterBuffer().data(),
                                                                            parameter_size);
                decode::DecodeAllocator::End();
                decoder.RemoveConsumer(modifier.get());
                modifier->AppendPreCalls(new_pre_calls);
                modifier->AppendPostCalls(new_post_calls);
            }
        }
    }
    else if (meta_data_type == format::MetaDataType::kSetOpaqueAddressCommand)
    {
        // This command does not support compression.
        assert(block_header.type != format::BlockType::kCompressedMetaDataBlock);

        format::SetOpaqueAddressCommand header;
        header.meta_header.block_header.size = block_header.size;
        header.meta_header.block_header.type = block_header.type;
        header.meta_header.meta_data_id      = meta_data_id;

        success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
        success = success && ReadBytes(&header.device_id, sizeof(header.device_id));
        success = success && ReadBytes(&header.object_id, sizeof(header.object_id));
        success = success && ReadBytes(&header.address, sizeof(header.address));

        if (success)
        {
            parameter_buffer_size = 0;
            meta_data_header.resize(sizeof(header));
            memcpy(meta_data_header.data(), &header, sizeof(header));
            for (auto& modifier : optimization_data_->modifiers)
            {
                modifier->SetParameterBuffer(&buffer);
                process_meta_data = true;
                decoder.AddConsumer(modifier.get());
                decoder.DispatchSetOpaqueAddressCommand(
                    header.thread_id, header.device_id, header.object_id, header.address);
                decoder.RemoveConsumer(modifier.get());
                modifier->AppendPreCalls(new_pre_calls);
                modifier->AppendPostCalls(new_post_calls);
            }
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockHeader, "Failed to read set opaque address meta-data block header");
        }
    }

    for (auto& modifier : optimization_data_->modifiers)
    {
        delete_current_call |= modifier->GetDeleteCurrentCall();
    }

    for (auto& new_call : new_pre_calls)
    {
        switch (new_call->type)
        {
            case util::CallModifierBase::NewCallDataType::MetaDataCall:
                WriteMetaCommand(&(new_call->parameter_buffer));
                break;
            case util::CallModifierBase::NewCallDataType::ApiCall:
                WriteFunctionCall(new_call->call_id, new_call->thread_id, &(new_call->parameter_buffer));
                break;
            default:
                GFXRECON_LOG_ERROR("Unprocessed PreCall NewCallDataType %d", new_call->type);
                exit(EXIT_FAILURE);
        }
    }

    if (delete_current_call)
    {
        if (!process_meta_data)
        {
            SkipBytes(static_cast<size_t>(block_header.size - sizeof(meta_data_id)));
        }
    }
    else
    {
        if (process_meta_data)
        {
            WriteBytes(meta_data_header.data(), meta_data_header.size());

            if (format::IsBlockCompressed(block_header.type))
            {
                WriteBytes(GetCompressedParameterBuffer().data(), parameter_buffer_size);
            }
            else
            {
                WriteBytes(GetParameterBuffer().data(), parameter_buffer_size);
            }
        }
        else
        {
            FileOptimizer::ProcessMetaData(block_header, meta_data_id);
        }
    }

    for (auto& new_call : new_post_calls)
    {
        switch (new_call->type)
        {
            case util::CallModifierBase::NewCallDataType::MetaDataCall:
                WriteMetaCommand(&(new_call->parameter_buffer));
                break;
            case util::CallModifierBase::NewCallDataType::ApiCall:
            default:
                GFXRECON_LOG_ERROR("Unprocessed PostCall NewCallDataType %d", new_call->type);
                exit(EXIT_FAILURE);
        }
    }

    return true;
}

bool VulkanFileOptimizer::ProcessFrameMarker(const format::BlockHeader& block_header, format::MarkerType marker_type)
{
    if (marker_type != format::kEndMarker)
    {
        GFXRECON_LOG_ERROR("Skipping unrecognized frame marker with type %u", marker_type);
        return FileTransformer::ProcessFrameMarker(block_header, marker_type);
    }

    uint64_t frame_number = 0;
    bool     success      = ReadBytes(&frame_number, sizeof(frame_number));

    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(GetCurrentBlockIndex());
    }

    bool delete_current_call = false;

    if (!success)
    {
        return false;
    }

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;

    for (auto& modifier : optimization_data_->modifiers)
    {
        decoder.AddConsumer(modifier.get());
        decode::DecodeAllocator::Begin();
        decoder.DispatchFrameEndMarker(frame_number);
        decode::DecodeAllocator::End();
        decoder.RemoveConsumer(modifier.get());
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
            default:
                GFXRECON_LOG_ERROR("Unrecognized PreCall NewCallDataType %d", new_call->type);
                exit(EXIT_FAILURE);
        }
    }

    if (!delete_current_call)
    {
        format::Marker marker;
        marker.header       = block_header;
        marker.marker_type  = marker_type;
        marker.frame_number = frame_number - frames_removed;
        if (!WriteBytes(&marker, sizeof(marker)))
        {
            HandleBlockWriteError(kErrorWritingBlockData, "Failed to write frame marker data");
            return false;
        }
    }
    else
    {
        frames_removed++;
    }

    for (auto& new_call : new_post_calls)
    {
        switch (new_call->type)
        {
            case util::CallModifierBase::NewCallDataType::ApiCall:
                WriteFunctionCall(new_call->call_id, new_call->thread_id, &(new_call->parameter_buffer));
                break;
            case util::CallModifierBase::NewCallDataType::MetaDataCall:
                WriteMetaCommand(&(new_call->parameter_buffer));
                break;
            default:
                GFXRECON_LOG_ERROR("Unrecognized PostCall NewCallDataType %d", new_call->type);
                exit(EXIT_FAILURE);
        }
    }

    return success;
}

GFXRECON_END_NAMESPACE(gfxrecon)
