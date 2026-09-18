/*
** Copyright (c) 2026 LunarG, Inc.
** Copyright (c) 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "util/interval_tree.h"

TEST_CASE("interval_tree remains searchable after ordered insertion", "[interval_tree]")
{
    gfxrecon::util::interval_tree<uint64_t> tree;

    constexpr uint64_t kIntervalCount = 4096;
    for (uint64_t i = 0; i < kIntervalCount; ++i)
    {
        const uint64_t begin = i * 3;
        tree.insert({ begin, begin + 1 });
    }

    for (uint64_t i = 0; i < kIntervalCount; ++i)
    {
        const uint64_t begin = i * 3;
        REQUIRE(tree.intersects({ begin, begin + 1 }));
        REQUIRE_FALSE(tree.intersects({ begin + 1, begin + 2 }));
        REQUIRE(tree.intersection({ begin, begin + 1 }).size() == 1);
        REQUIRE(tree.intersection({ begin + 1, begin + 2 }).empty());
    }
}

TEST_CASE("interval_tree remains searchable after balanced erasure", "[interval_tree]")
{
    gfxrecon::util::interval_tree<uint64_t> tree;

    constexpr uint64_t kIntervalCount = 1024;
    for (uint64_t i = 0; i < kIntervalCount; ++i)
    {
        const uint64_t begin = i * 3;
        tree.insert({ begin, begin + 1 });
    }

    for (uint64_t i = 0; i < kIntervalCount; i += 2)
    {
        const uint64_t begin = i * 3;
        tree.erase({ begin, begin + 1 });
    }

    for (uint64_t i = 0; i < kIntervalCount; ++i)
    {
        const uint64_t begin = i * 3;
        REQUIRE((!tree.intersection({ begin, begin + 1 }).empty()) == ((i % 2) != 0));
    }
}

TEST_CASE("interval_tree returns every intersection and its payload", "[interval_tree]")
{
    gfxrecon::util::interval_tree<uint64_t, uint32_t> tree;
    tree.insert({ 10, 30 }, 1);
    tree.insert({ 20, 40 }, 2);
    tree.insert({ 50, 60 }, 3);

    const auto intersections = tree.intersection({ 25, 55 });
    REQUIRE(intersections.size() == 3);
    REQUIRE(intersections[0].payload == 1);
    REQUIRE(intersections[1].payload == 2);
    REQUIRE(intersections[2].payload == 3);

    tree.insert({ 20, 40 }, 4);
    tree.insert({ 20, 40 }, 4);
    const auto aliases = tree.contains(25);
    REQUIRE(aliases.size() == 3);
    REQUIRE(aliases[0].payload == 1);
    REQUIRE(aliases[1].payload == 2);
    REQUIRE(aliases[2].payload == 4);
}

TEST_CASE("interval_tree contains uses half-open interval boundaries", "[interval_tree]")
{
    gfxrecon::util::interval_tree<uint64_t, uint32_t> tree;
    tree.insert({ 10, 30 }, 1);
    tree.insert({ 20, 40 }, 2);

    REQUIRE(tree.contains(9).empty());
    REQUIRE(tree.contains(10).size() == 1);
    REQUIRE(tree.contains(20).size() == 2);
    REQUIRE(tree.contains(29).size() == 2);
    REQUIRE(tree.contains(30).size() == 1);
    REQUIRE(tree.contains(40).empty());
}
