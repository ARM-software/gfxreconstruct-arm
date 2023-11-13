#include "feature_file_optimizer.h"
#include "format/format_util.h"
#include "util/logging.h"
#include "util/platform.h"
#include "format/format_util.h"
#include "util/memory_output_stream.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

bool FeatureFileOptimizer::ProcessFunctionCall(const format::BlockHeader& block_header, format::ApiCallId call_id)
{
    if ((call_id == format::ApiCall_vkCreateDevice) || (call_id == format::ApiCall_vkCreateInstance))
    {
        size_t              parameter_buffer_size = static_cast<size_t>(block_header.size) - sizeof(call_id);
        uint64_t            uncompressed_size     = 0;
        decode::ApiCallInfo call_info{ GetCurrentBlockIndex() };
        bool                success = ReadBytes(&call_info.thread_id, sizeof(call_info.thread_id));

        parameter_buffer_size -= sizeof(call_info.thread_id);

        if (format::IsBlockCompressed(block_header.type))
        {
            parameter_buffer_size -= sizeof(uncompressed_size);
            success = ReadBytes(&uncompressed_size, sizeof(uncompressed_size));

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
            success = ReadParameterBuffer(parameter_buffer_size);

            if (!success)
            {
                HandleBlockReadError(kErrorReadingBlockData, "Failed to read function call block data");
            }
        }

        encode::ParameterBuffer buffer;
        ft_consumer_->SetBuffer(&buffer);

        for (auto decoder : decoders_)
        {

            decode::DecodeAllocator::Begin();
            decoder->DecodeFunctionCall(call_id, call_info, GetParameterBuffer().data(), parameter_buffer_size);
            decode::DecodeAllocator::End();
        }
        WriteFunctionCall(call_id, call_info.thread_id, &buffer);
        return true;
    }

    return FileTransformer::ProcessFunctionCall(block_header, call_id);
}

// TODO: This is the same code used by CaptureManager to write function call data. It could be moved to a format
// utility.
void FeatureFileOptimizer::WriteFunctionCall(format::ApiCallId         call_id,
                                             format::ThreadId          thread_id,
                                             util::MemoryOutputStream* parameter_buffer)
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

    auto compressor                  = GetCompressor();
    auto compressed_parameter_buffer = GetCompressedParameterBuffer();

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

GFXRECON_END_NAMESPACE(gfxrecon)
