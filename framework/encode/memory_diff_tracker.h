/*
** Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef GFXRECON_ENCODE_MEMORY_DIFF_TRACKER_H
#define GFXRECON_ENCODE_MEMORY_DIFF_TRACKER_H

#include "format/format.h"
#include "util/range_list.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)

class MemoryDiffTracker
{
  public:
    class RangeWriter
    {
      public:
        virtual ~RangeWriter() {}

        virtual void WriteRange(format::HandleId memory_id, uint64_t offset, uint64_t size, const uint8_t* data) = 0;
    };

  public:
    explicit MemoryDiffTracker(RangeWriter* writer);

    void TrackMemory(format::HandleId memory_id, uint64_t allocation_size);
    void RemoveMemory(format::HandleId memory_id);
    void Clear();

    void EmitRange(format::HandleId memory_id,
                   uint64_t         baseline_offset,
                   uint64_t         write_offset,
                   uint64_t         size,
                   const uint8_t*   data);

  private:
    struct MemoryInfo
    {
        uint64_t             allocation_size{ 0 };
        util::RangeList      valid_ranges;
        std::vector<uint8_t> baseline;
    };

  private:
    bool EnsureBaseline(MemoryInfo* memory_info, uint64_t required_size);
    void WriteRange(format::HandleId memory_id, uint64_t offset, uint64_t size, const uint8_t* data);

  private:
    RangeWriter*                                     writer_{ nullptr };
    std::unordered_map<format::HandleId, MemoryInfo> memory_info_;
};

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_ENCODE_MEMORY_DIFF_TRACKER_H
