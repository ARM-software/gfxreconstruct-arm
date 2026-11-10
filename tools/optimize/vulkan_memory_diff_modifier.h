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
#ifndef GFXRECON_DECODE_VULKAN_MEMORY_DIFF_MODIFIER_BASE_H
#define GFXRECON_DECODE_VULKAN_MEMORY_DIFF_MODIFIER_BASE_H

#include "format/format.h"
#include "util/defines.h"
#include "util/vulkan_modifier_base.h"
#include <unordered_map>
#include "decode/vulkan_optimize_options.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanMemoryDiffModifier : public util::VulkanModifierBase
{
  public:
    VulkanMemoryDiffModifier(const VulkanOptimizationOptions& options) : options_(options){};

    bool CanOptimize() override;
    void ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override;

  private:
    struct ByteDiff
    {
        std::size_t               offset;
        std::vector<std::uint8_t> data;
    };
    std::vector<VulkanMemoryDiffModifier::ByteDiff>
    diff_the_vectors(const uint8_t* new_data, const uint8_t* old_data, uint64_t length);

    void WriteFillMemoryCmd(uint64_t memory_id, uint64_t fill_offset, uint64_t fill_size, const uint8_t* fill_data);

    std::unordered_map<format::HandleId, std::unordered_map<uint64_t, std::vector<uint8_t>>> memory_map;

    VulkanOptimizationOptions options_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_MEMORY_DIFF_MODIFIER_BASE_H
