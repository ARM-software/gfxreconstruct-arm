
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "format/format.h"
#include "format/format_util.h"
#include "encode/parameter_buffer.h"
#include "encode/parameter_encoder.h"
#include "encode/custom_vulkan_struct_encoders.h"
#include "decode/custom_vulkan_struct_decoders.h"
#include "decode/decode_allocator.h"
#include "vulkan/vulkan.h"

#include <vector>

TEST_CASE("VkDataGraphPipelineConstantARM can be encoded and decoded", "[enc/dec]")
{
    using namespace gfxrecon;
    using namespace gfxrecon::decode;
    gfxrecon::util::Log::Init(gfxrecon::util::Log::kErrorSeverity);
    auto parameter_buffer_  = std::make_unique<encode::ParameterBuffer>();
    auto parameter_encoder_ = std::make_unique<encode::ParameterEncoder>(parameter_buffer_.get());

    auto* encoder = parameter_encoder_.get();

    // Dimensions and tensor description
    std::vector<int64_t> dimensions = { 32, 3, 3, 12 };

    VkTensorDescriptionARM tensorDescription{ VK_STRUCTURE_TYPE_TENSOR_DESCRIPTION_ARM,
                                              nullptr,
                                              VK_TENSOR_TILING_OPTIMAL_ARM,
                                              VK_FORMAT_R8_SINT, // one byte per element
                                              static_cast<uint32_t>(dimensions.size()),
                                              dimensions.data(),
                                              nullptr, // pStrides=nullptr => packed layout
                                              VK_TENSOR_USAGE_DATA_GRAPH_BIT_ARM };

    // Allocate and populate constant data: 3456 bytes
    const size_t        elem_size  = 1; // R8_SINT
    const size_t        elem_count = 32 * 3 * 3 * 12;
    std::vector<int8_t> constantBytes(elem_count);

    // Fill with a deterministic pattern for testing
    for (size_t i = 0; i < elem_count; ++i)
    {
        constantBytes[i] = static_cast<int8_t>((i % 127) - 63); // any pattern
    }

    const void* constantData = constantBytes.data();

    VkDataGraphPipelineConstantARM pipelineConstant{ VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_CONSTANT_ARM,
                                                     &tensorDescription,
                                                     /*id=*/0,
                                                     constantData };

    gfxrecon::encode::EncodeStruct(encoder, pipelineConstant);

    DecodeAllocator::Begin();

    Decoded_VkDataGraphPipelineConstantARM wrapper;
    VkDataGraphPipelineConstantARM         decoded_value;
    wrapper.decoded_value = &decoded_value;

    DecodeStruct(parameter_buffer_->GetData(), parameter_buffer_->GetDataSize(), &wrapper);

    auto* decoded_tensor_desc = reinterpret_cast<const VkTensorDescriptionARM*>(decoded_value.pNext);
    REQUIRE(decoded_tensor_desc->sType == tensorDescription.sType);
    REQUIRE(decoded_tensor_desc->pNext == tensorDescription.pNext);
    REQUIRE(decoded_tensor_desc->tiling == tensorDescription.tiling);
    REQUIRE(decoded_tensor_desc->format == tensorDescription.format);
    REQUIRE(decoded_tensor_desc->dimensionCount == tensorDescription.dimensionCount);

    // Compare dimensions array
    REQUIRE(decoded_tensor_desc->pDimensions != nullptr);
    for (uint32_t i = 0; i < tensorDescription.dimensionCount; ++i)
    {
        REQUIRE(decoded_tensor_desc->pDimensions[i] == tensorDescription.pDimensions[i]);
    }

    // pStrides is nullptr in both
    REQUIRE(decoded_tensor_desc->pStrides == tensorDescription.pStrides);

    REQUIRE(decoded_tensor_desc->usage == tensorDescription.usage);

    for (size_t i = 0; i < elem_count; ++i)
    {
        INFO("Mismatch at pConstantData index "
             << i << " decoded=" << +static_cast<int>(static_cast<const int8_t*>(decoded_value.pConstantData)[i])
             << " expected=" << +static_cast<int>(constantBytes[i]));
        REQUIRE(constantBytes[i] == static_cast<const int8_t*>(decoded_value.pConstantData)[i]);
    }

    DecodeAllocator::End();
    gfxrecon::util::Log::Release();
}
