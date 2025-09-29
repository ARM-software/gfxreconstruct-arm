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

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

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

bool Dx12FileOptimizerARM::ProcessMethodCall(const format::MethodCallHeader& header, uint64_t block_index)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessMethodCall(header, block_index);
    }

    if (unreferenced_blocks_.find(block_index) != unreferenced_blocks_.end())
    {
        return FileOptimizer::ProcessMethodCall(header, block_index);
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
                                     "Failed to read compressed method call block data");
            }
        }
        else
        {
            HandleBlockReadError(kErrorReadingCompressedBlockHeader,
                                 "Failed to read compressed method call block header");
        }
    }
    else
    {
        success = success && ReadParameterBuffer(parameter_buffer_size);

        if (!success)
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read method call block data");
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
            decoder.DecodeMethodCall(
                header.api_call_id, header.object_id, call_info, buffer.GetData(), buffer.GetDataSize());
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
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
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
            WriteMethodCall(header.api_call_id, header.object_id, call_info.thread_id, &buffer);
        }
    }

    for (auto& new_call : new_post_calls)
    {
        switch (new_call->type)
        {
            case util::CallModifierBase::NewCallDataType::ApiCall:
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
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

void Dx12FileOptimizerARM::WriteMetaCommand(const util::MemoryOutputStream* parameter_buffer)
{
    // Since Metacommands use Custom Structs and are not compressed we do the whole encoding on the modifier side

    assert(parameter_buffer != nullptr);

    const void* data_pointer = reinterpret_cast<const void*>(parameter_buffer->GetData());
    size_t      data_size    = parameter_buffer->GetDataSize();

    // Write Custom Metacommand Struct + Extra data the metacommand may use.
    WriteBytes(data_pointer, data_size);
}

bool Dx12FileOptimizerARM::ProcessMarker(const format::Marker& marker)
{
    if (marker.header.type != format::kFrameMarkerBlock || marker.marker_type != format::kEndMarker)
    {
        GFXRECON_LOG_DEBUG("Skipping unrecognized marker with type %u", marker.marker_type);
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
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
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
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
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

bool Dx12FileOptimizerARM::ProcessFillMemoryCommand(const format::FillMemoryCommandHeader& header)
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
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
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

bool Dx12FileOptimizerARM::ProcessInitSubresourceCommand(const format::InitSubresourceCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessInitSubresourceCommand(header);
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
            decoder.DispatchInitSubresourceCommand(header, GetParameterBuffer().data());
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
            HandleBlockReadError(kErrorReadingCompressedBlockData, "Failed to read sub resource meta-data block");
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read sub resource meta-data block");
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
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
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

bool Dx12FileOptimizerARM::ProcessInitDx12AccelerationStructureCommand(
    const format::InitDx12AccelerationStructureCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessInitDx12AccelerationStructureCommand(header);
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

    bool success = true;
    // Parse geometry descs.
    std::vector<format::InitDx12AccelerationStructureGeometryDesc> geom_descs;
    for (uint32_t i = 0; i < header.inputs_num_geometry_descs; ++i)
    {
        format::InitDx12AccelerationStructureGeometryDesc geom_desc;
        success = ReadBytes(&geom_desc.geometry_type, sizeof(geom_desc.geometry_type));
        success = success && ReadBytes(&geom_desc.geometry_flags, sizeof(geom_desc.geometry_flags));
        success = success && ReadBytes(&geom_desc.aabbs_count, sizeof(geom_desc.aabbs_count));
        success = success && ReadBytes(&geom_desc.aabbs_stride, sizeof(geom_desc.aabbs_stride));
        success = success && ReadBytes(&geom_desc.triangles_has_transform, sizeof(geom_desc.triangles_has_transform));
        success = success && ReadBytes(&geom_desc.triangles_index_format, sizeof(geom_desc.triangles_index_format));
        success = success && ReadBytes(&geom_desc.triangles_vertex_format, sizeof(geom_desc.triangles_vertex_format));
        success = success && ReadBytes(&geom_desc.triangles_index_count, sizeof(geom_desc.triangles_index_count));
        success = success && ReadBytes(&geom_desc.triangles_vertex_count, sizeof(geom_desc.triangles_vertex_count));
        success = success && ReadBytes(&geom_desc.triangles_vertex_stride, sizeof(geom_desc.triangles_vertex_stride));
        geom_descs.push_back(geom_desc);
    }

    if (format::IsBlockCompressed(header.meta_header.block_header.type))
    {
        size_t uncompressed_size = 0;
        size_t compressed_size =
            static_cast<size_t>(header.meta_header.block_header.size) - format::GetMetaDataBlockBaseSize(header) -
            (sizeof(format::InitDx12AccelerationStructureGeometryDesc) * header.inputs_num_geometry_descs);
        parameter_buffer_size = compressed_size;
        success               = ReadCompressedParameterBuffer(
            compressed_size, static_cast<size_t>(header.inputs_data_size), &uncompressed_size);
    }
    else
    {
        parameter_buffer_size = header.inputs_data_size;
        success               = ReadParameterBuffer(static_cast<size_t>(header.inputs_data_size));
    }

    if (success)
    {
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decoder.DispatchInitDx12AccelerationStructureCommand(header, geom_descs, GetParameterBuffer().data());
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
            HandleBlockReadError(kErrorReadingCompressedBlockData, "Failed to read init dx12 AS meta-data block");
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read init dx12 AS meta-data block");
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
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
                break;
            default:
                GFXRECON_LOG_ERROR("Unprocessed PreCall NewCallDataType %d", new_call->type);
                exit(EXIT_FAILURE);
        }
    }

    if (!delete_current_call)
    {
        WriteBytes(&header, sizeof(header));
        if (geom_descs.size() > 0)
        {
            WriteBytes(geom_descs.data(),
                       sizeof(format::InitDx12AccelerationStructureGeometryDesc) * geom_descs.size());
        }

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

bool Dx12FileOptimizerARM::ProcessFillMemoryResourceValueCommand(
    const format::FillMemoryResourceValueCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessFillMemoryResourceValueCommand(header);
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

    uint64_t data_size = header.resource_value_count * (sizeof(format::ResourceValueType) + sizeof(uint64_t));
    GFXRECON_CHECK_CONVERSION_DATA_LOSS(size_t, data_size);

    bool success;
    if (format::IsBlockCompressed(header.meta_header.block_header.type))
    {
        size_t uncompressed_size = 0;
        size_t compressed_size   = static_cast<size_t>(header.meta_header.block_header.size) -
                                 sizeof(header.meta_header.meta_data_id) - sizeof(header.thread_id) -
                                 sizeof(header.resource_value_count);
        parameter_buffer_size    = compressed_size;
        size_t uncompressed_data = static_cast<size_t>(data_size);
        success = ReadCompressedParameterBuffer(compressed_size, uncompressed_data, &uncompressed_size);
    }
    else
    {
        parameter_buffer_size = data_size;
        success               = ReadParameterBuffer(static_cast<size_t>(data_size));
    }

    if (success)
    {
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decoder.DispatchFillMemoryResourceValueCommand(header, GetParameterBuffer().data());
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
            HandleBlockReadError(kErrorReadingCompressedBlockData, "Failed to read fill memory value meta-data block");
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read fill memory value meta-data block");
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
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
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

bool Dx12FileOptimizerARM::ProcessFillMemoryResourceAddressCommand(
    const format::FillMemoryResourceAddressCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return FileOptimizer::ProcessFillMemoryResourceAddressCommand(header);
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

    uint64_t data_size = header.resource_address_count * sizeof(format::Dx12FillMemoryResourceAddressInfo);
    GFXRECON_CHECK_CONVERSION_DATA_LOSS(size_t, data_size);

    bool success;
    if (format::IsBlockCompressed(header.meta_header.block_header.type))
    {
        size_t uncompressed_size = 0;
        size_t compressed_size   = static_cast<size_t>(header.meta_header.block_header.size) -
                                 sizeof(header.meta_header.meta_data_id) - sizeof(header.thread_id) -
                                 sizeof(header.resource_address_count);
        size_t uncompressed_data = static_cast<size_t>(data_size);
        success = ReadCompressedParameterBuffer(compressed_size, uncompressed_data, &uncompressed_size);
    }
    else
    {
        parameter_buffer_size = data_size;
        success               = ReadParameterBuffer(static_cast<size_t>(data_size));
    }

    if (success)
    {
        for (auto& modifier : optimization_data_->modifiers)
        {
            modifier->SetParameterBuffer(&buffer);
            decoder.AddConsumer(modifier.get());
            decoder.DispatchFillMemoryResourceAddressCommand(header, GetParameterBuffer().data());
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
            HandleBlockReadError(kErrorReadingCompressedBlockData,
                                 "Failed to read fill memory address meta-data block");
        }
        else
        {
            HandleBlockReadError(kErrorReadingBlockData, "Failed to read fill memory address meta-data block");
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
                WriteMethodCall(
                    new_call->call_id, new_call->object_id, new_call->thread_id, &(new_call->parameter_buffer));
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
