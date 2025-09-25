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

#ifndef GFXRECON_DECODE_FILE_TRANSFORMER_H
#define GFXRECON_DECODE_FILE_TRANSFORMER_H

#include "format/format.h"
#include "util/defines.h"
#include "util/compressor.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class FileTransformer
{
  public:
    enum Error : int32_t
    {
        kErrorNone                         = 0,
        kErrorInvalidFileDescriptor        = -1,
        kErrorOpeningFile                  = -2,
        kErrorReadingFile                  = -3,
        kErrorReadingFileHeader            = -4,
        kErrorReadingBlockHeader           = -5,
        kErrorReadingCompressedBlockHeader = -6,
        kErrorReadingBlockData             = -7,
        kErrorReadingCompressedBlockData   = -8,
        kErrorInvalidFourCC                = -9,
        kErrorUnsupportedCompressionType   = -10,
        kErrorSeekingFile                  = -11,
        kErrorWritingFile                  = -12,
        kErrorWritingFileHeader            = -13,
        kErrorWritingBlockHeader           = -14,
        kErrorWritingCompressedBlockHeader = -15,
        kErrorWritingBlockData             = -16,
        kErrorWritingCompressedBlockData   = -17,
        kErrorCopyingBlockData             = -18,
        kErrorUnsupportedBlockType         = -19
    };

  public:
    FileTransformer();

    virtual ~FileTransformer();

    bool Initialize(const std::string& input_filename, const std::string& output_filename, const std::string& tool);

    bool Initialize(const std::string& input_filename, const std::string& output_filename);

    // Returns false if processing failed.  Use GetErrorState() to determine error condition for failure case.
    virtual bool Process();

    const std::vector<format::FileOptionPair>& GetFileOptions() const { return file_options_; }

    uint64_t GetNumBytesRead() const { return bytes_read_; }

    uint64_t GetNumBytesWritten() const { return bytes_written_; }

    Error GetErrorState() const { return error_state_; }

  protected:
    bool IsFileValid(FILE* fd) const { return ((fd != nullptr) && !feof(fd) && !ferror(fd)); }

    bool IsLoadingState() const { return loading_state_; }

    std::vector<uint8_t>& GetParameterBuffer() { return parameter_buffer_; }

    const std::vector<uint8_t>& GetParameterBuffer() const { return parameter_buffer_; }

    std::vector<uint8_t>& GetCompressedParameterBuffer() { return compressed_parameter_buffer_; }

    const std::vector<uint8_t>& GetCompressedParameterBuffer() const { return compressed_parameter_buffer_; }

    util::Compressor* GetCompressor() { return compressor_.get(); }

    const util::Compressor* GetCompressor() const { return compressor_.get(); }

    bool WriteBlockHeader(const format::BlockHeader& block_header);

    bool ReadParameterBuffer(size_t buffer_size);

    bool ReadCompressedParameterBuffer(size_t  compressed_buffer_size,
                                       size_t  expected_uncompressed_size,
                                       size_t* uncompressed_buffer_size);

    bool ReadBytes(void* buffer, size_t buffer_size);

    bool WriteBytes(const void* buffer, size_t buffer_size);

    bool SkipBytes(uint64_t skip_size);

    bool CopyBytes(uint64_t copy_size);

    void HandleBlockReadError(Error error_code, const char* error_message);

    void HandleBlockWriteError(Error error_code, const char* error_message);

    void HandleBlockCopyError(Error error_code, const char* error_message);

    bool CreateCompressor(format::CompressionType type, std::unique_ptr<util::Compressor>* compressor);

    virtual bool WriteFileHeader(const format::FileHeader& header, const std::vector<format::FileOptionPair>& options);

    virtual bool ProcessFunctionCall(const format::FunctionCallHeader& header);
    virtual bool ProcessMethodCall(const format::MethodCallHeader& header, uint64_t block_index = 0);
    virtual bool ProcessMetaData(const format::MetaDataHeader& meta_header);
    virtual bool ProcessMarker(const format::Marker& marker);
    virtual bool
    ProcessAnnotation(const format::AnnotationHeader& header, const std::string& label, const std::string& data);

    virtual bool ProcessDisplayMessageCommand(const format::DisplayMessageCommandHeader& header);
    virtual bool ProcessFillMemoryCommand(const format::FillMemoryCommandHeader& header);
    virtual bool ProcessResizeWindowCommand(const format::ResizeWindowCommand& header);
    virtual bool ProcessSetSwapchainImageStateCommand(const format::SetSwapchainImageStateCommandHeader& header);
    virtual bool ProcessBeginResourceInitCommand(const format::BeginResourceInitCommand& header);
    virtual bool ProcessEndResourceInitCommand(const format::EndResourceInitCommand& header);
    virtual bool ProcessInitBufferCommand(const format::InitBufferCommandHeader& header);
    virtual bool ProcessInitImageCommand(const format::InitImageCommandHeader& header);
    virtual bool ProcessDestroyHardwareBufferCommand(const format::DestroyHardwareBufferCommand& header);
    virtual bool ProcessSetDevicePropertiesCommand(const format::SetDevicePropertiesCommand& header);
    virtual bool ProcessSetDeviceMemoryPropertiesCommand(const format::SetDeviceMemoryPropertiesCommand& header);
    virtual bool ProcessResizeWindowCommand2(const format::ResizeWindowCommand2& header);
    virtual bool ProcessSetOpaqueAddressCommand(const format::SetOpaqueAddressCommand& header);
    virtual bool
    ProcessSetRayTracingShaderGroupHandlesCommand(const format::SetRayTracingShaderGroupHandlesCommandHeader& header);
    virtual bool ProcessCreateHeapAllocationCommand(const format::CreateHeapAllocationCommand& header);
    virtual bool ProcessInitSubresourceCommand(const format::InitSubresourceCommandHeader& header);
    virtual bool ProcessExeFileInfoCommand(const format::ExeFileInfoBlock& header);
    virtual bool
    ProcessInitDx12AccelerationStructureCommand(const format::InitDx12AccelerationStructureCommandHeader& header);
    virtual bool ProcessFillMemoryResourceValueCommand(const format::FillMemoryResourceValueCommandHeader& header);
    virtual bool ProcessDxgiAdapterInfoCommand(const format::DxgiAdapterInfoCommandHeader& header);
    virtual bool ProcessDriverInfoCommand(const format::DriverInfoBlock& header);
    virtual bool ProcessCreateHardwareBufferCommand(const format::CreateHardwareBufferCommandHeader& header);
    virtual bool ProcessDx12RuntimeInfoCommand(const format::Dx12RuntimeInfoCommandHeader& header);
    virtual bool ProcessParentToChildDependency(const format::ParentToChildDependencyHeader& header);
    virtual bool
    ProcessVulkanBuildAccelerationStructuresCommand(const format::VulkanMetaBuildAccelerationStructuresHeader& header);
    virtual bool
    ProcessVulkanCopyAccelerationStructuresCommand(const format::VulkanCopyAccelerationStructuresCommandHeader& header);
    virtual bool ProcessVulkanWriteAccelerationStructuresPropertiesCommand(
        const format::VulkanWriteAccelerationStructuresPropertiesCommandHeader& header);
    virtual bool ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader& header);
    virtual bool ProcessFixShadowMemoryCommand(const format::FixShadowMemoryCommand& header);
    virtual bool ProcessFixDescriptorDataCommand(const format::FixDescriptorDataCommandHeader& header);
    virtual bool ProcessSetEnvironmentVariablesCommand(const format::SetEnvironmentVariablesCommand& header);
    virtual bool ProcessExecuteBlocksFromFile(const format::ExecuteBlocksFromFile& header);
    virtual bool ProcessFixShaderGroupHandleCommand(const format::FixShaderGroupHandleCommandHeader& header);
    virtual bool ProcessInitTensorCommand(const format::InitTensorCommandHeader& header);
    virtual bool ProcessFillMemoryResourceAddressCommand(const format::FillMemoryResourceAddressCommandHeader& header);

    uint64_t GetCurrentBlockIndex() { return block_index_; }

  private:
    bool ProcessFileHeader();

    bool ProcessNextBlock();

    bool ReadBlockHeader(format::BlockHeader* block_header);

  protected:
    std::string                         input_filename_;
    std::string                         output_filename_;
    std::string                         tool_;
    FILE*                               input_file_;
    FILE*                               output_file_;
    std::vector<format::FileOptionPair> file_options_;
    format::EnabledOptions              enabled_options_;
    uint64_t                            bytes_read_;
    uint64_t                            bytes_written_;
    Error                               error_state_;
    bool                                loading_state_;
    std::vector<uint8_t>                parameter_buffer_;
    std::vector<uint8_t>                compressed_parameter_buffer_;
    std::unique_ptr<util::Compressor>   compressor_;
    uint64_t                            block_index_{ 0 };
    format::FileHeader                  file_header_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_FILE_TRANSFORMER_H
