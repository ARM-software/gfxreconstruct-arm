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

void FileOptimizer::SetRemovedThreads(const std::unordered_set<format::ThreadId>& removed_threads_ids)
{
    removed_threads_ids_ = removed_threads_ids;
}

uint64_t FileOptimizer::GetUnreferencedBlocksSize()
{
    return unreferenced_blocks_.size();
}

bool FileOptimizer::ProcessFunctionCall(const format::FunctionCallHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) == removed_threads_ids_.end())
    {
        return FileTransformer::ProcessFunctionCall(header);
    }
    else
    {
        // Total number of bytes remaining to be read for the current block.
        const uint64_t unread_bytes = header.block_header.size - (sizeof(header) - sizeof(header.block_header));

        if (!SkipBytes(unread_bytes))
        {
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip function call block data");
            return false;
        }
    }

    return true;
}

bool FileOptimizer::ProcessMethodCall(const format::MethodCallHeader& header, uint64_t block_index)
{
    bool ignore_call = (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end());

    if (header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device_CreateGraphicsPipelineState ||
        header.api_call_id == format::ApiCallId::ApiCall_ID3D12Device_CreateComputePipelineState ||
        header.api_call_id == format::ApiCallId::ApiCall_ID3D12PipelineLibrary_StorePipeline)
    {
        // If the buffer is in the unused list, omit the call block from the file.
        if (unreferenced_blocks_.find(block_index) != unreferenced_blocks_.end())
        {
            unreferenced_blocks_.erase(block_index);
            ignore_call = true;
        }
    }

    if (ignore_call)
    {
        // Total number of bytes remaining to be read for the current block.
        const uint64_t unread_bytes = header.block_header.size - sizeof(header) + sizeof(header.block_header);

        if (!SkipBytes(unread_bytes))
        {
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip method call block data");
            return false;
        }

        return true;
    }
    else
    {
        return FileTransformer::ProcessMethodCall(header, block_index);
    }
}

bool FileOptimizer::ProcessDisplayMessageCommand(const format::DisplayMessageCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessDisplayMessageCommand(header);
}

bool FileOptimizer::ProcessFillMemoryCommand(const format::FillMemoryCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessFillMemoryCommand(header);
}

bool FileOptimizer::ProcessResizeWindowCommand(const format::ResizeWindowCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessResizeWindowCommand(header);
}

bool FileOptimizer::ProcessSetSwapchainImageStateCommand(const format::SetSwapchainImageStateCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessSetSwapchainImageStateCommand(header);
}

bool FileOptimizer::ProcessBeginResourceInitCommand(const format::BeginResourceInitCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessBeginResourceInitCommand(header);
}

bool FileOptimizer::ProcessEndResourceInitCommand(const format::EndResourceInitCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessEndResourceInitCommand(header);
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
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip init bimage data meta-data block data");
            return false;
        }
    }
    else if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
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
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip init bimage data meta-data block data");
            return false;
        }
    }
    else if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    else
    {
        return FileTransformer::ProcessInitImageCommand(header);
    }

    return true;
}

bool FileOptimizer::ProcessDestroyHardwareBufferCommand(const format::DestroyHardwareBufferCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessDestroyHardwareBufferCommand(header);
}

bool FileOptimizer::ProcessSetDevicePropertiesCommand(const format::SetDevicePropertiesCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessSetDevicePropertiesCommand(header);
}

bool FileOptimizer::ProcessSetDeviceMemoryPropertiesCommand(const format::SetDeviceMemoryPropertiesCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessSetDeviceMemoryPropertiesCommand(header);
}

bool FileOptimizer::ProcessResizeWindowCommand2(const format::ResizeWindowCommand2& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessResizeWindowCommand2(header);
}

bool FileOptimizer::ProcessSetOpaqueAddressCommand(const format::SetOpaqueAddressCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessSetOpaqueAddressCommand(header);
}

bool FileOptimizer::ProcessSetRayTracingShaderGroupHandlesCommand(
    const format::SetRayTracingShaderGroupHandlesCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessSetRayTracingShaderGroupHandlesCommand(header);
}

bool FileOptimizer::ProcessCreateHeapAllocationCommand(const format::CreateHeapAllocationCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessCreateHeapAllocationCommand(header);
}

bool FileOptimizer::ProcessInitSubresourceCommand(const format::InitSubresourceCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessInitSubresourceCommand(header);
}

bool FileOptimizer::ProcessExeFileInfoCommand(const format::ExeFileInfoBlock& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessExeFileInfoCommand(header);
}

bool FileOptimizer::ProcessInitDx12AccelerationStructureCommand(
    const format::InitDx12AccelerationStructureCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessInitDx12AccelerationStructureCommand(header);
}

bool FileOptimizer::ProcessFillMemoryResourceValueCommand(const format::FillMemoryResourceValueCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessFillMemoryResourceValueCommand(header);
}

bool FileOptimizer::ProcessDxgiAdapterInfoCommand(const format::DxgiAdapterInfoCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessDxgiAdapterInfoCommand(header);
}

bool FileOptimizer::ProcessDriverInfoCommand(const format::DriverInfoBlock& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessDriverInfoCommand(header);
}

bool FileOptimizer::ProcessCreateHardwareBufferCommand(const format::CreateHardwareBufferCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessCreateHardwareBufferCommand(header);
}

bool FileOptimizer::ProcessDx12RuntimeInfoCommand(const format::Dx12RuntimeInfoCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessDx12RuntimeInfoCommand(header);
}

bool FileOptimizer::ProcessParentToChildDependency(const format::ParentToChildDependencyHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessParentToChildDependency(header);
}

bool FileOptimizer::ProcessSetEnvironmentVariablesCommand(const format::SetEnvironmentVariablesCommand& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessSetEnvironmentVariablesCommand(header);
}

bool FileOptimizer::ProcessExecuteBlocksFromFile(const format::ExecuteBlocksFromFile& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessExecuteBlocksFromFile(header);
}

bool FileOptimizer::RemoveThreadBlock(const format::BlockHeader& header, size_t size_read)
{
    const uint64_t unread_bytes = header.size - (size_read - sizeof(header));

    if (!SkipBytes(unread_bytes))
    {
        HandleBlockReadError(kErrorSeekingFile, "Failed to skip thread-removed block");
        return false;
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
        const char*              label        = format::kAnnotationLabelRemovedResource;
        const std::string        data         = "Removed tensor " + std::to_string(header.tensor_id);
        const size_t             label_length = util::platform::StringLength(label);
        const size_t             data_length  = data.length();
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
            HandleBlockReadError(kErrorSeekingFile, "Failed to skip init bimage data meta-data block data");
            return false;
        }
    }
    else if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    else
    {
        return FileTransformer::ProcessInitTensorCommand(header);
    }
    return true;
}

bool FileOptimizer::ProcessFillMemoryResourceAddressCommand(
    const format::FillMemoryResourceAddressCommandHeader& header)
{
    if (removed_threads_ids_.find(header.thread_id) != removed_threads_ids_.end())
    {
        return RemoveThreadBlock(header.meta_header.block_header, sizeof(header));
    }
    return FileTransformer::ProcessFillMemoryResourceAddressCommand(header);
}

GFXRECON_END_NAMESPACE(gfxrecon)
