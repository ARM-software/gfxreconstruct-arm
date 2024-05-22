/*
** Copyright (c) 2024 LunarG, Inc.
** Copyright (c) 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#ifndef GFXRECON_VULKAN_FILE_OPTIMIZER_H
#define GFXRECON_VULKAN_FILE_OPTIMIZER_H

#include "file_optimizer.h"
#include "util/defines.h"
#include "decode/vulkan_feature_tracker_consumer_base.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

class VulkanFileOptimizer : public FileOptimizer
{
  public:
    struct VulkanOptimizationData
    {
        std::unordered_set<gfxrecon::format::HandleId>                      unreferenced_ids;
        std::unique_ptr<gfxrecon::decode::VulkanFeatureTrackerConsumerBase> feature_tracker_consumer;
    };

    VulkanFileOptimizer(VulkanOptimizationData& optimization_data) :
        FileOptimizer(optimization_data.unreferenced_ids), optimization_data_(optimization_data)
    {}

    void AddDecoder(decode::ApiDecoder* decoder) { decoders_.push_back(decoder); }

  private:
    virtual bool ProcessFunctionCall(const format::BlockHeader& block_header, format::ApiCallId call_id) override;

    void WriteFunctionCall(format::ApiCallId         call_id,
                           format::ThreadId          thread_id,
                           util::MemoryOutputStream* parameter_buffer);

    VulkanOptimizationData&          optimization_data_;
    std::vector<decode::ApiDecoder*> decoders_;
};

GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_FILE_OPTIMIZER_H
