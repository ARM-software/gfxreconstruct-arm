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

#include "decode/vulkan_micromap_builder.h"
#include "format/format.h"
#include "vulkan_resource_allocator_mock.h"

#include <memory>
#include <utility>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanMicromapBuilderTestAccess
{
  public:
    static void AddPendingCompaction(VulkanMicromapBuilder&   builder,
                                     VkDevice                 device,
                                     VkQueryPool              query_pool,
                                     uint32_t                 first_query,
                                     VkBuffer                 buffer,
                                     size_t                   result_count,
                                     VulkanResourceAllocator* allocator)
    {
        VulkanBufferInfo buffer_info{};
        buffer_info.handle = buffer;

        std::vector<VkMicromapEXT> parents(result_count, VK_NULL_HANDLE);
        builder.compacted_sizes_unprocessed_[query_pool].push_back(
            { first_query,
              std::make_unique<VulkanInternalBufferManager::BufferInfoWrapper>(
                  buffer_info, VulkanDeviceMemoryInfo{}, allocator, device),
              std::move(parents) });
    }
};

struct QueryCopyCapture
{
    VkCommandBuffer       command_buffer{ VK_NULL_HANDLE };
    VkCommandBuffer       barrier_command_buffer{ VK_NULL_HANDLE };
    VkQueryPool           query_pool{ VK_NULL_HANDLE };
    uint32_t              first_query{ 0 };
    uint32_t              query_count{ 0 };
    VkBuffer              buffer{ VK_NULL_HANDLE };
    VkDeviceSize          offset{ 0 };
    VkDeviceSize          stride{ 0 };
    VkQueryResultFlags    flags{ 0 };
    VkPipelineStageFlags  src_stage_mask{ 0 };
    VkPipelineStageFlags  dst_stage_mask{ 0 };
    uint32_t              buffer_barrier_count{ 0 };
    VkBufferMemoryBarrier buffer_barrier{};
};

thread_local QueryCopyCapture query_copy_capture;

VKAPI_ATTR void VKAPI_CALL CaptureCmdCopyQueryPoolResults(VkCommandBuffer    command_buffer,
                                                          VkQueryPool        query_pool,
                                                          uint32_t           first_query,
                                                          uint32_t           query_count,
                                                          VkBuffer           buffer,
                                                          VkDeviceSize       offset,
                                                          VkDeviceSize       stride,
                                                          VkQueryResultFlags flags)
{
    query_copy_capture.command_buffer = command_buffer;
    query_copy_capture.query_pool     = query_pool;
    query_copy_capture.first_query    = first_query;
    query_copy_capture.query_count    = query_count;
    query_copy_capture.buffer         = buffer;
    query_copy_capture.offset         = offset;
    query_copy_capture.stride         = stride;
    query_copy_capture.flags          = flags;
}

VKAPI_ATTR void VKAPI_CALL CaptureCmdPipelineBarrier(VkCommandBuffer      command_buffer,
                                                     VkPipelineStageFlags src_stage_mask,
                                                     VkPipelineStageFlags dst_stage_mask,
                                                     VkDependencyFlags,
                                                     uint32_t,
                                                     const VkMemoryBarrier*,
                                                     uint32_t                     buffer_barrier_count,
                                                     const VkBufferMemoryBarrier* buffer_barriers,
                                                     uint32_t,
                                                     const VkImageMemoryBarrier*)
{
    query_copy_capture.barrier_command_buffer = command_buffer;
    query_copy_capture.src_stage_mask         = src_stage_mask;
    query_copy_capture.dst_stage_mask         = dst_stage_mask;
    query_copy_capture.buffer_barrier_count   = buffer_barrier_count;
    if ((buffer_barrier_count > 0) && (buffer_barriers != nullptr))
    {
        query_copy_capture.buffer_barrier = buffer_barriers[0];
    }
}

template <typename T>
T MakeHandle(format::HandleId id)
{
    return format::FromHandleId<T>(id);
}

TEST_CASE("Micromap compacted-size query barrier covers every result byte", "[decode][micromap][query]")
{
    const size_t result_count = GENERATE(size_t{ 1 }, size_t{ 3 });
    query_copy_capture        = {};

    graphics::VulkanDeviceTable      device_table{};
    VulkanResourceAllocatorMock      allocator{};
    VulkanObjectInfoTable            object_info_table{};
    VulkanDeviceAddressTracker       device_address_tracker{ object_info_table };
    VkPhysicalDeviceMemoryProperties memory_properties{};

    device_table.CmdCopyQueryPoolResults = CaptureCmdCopyQueryPoolResults;
    device_table.CmdPipelineBarrier      = CaptureCmdPipelineBarrier;

    // Will be retrieved by MarkingLayersUtil as key for identifying device
    const void* device_dispatch_table = nullptr;

    const VkDevice        device         = reinterpret_cast<const VkDevice>(&device_dispatch_table);
    const VkCommandBuffer command_buffer = MakeHandle<VkCommandBuffer>(2002);
    const VkQueryPool     query_pool     = MakeHandle<VkQueryPool>(2003);
    const VkBuffer        buffer         = MakeHandle<VkBuffer>(2004);
    constexpr uint32_t    kFirstQuery    = 15;

    VkDeviceCreateInfo            device_create_info;
    graphics::VulkanInstanceTable instance_table;
    allocator.Initialize(nullptr, device, device_create_info, {}, instance_table, &device_table);

    VulkanMicromapBuilder builder(&device_table, device, &allocator, memory_properties, device_address_tracker);
    VulkanMicromapBuilderTestAccess::AddPendingCompaction(
        builder, device, query_pool, kFirstQuery, buffer, result_count, &allocator);

    VulkanCommandBufferInfo command_buffer_info{};
    command_buffer_info.handle = command_buffer;
    VulkanQueryPoolInfo query_pool_info{};
    query_pool_info.handle = query_pool;

    builder.OnCmdCopyQueryPoolResults(&command_buffer_info, &query_pool_info);

    REQUIRE(query_copy_capture.command_buffer == command_buffer);
    REQUIRE(query_copy_capture.barrier_command_buffer == command_buffer);
    REQUIRE(query_copy_capture.query_pool == query_pool);
    REQUIRE(query_copy_capture.first_query == kFirstQuery);
    REQUIRE(query_copy_capture.query_count == result_count);
    REQUIRE(query_copy_capture.buffer == buffer);
    REQUIRE(query_copy_capture.offset == 0);
    REQUIRE(query_copy_capture.stride == sizeof(uint64_t));
    REQUIRE(query_copy_capture.flags == (VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    REQUIRE(query_copy_capture.src_stage_mask == VK_PIPELINE_STAGE_TRANSFER_BIT);
    REQUIRE(query_copy_capture.dst_stage_mask == VK_PIPELINE_STAGE_HOST_BIT);
    REQUIRE(query_copy_capture.buffer_barrier_count == 1);
    REQUIRE(query_copy_capture.buffer_barrier.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
    REQUIRE(query_copy_capture.buffer_barrier.srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
    REQUIRE(query_copy_capture.buffer_barrier.dstAccessMask == VK_ACCESS_HOST_READ_BIT);
    REQUIRE(query_copy_capture.buffer_barrier.buffer == buffer);
    REQUIRE(query_copy_capture.buffer_barrier.offset == 0);
    REQUIRE(query_copy_capture.buffer_barrier.size == result_count * sizeof(uint64_t));
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
