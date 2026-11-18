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
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "vulkan_acceleration_structure_modifier.h"
#include "decode/decode_allocator.h"
#include "decode/value_decoder.h"
#include "encode/parameter_encoder.h"
#include "encode/struct_pointer_encoder.h"

#include <cstring>

using namespace gfxrecon;
using namespace gfxrecon::decode;

TEST_CASE("AS processing replaces size queries while preserving address fixups", "[optimizer][as]")
{
    VulkanAccelerationStructureModifier         modifier;
    ApiCallInfo                                 call{};
    args::GetAccelerationStructureBuildSizesKHR sizes{};
    modifier.Process_vkGetAccelerationStructureBuildSizesKHR(call, sizes);
    CHECK_FALSE(modifier.GetDeleteCurrentCall());

    encode::ParameterBuffer buffer;
    modifier.SetParameterBuffer(&buffer);
    modifier.Process_vkGetAccelerationStructureBuildSizesKHR(call, sizes);
    CHECK(modifier.GetDeleteCurrentCall());
    format::FixDeviceAddressCommandHeader address{};
    modifier.ProcessFixDeviceAddressCommand(address, {});
    CHECK_FALSE(modifier.GetDeleteCurrentCall());
    format::FixShaderGroupHandleCommandHeader handle{};
    modifier.ProcessFixShaderGroupHandleCommand(handle, {});
    CHECK_FALSE(modifier.GetDeleteCurrentCall());
}

TEST_CASE("AS builds and compact copies emit replay preparation", "[optimizer][as]")
{
    decode::DecoderAllocGuard<true>     guard;
    VulkanAccelerationStructureModifier modifier;
    constexpr format::HandleId          buffer_id = 10, source_id = 20, compact_id = 21;
    ApiCallInfo                         call{};

    auto decode_struct = [](auto& decoder, const auto* value) {
        encode::ParameterBuffer  bytes;
        encode::ParameterEncoder encoder(&bytes);
        encode::EncodeStructPtr(&encoder, value);
        decoder.Decode(bytes.GetData(), bytes.GetDataSize());
    };
    auto decode_handle = [](auto& decoder, const format::HandleId* value) {
        encode::ParameterBuffer  bytes;
        encode::ParameterEncoder encoder(&bytes);
        encoder.EncodeHandleIdPtr(value);
        decoder.Decode(bytes.GetData(), bytes.GetDataSize());
    };

    VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_info.size = 4096;
    args::CreateBuffer buffer{};
    buffer.result = VK_SUCCESS;
    decode_struct(buffer.pCreateInfo, &buffer_info);
    decode_handle(buffer.pBuffer, &buffer_id);
    call.index = 1;
    modifier.Process_vkCreateBuffer(call, buffer);

    VkAccelerationStructureCreateInfoKHR as_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    as_info.size = 2048;
    as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    args::CreateAccelerationStructureKHR source{};
    source.result = VK_SUCCESS;
    source.device = 1;
    decode_struct(source.pCreateInfo, &as_info);
    source.pCreateInfo.GetMetaStructPointer()->buffer = buffer_id;
    decode_handle(source.pAccelerationStructure, &source_id);
    call.index = 2;
    modifier.Process_vkCreateAccelerationStructureKHR(call, source);

    VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geometry.geometryType         = VK_GEOMETRY_TYPE_AABBS_KHR;
    geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    VkAccelerationStructureBuildGeometryInfoKHR build_info{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
    };
    build_info.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_info.geometryCount = 1;
    build_info.pGeometries   = &geometry;
    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount                                     = 3;
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &range };
    args::CmdBuildAccelerationStructuresKHR         build{};
    build.infoCount = 1;
    {
        encode::ParameterBuffer  bytes;
        encode::ParameterEncoder encoder(&bytes);
        encode::EncodeStructArray(&encoder, &build_info, 1);
        build.pInfos.Decode(bytes.GetData(), bytes.GetDataSize());
    }
    build.pInfos.GetMetaStructPointer()->dstAccelerationStructure = source_id;
    {
        encode::ParameterBuffer  bytes;
        encode::ParameterEncoder encoder(&bytes);
        encode::EncodeStructArray2D(&encoder, ranges, 1, 1);
        build.ppBuildRangeInfos.Decode(bytes.GetData(), bytes.GetDataSize());
    }
    modifier.Process_vkCmdBuildAccelerationStructuresKHR(call, build);

    args::CreateAccelerationStructureKHR compact{};
    compact.result = VK_SUCCESS;
    compact.device = 1;
    decode_struct(compact.pCreateInfo, &as_info);
    compact.pCreateInfo.GetMetaStructPointer()->buffer = buffer_id;
    decode_handle(compact.pAccelerationStructure, &compact_id);
    call.index = 3;
    modifier.Process_vkCreateAccelerationStructureKHR(call, compact);

    VkCopyAccelerationStructureInfoKHR copy_info{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
    copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
    args::CmdCopyAccelerationStructureKHR copy{};
    decode_struct(copy.pInfo, &copy_info);
    copy.pInfo.GetMetaStructPointer()->src = source_id;
    copy.pInfo.GetMetaStructPointer()->dst = compact_id;
    modifier.Process_vkCmdCopyAccelerationStructureKHR(call, copy);

    encode::ParameterBuffer output;
    modifier.SetParameterBuffer(&output);
    std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> calls;
    modifier.Process_vkCreateAccelerationStructureKHR(call, source);
    modifier.AppendPreCalls(calls);
    REQUIRE_FALSE(calls.empty());
    CHECK(calls.back()->type == util::CallModifierBase::ApiCall);
    CHECK(calls.back()->call_id == format::ApiCallId::ApiCall_vkGetAccelerationStructureBuildSizesKHR);
    const auto*                                 data     = calls.back()->parameter_buffer.GetData();
    const size_t                                length   = calls.back()->parameter_buffer.GetDataSize();
    size_t                                      position = 0;
    args::GetAccelerationStructureBuildSizesKHR query{};
    position += decode::ValueDecoder::DecodeHandleIdValue(data + position, length - position, &query.device);
    position += decode::ValueDecoder::DecodeEnumValue(data + position, length - position, &query.buildType);
    position += query.pBuildInfo.Decode(data + position, length - position);
    position += query.pMaxPrimitiveCounts.DecodeUInt32(data + position, length - position);
    position += query.pSizeInfo.Decode(data + position, length - position);
    CHECK(position == length);
    CHECK(query.device == 1);
    CHECK(query.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR);
    REQUIRE(query.pBuildInfo.GetPointer() != nullptr);
    CHECK(query.pBuildInfo.GetPointer()->type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
    CHECK(query.pBuildInfo.GetPointer()->geometryCount == 1);
    REQUIRE(query.pBuildInfo.GetPointer()->pGeometries != nullptr);
    CHECK(query.pBuildInfo.GetPointer()->pGeometries[0].geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);
    REQUIRE(query.pMaxPrimitiveCounts.GetPointer() != nullptr);
    CHECK(query.pMaxPrimitiveCounts.GetPointer()[0] == 3);

    calls.clear();
    modifier.Process_vkCreateAccelerationStructureKHR(call, compact);
    modifier.AppendPreCalls(calls);
    REQUIRE(calls.size() == 2);
    const auto& dependency = calls.back();
    REQUIRE(dependency->type == util::CallModifierBase::MetaDataCall);
    REQUIRE(dependency->parameter_buffer.GetDataSize() >=
            sizeof(format::ParentToChildDependencyHeader) + sizeof(compact_id));
    format::ParentToChildDependencyHeader header{};
    std::memcpy(&header, dependency->parameter_buffer.GetData(), sizeof(header));
    CHECK(header.dependency_type == format::ParentToChildDependencyType::kAccelerationStructureCompactionDependency);
    CHECK(header.parent_id == source_id);
    CHECK(header.child_count == 1);
    format::HandleId child{};
    std::memcpy(&child, dependency->parameter_buffer.GetData() + sizeof(header), sizeof(child));
    CHECK(child == compact_id);
}
