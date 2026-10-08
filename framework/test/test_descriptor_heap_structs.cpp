/*
** Copyright (c) 2026 LunarG, Inc.
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

#include "decode/decode_allocator.h"
#include "decode/custom_vulkan_struct_decoders.h"
#include "encode/custom_vulkan_struct_encoders.h"
#include "encode/parameter_buffer.h"
#include "encode/parameter_encoder.h"
#include "generated/generated_vulkan_struct_decoders.h"
#include "util/logging.h"

#include "vulkan/vulkan.h"

struct DescriptorMappingFixture
{
    gfxrecon::encode::ParameterBuffer  buffer;
    gfxrecon::encode::ParameterEncoder encoder{ &buffer };

    VkDescriptorSetAndBindingMappingEXT                           decoded{};
    gfxrecon::decode::Decoded_VkDescriptorSetAndBindingMappingEXT wrapper{};

    DescriptorMappingFixture()
    {
        gfxrecon::util::Log::Init(gfxrecon::util::LoggingSeverity::kError);
        gfxrecon::decode::DecodeAllocator::Begin();
    }

    ~DescriptorMappingFixture()
    {
        gfxrecon::decode::DecodeAllocator::End();
        gfxrecon::util::Log::Release();
    }

    void RoundTrip(const VkDescriptorSetAndBindingMappingEXT& original)
    {
        gfxrecon::encode::EncodeStruct(&encoder, original);
        wrapper.decoded_value = &decoded;
        REQUIRE(gfxrecon::decode::DecodeStruct(buffer.GetData(), buffer.GetDataSize(), &wrapper) ==
                buffer.GetDataSize());

        REQUIRE(decoded.sType == original.sType);
        REQUIRE(decoded.pNext == original.pNext);
        REQUIRE(decoded.descriptorSet == original.descriptorSet);
        REQUIRE(decoded.firstBinding == original.firstBinding);
        REQUIRE(decoded.bindingCount == original.bindingCount);
        REQUIRE(decoded.resourceMask == original.resourceMask);
        REQUIRE(decoded.source == original.source);
        REQUIRE(wrapper.sourceData != nullptr);
        REQUIRE(wrapper.sourceData->decoded_value == &decoded.sourceData);
    }
};

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT constant offset source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkSamplerCreateInfo sampler{};
    sampler.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter    = VK_FILTER_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,   nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT, {}
    };
    mapping.sourceData.constantOffset = { 11, 12, &sampler, 13, 14 };

    RoundTrip(mapping);

    REQUIRE(wrapper.sourceData->constantOffset != nullptr);
    REQUIRE(wrapper.sourceData->constantOffset->decoded_value == &decoded.sourceData.constantOffset);
    REQUIRE(decoded.sourceData.constantOffset.heapOffset == 11);
    REQUIRE(decoded.sourceData.constantOffset.heapArrayStride == 12);
    REQUIRE(decoded.sourceData.constantOffset.samplerHeapOffset == 13);
    REQUIRE(decoded.sourceData.constantOffset.samplerHeapArrayStride == 14);
    REQUIRE(decoded.sourceData.constantOffset.pEmbeddedSampler != nullptr);
    REQUIRE(decoded.sourceData.constantOffset.pEmbeddedSampler->sType == sampler.sType);
    REQUIRE(decoded.sourceData.constantOffset.pEmbeddedSampler->magFilter == sampler.magFilter);
    REQUIRE(decoded.sourceData.constantOffset.pEmbeddedSampler->addressModeU == sampler.addressModeU);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT push index source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkSamplerCreateInfo sampler{};
    sampler.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter    = VK_FILTER_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT, nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,    {}
    };
    mapping.sourceData.pushIndex = { 21, 22, 23, 24, &sampler, VK_TRUE, 25, 26, 27, 28 };

    RoundTrip(mapping);

    REQUIRE(wrapper.sourceData->pushIndex != nullptr);
    REQUIRE(wrapper.sourceData->pushIndex->decoded_value == &decoded.sourceData.pushIndex);
    REQUIRE(decoded.sourceData.pushIndex.heapOffset == 21);
    REQUIRE(decoded.sourceData.pushIndex.pushOffset == 22);
    REQUIRE(decoded.sourceData.pushIndex.heapIndexStride == 23);
    REQUIRE(decoded.sourceData.pushIndex.heapArrayStride == 24);
    REQUIRE(decoded.sourceData.pushIndex.useCombinedImageSamplerIndex == VK_TRUE);
    REQUIRE(decoded.sourceData.pushIndex.samplerHeapOffset == 25);
    REQUIRE(decoded.sourceData.pushIndex.samplerPushOffset == 26);
    REQUIRE(decoded.sourceData.pushIndex.samplerHeapIndexStride == 27);
    REQUIRE(decoded.sourceData.pushIndex.samplerHeapArrayStride == 28);
    REQUIRE(decoded.sourceData.pushIndex.pEmbeddedSampler != nullptr);
    REQUIRE(decoded.sourceData.pushIndex.pEmbeddedSampler->magFilter == sampler.magFilter);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT indirect index source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkSamplerCreateInfo sampler{};
    sampler.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter    = VK_FILTER_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,  nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_EXT, {}
    };
    mapping.sourceData.indirectIndex = { 31, 32, 33, 34, 35, &sampler, VK_TRUE, 36, 37, 38, 39, 40 };

    RoundTrip(mapping);

    REQUIRE(wrapper.sourceData->indirectIndex != nullptr);
    REQUIRE(wrapper.sourceData->indirectIndex->decoded_value == &decoded.sourceData.indirectIndex);
    REQUIRE(decoded.sourceData.indirectIndex.heapOffset == 31);
    REQUIRE(decoded.sourceData.indirectIndex.pushOffset == 32);
    REQUIRE(decoded.sourceData.indirectIndex.addressOffset == 33);
    REQUIRE(decoded.sourceData.indirectIndex.heapIndexStride == 34);
    REQUIRE(decoded.sourceData.indirectIndex.heapArrayStride == 35);
    REQUIRE(decoded.sourceData.indirectIndex.useCombinedImageSamplerIndex == VK_TRUE);
    REQUIRE(decoded.sourceData.indirectIndex.samplerHeapOffset == 36);
    REQUIRE(decoded.sourceData.indirectIndex.samplerPushOffset == 37);
    REQUIRE(decoded.sourceData.indirectIndex.samplerAddressOffset == 38);
    REQUIRE(decoded.sourceData.indirectIndex.samplerHeapIndexStride == 39);
    REQUIRE(decoded.sourceData.indirectIndex.samplerHeapArrayStride == 40);
    REQUIRE(decoded.sourceData.indirectIndex.pEmbeddedSampler != nullptr);
    REQUIRE(decoded.sourceData.indirectIndex.pEmbeddedSampler->magFilter == sampler.magFilter);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT indirect index array source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkSamplerCreateInfo sampler{};
    sampler.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter    = VK_FILTER_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,        nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_INDIRECT_INDEX_ARRAY_EXT, {}
    };
    mapping.sourceData.indirectIndexArray = { 41, 42, 43, 44, &sampler, VK_TRUE, 45, 46, 47, 48 };

    RoundTrip(mapping);

    REQUIRE(wrapper.sourceData->indirectIndexArray != nullptr);
    REQUIRE(wrapper.sourceData->indirectIndexArray->decoded_value == &decoded.sourceData.indirectIndexArray);
    REQUIRE(decoded.sourceData.indirectIndexArray.heapOffset == 41);
    REQUIRE(decoded.sourceData.indirectIndexArray.pushOffset == 42);
    REQUIRE(decoded.sourceData.indirectIndexArray.addressOffset == 43);
    REQUIRE(decoded.sourceData.indirectIndexArray.heapIndexStride == 44);
    REQUIRE(decoded.sourceData.indirectIndexArray.useCombinedImageSamplerIndex == VK_TRUE);
    REQUIRE(decoded.sourceData.indirectIndexArray.samplerHeapOffset == 45);
    REQUIRE(decoded.sourceData.indirectIndexArray.samplerPushOffset == 46);
    REQUIRE(decoded.sourceData.indirectIndexArray.samplerAddressOffset == 47);
    REQUIRE(decoded.sourceData.indirectIndexArray.samplerHeapIndexStride == 48);
    REQUIRE(decoded.sourceData.indirectIndexArray.pEmbeddedSampler != nullptr);
    REQUIRE(decoded.sourceData.indirectIndexArray.pEmbeddedSampler->magFilter == sampler.magFilter);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT resource heap data source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT, nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_RESOURCE_HEAP_DATA_EXT,      {}
    };
    mapping.sourceData.heapData = { 51, 52 };

    RoundTrip(mapping);

    REQUIRE(wrapper.sourceData->heapData != nullptr);
    REQUIRE(wrapper.sourceData->heapData->decoded_value == &decoded.sourceData.heapData);
    REQUIRE(decoded.sourceData.heapData.heapOffset == 51);
    REQUIRE(decoded.sourceData.heapData.pushOffset == 52);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT push data source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkDescriptorSetAndBindingMappingEXT mapping{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
                                                 nullptr,
                                                 2,
                                                 3,
                                                 4,
                                                 5,
                                                 VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT,
                                                 {} };
    mapping.sourceData.pushDataOffset = 61;

    RoundTrip(mapping);

    REQUIRE(decoded.sourceData.pushDataOffset == 61);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT push address source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkDescriptorSetAndBindingMappingEXT mapping{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
                                                 nullptr,
                                                 2,
                                                 3,
                                                 4,
                                                 5,
                                                 VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT,
                                                 {} };
    mapping.sourceData.pushAddressOffset = 71;

    RoundTrip(mapping);

    REQUIRE(decoded.sourceData.pushAddressOffset == 71);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT indirect address source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT, nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_INDIRECT_ADDRESS_EXT,        {}
    };
    mapping.sourceData.indirectAddress = { 81, 82 };

    RoundTrip(mapping);

    REQUIRE(wrapper.sourceData->indirectAddress != nullptr);
    REQUIRE(wrapper.sourceData->indirectAddress->decoded_value == &decoded.sourceData.indirectAddress);
    REQUIRE(decoded.sourceData.indirectAddress.pushOffset == 81);
    REQUIRE(decoded.sourceData.indirectAddress.addressOffset == 82);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT shader record index source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkSamplerCreateInfo sampler{};
    sampler.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter    = VK_FILTER_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,       nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_SHADER_RECORD_INDEX_EXT, {}
    };
    mapping.sourceData.shaderRecordIndex = { 91, 92, 93, 94, &sampler, VK_TRUE, 95, 96, 97, 98 };

    RoundTrip(mapping);

    REQUIRE(wrapper.sourceData->shaderRecordIndex != nullptr);
    REQUIRE(wrapper.sourceData->shaderRecordIndex->decoded_value == &decoded.sourceData.shaderRecordIndex);
    REQUIRE(decoded.sourceData.shaderRecordIndex.heapOffset == 91);
    REQUIRE(decoded.sourceData.shaderRecordIndex.shaderRecordOffset == 92);
    REQUIRE(decoded.sourceData.shaderRecordIndex.heapIndexStride == 93);
    REQUIRE(decoded.sourceData.shaderRecordIndex.heapArrayStride == 94);
    REQUIRE(decoded.sourceData.shaderRecordIndex.useCombinedImageSamplerIndex == VK_TRUE);
    REQUIRE(decoded.sourceData.shaderRecordIndex.samplerHeapOffset == 95);
    REQUIRE(decoded.sourceData.shaderRecordIndex.samplerShaderRecordOffset == 96);
    REQUIRE(decoded.sourceData.shaderRecordIndex.samplerHeapIndexStride == 97);
    REQUIRE(decoded.sourceData.shaderRecordIndex.samplerHeapArrayStride == 98);
    REQUIRE(decoded.sourceData.shaderRecordIndex.pEmbeddedSampler != nullptr);
    REQUIRE(decoded.sourceData.shaderRecordIndex.pEmbeddedSampler->magFilter == sampler.magFilter);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT shader record data source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT, nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_DATA_EXT,      {}
    };
    mapping.sourceData.shaderRecordDataOffset = 101;

    RoundTrip(mapping);

    REQUIRE(decoded.sourceData.shaderRecordDataOffset == 101);
}

TEST_CASE_METHOD(DescriptorMappingFixture,
                 "VkDescriptorSetAndBindingMappingEXT shader record address source round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkDescriptorSetAndBindingMappingEXT mapping{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT, nullptr, 2, 3, 4, 5,
        VK_DESCRIPTOR_MAPPING_SOURCE_SHADER_RECORD_ADDRESS_EXT,   {}
    };
    mapping.sourceData.shaderRecordAddressOffset = 111;

    RoundTrip(mapping);

    REQUIRE(decoded.sourceData.shaderRecordAddressOffset == 111);
}

struct ResourceDescriptorFixture
{
    gfxrecon::encode::ParameterBuffer  buffer;
    gfxrecon::encode::ParameterEncoder encoder{ &buffer };

    VkResourceDescriptorInfoEXT                           decoded{};
    gfxrecon::decode::Decoded_VkResourceDescriptorInfoEXT wrapper{};

    ResourceDescriptorFixture()
    {
        gfxrecon::util::Log::Init(gfxrecon::util::LoggingSeverity::kError);
        gfxrecon::decode::DecodeAllocator::Begin();
    }

    ~ResourceDescriptorFixture()
    {
        gfxrecon::decode::DecodeAllocator::End();
        gfxrecon::util::Log::Release();
    }

    void RoundTrip(const VkResourceDescriptorInfoEXT& original)
    {
        gfxrecon::encode::EncodeStruct(&encoder, original);
        wrapper.decoded_value = &decoded;
        REQUIRE(gfxrecon::decode::DecodeStruct(buffer.GetData(), buffer.GetDataSize(), &wrapper) ==
                buffer.GetDataSize());

        REQUIRE(decoded.sType == original.sType);
        REQUIRE(decoded.pNext == original.pNext);
        REQUIRE(decoded.type == original.type);
        REQUIRE(wrapper.data != nullptr);
        REQUIRE(wrapper.data->decoded_value == &decoded.data);
    }
};

TEST_CASE_METHOD(ResourceDescriptorFixture,
                 "VkResourceDescriptorInfoEXT texel buffer data round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkTexelBufferDescriptorInfoEXT texel_buffer{
        VK_STRUCTURE_TYPE_TEXEL_BUFFER_DESCRIPTOR_INFO_EXT, nullptr, VK_FORMAT_R32_UINT, { 0x12340000, 256 }
    };
    VkResourceDescriptorInfoEXT resource{
        VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT, nullptr, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, {}
    };
    resource.data.pTexelBuffer = &texel_buffer;

    RoundTrip(resource);

    REQUIRE(wrapper.data->pTexelBuffer != nullptr);
    REQUIRE(wrapper.data->pTexelBuffer->GetPointer() == decoded.data.pTexelBuffer);
    REQUIRE(decoded.data.pTexelBuffer != nullptr);
    REQUIRE(decoded.data.pTexelBuffer->sType == texel_buffer.sType);
    REQUIRE(decoded.data.pTexelBuffer->pNext == nullptr);
    REQUIRE(decoded.data.pTexelBuffer->format == texel_buffer.format);
    REQUIRE(decoded.data.pTexelBuffer->addressRange.address == texel_buffer.addressRange.address);
    REQUIRE(decoded.data.pTexelBuffer->addressRange.size == texel_buffer.addressRange.size);
}

TEST_CASE_METHOD(ResourceDescriptorFixture,
                 "VkResourceDescriptorInfoEXT address range data round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkDeviceAddressRangeEXT     address_range{ 0x12340000, 512 };
    VkResourceDescriptorInfoEXT resource{
        VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT, nullptr, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {}
    };
    resource.data.pAddressRange = &address_range;

    RoundTrip(resource);

    REQUIRE(wrapper.data->pAddressRange != nullptr);
    REQUIRE(wrapper.data->pAddressRange->GetPointer() == decoded.data.pAddressRange);
    REQUIRE(decoded.data.pAddressRange != nullptr);
    REQUIRE(decoded.data.pAddressRange->address == address_range.address);
    REQUIRE(decoded.data.pAddressRange->size == address_range.size);
}

TEST_CASE_METHOD(ResourceDescriptorFixture,
                 "VkResourceDescriptorInfoEXT image data round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkImageViewCreateInfo image_view{};
    image_view.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view.format   = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageDescriptorInfoEXT image{
        VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT, nullptr, &image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkResourceDescriptorInfoEXT resource{
        VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT, nullptr, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {}
    };
    resource.data.pImage = &image;

    RoundTrip(resource);

    REQUIRE(wrapper.data->pImage != nullptr);
    REQUIRE(wrapper.data->pImage->GetPointer() == decoded.data.pImage);
    REQUIRE(decoded.data.pImage != nullptr);
    REQUIRE(decoded.data.pImage->sType == image.sType);
    REQUIRE(decoded.data.pImage->pNext == nullptr);
    REQUIRE(decoded.data.pImage->layout == image.layout);
    REQUIRE(decoded.data.pImage->pView != nullptr);
    REQUIRE(decoded.data.pImage->pView->sType == image_view.sType);
    REQUIRE(decoded.data.pImage->pView->viewType == image_view.viewType);
    REQUIRE(decoded.data.pImage->pView->format == image_view.format);
}

TEST_CASE_METHOD(ResourceDescriptorFixture,
                 "VkResourceDescriptorInfoEXT tensor data round trips",
                 "[enc/dec][descriptor_heap]")
{
    VkTensorViewCreateInfoARM   tensor_view{ VK_STRUCTURE_TYPE_TENSOR_VIEW_CREATE_INFO_ARM,
                                           nullptr,
                                           VK_TENSOR_VIEW_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_ARM,
                                           VK_NULL_HANDLE,
                                           VK_FORMAT_R8_UINT };
    VkResourceDescriptorInfoEXT resource{
        VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT, nullptr, VK_DESCRIPTOR_TYPE_TENSOR_ARM, {}
    };
    resource.data.pTensorARM = &tensor_view;

    RoundTrip(resource);

    REQUIRE(wrapper.data->pTensorARM != nullptr);
    REQUIRE(wrapper.data->pTensorARM->GetPointer() == decoded.data.pTensorARM);
    REQUIRE(decoded.data.pTensorARM != nullptr);
    REQUIRE(decoded.data.pTensorARM->sType == tensor_view.sType);
    REQUIRE(decoded.data.pTensorARM->pNext == nullptr);
    REQUIRE(decoded.data.pTensorARM->flags == tensor_view.flags);
    REQUIRE(decoded.data.pTensorARM->tensor == VK_NULL_HANDLE);
    REQUIRE(decoded.data.pTensorARM->format == tensor_view.format);
}
