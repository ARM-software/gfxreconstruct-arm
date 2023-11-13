#ifndef GFXRECON_FEATURE_FILE_OPTIMIZER_H
#define GFXRECON_FEATURE_FILE_OPTIMIZER_H

#include "file_optimizer.h"
#include "util/defines.h"
#include "vulkan/vulkan.h"
#include "generated/generated_vulkan_decoder.h"
#include "decode/vulkan_feature_tracker_consumer_base.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

class FeatureFileOptimizer : public decode::FileTransformer
{
  public:
    void AddDecoder(decode::ApiDecoder* decoder) { decoders_.push_back(decoder); }

    void RemoveDecoder(decode::ApiDecoder* decoder)
    {
        decoders_.erase(std::remove(decoders_.begin(), decoders_.end(), decoder), decoders_.end());
    }

    void SetConsumer(gfxrecon::decode::VulkanFeatureTrackerConsumerBase* ft_consumer) { ft_consumer_ = ft_consumer; }

  private:
    virtual bool ProcessFunctionCall(const format::BlockHeader& block_header, format::ApiCallId call_id) override;
    void         WriteFunctionCall(format::ApiCallId         call_id,
                                   format::ThreadId          thread_id,
                                   util::MemoryOutputStream* parameter_buffer);
    std::vector<decode::ApiDecoder*>                    decoders_;
    gfxrecon::decode::VulkanFeatureTrackerConsumerBase* ft_consumer_;
};

GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_FEATURE_FILE_OPTIMIZER_H
