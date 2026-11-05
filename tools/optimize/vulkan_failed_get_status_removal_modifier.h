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

#ifndef GFXRECON_DECODE_VULKAN_FAILED_GET_STATUS_REMOVAL_MODIFIER_H
#define GFXRECON_DECODE_VULKAN_FAILED_GET_STATUS_REMOVAL_MODIFIER_H

#include "util/vulkan_modifier_base.h"

#include "vulkan/vulkan.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanFailedGetStatusRemovalModifier : public util::VulkanModifierBase
{
  public:
    VulkanFailedGetStatusRemovalModifier(bool keep_failed_get_status) : keep_failed_get_status_(keep_failed_get_status)
    {}

    bool CanOptimize() override { return !keep_failed_get_status_; }

    void Process_vkGetFenceStatus(const ApiCallInfo& call_info, args::GetFenceStatus& args) override;

    void Process_vkWaitForFences(const ApiCallInfo& call_info, args::WaitForFences& args) override;

    void Process_vkGetQueryPoolResults(const ApiCallInfo& call_info, args::GetQueryPoolResults& args) override;

  private:
    bool keep_failed_get_status_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_FAILED_GET_STATUS_REMOVAL_MODIFIER_H
