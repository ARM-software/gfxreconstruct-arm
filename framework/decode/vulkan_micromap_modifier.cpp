#include "decode/vulkan_micromap_modifier.h"

#include "util/logging.h"

#include <cassert>
#include <stdexcept>

#include "generated/generated_vulkan_api_call_encoders.h"

#include "encode/custom_vulkan_encoder_commands.h"
#include "encode/custom_vulkan_array_size_2d.h"
#include "encode/parameter_encoder.h"
#include "encode/struct_pointer_encoder.h"
#include "encode/vulkan_capture_manager.h"
#include "encode/vulkan_handle_wrapper_util.h"
#include "encode/vulkan_handle_wrappers.h"
#include "format/api_call_id.h"
#include "generated/generated_vulkan_command_buffer_util.h"
#include "generated/generated_vulkan_struct_handle_wrappers.h"
#include "util/defines.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

/*TODOs:
 * Add support for alias micromaps
 * Add support for compaction (vkCmdCopyMicromapEXT)
 * The current implementation assumes that the first build command contains the right info for the storage buffer size
 * computation. This can potentially not be the case.
 */

VulkanMicromapModifier::VulkanMicromapModifier() {}

void VulkanMicromapModifier::Process_vkCreateMicromapEXT(
    const ApiCallInfo&                                     call_info,
    VkResult                                               returnValue,
    format::HandleId                                       device,
    StructPointerDecoder<Decoded_VkMicromapCreateInfoEXT>* pCreateInfo,
    StructPointerDecoder<Decoded_VkAllocationCallbacks>*   pAllocator,
    HandlePointerDecoder<VkMicromapEXT>*                   pMicromap)
{
    if (!IsModificationPass())
    {
        return;
    }

    format::HandleId handle = *pMicromap->GetPointer();

    assert(handle_to_build_info_.count(handle) == 1);

    auto new_call       = CreatePreCall();
    new_call->call_id   = gfxrecon::format::ApiCallId::ApiCall_vkGetMicromapBuildSizesEXT;
    new_call->thread_id = 1;
    gfxrecon::encode::ParameterEncoder encoder(&new_call->parameter_buffer);
    encoder.EncodeHandleIdValue(device);

    VkMicromapBuildInfoEXT pBuildInfo = handle_to_build_info_[handle].info;
    pBuildInfo.pUsageCounts           = handle_to_build_info_[handle].usages.data();
    // TODO:Encoding a handle that doesn't exist yet is not possible. Find a workaround around that for the future
    // pBuildInfo.dstMicromap            = (VkMicromapEXT)handle;
    const VkMicromapBuildSizesInfoEXT pSizeInfo{ VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT };

    encoder.EncodeEnumValue(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);
    encode::EncodeStructPtr(&encoder, &pBuildInfo);
    encode::EncodeStructPtr(&encoder, &pSizeInfo);
}

void VulkanMicromapModifier::Process_vkCmdBuildMicromapsEXT(
    const ApiCallInfo&                                    call_info,
    format::HandleId                                      commandBuffer,
    uint32_t                                              infoCount,
    StructPointerDecoder<Decoded_VkMicromapBuildInfoEXT>* pInfos)
{
    if (IsModificationPass())
    {
        return;
    }

    auto pInfosDec = pInfos->GetMetaStructPointer();

    for (uint64_t i = 0; i < infoCount; i++)
    {
        if (handle_to_build_info_.count(pInfosDec[i].dstMicromap) == 0)
        {
            handle_to_build_info_[pInfosDec[i].dstMicromap] = { *pInfosDec[i].decoded_value,
                                                                std::vector<VkMicromapUsageEXT>(
                                                                    pInfosDec[i].decoded_value->pUsageCounts,
                                                                    pInfosDec[i].decoded_value->pUsageCounts +
                                                                        pInfosDec[i].decoded_value->usageCountsCount) };
        }
    }
}

void VulkanMicromapModifier::Process_vkGetMicromapBuildSizesEXT(
    const ApiCallInfo&                                         call_info,
    format::HandleId                                           device,
    VkAccelerationStructureBuildTypeKHR                        buildType,
    StructPointerDecoder<Decoded_VkMicromapBuildInfoEXT>*      pBuildInfo,
    StructPointerDecoder<Decoded_VkMicromapBuildSizesInfoEXT>* pSizeInfo)
{
    // TODO: delete original call since we insert it ourselves. This is meant to be done at the end of other TODOs
    // delete_current_call = true;
}

bool VulkanMicromapModifier::CanOptimize()
{
    bool result = (!handle_to_build_info_.empty());

    return result;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
