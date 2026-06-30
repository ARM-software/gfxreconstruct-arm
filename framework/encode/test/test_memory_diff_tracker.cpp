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

#include <catch2/catch.hpp>

#include "encode/memory_diff_tracker.h"

#include <cstdint>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)

namespace
{
struct WrittenRange
{
    format::HandleId     memory_id{ format::kNullHandleId };
    uint64_t             offset{ 0 };
    std::vector<uint8_t> data;
};

class RecordingRangeWriter : public MemoryDiffTracker::RangeWriter
{
  public:
    void WriteRange(format::HandleId memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override
    {
        WrittenRange range;
        range.memory_id = memory_id;
        range.offset    = offset;
        range.data.assign(data, data + static_cast<size_t>(size));
        ranges.push_back(range);
    }

    std::vector<WrittenRange> ranges;
};
} // namespace

TEST_CASE("memory diff tracker emits first range in full and suppresses unchanged ranges", "[memory_diff]")
{
    RecordingRangeWriter writer;
    MemoryDiffTracker    tracker(&writer);
    std::vector<uint8_t> data{ 1, 2, 3, 4 };

    tracker.TrackMemory(1, 64);
    tracker.EmitRange(1, 4, 40, data.size(), data.data());

    REQUIRE(writer.ranges.size() == 1);
    CHECK(writer.ranges[0].memory_id == 1);
    CHECK(writer.ranges[0].offset == 40);
    CHECK(writer.ranges[0].data == data);

    writer.ranges.clear();
    tracker.EmitRange(1, 4, 40, data.size(), data.data());
    CHECK(writer.ranges.empty());

    data[1] = 8;
    tracker.EmitRange(1, 4, 40, data.size(), data.data());

    REQUIRE(writer.ranges.size() == 1);
    CHECK(writer.ranges[0].offset == 41);
    CHECK(writer.ranges[0].data == std::vector<uint8_t>{ 8 });
}

TEST_CASE("memory diff tracker emits separated word and tail changes", "[memory_diff]")
{
    RecordingRangeWriter writer;
    MemoryDiffTracker    tracker(&writer);
    std::vector<uint8_t> data(17, 0);

    tracker.TrackMemory(2, 64);
    tracker.EmitRange(2, 0, 100, data.size(), data.data());
    writer.ranges.clear();

    for (uint32_t i = 0; i < 8; ++i)
    {
        data[i] = static_cast<uint8_t>(i + 1);
    }
    data[16] = 99;

    tracker.EmitRange(2, 0, 100, data.size(), data.data());

    REQUIRE(writer.ranges.size() == 2);
    CHECK(writer.ranges[0].offset == 100);
    CHECK(writer.ranges[0].data == std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6, 7, 8 });
    CHECK(writer.ranges[1].offset == 116);
    CHECK(writer.ranges[1].data == std::vector<uint8_t>{ 99 });
}

TEST_CASE("memory diff tracker remove clears the baseline", "[memory_diff]")
{
    RecordingRangeWriter writer;
    MemoryDiffTracker    tracker(&writer);
    std::vector<uint8_t> data{ 10, 20, 30 };

    tracker.TrackMemory(3, 64);
    tracker.EmitRange(3, 0, 0, data.size(), data.data());
    writer.ranges.clear();

    tracker.RemoveMemory(3);
    tracker.TrackMemory(3, 64);
    tracker.EmitRange(3, 0, 0, data.size(), data.data());

    REQUIRE(writer.ranges.size() == 1);
    CHECK(writer.ranges[0].offset == 0);
    CHECK(writer.ranges[0].data == data);
}

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)
