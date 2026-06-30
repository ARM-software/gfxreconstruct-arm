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

#include "encode/memory_diff_tracker.h"

#include "util/logging.h"
#include "util/platform.h"

#include <cassert>
#include <cinttypes>
#include <cstring>
#include <limits>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)

constexpr size_t kDiffWordSize = sizeof(uint64_t);

static inline uint64_t LoadDiffWord(const uint8_t* data)
{
    uint64_t word = 0;
    std::memcpy(&word, data, kDiffWordSize);
    return word;
}

static inline void StoreDiffWord(uint8_t* data, uint64_t word)
{
    std::memcpy(data, &word, kDiffWordSize);
}

MemoryDiffTracker::MemoryDiffTracker(RangeWriter* writer) : writer_(writer) {}

void MemoryDiffTracker::TrackMemory(format::HandleId memory_id, uint64_t allocation_size)
{
    MemoryInfo& memory_info     = memory_info_[memory_id];
    memory_info.allocation_size = allocation_size;
}

void MemoryDiffTracker::RemoveMemory(format::HandleId memory_id)
{
    memory_info_.erase(memory_id);
}

void MemoryDiffTracker::Clear()
{
    memory_info_.clear();
}

void MemoryDiffTracker::EmitRange(
    format::HandleId memory_id, uint64_t baseline_offset, uint64_t write_offset, uint64_t size, const uint8_t* data)
{
    if (size == 0 || data == nullptr)
    {
        return;
    }

    if (baseline_offset > (std::numeric_limits<uint64_t>::max() - size) ||
        write_offset > (std::numeric_limits<uint64_t>::max() - size))
    {
        GFXRECON_LOG_ERROR("Memory diff range is too large to process: baseline_offset=%" PRIu64
                           ", write_offset=%" PRIu64 ", size=%" PRIu64,
                           baseline_offset,
                           write_offset,
                           size);
        return;
    }

    const uint64_t required_size = baseline_offset + size;
    if (required_size > std::numeric_limits<size_t>::max() || size > std::numeric_limits<size_t>::max())
    {
        GFXRECON_LOG_ERROR("Memory diff range is too large to process: baseline_offset=%" PRIu64
                           ", write_offset=%" PRIu64 ", size=%" PRIu64,
                           baseline_offset,
                           write_offset,
                           size);
        return;
    }

    MemoryInfo& memory_info = memory_info_[memory_id];
    if (!EnsureBaseline(&memory_info, required_size))
    {
        return;
    }

    const size_t baseline_offset_size = static_cast<size_t>(baseline_offset);
    const size_t range_size           = static_cast<size_t>(size);
    uint8_t*     baseline             = memory_info.baseline.data() + baseline_offset_size;

    if (!memory_info.valid_ranges.ContainsRange(baseline_offset, size))
    {
        WriteRange(memory_id, write_offset, size, data);
        util::platform::MemoryCopy(baseline, range_size, data, range_size);
        memory_info.valid_ranges.AddRange(baseline_offset, size);
        return;
    }

    size_t cursor = 0;
    while ((range_size - cursor) >= kDiffWordSize)
    {
        while ((range_size - cursor) >= kDiffWordSize && LoadDiffWord(baseline + cursor) == LoadDiffWord(data + cursor))
        {
            cursor += kDiffWordSize;
        }

        const size_t changed_begin = cursor;

        while ((range_size - cursor) >= kDiffWordSize)
        {
            const uint64_t data_word = LoadDiffWord(data + cursor);
            if (LoadDiffWord(baseline + cursor) == data_word)
            {
                break;
            }

            StoreDiffWord(baseline + cursor, data_word);
            cursor += kDiffWordSize;
        }

        if (cursor > changed_begin)
        {
            const uint64_t changed_offset = write_offset + changed_begin;
            const uint64_t changed_size   = cursor - changed_begin;
            WriteRange(memory_id, changed_offset, changed_size, data + changed_begin);
        }
    }

    while (cursor < range_size)
    {
        while (cursor < range_size && baseline[cursor] == data[cursor])
        {
            ++cursor;
        }

        const size_t changed_begin = cursor;

        while (cursor < range_size && baseline[cursor] != data[cursor])
        {
            baseline[cursor] = data[cursor];
            ++cursor;
        }

        if (cursor > changed_begin)
        {
            const uint64_t changed_offset = write_offset + changed_begin;
            const uint64_t changed_size   = cursor - changed_begin;
            WriteRange(memory_id, changed_offset, changed_size, data + changed_begin);
        }
    }
}

bool MemoryDiffTracker::EnsureBaseline(MemoryInfo* memory_info, uint64_t required_size)
{
    assert(memory_info != nullptr);

    if (required_size > memory_info->baseline.size())
    {
        memory_info->baseline.resize(static_cast<size_t>(required_size), 0);
    }

    return true;
}

void MemoryDiffTracker::WriteRange(format::HandleId memory_id, uint64_t offset, uint64_t size, const uint8_t* data)
{
    if (writer_ != nullptr)
    {
        writer_->WriteRange(memory_id, offset, size, data);
    }
}

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)
