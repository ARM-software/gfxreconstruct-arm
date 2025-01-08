#ifndef GFXRECON_DECODE_VULKAN_MICROMAP_MODIFIER_BASE_H
#define GFXRECON_DECODE_VULKAN_MICROMAP_MODIFIER_BASE_H

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

class VulkanMicromapModifier : public util::VulkanModifierBase
{
  public:
    VulkanMicromapModifier();

    bool CanOptimize() override;

    virtual void Process_vkCreateMicromapEXT(const ApiCallInfo&                                     call_info,
                                             VkResult                                               returnValue,
                                             format::HandleId                                       device,
                                             StructPointerDecoder<Decoded_VkMicromapCreateInfoEXT>* pCreateInfo,
                                             StructPointerDecoder<Decoded_VkAllocationCallbacks>*   pAllocator,
                                             HandlePointerDecoder<VkMicromapEXT>*                   pMicromap) override;

    virtual void Process_vkCmdBuildMicromapsEXT(const ApiCallInfo&                                    call_info,
                                                format::HandleId                                      commandBuffer,
                                                uint32_t                                              infoCount,
                                                StructPointerDecoder<Decoded_VkMicromapBuildInfoEXT>* pInfos) override;

    virtual void
    Process_vkGetMicromapBuildSizesEXT(const ApiCallInfo&                                         call_info,
                                       format::HandleId                                           device,
                                       VkAccelerationStructureBuildTypeKHR                        buildType,
                                       StructPointerDecoder<Decoded_VkMicromapBuildInfoEXT>*      pBuildInfo,
                                       StructPointerDecoder<Decoded_VkMicromapBuildSizesInfoEXT>* pSizeInfo) override;

    virtual void Process_vkCmdCopyMicromapEXT(const ApiCallInfo&                                   call_info,
                                              format::HandleId                                     commandBuffer,
                                              StructPointerDecoder<Decoded_VkCopyMicromapInfoEXT>* pInfo) override;

  private:
    struct BuildInfoMicromaps
    {
        bool                            is_first_built;
        VkMicromapBuildInfoEXT          info;
        std::vector<VkMicromapUsageEXT> usages;

        bool             is_first_copied;
        format::HandleId source_of_compaction;
    };

    std::unordered_map<format::HandleId, BuildInfoMicromaps> handle_id_to_build_info_{};
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_MICROMAP_MODIFIER_BASE_H
