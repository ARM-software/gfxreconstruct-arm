/*
** Copyright (c) 2020 LunarG, Inc.
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

#include "file_transformer.h"

#include PROJECT_VERSION_HEADER_FILE
#include "format/format_arm.h"
#include "format/format_util.h"
#include "util/logging.h"
#include "util/platform.h"

#include <cassert>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

FileTransformer::FileTransformer() :
    input_file_(nullptr), output_file_(nullptr), bytes_read_(0), bytes_written_(0),
    error_state_(kErrorInvalidFileDescriptor), loading_state_(false)
{}

FileTransformer::~FileTransformer()
{
    if (input_file_ != nullptr)
    {
        fclose(input_file_);
    }

    if (output_file_ != nullptr)
    {
        fclose(output_file_);
    }
}

bool FileTransformer::Initialize(const std::string& input_filename,
                                 const std::string& output_filename,
                                 const std::string& tool)
{
    input_filename_  = input_filename;
    output_filename_ = output_filename;
    tool_            = tool;

    bool success = false;

    int32_t result = util::platform::FileOpen(&input_file_, input_filename.c_str(), "rb");

    if ((result == 0) && (input_file_ != nullptr))
    {
        result = util::platform::FileOpen(&output_file_, output_filename.c_str(), "wb");

        if ((result == 0) && (output_file_ != nullptr))
        {
            success = ProcessFileHeader();
        }
        else
        {
            GFXRECON_LOG_ERROR("Failed to open output file %s", output_filename.c_str());
            error_state_ = kErrorOpeningFile;
        }
    }
    else
    {
        GFXRECON_LOG_ERROR("Failed to open input file %s", input_filename.c_str());
        error_state_ = kErrorOpeningFile;
    }

    if (success)
    {
        error_state_ = kErrorNone;
    }
    else
    {
        if (input_file_ != nullptr)
        {
            fclose(input_file_);
            input_file_ = nullptr;
        }

        if (output_file_ != nullptr)
        {
            fclose(output_file_);
            output_file_ = nullptr;
        }
    }

    return success;
}

bool FileTransformer::Initialize(const std::string& input_filename, const std::string& output_filename)
{
    return Initialize(input_filename, output_filename, "");
}

// Returns false if processing failed.  Use GetErrorState() to determine error condition for failure case.
bool FileTransformer::Process()
{
    bool success = true;

    const char*  label        = format::kAnnotationLabelTransformer;
    const size_t label_length = util::platform::StringLength(label);

    if (!tool_.empty())
    {
        std::string data = "";
        data += "{\n";
        data += "  \"input\": " + input_filename_ + ",\n";
        data += "  \"output\": " + output_filename_ + ",\n";
        data += "  \"tool\": " + tool_ + "\n";
        data += "}";
        const size_t data_length = data.size();

        format::AnnotationHeader annotation;
        annotation.block_header.size = format::GetAnnotationBlockBaseSize() + label_length + data_length;
        annotation.block_header.type = format::BlockType::kAnnotation;
        annotation.annotation_type   = format::kJson;
        annotation.label_length      = label_length;
        annotation.data_length       = data_length;
        if (!WriteBytes(&annotation, sizeof(annotation)) || !WriteBytes(label, label_length) ||
            !WriteBytes(data.c_str(), data_length))
        {
            HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write transformer annotation");
            return false;
        }
    }

    block_index_ = 0;
    while (success)
    {
        success = ProcessNextBlock();
        block_index_++;
    }

    if (!success && (error_state_ == kErrorNone))
    {
        // If a failure occured, but no error code was set, check for a file error.
        if ((input_file_ == nullptr) || (output_file_ == nullptr))
        {
            error_state_ = kErrorInvalidFileDescriptor;
        }
        else if (ferror(input_file_))
        {
            error_state_ = kErrorReadingFile;
        }
        else if (ferror(output_file_))
        {
            error_state_ = kErrorWritingFile;
        }
    }

    return (error_state_ == kErrorNone);
}

bool FileTransformer::ProcessFileHeader()
{
    bool success = false;

    if (ReadBytes(&file_header_, sizeof(file_header_)))
    {
        success = format::ValidateFileHeader(file_header_);

        if (success)
        {
            file_options_.resize(file_header_.num_options);

            size_t option_data_size = file_header_.num_options * sizeof(format::FileOptionPair);

            success = ReadBytes(file_options_.data(), option_data_size);

            if (success)
            {
                for (const auto& option : file_options_)
                {
                    switch (option.key)
                    {
                        case format::FileOption::kCompressionType:
                            enabled_options_.compression_type = static_cast<format::CompressionType>(option.value);
                            break;
                        default:
                            GFXRECON_LOG_WARNING("Ignoring unrecognized file header option %u", option.key);
                            break;
                    }
                }

                success = CreateCompressor(enabled_options_.compression_type, &compressor_);
            }

            if (success)
            {
                format::FileHeader modified_header = file_header_;

                // Set the output trace version to the optimizer version.
                modified_header.major_version = GFXRECON_TRACE_VERSION_MAJOR;
                modified_header.minor_version = GFXRECON_TRACE_VERSION_MINOR;

                // Write header to output file.
                success = WriteFileHeader(modified_header, file_options_);
            }
        }
        else
        {
            GFXRECON_LOG_ERROR("File header contains invalid four character code");
            error_state_ = kErrorInvalidFourCC;
        }
    }
    else
    {
        GFXRECON_LOG_ERROR("Failed to read file header");
        error_state_ = kErrorReadingFileHeader;
    }

    return success;
}

bool FileTransformer::ProcessNextBlock()
{
    format::BlockHeader block_header;
    bool                success = true;

    success = ReadBlockHeader(&block_header);

    if (success)
    {
        if (format::RemoveCompressedBlockBit(block_header.type) == format::BlockType::kFunctionCallBlock)
        {
            format::FunctionCallHeader header;
            header.block_header = block_header;

            success = ReadBytes(&header.api_call_id, sizeof(header.api_call_id));
            success = success && ReadBytes(&header.thread_id, sizeof(header.thread_id));

            if (success)
            {
                success = ProcessFunctionCall(header);
            }
            else
            {
                HandleBlockReadError(kErrorReadingBlockHeader, "Failed to read function call block header");
            }
        }
        else if (format::RemoveCompressedBlockBit(block_header.type) == format::BlockType::kMetaDataBlock)
        {
            format::MetaDataHeader header;
            header.block_header = block_header;

            success = ReadBytes(&header.meta_data_id, sizeof(header.meta_data_id));

            if (success)
            {
                success = ProcessMetaData(header);
            }
            else
            {
                HandleBlockReadError(kErrorReadingBlockHeader, "Failed to read meta-data block header");
            }
        }
        else if (block_header.type == format::BlockType::kFrameMarkerBlock ||
                 block_header.type == format::BlockType::kStateMarkerBlock)
        {
            format::Marker marker;
            marker.header = block_header;

            success = ReadBytes(&marker.marker_type, sizeof(marker.marker_type));
            success = success && ReadBytes(&marker.frame_number, sizeof(marker.frame_number));

            if (success)
            {
                success = ProcessMarker(marker);
            }
            else
            {
                HandleBlockReadError(kErrorReadingBlockHeader, "Failed to read marker block");
            }
        }
        else if (format::RemoveCompressedBlockBit(block_header.type) == format::BlockType::kMethodCallBlock)
        {
            format::MethodCallHeader header;
            header.block_header = block_header;

            success = ReadBytes(&header.api_call_id, sizeof(header.api_call_id));
            success = success && ReadBytes(&header.object_id, sizeof(header.object_id));
            success = success && ReadBytes(&header.thread_id, sizeof(header.thread_id));

            if (success)
            {
                success = ProcessMethodCall(header, block_index_);
            }
            else
            {
                HandleBlockReadError(kErrorReadingBlockHeader, "Failed to read method call block header");
            }
        }
        else if (block_header.type == format::BlockType::kAnnotation)
        {
            format::AnnotationHeader header;
            header.block_header = block_header;

            success = ReadBytes(&header.annotation_type, sizeof(header.annotation_type));
            success = success && ReadBytes(&header.label_length, sizeof(header.label_length));
            success = success && ReadBytes(&header.data_length, sizeof(header.data_length));

            if (success && ((header.label_length > 0) || (header.data_length > 0)))
            {
                std::string label;
                std::string data;
                const auto  size_sum = header.label_length + header.data_length;
                GFXRECON_CHECK_CONVERSION_DATA_LOSS(size_t, size_sum);
                const size_t total_length = static_cast<size_t>(size_sum);

                success = ReadParameterBuffer(total_length);
                if (success)
                {
                    if (header.label_length > 0)
                    {
                        auto label_start = parameter_buffer_.begin();
                        label.assign(label_start, std::next(label_start, header.label_length));
                    }

                    if (header.data_length > 0)
                    {
                        auto data_start = std::next(parameter_buffer_.begin(), header.label_length);
                        GFXRECON_CHECK_CONVERSION_DATA_LOSS(size_t, header.data_length);
                        data.assign(data_start, std::next(data_start, static_cast<size_t>(header.data_length)));
                    }

                    ProcessAnnotation(header, label, data);
                }
            }

            if (!success)
            {
                HandleBlockReadError(kErrorReadingBlockData, "Failed to read annotation block");
            }
        }
        else
        {
            // Copy the block to the output file.
            success = WriteBlockHeader(block_header);

            if (success)
            {
                success = CopyBytes(block_header.size);

                if (!success)
                {
                    GFXRECON_LOG_ERROR("Failed to write block data");
                    error_state_ = kErrorWritingBlockData;
                }
            }
        }
    }
    else
    {
        if (!feof(input_file_))
        {
            // If we have not hit a normal EOF condition, report an error reading the block header.
            GFXRECON_LOG_ERROR("Failed to read block header");
            error_state_ = kErrorReadingBlockHeader;
        }
    }

    return success;
}

bool FileTransformer::ReadBlockHeader(format::BlockHeader* block_header)
{
    assert(block_header != nullptr);

    if (ReadBytes(block_header, sizeof(*block_header)))
    {
        return true;
    }

    return false;
}

bool FileTransformer::WriteBlockHeader(const format::BlockHeader& block_header)
{
    if (!WriteBytes(&block_header, sizeof(block_header)))
    {
        GFXRECON_LOG_ERROR("Failed to write block header");
        error_state_ = kErrorWritingBlockHeader;
        return false;
    }
    return true;
}

bool FileTransformer::ReadParameterBuffer(size_t buffer_size)
{
    if (buffer_size > parameter_buffer_.size())
    {
        parameter_buffer_.resize(buffer_size);
    }

    return ReadBytes(parameter_buffer_.data(), buffer_size);
}

bool FileTransformer::ReadCompressedParameterBuffer(size_t  compressed_buffer_size,
                                                    size_t  expected_uncompressed_size,
                                                    size_t* uncompressed_buffer_size)
{
    // This should only be null if initialization failed.
    assert(compressor_ != nullptr);

    if (compressed_buffer_size > compressed_parameter_buffer_.size())
    {
        compressed_parameter_buffer_.resize(compressed_buffer_size);
    }

    if (ReadBytes(compressed_parameter_buffer_.data(), compressed_buffer_size))
    {
        if (parameter_buffer_.size() < expected_uncompressed_size)
        {
            parameter_buffer_.resize(expected_uncompressed_size);
        }

        size_t uncompressed_size = compressor_->Decompress(compressed_buffer_size,
                                                           compressed_parameter_buffer_.data(),
                                                           expected_uncompressed_size,
                                                           &parameter_buffer_);
        if ((0 < uncompressed_size) && (uncompressed_size == expected_uncompressed_size))
        {
            *uncompressed_buffer_size = uncompressed_size;
            return true;
        }
    }

    return false;
}

bool FileTransformer::ReadBytes(void* buffer, size_t buffer_size)
{
    if (util::platform::FileRead(buffer, buffer_size, input_file_))
    {
        bytes_read_ += buffer_size;
        return true;
    }
    return false;
}

bool FileTransformer::WriteBytes(const void* buffer, size_t buffer_size)
{
    if (util::platform::FileWrite(buffer, buffer_size, output_file_))
    {
        bytes_written_ += buffer_size;
        return true;
    }
    return false;
}

bool FileTransformer::SkipBytes(uint64_t skip_size)
{
    bool success = util::platform::FileSeek(input_file_, skip_size, util::platform::FileSeekCurrent);

    if (success)
    {
        // These technically count as bytes read/processed.
        bytes_read_ += skip_size;
    }

    return success;
}

bool FileTransformer::CopyBytes(uint64_t copy_size)
{
    GFXRECON_CHECK_CONVERSION_DATA_LOSS(size_t, copy_size);
    if (ReadParameterBuffer(static_cast<size_t>(copy_size)))
    {
        if (WriteBytes(parameter_buffer_.data(), static_cast<size_t>(copy_size)))
        {
            return true;
        }
    }

    return false;
}

void FileTransformer::HandleBlockReadError(Error error_code, const char* error_message)
{
    // Report incomplete block at end of file as a warning, other I/O errors as an error.
    if (feof(input_file_) && !ferror(input_file_))
    {
        GFXRECON_LOG_WARNING("Incomplete block at end of file");
    }
    else
    {
        GFXRECON_LOG_ERROR("%s", error_message);
        error_state_ = error_code;
    }
}

void FileTransformer::HandleBlockWriteError(Error error_code, const char* error_message)
{
    GFXRECON_LOG_ERROR("%s", error_message);
    error_state_ = error_code;
}

void FileTransformer::HandleBlockCopyError(Error error_code, const char* error_message)
{
    if (ferror(output_file_))
    {
        HandleBlockWriteError(error_code, error_message);
    }
    else
    {
        HandleBlockReadError(error_code, error_message);
    }
}

bool FileTransformer::CreateCompressor(format::CompressionType type, std::unique_ptr<util::Compressor>* compressor)
{
    assert(compressor != nullptr);

    if (type != format::CompressionType::kNone)
    {
        (*compressor) = std::unique_ptr<util::Compressor>(format::CreateCompressor(type));

        if ((*compressor) == nullptr)
        {
            GFXRECON_LOG_ERROR("Failed to initialize file compression module (type = %u); processing of "
                               "compressed data will not be possible",
                               type);
            error_state_ = kErrorUnsupportedCompressionType;
            return false;
        }
    }

    return true;
}

bool FileTransformer::WriteFileHeader(const format::FileHeader&                  header,
                                      const std::vector<format::FileOptionPair>& options)
{
    bool success = WriteBytes(&header, sizeof(header));
    success      = success && WriteBytes(options.data(), options.size() * sizeof(format::FileOptionPair));

    if (!success)
    {
        GFXRECON_LOG_ERROR("Failed to write file header");
        error_state_ = kErrorWritingFileHeader;
    }

    return success;
}

bool FileTransformer::ProcessFunctionCall(const format::FunctionCallHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write function call block header");
        return false;
    }

    if (!CopyBytes(header.block_header.size + sizeof(header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy function call block data");
        return false;
    }

    return true;
}

bool FileTransformer::ProcessMethodCall(const format::MethodCallHeader& header, uint64_t block_index)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write method call block header");
        return false;
    }

    if (!CopyBytes(header.block_header.size + sizeof(header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy method call block data");
        return false;
    }

    return true;
}

bool FileTransformer::ProcessMetaData(const format::MetaDataHeader& meta_header)
{
    auto meta_data_id = format::arm::MetaDataType::GetVersionedMetaDataId(file_header_, meta_header.meta_data_id);
    format::MetaDataType meta_data_type = format::GetMetaDataType(meta_data_id);

    switch (meta_data_type)
    {
        case format::MetaDataType::kDisplayMessageCommand:
        {
            format::DisplayMessageCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));

            if (success)
            {
                return ProcessDisplayMessageCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kFillMemoryCommand:
        {
            format::FillMemoryCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.memory_id, sizeof(header.memory_id));
            success      = success && ReadBytes(&header.memory_offset, sizeof(header.memory_offset));
            success      = success && ReadBytes(&header.memory_size, sizeof(header.memory_size));

            if (success)
            {
                return ProcessFillMemoryCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kResizeWindowCommand:
        {
            format::ResizeWindowCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.surface_id, sizeof(header.surface_id));
            success      = success && ReadBytes(&header.width, sizeof(header.width));
            success      = success && ReadBytes(&header.height, sizeof(header.height));

            if (success)
            {
                return ProcessResizeWindowCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kSetSwapchainImageStateCommand:
        {
            format::SetSwapchainImageStateCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.swapchain_id, sizeof(header.swapchain_id));
            success      = success && ReadBytes(&header.last_presented_image, sizeof(header.last_presented_image));
            success      = success && ReadBytes(&header.image_info_count, sizeof(header.image_info_count));

            if (success)
            {
                return ProcessSetSwapchainImageStateCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kBeginResourceInitCommand:
        {
            format::BeginResourceInitCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.max_resource_size, sizeof(header.max_resource_size));
            success      = success && ReadBytes(&header.max_copy_size, sizeof(header.max_copy_size));

            if (success)
            {
                return ProcessBeginResourceInitCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kEndResourceInitCommand:
        {
            format::EndResourceInitCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));

            if (success)
            {
                return ProcessEndResourceInitCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kInitBufferCommand:
        {
            format::InitBufferCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.buffer_id, sizeof(header.buffer_id));
            success      = success && ReadBytes(&header.data_size, sizeof(header.data_size));

            if (success)
            {
                return ProcessInitBufferCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kInitImageCommand:
        {
            format::InitImageCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.image_id, sizeof(header.image_id));
            success      = success && ReadBytes(&header.data_size, sizeof(header.data_size));
            success      = success && ReadBytes(&header.aspect, sizeof(header.aspect));
            success      = success && ReadBytes(&header.layout, sizeof(header.layout));
            success      = success && ReadBytes(&header.level_count, sizeof(header.level_count));

            if (success)
            {
                return ProcessInitImageCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kCreateHardwareBufferCommand_deprecated:
        {
            format::CreateHardwareBufferCommandHeader header;
            header.meta_header.block_header.size = meta_header.block_header.size + sizeof(header) -
                                                   sizeof(format::CreateHardwareBufferCommandHeader_deprecated);
            header.meta_header.block_header.type = meta_header.block_header.type;
            header.meta_header.meta_data_id      = format::MakeMetaDataId(
                format::GetMetaDataApi(meta_header.meta_data_id), format::MetaDataType::kCreateHardwareBufferCommand);
            header.device_id = format::kNullHandleId;

            uint32_t usage = 0;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.memory_id, sizeof(header.memory_id));
            success      = success && ReadBytes(&header.buffer_id, sizeof(header.buffer_id));
            success      = success && ReadBytes(&header.format, sizeof(header.format));
            success      = success && ReadBytes(&header.width, sizeof(header.width));
            success      = success && ReadBytes(&header.height, sizeof(header.height));
            success      = success && ReadBytes(&header.stride, sizeof(header.stride));
            success      = success && ReadBytes(&usage, sizeof(usage));
            success      = success && ReadBytes(&header.layers, sizeof(header.layers));
            success      = success && ReadBytes(&header.planes, sizeof(header.planes));

            header.usage = usage;

            if (success)
            {
                return ProcessCreateHardwareBufferCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kDestroyHardwareBufferCommand:
        {
            format::DestroyHardwareBufferCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.buffer_id, sizeof(header.buffer_id));

            if (success)
            {
                return ProcessDestroyHardwareBufferCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kSetDevicePropertiesCommand:
        {
            format::SetDevicePropertiesCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.physical_device_id, sizeof(header.physical_device_id));
            success      = success && ReadBytes(&header.api_version, sizeof(header.api_version));
            success      = success && ReadBytes(&header.driver_version, sizeof(header.driver_version));
            success      = success && ReadBytes(&header.vendor_id, sizeof(header.vendor_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.device_type, sizeof(header.device_type));
            success      = success && ReadBytes(&header.pipeline_cache_uuid, sizeof(header.pipeline_cache_uuid));
            success      = success && ReadBytes(&header.device_name_len, sizeof(header.device_name_len));

            if (success)
            {
                return ProcessSetDevicePropertiesCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kSetDeviceMemoryPropertiesCommand:
        {
            format::SetDeviceMemoryPropertiesCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.physical_device_id, sizeof(header.physical_device_id));
            success      = success && ReadBytes(&header.memory_type_count, sizeof(header.memory_type_count));
            success      = success && ReadBytes(&header.memory_heap_count, sizeof(header.memory_heap_count));

            if (success)
            {
                return ProcessSetDeviceMemoryPropertiesCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kResizeWindowCommand2:
        {
            format::ResizeWindowCommand2 header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.surface_id, sizeof(header.surface_id));
            success      = success && ReadBytes(&header.width, sizeof(header.width));
            success      = success && ReadBytes(&header.height, sizeof(header.height));
            success      = success && ReadBytes(&header.pre_transform, sizeof(header.pre_transform));

            if (success)
            {
                return ProcessResizeWindowCommand2(header);
            }

            return false;
        }
        case format::MetaDataType::kSetOpaqueAddressCommand:
        {
            format::SetOpaqueAddressCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.object_id, sizeof(header.object_id));
            success      = success && ReadBytes(&header.address, sizeof(header.address));

            if (success)
            {
                return ProcessSetOpaqueAddressCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kSetRayTracingShaderGroupHandlesCommand:
        {
            format::SetRayTracingShaderGroupHandlesCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.pipeline_id, sizeof(header.pipeline_id));
            success      = success && ReadBytes(&header.data_size, sizeof(header.data_size));

            if (success)
            {
                return ProcessSetRayTracingShaderGroupHandlesCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kCreateHeapAllocationCommand:
        {
            format::CreateHeapAllocationCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.allocation_id, sizeof(header.allocation_id));
            success      = success && ReadBytes(&header.allocation_size, sizeof(header.allocation_size));

            if (success)
            {
                return ProcessCreateHeapAllocationCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kInitSubresourceCommand:
        {
            format::InitSubresourceCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.resource_id, sizeof(header.resource_id));
            success      = success && ReadBytes(&header.subresource, sizeof(header.subresource));
            success      = success && ReadBytes(&header.initial_state, sizeof(header.initial_state));
            success      = success && ReadBytes(&header.resource_state, sizeof(header.resource_state));
            success      = success && ReadBytes(&header.barrier_flags, sizeof(header.barrier_flags));
            success      = success && ReadBytes(&header.data_size, sizeof(header.data_size));

            if (success)
            {
                return ProcessInitSubresourceCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kExeFileInfoCommand:
        {
            format::ExeFileInfoBlock header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.info_record, sizeof(header.info_record));

            if (success)
            {
                return ProcessExeFileInfoCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kInitDx12AccelerationStructureCommand:
        {
            format::InitDx12AccelerationStructureCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.dest_acceleration_structure_data,
                                           sizeof(header.dest_acceleration_structure_data));
            success      = success && ReadBytes(&header.copy_source_gpu_va, sizeof(header.copy_source_gpu_va));
            success      = success && ReadBytes(&header.copy_mode, sizeof(header.copy_mode));
            success      = success && ReadBytes(&header.inputs_type, sizeof(header.inputs_type));
            success      = success && ReadBytes(&header.inputs_flags, sizeof(header.inputs_flags));
            success = success && ReadBytes(&header.inputs_num_instance_descs, sizeof(header.inputs_num_instance_descs));
            success = success && ReadBytes(&header.inputs_num_geometry_descs, sizeof(header.inputs_num_geometry_descs));
            success = success && ReadBytes(&header.inputs_data_size, sizeof(header.inputs_data_size));

            if (success)
            {
                return ProcessInitDx12AccelerationStructureCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kFillMemoryResourceValueCommand:
        {
            format::FillMemoryResourceValueCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.resource_value_count, sizeof(header.resource_value_count));

            if (success)
            {
                return ProcessFillMemoryResourceValueCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kDxgiAdapterInfoCommand:
        {
            format::DxgiAdapterInfoCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.adapter_desc, sizeof(header.adapter_desc));

            if (success)
            {
                return ProcessDxgiAdapterInfoCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kDriverInfoCommand:
        {
            format::DriverInfoBlock header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.driver_record, sizeof(header.driver_record));

            if (success)
            {
                return ProcessDriverInfoCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kCreateHardwareBufferCommand_deprecated2:
        {
            format::CreateHardwareBufferCommandHeader header;
            header.meta_header.block_header.size = meta_header.block_header.size + sizeof(header) -
                                                   sizeof(format::CreateHardwareBufferCommandHeader_deprecated2);
            header.meta_header.block_header.type = meta_header.block_header.type;
            header.meta_header.meta_data_id      = format::MakeMetaDataId(
                format::GetMetaDataApi(meta_header.meta_data_id), format::MetaDataType::kCreateHardwareBufferCommand);
            header.device_id = format::kNullHandleId;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.memory_id, sizeof(header.memory_id));
            success      = success && ReadBytes(&header.buffer_id, sizeof(header.buffer_id));
            success      = success && ReadBytes(&header.format, sizeof(header.format));
            success      = success && ReadBytes(&header.width, sizeof(header.width));
            success      = success && ReadBytes(&header.height, sizeof(header.height));
            success      = success && ReadBytes(&header.stride, sizeof(header.stride));
            success      = success && ReadBytes(&header.usage, sizeof(header.usage));
            success      = success && ReadBytes(&header.layers, sizeof(header.layers));
            success      = success && ReadBytes(&header.planes, sizeof(header.planes));

            if (success)
            {
                return ProcessCreateHardwareBufferCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kDx12RuntimeInfoCommand:
        {
            format::Dx12RuntimeInfoCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.runtime_info, sizeof(header.runtime_info));

            if (success)
            {
                return ProcessDx12RuntimeInfoCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kParentToChildDependency:
        {
            format::ParentToChildDependencyHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.dependency_type, sizeof(header.dependency_type));
            success      = success && ReadBytes(&header.parent_id, sizeof(header.parent_id));
            success      = success && ReadBytes(&header.child_count, sizeof(header.child_count));

            if (success)
            {
                return ProcessParentToChildDependency(header);
            }

            return false;
        }
        case format::MetaDataType::kVulkanBuildAccelerationStructuresCommand:
        {
            format::VulkanMetaBuildAccelerationStructuresHeader header;
            header.meta_header = meta_header;
            return ProcessVulkanBuildAccelerationStructuresCommand(header);
        }
        case format::MetaDataType::kVulkanCopyAccelerationStructuresCommand:
        {
            format::VulkanCopyAccelerationStructuresCommandHeader header;
            header.meta_header = meta_header;
            return ProcessVulkanCopyAccelerationStructuresCommand(header);
        }
        case format::MetaDataType::kVulkanWriteAccelerationStructuresPropertiesCommand:
        {
            format::VulkanWriteAccelerationStructuresPropertiesCommandHeader header;
            header.meta_header = meta_header;
            return ProcessVulkanWriteAccelerationStructuresPropertiesCommand(header);
        }
        case format::MetaDataType::kFixDeviceAddressCommand:
        {
            format::FixDeviceAddressCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.relation_id, sizeof(header.relation_id));
            success      = success && ReadBytes(&header.num_of_locations, sizeof(header.num_of_locations));

            if (success)
            {
                return ProcessFixDeviceAddressCommand(header);
            }

            return false;
        }
        case format::arm::MetaDataType::kFixDescriptorDataCommand:
        {
            format::FixDescriptorDataCommandHeader header;
            header.meta_header = meta_header;
            bool success       = ReadBytes(&header.memory_id, sizeof(header.memory_id));
            success            = success && ReadBytes(&header.num_of_locations, sizeof(header.num_of_locations));

            if (success)
            {
                return ProcessFixDescriptorDataCommand(header);
            }
            return false;
        }
        case format::arm::MetaDataType::kFixShadowMemoryCommand:
        {
            format::FixShadowMemoryCommand header;
            header.meta_header = meta_header;
            bool success       = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success            = success && ReadBytes(&header.memory_id, sizeof(header.memory_id));
            success            = success && ReadBytes(&header.map_memory, sizeof(header.map_memory));
            success            = success && ReadBytes(&header.shadow_memory, sizeof(header.shadow_memory));
            if (success)
            {
                return ProcessFixShadowMemoryCommand(header);
            }
            return false;
        }
        case format::MetaDataType::kSetEnvironmentVariablesCommand:
        {
            format::SetEnvironmentVariablesCommand header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.string_length, sizeof(header.string_length));

            if (success)
            {
                return ProcessSetEnvironmentVariablesCommand(header);
            }

            return false;
        }
        case format::MetaDataType::kExecuteBlocksFromFile:
        {
            format::ExecuteBlocksFromFile header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.n_blocks, sizeof(header.n_blocks));
            success      = success && ReadBytes(&header.offset, sizeof(header.offset));
            success      = success && ReadBytes(&header.filename_length, sizeof(header.filename_length));

            if (success)
            {
                return ProcessExecuteBlocksFromFile(header);
            }

            return false;
        }

        case format::MetaDataType::kCreateHardwareBufferCommand:
        {
            format::CreateHardwareBufferCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success      = success && ReadBytes(&header.memory_id, sizeof(header.memory_id));
            success      = success && ReadBytes(&header.buffer_id, sizeof(header.buffer_id));
            success      = success && ReadBytes(&header.format, sizeof(header.format));
            success      = success && ReadBytes(&header.width, sizeof(header.width));
            success      = success && ReadBytes(&header.height, sizeof(header.height));
            success      = success && ReadBytes(&header.stride, sizeof(header.stride));
            success      = success && ReadBytes(&header.usage, sizeof(header.usage));
            success      = success && ReadBytes(&header.layers, sizeof(header.layers));
            success      = success && ReadBytes(&header.planes, sizeof(header.planes));

            if (success)
            {
                return ProcessCreateHardwareBufferCommand(header);
            }

            return false;
        }
        case format::arm::MetaDataType::kFixShaderGroupHandleCommand:
        {
            format::FixShaderGroupHandleCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.relation_id, sizeof(header.relation_id));
            success      = success && ReadBytes(&header.num_of_locations, sizeof(header.num_of_locations));

            if (success)
            {
                return ProcessFixShaderGroupHandleCommand(header);
            }

            return false;
        }
        case format::arm::MetaDataType::kInitTensorCommand:
        {
            format::InitTensorCommandHeader header;
            header.meta_header = meta_header;
            bool success       = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success            = success && ReadBytes(&header.device_id, sizeof(header.device_id));
            success            = success && ReadBytes(&header.tensor_id, sizeof(header.tensor_id));
            success            = success && ReadBytes(&header.data_size, sizeof(header.data_size));
            if (success)
            {
                return ProcessInitTensorCommand(header);
            }
            return false;
        }
        case format::arm::MetaDataType::kFillMemoryResourceAddressCommand:
        {
            format::FillMemoryResourceAddressCommandHeader header;
            header.meta_header = meta_header;

            bool success = ReadBytes(&header.thread_id, sizeof(header.thread_id));
            success      = success && ReadBytes(&header.resource_address_count, sizeof(header.resource_address_count));

            if (success)
            {
                return ProcessFillMemoryResourceAddressCommand(header);
            }

            return false;
        }
        default:
        {
            GFXRECON_LOG_ERROR("Unrecognized meta-data type %u", meta_data_type);
            return false;
        }
    }
}

bool FileTransformer::ProcessMarker(const format::Marker& marker)
{
    if (marker.header.type == format::kStateMarkerBlock)
    {
        if (marker.marker_type == format::kBeginMarker)
        {
            loading_state_ = true;
        }
        else if (marker.marker_type == format::kEndMarker)
        {
            loading_state_ = false;
        }
    }

    if (!WriteBytes(&marker, sizeof(marker)))
    {
        HandleBlockWriteError(kErrorWritingBlockData, "Failed to write frame marker data");
        return false;
    }

    return true;
}

bool FileTransformer::ProcessAnnotation(const format::AnnotationHeader& header,
                                        const std::string&              label,
                                        const std::string&              data)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write annotation block header");
        return false;
    }

    if (!WriteBytes(label.data(), label.size()))
    {
        HandleBlockWriteError(kErrorWritingBlockData, "Failed to write annotation block label");
        return false;
    }

    if (!WriteBytes(data.data(), data.size()))
    {
        HandleBlockWriteError(kErrorWritingBlockData, "Failed to write annotation block data");
        return false;
    }

    return true;
}

bool FileTransformer::ProcessDisplayMessageCommand(const format::DisplayMessageCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessFillMemoryCommand(const format::FillMemoryCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessResizeWindowCommand(const format::ResizeWindowCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessSetSwapchainImageStateCommand(const format::SetSwapchainImageStateCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessBeginResourceInitCommand(const format::BeginResourceInitCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessEndResourceInitCommand(const format::EndResourceInitCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessInitBufferCommand(const format::InitBufferCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessInitImageCommand(const format::InitImageCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessDestroyHardwareBufferCommand(const format::DestroyHardwareBufferCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessSetDevicePropertiesCommand(const format::SetDevicePropertiesCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessSetDeviceMemoryPropertiesCommand(const format::SetDeviceMemoryPropertiesCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessResizeWindowCommand2(const format::ResizeWindowCommand2& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessSetOpaqueAddressCommand(const format::SetOpaqueAddressCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessSetRayTracingShaderGroupHandlesCommand(
    const format::SetRayTracingShaderGroupHandlesCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessCreateHeapAllocationCommand(const format::CreateHeapAllocationCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessInitSubresourceCommand(const format::InitSubresourceCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessExeFileInfoCommand(const format::ExeFileInfoBlock& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessInitDx12AccelerationStructureCommand(
    const format::InitDx12AccelerationStructureCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessFillMemoryResourceValueCommand(const format::FillMemoryResourceValueCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessDxgiAdapterInfoCommand(const format::DxgiAdapterInfoCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessDriverInfoCommand(const format::DriverInfoBlock& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessCreateHardwareBufferCommand(const format::CreateHardwareBufferCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessDx12RuntimeInfoCommand(const format::Dx12RuntimeInfoCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessParentToChildDependency(const format::ParentToChildDependencyHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessVulkanBuildAccelerationStructuresCommand(
    const format::VulkanMetaBuildAccelerationStructuresHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessVulkanCopyAccelerationStructuresCommand(
    const format::VulkanCopyAccelerationStructuresCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessVulkanWriteAccelerationStructuresPropertiesCommand(
    const format::VulkanWriteAccelerationStructuresPropertiesCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessFixDescriptorDataCommand(const format::FixDescriptorDataCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessFixShadowMemoryCommand(const format::FixShadowMemoryCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessSetEnvironmentVariablesCommand(const format::SetEnvironmentVariablesCommand& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessExecuteBlocksFromFile(const format::ExecuteBlocksFromFile& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}
bool FileTransformer::ProcessFixShaderGroupHandleCommand(const format::FixShaderGroupHandleCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}

bool FileTransformer::ProcessInitTensorCommand(const format::InitTensorCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }
    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }
    return true;
}

bool FileTransformer::ProcessFillMemoryResourceAddressCommand(
    const format::FillMemoryResourceAddressCommandHeader& header)
{
    if (!WriteBytes(&header, sizeof(header)))
    {
        HandleBlockWriteError(kErrorWritingBlockHeader, "Failed to write meta-data block header");
        return false;
    }

    if (!CopyBytes(header.meta_header.block_header.size + sizeof(header.meta_header.block_header) - sizeof(header)))
    {
        HandleBlockCopyError(kErrorCopyingBlockData, "Failed to copy meta-data block data");
        return false;
    }

    return true;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
