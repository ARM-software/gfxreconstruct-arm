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

#include "file_optimizer.h"

#include "format/format.h"
#include "format/format_util.h"
#include "util/logging.h"
#include "util/platform.h"

#include <cassert>
#include <string>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

FileOptimizer::FileOptimizer(const std::unordered_set<format::HandleId>& unreferenced_ids) :
    unreferenced_ids_(unreferenced_ids)
{}

FileOptimizer::FileOptimizer(std::unordered_set<format::HandleId>&& unreferenced_ids) :
    unreferenced_ids_(std::move(unreferenced_ids))
{}

void FileOptimizer::SetUnreferencedBlocks(const std::unordered_set<uint64_t>& unreferenced_blocks)
{
    unreferenced_blocks_ = unreferenced_blocks;
}

uint64_t FileOptimizer::GetUnreferencedBlocksSize()
{
    return unreferenced_blocks_.size();
}

bool FileOptimizer::ProcessMethodCall(const format::MethodCallHeader& header, uint64_t block_index)
{
    if (header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device_CreateGraphicsPipelineState ||
        header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device_CreateComputePipelineState ||
        header.api_call_id == format::ApiCallId::ApiCall_ID3D12PipelineLibrary_StorePipeline)
    {
        // If the buffer is in the unused list, omit the call block from the file.
        if (unreferenced_blocks_.find(block_index) != unreferenced_blocks_.end())
        {
            unreferenced_blocks_.erase(block_index);

            // Total number of bytes remaining to be read for the current block.
            const uint64_t unread_bytes = header.block_header.size - sizeof(header) + sizeof(header.block_header);

            if (!SkipBytes(unread_bytes))
            {
                HandleBlockReadError(kErrorSeekingFile, "Failed to skip method call block data");
                return false;
            }

            return true;
        }
    }

    return FileTransformer::ProcessMethodCall(header, block_index);
}

bool FileOptimizer::ProcessInitBufferCommand(const format::InitBufferCommandHeader& header)
{
    // If the buffer is in the unused list, omit its initialization data from the file.
    if (unreferenced_ids_.find(header.buffer_id) != unreferenced_ids_.end())
    {
        // In its place insert a dummy annotation meta command. This should keep the block index when
        // replaying an optimized trimmed capture in in alignment with the block index calculated
        // at capture time
        const char*       label = format::kAnnotationLabelRemovedResource;
        const std::string data  = "Removed buffer " + std::to_string(header.buffer_id);

        const size_t label_length = util::platform::StringLength(label);
        const size_t data_length  = data.length();

        format::AnnotationHeader annotation;
        annotation.block_header.size = format::GetAnnotationBlockBaseSize() + label_length + data_length;
        annotation.block_header.type = format::BlockType::kAnnotation;
        annotation.annotation_type   = format::kText;
        annotation.label_length      = static_cast<uint32_t>(label_length);
        annotation.data_length       = static_cast<uint64_t>(data.length());

        if (!WriteBytes(&annotation, sizeof(annotation)) || !WriteBytes(label, label_length) ||
            !WriteBytes(data.c_str(), data_length))
        {
            HandleBlockWriteError(kErrorReadingBlockHeader, "Failed to write annotation meta-data block");
            return false;
        }

        // Total number of bytes remaining to be read for the current block.
        const uint64_t unread_bytes =
            header.meta_header.block_header.size - (sizeof(header) - sizeof(header.meta_header.block_header));

        if (!SkipBytes(unread_bytes))
        {
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip init buffer data meta-data block data");
            return false;
        }
    }
    else
    {
        return FileTransformer::ProcessInitBufferCommand(header);
    }

    return true;
}

bool FileOptimizer::ProcessInitImageCommand(const format::InitImageCommandHeader& header)
{
    // If the image is in the unused list, omit its initialization data from the file.
    if (unreferenced_ids_.find(header.image_id) != unreferenced_ids_.end())
    {
        // In its place insert a dummy annotation meta command. This should keep the block index when
        // replaying an optimized trimmed capture in in alignment with the block index calculated
        // at capture time
        const char*       label = format::kAnnotationLabelRemovedResource;
        const std::string data  = "Removed subresource from image " + std::to_string(header.image_id);

        const size_t label_length = util::platform::StringLength(label);
        const size_t data_length  = data.length();

        format::AnnotationHeader annotation;
        annotation.block_header.size = format::GetAnnotationBlockBaseSize() + label_length + data_length;
        annotation.block_header.type = format::BlockType::kAnnotation;
        annotation.annotation_type   = format::kText;
        annotation.label_length      = static_cast<uint32_t>(label_length);
        annotation.data_length       = static_cast<uint64_t>(data.length());

        if (!WriteBytes(&annotation, sizeof(annotation)) || !WriteBytes(label, label_length) ||
            !WriteBytes(data.c_str(), data_length))
        {
            HandleBlockWriteError(kErrorReadingBlockHeader, "Failed to write annotation meta-data block");
            return false;
        }

        // Total number of bytes remaining to be read for the current block.
        const uint64_t unread_bytes =
            header.meta_header.block_header.size - (sizeof(header) - sizeof(header.meta_header.block_header));

        if (!SkipBytes(unread_bytes))
        {
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip init image data meta-data block data");
            return false;
        }
    }
    else
    {
        return FileTransformer::ProcessInitImageCommand(header);
    }

    return true;
}

bool FileOptimizer::ProcessInitTensorCommand(const format::InitTensorCommandHeader& header)
{
    // If the tensor is in the unused list, omit its initialization data from the file.
    if (unreferenced_ids_.find(header.tensor_id) != unreferenced_ids_.end())
    {
        // In its place insert a dummy annotation meta command. This should keep the block index when
        // replaying an optimized trimmed capture in in alignment with the block index calculated
        // at capture time
        const char*       label = format::kAnnotationLabelRemovedResource;
        const std::string data  = "Removed tensor " + std::to_string(header.tensor_id);

        const size_t label_length = util::platform::StringLength(label);
        const size_t data_length  = data.length();

        format::AnnotationHeader annotation;
        annotation.block_header.size = format::GetAnnotationBlockBaseSize() + label_length + data_length;
        annotation.block_header.type = format::BlockType::kAnnotation;
        annotation.annotation_type   = format::kText;
        annotation.label_length      = static_cast<uint32_t>(label_length);
        annotation.data_length       = static_cast<uint64_t>(data.length());

        if (!WriteBytes(&annotation, sizeof(annotation)) || !WriteBytes(label, label_length) ||
            !WriteBytes(data.c_str(), data_length))
        {
            HandleBlockWriteError(kErrorReadingBlockHeader, "Failed to write annotation meta-data block");
            return false;
        }

        // Total number of bytes remaining to be read for the current block.
        const uint64_t unread_bytes =
            header.meta_header.block_header.size - (sizeof(header) - sizeof(header.meta_header.block_header));

        if (!SkipBytes(unread_bytes))
        {
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip init tensor data meta-data block data");
            return false;
        }
    }
    else
    {
        return FileTransformer::ProcessInitTensorCommand(header);
    }

    return true;
}

GFXRECON_END_NAMESPACE(gfxrecon)
