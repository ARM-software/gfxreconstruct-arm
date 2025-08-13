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

bool VulkanFileOptimizer::ProcessFunctionCall(const format::FunctionCallHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessFunctionCall(header);
    }

    size_t parameter_buffer_size =
        static_cast<size_t>(header.block_header.size) - (sizeof(header) - sizeof(header.block_header));
    uint64_t            uncompressed_size = 0;
    decode::ApiCallInfo call_info{ GetCurrentBlockIndex(), header.thread_id };
    bool                success = true;

    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(GetCurrentBlockIndex());
    }
    if (format::IsBlockCompressed(header.block_header.type))
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
            decoder.DecodeFunctionCall(header.api_call_id, call_info, buffer.GetData(), buffer.GetDataSize());
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
            WriteFunctionCall(header.api_call_id, call_info.thread_id, &buffer);
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

bool VulkanFileOptimizer::ProcessFillMemoryCommand(const format::FillMemoryCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessFillMemoryCommand(header);
    }

    uint64_t                index                 = GetCurrentBlockIndex();
    uint64_t                parameter_buffer_size = 0;
    encode::ParameterBuffer buffer;
    bool                    delete_current_call = false;

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(index);
    }

    bool success;
    if (format::IsBlockCompressed(header.meta_header.block_header.type))
    {
        size_t uncompressed_size = 0;
        size_t compressed_size =
            static_cast<size_t>(header.meta_header.block_header.size) - format::GetMetaDataBlockBaseSize(header);
        parameter_buffer_size = compressed_size;
        success =
            ReadCompressedParameterBuffer(compressed_size, static_cast<size_t>(header.memory_size), &uncompressed_size);
    }
    else
    {
        parameter_buffer_size = header.memory_size;
        success               = ReadParameterBuffer(static_cast<size_t>(header.memory_size));
    }

    if (success)
    {
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
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
        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            HandleBlockReadError(kErrorReadingCompressedBlockData, "Failed to read fill memory meta-data block");
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read fill memory meta-data block");
        }

        return false;
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

    if (!delete_current_call)
    {
        WriteBytes(&header, sizeof(header));

        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            WriteBytes(GetCompressedParameterBuffer().data(), parameter_buffer_size);
        }
        else
        {
            WriteBytes(GetParameterBuffer().data(), parameter_buffer_size);
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

bool VulkanFileOptimizer::ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader& header)
{
    uint64_t                index                 = GetCurrentBlockIndex();
    uint64_t                parameter_buffer_size = 0;
    encode::ParameterBuffer buffer;
    bool                    delete_current_call = false;

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(index);
    }

    parameter_buffer_size = header.num_of_locations * sizeof(format::AddressLocationInfo);
    bool success =
        ReadParameterBuffer(static_cast<size_t>(header.num_of_locations * sizeof(format::AddressLocationInfo)));

    if (success)
    {
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decoder.DispatchFixDeviceAddresCommand(
                header, reinterpret_cast<const format::AddressLocationInfo*>(GetParameterBuffer().data()));
            decoder.RemoveConsumer(modifier.get());
            modifier->AppendPreCalls(new_pre_calls);
            modifier->AppendPostCalls(new_post_calls);
        }
    }
    else
    {
        parameter_buffer_size = 0;
        HandleBlockReadError(kErrorReadingBlockData, "Failed to read fix device address meta-data block");
        return false;
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

    if (!delete_current_call)
    {
        WriteBytes(&header, sizeof(header));
        WriteBytes(GetParameterBuffer().data(), parameter_buffer_size);
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

bool VulkanFileOptimizer::ProcessInitBufferCommand(const format::InitBufferCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessInitBufferCommand(header);
    }

    uint64_t                index                 = GetCurrentBlockIndex();
    uint64_t                parameter_buffer_size = 0;
    encode::ParameterBuffer buffer;
    bool                    delete_current_call = false;

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(index);
    }

    bool success;
    if (format::IsBlockCompressed(header.meta_header.block_header.type))
    {
        size_t uncompressed_size = 0;
        size_t compressed_size =
            static_cast<size_t>(header.meta_header.block_header.size) - format::GetMetaDataBlockBaseSize(header);
        parameter_buffer_size = compressed_size;
        success =
            ReadCompressedParameterBuffer(compressed_size, static_cast<size_t>(header.data_size), &uncompressed_size);
    }
    else
    {
        parameter_buffer_size = header.data_size;
        success               = ReadParameterBuffer(static_cast<size_t>(header.data_size));
    }

    if (success)
    {
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decoder.DispatchInitBufferCommand(
                header.thread_id, header.device_id, header.buffer_id, header.data_size, GetParameterBuffer().data());
            decoder.RemoveConsumer(modifier.get());
            modifier->AppendPreCalls(new_pre_calls);
            modifier->AppendPostCalls(new_post_calls);
        }
    }
    else
    {
        parameter_buffer_size = 0;
        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            HandleBlockReadError(kErrorReadingCompressedBlockData, "Failed to read fill memory meta-data block");
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read fill memory meta-data block");
        }

        return false;
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

    if (!delete_current_call)
    {
        WriteBytes(&header, sizeof(header));

        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            WriteBytes(GetCompressedParameterBuffer().data(), parameter_buffer_size);
        }
        else
        {
            WriteBytes(GetParameterBuffer().data(), parameter_buffer_size);
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

bool VulkanFileOptimizer::ProcessSetOpaqueAddressCommand(const format::SetOpaqueAddressCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessSetOpaqueAddressCommand(header);
    }

    // This command does not support compression.
    GFXRECON_ASSERT(header.meta_header.block_header.type != format::BlockType::kCompressedMetaDataBlock);

    uint64_t                index                 = GetCurrentBlockIndex();
    uint64_t                parameter_buffer_size = 0;
    encode::ParameterBuffer buffer;
    bool                    delete_current_call = false;

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(index);
    }

    parameter_buffer_size = 0;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetParameterBuffer(&buffer);
        decoder.AddConsumer(modifier.get());
        decoder.DispatchSetOpaqueAddressCommand(header.thread_id, header.device_id, header.object_id, header.address);
        decoder.RemoveConsumer(modifier.get());
        modifier->AppendPreCalls(new_pre_calls);
        modifier->AppendPostCalls(new_post_calls);
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

    if (!delete_current_call)
    {
        WriteBytes(&header, sizeof(header));

        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            WriteBytes(GetCompressedParameterBuffer().data(), parameter_buffer_size);
        }
        else
        {
            WriteBytes(GetParameterBuffer().data(), parameter_buffer_size);
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

bool VulkanFileOptimizer::ProcessVulkanBuildAccelerationStructuresCommand(
    const format::VulkanMetaBuildAccelerationStructuresHeader& header)
{
    uint64_t                index                 = GetCurrentBlockIndex();
    uint64_t                parameter_buffer_size = 0;
    encode::ParameterBuffer buffer;
    bool                    delete_current_call = false;

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(index);
    }

    size_t parameter_size =
        static_cast<size_t>(header.meta_header.block_header.size) - sizeof(header.meta_header.meta_data_id);
    bool success = ReadParameterBuffer(parameter_size);

    if (success)
    {
        parameter_buffer_size = parameter_size;
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decode::DecodeAllocator::Begin();
            decoder.DispatchVulkanAccelerationStructuresBuildMetaCommand(GetParameterBuffer().data(), parameter_size);
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

    if (!delete_current_call)
    {
        WriteBytes(&header, sizeof(header));

        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            WriteBytes(GetCompressedParameterBuffer().data(), parameter_buffer_size);
        }
        else
        {
            WriteBytes(GetParameterBuffer().data(), parameter_buffer_size);
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

bool VulkanFileOptimizer::ProcessVulkanCopyAccelerationStructuresCommand(
    const format::VulkanCopyAccelerationStructuresCommandHeader& header)
{
    uint64_t                index                 = GetCurrentBlockIndex();
    uint64_t                parameter_buffer_size = 0;
    encode::ParameterBuffer buffer;
    bool                    delete_current_call = false;

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(index);
    }

    size_t parameter_size =
        static_cast<size_t>(header.meta_header.block_header.size) - sizeof(header.meta_header.meta_data_id);
    bool success = ReadParameterBuffer(parameter_size);

    if (success)
    {
        parameter_buffer_size = parameter_size;
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decode::DecodeAllocator::Begin();
            decoder.DispatchVulkanAccelerationStructuresCopyMetaCommand(GetParameterBuffer().data(), parameter_size);
            decode::DecodeAllocator::End();
            decoder.RemoveConsumer(modifier.get());
            modifier->AppendPreCalls(new_pre_calls);
            modifier->AppendPostCalls(new_post_calls);
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

    if (!delete_current_call)
    {
        WriteBytes(&header, sizeof(header));

        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            WriteBytes(GetCompressedParameterBuffer().data(), parameter_buffer_size);
        }
        else
        {
            WriteBytes(GetParameterBuffer().data(), parameter_buffer_size);
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

void VulkanFileOptimizer::WriteMetaCommand(const util::MemoryOutputStream* parameter_buffer)
{
    // Since Metacommands use Custom Structs and are not compressed we do the whole encoding on the modifier side

    assert(parameter_buffer != nullptr);

    const void* data_pointer = reinterpret_cast<const void*>(parameter_buffer->GetData());
    size_t      data_size    = parameter_buffer->GetDataSize();

    // Write Custom Metacommand Struct + Extra data the metacommand may use.
    WriteBytes(data_pointer, data_size);
}

bool VulkanFileOptimizer::ProcessMarker(const format::Marker& marker)
{
    if (marker.header.type != format::kFrameMarkerBlock || marker.marker_type != format::kEndMarker)
    {
        GFXRECON_LOG_ERROR("Skipping unrecognized marker with type %u", marker.marker_type);
        return FileOptimizer::ProcessMarker(marker);
    }

    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(GetCurrentBlockIndex());
    }

    bool delete_current_call = false;

    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;

    for (auto& modifier : optimization_data_->modifiers)
    {
        decoder.AddConsumer(modifier.get());
        decode::DecodeAllocator::Begin();
        decoder.DispatchFrameEndMarker(marker.frame_number);
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
        format::Marker new_marker = marker;
        new_marker.frame_number -= frames_removed;
        if (!WriteBytes(&new_marker, sizeof(new_marker)))
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

    return true;
}

bool VulkanFileOptimizer::ProcessInitTensorCommand(const format::InitTensorCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessInitTensorCommand(header);
    }
    uint64_t                                                          index                 = GetCurrentBlockIndex();
    uint64_t                                                          parameter_buffer_size = 0;
    encode::ParameterBuffer                                           buffer;
    bool                                                              delete_current_call = false;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_pre_calls;
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> new_post_calls;
    for (auto& modifier : optimization_data_->modifiers)
    {
        modifier->SetCurrentBlockIndex(index);
    }
    bool success;
    if (format::IsBlockCompressed(header.meta_header.block_header.type))
    {
        size_t uncompressed_size = 0;
        size_t compressed_size =
            static_cast<size_t>(header.meta_header.block_header.size) - format::GetMetaDataBlockBaseSize(header);
        parameter_buffer_size = compressed_size;
        success =
            ReadCompressedParameterBuffer(compressed_size, static_cast<size_t>(header.data_size), &uncompressed_size);
    }
    else
    {
        parameter_buffer_size = header.data_size;
        success               = ReadParameterBuffer(static_cast<size_t>(header.data_size));
    }
    if (success)
    {
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decoder.DispatchInitTensorCommand(
                header.thread_id, header.device_id, header.tensor_id, header.data_size, GetParameterBuffer().data());
            decoder.RemoveConsumer(modifier.get());
            modifier->AppendPreCalls(new_pre_calls);
            modifier->AppendPostCalls(new_post_calls);
        }
    }
    else
    {
        parameter_buffer_size = 0;
        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            HandleBlockReadError(kErrorReadingCompressedBlockData, "Failed to read fill memory meta-data block");
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read fill memory meta-data block");
        }
        return false;
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
    if (!delete_current_call)
    {
        WriteBytes(&header, sizeof(header));
        if (format::IsBlockCompressed(header.meta_header.block_header.type))
        {
            WriteBytes(GetCompressedParameterBuffer().data(), parameter_buffer_size);
        }
        else
        {
            WriteBytes(GetParameterBuffer().data(), parameter_buffer_size);
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

GFXRECON_END_NAMESPACE(gfxrecon)
