/*
** Copyright (c) 2025 LunarG, Inc.
** Copyright (c) 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#ifndef GFXRECON_DECODE_VULKAN_MICROMAP_EXT_MODIFIER_BASE_H
#define GFXRECON_DECODE_VULKAN_MICROMAP_EXT_MODIFIER_BASE_H

#include "decode/referenced_resource_table.h"
#include "generated/generated_vulkan_consumer.h"
#include "util/defines.h"
#include "util/memory_output_stream.h"
#include "encode/parameter_buffer.h"
#include "util/vulkan_modifier_base.h"

#include "vulkan/vulkan.h"

#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanMicromapEXTModifier : public util::VulkanModifierBase
{
  public:
    VulkanMicromapEXTModifier();

    bool CanOptimize() override;

    void Process_vkCreateMicromapEXT(const ApiCallInfo& call_info, args::CreateMicromapEXT& args) override;

    void Process_vkCmdBuildMicromapsEXT(const ApiCallInfo& call_info, args::CmdBuildMicromapsEXT& args) override;

    void Process_vkGetMicromapBuildSizesEXT(const ApiCallInfo&              call_info,
                                            args::GetMicromapBuildSizesEXT& args) override;

    void Process_vkCmdCopyMicromapEXT(const ApiCallInfo& call_info, args::CmdCopyMicromapEXT& args) override;

  private:
    struct BuildInfoMicromapsEXT
    {
        bool                            is_first_built;
        VkMicromapBuildInfoEXT          info;
        std::vector<VkMicromapUsageEXT> usages;

        bool             is_first_copied;
        format::HandleId source_of_compaction;
    };

    std::unordered_map<format::HandleId, BuildInfoMicromapsEXT> handle_id_to_build_info_{};
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_MICROMAP_EXT_MODIFIER_BASE_H
