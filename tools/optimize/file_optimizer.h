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

#ifndef GFXRECON_FILE_OPTIMIZER_H
#define GFXRECON_FILE_OPTIMIZER_H

#include "decode/file_transformer.h"
#include "util/defines.h"

#include <unordered_set>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

class FileOptimizer : public decode::FileTransformer
{
  public:
    FileOptimizer(){};

    FileOptimizer(const std::unordered_set<format::HandleId>& unreferenced_ids);

    FileOptimizer(std::unordered_set<format::HandleId>&& unreferenced_ids);

    void SetUnreferencedBlocks(const std::unordered_set<uint64_t>& unreferenced_blocks);

    uint64_t GetUnreferencedBlocksSize();

  protected:
    virtual bool ProcessMethodCall(const format::MethodCallHeader& header, uint64_t block_index = 0) override;
    virtual bool ProcessInitBufferCommand(const format::InitBufferCommandHeader& header) override;
    virtual bool ProcessInitImageCommand(const format::InitImageCommandHeader& header) override;
    virtual bool ProcessInitTensorCommand(const format::InitTensorCommandHeader& header) override;

  protected:
    std::unordered_set<format::HandleId> unreferenced_ids_;
    std::unordered_set<uint64_t>         unreferenced_blocks_;
};

GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_FILE_OPTIMIZER_H
