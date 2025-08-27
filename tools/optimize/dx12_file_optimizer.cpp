/*
** Copyright (c) 2022-2025 LunarG, Inc.
** Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
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

#include "dx12_file_optimizer.h"

#include "format/format_util.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

void Dx12FileOptimizer::SetFillCommandResourceValues(
    const decode::Dx12FillCommandResourceValueMap* fill_command_resource_values,
    bool                                           inject_noop_resource_value_optimization)
{
    inject_noop_resource_value_optimization_ = inject_noop_resource_value_optimization;

    fill_command_resource_values_ = fill_command_resource_values;
    if (fill_command_resource_values_ != nullptr)
    {
        // A NOOP RV optimization block should only be added if there weren't any real fill_command_resource_values
        // found.
        GFXRECON_ASSERT((inject_noop_resource_value_optimization_ == false) || fill_command_resource_values->empty());

        resource_values_iter_ = fill_command_resource_values_->begin();
    }
}

void Dx12FileOptimizer::SetFillCommandResourceAddresses(
    const decode::Dx12FillCommandResourceAddressMap* fill_command_resource_addresses)
{
    fill_command_resource_addresses_ = fill_command_resource_addresses;
    if (fill_command_resource_addresses_ != nullptr)
    {
        GFXRECON_ASSERT(fill_command_resource_addresses->empty());
        resource_addresses_iter_ = fill_command_resource_addresses_->begin();
    }
}

void Dx12FileOptimizer::SetPrebuildInfoResourceValues(
    const decode::Dx12PrebuildInfoResourceValueMap* prebuild_info_resource_values)
{
    GFXRECON_ASSERT((prebuild_info_resource_values != nullptr) && !prebuild_info_resource_values->empty());
    prebuild_info_resource_values_ = prebuild_info_resource_values;
}

bool Dx12FileOptimizer::AddFillMemoryResourceValueCommand()
{
    bool success = true;

    GFXRECON_ASSERT(resource_values_iter_->first == GetCurrentBlockIndex());

    format::FillMemoryResourceValueCommandHeader rv_header;
    rv_header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
    rv_header.meta_header.meta_data_id      = format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_D3D12,
                                                                format::MetaDataType::kFillMemoryResourceValueCommand);
    rv_header.thread_id                     = 0;
    rv_header.resource_value_count          = resource_values_iter_->second.size();

    size_t       header_size = sizeof(format::FillMemoryResourceValueCommandHeader);
    const size_t uncompressed_size =
        resource_values_iter_->second.size() * (sizeof(format::ResourceValueType) + sizeof(uint64_t));

    bool not_compressed = true;

    if (uncompressed_size > 0)
    {
        write_buffer_.clear();
        write_buffer_.resize(uncompressed_size);

        // Write resource value data to uncompressed buffer.
        auto type_data_pos = write_buffer_.data();
        auto offset_data_pos =
            write_buffer_.data() + (rv_header.resource_value_count * sizeof(format::ResourceValueType));
        for (const auto& resource_value_pair : resource_values_iter_->second)
        {
            auto type   = resource_value_pair.type;
            auto offset = resource_value_pair.offset;

            util::platform::MemoryCopy(type_data_pos, sizeof(type), &type, sizeof(type));
            util::platform::MemoryCopy(offset_data_pos, sizeof(offset), &offset, sizeof(offset));

            type_data_pos += sizeof(resource_value_pair.type);
            offset_data_pos += sizeof(resource_value_pair.offset);
        }
        GFXRECON_ASSERT(type_data_pos ==
                        (write_buffer_.data() + (rv_header.resource_value_count * sizeof(format::ResourceValueType))));
        GFXRECON_ASSERT(offset_data_pos == (write_buffer_.data() + uncompressed_size));

        std::vector<uint8_t> compressed_write_buffer;

        if (GetCompressor() != nullptr)
        {
            size_t compressed_size =
                GetCompressor()->Compress(write_buffer_.size(), write_buffer_.data(), &compressed_write_buffer, 0);

            if ((compressed_size > 0) && (compressed_size < uncompressed_size))
            {
                not_compressed = false;

                // Calculate size of packet with compressed data size.
                rv_header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(rv_header) + compressed_size;
                rv_header.meta_header.block_header.type = format::BlockType::kCompressedMetaDataBlock;

                success = success && WriteBytes(&rv_header, header_size);
                success = success && WriteBytes(compressed_write_buffer.data(), compressed_size);
            }
        }
    }

    // If the data was not compressed, write the uncompressed data here.
    if (not_compressed)
    {
        // Calculate size of packet with uncompressed data size.
        rv_header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(rv_header) + uncompressed_size;

        success = success && WriteBytes(&rv_header, header_size);
        success = success && WriteBytes(write_buffer_.data(), uncompressed_size);
    }

    ++resource_values_iter_;
    ++num_optimized_fill_commands_;

    return success;
}

bool Dx12FileOptimizer::AddFillMemoryResourceAddressCommand()
{
    bool success = true;

    GFXRECON_ASSERT(resource_addresses_iter_->first == GetCurrentBlockIndex());

    format::FillMemoryResourceAddressCommandHeader ra_header;
    ra_header.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
    ra_header.meta_header.meta_data_id      = format::MakeMetaDataId(
        format::ApiFamilyId::ApiFamily_D3D12, format::MetaDataType::kFillMemoryResourceAddressCommand);
    ra_header.thread_id              = 1;
    ra_header.resource_address_count = resource_addresses_iter_->second.size();

    size_t       header_size = sizeof(format::FillMemoryResourceAddressCommandHeader);
    const size_t uncompressed_size =
        resource_addresses_iter_->second.size() * sizeof(decode::Dx12FillCommandResourceAddress);

    bool not_compressed = true;

    if (uncompressed_size > 0)
    {
        write_buffer_.clear();
        write_buffer_.resize(uncompressed_size);

        // Write resource address data to uncompressed buffer.
        auto data_pos = write_buffer_.data();
        for (const auto& resource_address : resource_addresses_iter_->second)
        {
            util::platform::MemoryCopy(data_pos,
                                       sizeof(decode::Dx12FillCommandResourceAddress),
                                       &resource_address,
                                       sizeof(decode::Dx12FillCommandResourceAddress));

            data_pos += sizeof(decode::Dx12FillCommandResourceAddress);
        }
        GFXRECON_ASSERT(data_pos == (write_buffer_.data() + (ra_header.resource_address_count *
                                                             sizeof(decode::Dx12FillCommandResourceAddress))));

        std::vector<uint8_t> compressed_write_buffer;

        if (GetCompressor() != nullptr)
        {
            size_t compressed_size =
                GetCompressor()->Compress(write_buffer_.size(), write_buffer_.data(), &compressed_write_buffer, 0);

            if ((compressed_size > 0) && (compressed_size < uncompressed_size))
            {
                not_compressed = false;

                // Calculate size of packet with compressed data size.
                ra_header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(ra_header) + compressed_size;
                ra_header.meta_header.block_header.type = format::BlockType::kCompressedMetaDataBlock;

                success = success && WriteBytes(&ra_header, header_size);
                success = success && WriteBytes(compressed_write_buffer.data(), compressed_size);
            }
        }
    }

    // If the data was not compressed, write the uncompressed data here.
    if (not_compressed)
    {
        // Calculate size of packet with uncompressed data size.
        ra_header.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(ra_header) + uncompressed_size;

        success = success && WriteBytes(&ra_header, header_size);
        success = success && WriteBytes(write_buffer_.data(), uncompressed_size);
    }

    ++resource_addresses_iter_;
    ++num_optimized_fill_commands_;

    return success;
}

void Dx12FileOptimizer::WriteMethodCall(format::ApiCallId               call_id,
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

bool Dx12FileOptimizer::AddPrebuildInfoResourceValueCommand(const format::BlockHeader& block_header,
                                                            format::ApiCallId          call_id)
{
    bool success = true;
    GFXRECON_ASSERT(prebuild_info_resource_values_ != nullptr);

    auto prebuild_iter = prebuild_info_resource_values_->find(GetCurrentBlockIndex());
    GFXRECON_ASSERT(prebuild_iter != prebuild_info_resource_values_->end());

    auto& build_descs = prebuild_iter->second;
    for (auto iter = build_descs.begin(); iter != build_descs.end(); ++iter)
    {
        format::HandleId  object_id     = iter->second.object_id;
        const auto&       prebuild_info = iter->second.get_prebuild_info;
        format::ThreadId  thread_id     = 0;
        format::ApiCallId api_call_id =
            format::ApiCallId::ApiCall_ID3D12Device5_GetRaytracingAccelerationStructurePrebuildInfo;

        WriteMethodCall(api_call_id, object_id, thread_id, &prebuild_info);
    }

    return success;
}

bool Dx12FileOptimizer::ProcessMethodCall(const format::MethodCallHeader& header, uint64_t block_index)
{
    if ((header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device_CreateCommittedResource) ||
        (header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device4_CreateCommittedResource1) ||
        (header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device8_CreateCommittedResource2) ||
        (header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device10_CreateCommittedResource3) ||
        (header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device_CreatePlacedResource) ||
        (header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device8_CreatePlacedResource1) ||
        (header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device10_CreatePlacedResource2) ||
        (header.api_call_id == format::ApiCallId::ApiCall_IUnknown_QueryInterface))
    {
        GFXRECON_ASSERT(prebuild_info_resource_values_ != nullptr);

        if (prebuild_info_resource_values_->find(GetCurrentBlockIndex()) != prebuild_info_resource_values_->end())
        {
            if (!AddPrebuildInfoResourceValueCommand(header.block_header, header.api_call_id))
            {
                GFXRECON_LOG_ERROR("Failed to write the GetRaytracingAccelerationStructurePrebuildInfo needed "
                                   "for DXR or EI optimization. Optimized file may be invalid.");
            }
        }
    }

    return FileOptimizer::ProcessMethodCall(header, block_index);
}

bool Dx12FileOptimizer::ProcessMetaData(const format::MetaDataHeader& meta_header)
{
    format::MetaDataType meta_data_type = format::GetMetaDataType(meta_header.meta_data_id);

    // If needed, add a FillMemoryResourceValueCommand before the fill memory command.
    if ((meta_data_type == format::MetaDataType::kFillMemoryCommand) ||
        (meta_data_type == format::MetaDataType::kInitSubresourceCommand))
    {
        if ((fill_command_resource_values_ != nullptr) && (!fill_command_resource_values_->empty()))
        {
            if ((resource_values_iter_ != fill_command_resource_values_->end()) &&
                (resource_values_iter_->first == GetCurrentBlockIndex()))
            {
                if (!AddFillMemoryResourceValueCommand())
                {
                    GFXRECON_LOG_ERROR("Failed to write the FillMemoryResourceValueCommand needed for DXR or EI "
                                       "optimization. Optimized file may be invalid.");
                }
            }
        }
        else if (inject_noop_resource_value_optimization_)
        {
            // Only inject one noop block.
            inject_noop_resource_value_optimization_ = false;

            decode::Dx12FillCommandResourceValueMap rvm;
            rvm[GetCurrentBlockIndex()]   = {};
            fill_command_resource_values_ = &rvm;
            resource_values_iter_         = fill_command_resource_values_->begin();

            if (!AddFillMemoryResourceValueCommand())
            {
                GFXRECON_LOG_ERROR("Failed to write the FillMemoryResourceValueCommand needed for DXR/EI optimization. "
                                   "Optimized file may be invalid.");
            }

            fill_command_resource_values_ = nullptr;
            resource_values_iter_         = {};
        }
        else if ((fill_command_resource_addresses_ != nullptr) && (!fill_command_resource_addresses_->empty()))
        {
            if ((resource_addresses_iter_ != fill_command_resource_addresses_->end()) &&
                (resource_addresses_iter_->first == GetCurrentBlockIndex()))
            {
                if (!AddFillMemoryResourceAddressCommand())
                {
                    GFXRECON_LOG_ERROR("Failed to write the FillMemoryResourceAddressCommand needed for DXR or EI "
                                       "optimization. Optimized file may be invalid.");
                }
            }
        }
    }
    else if (meta_data_type == format::MetaDataType::kFillMemoryResourceValueCommand)
    {
        // Total number of bytes remaining to be read for the current block.
        const uint64_t unread_bytes =
            meta_header.block_header.size - sizeof(meta_header) + sizeof(meta_header.block_header);

        if (!FileOptimizer::SkipBytes(unread_bytes))
        {
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip meta block data");
            return false;
        }

        return true;
    }
    else if (meta_data_type == format::MetaDataType::kFillMemoryResourceAddressCommand)
    {
        // Total number of bytes remaining to be read for the current block.
        const uint64_t unread_bytes =
            meta_header.block_header.size - sizeof(meta_header) + sizeof(meta_header.block_header);

        if (!FileOptimizer::SkipBytes(unread_bytes))
        {
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip meta block data");
            return false;
        }

        return true;
    }

    return FileOptimizer::ProcessMetaData(meta_header);
}

GFXRECON_END_NAMESPACE(gfxrecon)
