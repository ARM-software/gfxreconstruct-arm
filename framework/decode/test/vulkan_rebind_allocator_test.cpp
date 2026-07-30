/*
** Copyright (c) 2026 LunarG, Inc
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

#include <gmock/gmock.h>

#include "decode/vulkan_rebind_allocator.h"
#include "util/logging.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class VulkanRebindAllocatorTestAccess
{
  public:
    using ResourceAllocInfo = VulkanRebindAllocator::ResourceAllocInfo;
    using MemoryAllocInfo   = VulkanRebindAllocator::MemoryAllocInfo;
    using VmaMemoryInfo     = VulkanRebindAllocator::VmaMemoryInfo;
    using StagingResources  = VulkanRebindAllocator::StagingResources;
    using VmaBackend        = VulkanRebindAllocator::VmaBackend;

    static ResourceAllocInfo MakeResourceAllocInfo(VkObjectType object_type, VkFormat format = VK_FORMAT_UNDEFINED)
    {
        ResourceAllocInfo resource_alloc_info{};
        resource_alloc_info.object_type = object_type;
        resource_alloc_info.format      = format;
        return resource_alloc_info;
    }

    static MemoryAllocInfo MakeMemoryAllocInfo(ResourceAllocInfo* resource_alloc_info, uint64_t original_handle)
    {
        MemoryAllocInfo memory_alloc_info{};
        memory_alloc_info.original_objects.emplace(original_handle, resource_alloc_info);
        return memory_alloc_info;
    }

    static VmaMemoryInfo MakeVmaMemoryInfo(MemoryAllocInfo* memory_alloc_info, void* mapped_pointer)
    {
        VmaMemoryInfo memory_info{};
        memory_info.memory_info    = memory_alloc_info;
        memory_info.mapped_pointer = mapped_pointer;
        return memory_info;
    }

    static void SetState(VulkanRebindAllocator&                    allocator,
                         VkDevice                                  device,
                         VkCommandPool                             cmd_pool,
                         VkQueue                                   staging_queue,
                         const VulkanResourceAllocator::Functions& functions,
                         VmaBackend*                               vma_backend = nullptr)
    {
        allocator.device_        = device;
        allocator.cmd_pool_      = cmd_pool;
        allocator.staging_queue_ = staging_queue;
        allocator.functions_     = functions;
        allocator.allocator_     = reinterpret_cast<VmaAllocator>(static_cast<uintptr_t>(1));
        if (vma_backend != nullptr)
        {
            allocator.vma_backend_ = vma_backend;
        }
    }

    static void SetMemoryProperties(VulkanRebindAllocator&                  allocator,
                                    const VkPhysicalDeviceMemoryProperties& capture_memory_properties,
                                    const VkPhysicalDeviceMemoryProperties& replay_memory_properties)
    {
        allocator.capture_memory_properties_ = capture_memory_properties;
        allocator.replay_memory_properties_  = replay_memory_properties;
    }

    static VmaMemoryUsage GetImageMemoryUsage(VulkanRebindAllocator&      allocator,
                                              VkImageUsageFlags           image_usage,
                                              VkImageTiling               tiling,
                                              VkMemoryPropertyFlags       capture_properties,
                                              const VkMemoryRequirements& replay_requirements)
    {
        return allocator.GetImageMemoryUsage(image_usage, tiling, capture_properties, replay_requirements);
    }

    static VmaMemoryUsage AdjustMemoryUsage(VulkanRebindAllocator&      allocator,
                                            VmaMemoryUsage              desired_usage,
                                            const VkMemoryRequirements& replay_requirements)
    {
        return allocator.AdjustMemoryUsage(desired_usage, replay_requirements);
    }

    static std::vector<VmaAllocationCreateInfo> BuildAllocationRequests(VulkanRebindAllocator&         allocator,
                                                                        const VmaAllocationCreateInfo& base_request,
                                                                        VkMemoryPropertyFlags capture_properties)
    {
        return allocator.BuildAllocationRequests(base_request, capture_properties);
    }

    static VkResult AllocateMemoryForImage(VulkanRebindAllocator&                  allocator,
                                           VkImage                                 image,
                                           VkDeviceSize                            memory_offset,
                                           const VkPhysicalDeviceMemoryProperties& device_memory_properties,
                                           ResourceAllocInfo&                      resource_alloc_info,
                                           MemoryAllocInfo&                        memory_alloc_info,
                                           VmaMemoryInfo**                         vma_mem_info)
    {
        return allocator.AllocateMemoryForImage(
            image, memory_offset, device_memory_properties, resource_alloc_info, memory_alloc_info, vma_mem_info);
    }

    static void RemoveDataGraphSessionMemory(VulkanRebindAllocator&        allocator,
                                             ResourceAllocInfo&            resource_alloc_info,
                                             VkDataGraphPipelineSessionARM session)
    {
        allocator.RemoveVmaMemoryInfo(resource_alloc_info, VK_HANDLE_TO_UINT64(session));
    }

    static VkResult InitializeDataGraphSessionMemory(VulkanRebindAllocator&        allocator,
                                                     VkDataGraphPipelineSessionARM session,
                                                     ResourceAllocInfo&            resource_alloc_info)
    {
        return allocator.InitializeDataGraphPipelineSessionMemory(session, &resource_alloc_info);
    }

    static std::vector<StagingResources>& GetStagingResources(VulkanRebindAllocator& allocator)
    {
        return allocator.staging_resources_;
    }

    static void WriteBoundResourceStaging(VulkanRebindAllocator& allocator,
                                          ResourceAllocInfo*     resource_alloc_info,
                                          VmaMemoryInfo*         bound_memory_info,
                                          size_t                 src_offset,
                                          size_t                 dst_offset,
                                          size_t                 data_size,
                                          const uint8_t*         data)
    {
        allocator.WriteBoundResourceStaging(
            resource_alloc_info, bound_memory_info, src_offset, dst_offset, data_size, data);
    }

    static void WriteBoundResource(VulkanRebindAllocator& allocator,
                                   ResourceAllocInfo*     resource_alloc_info,
                                   VmaMemoryInfo*         bound_memory_info,
                                   size_t                 src_offset,
                                   size_t                 dst_offset,
                                   size_t                 data_size,
                                   const uint8_t*         data)
    {
        allocator.WriteBoundResource(resource_alloc_info, bound_memory_info, src_offset, dst_offset, data_size, data);
    }
};

namespace rebind_allocator_test
{
using ::testing::_;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Truly;

template <typename T>
T MakeHandle(uint64_t value)
{
    if constexpr (std::is_pointer_v<T>)
    {
        return reinterpret_cast<T>(static_cast<uintptr_t>(value));
    }
    else
    {
        return static_cast<T>(value);
    }
}

class MockVulkanFunctions
{
  public:
    MOCK_METHOD(VkResult, AllocateCommandBuffers, (VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*));
    MOCK_METHOD(VkResult, BeginCommandBuffer, (VkCommandBuffer, const VkCommandBufferBeginInfo*));
    MOCK_METHOD(void, CmdCopyBuffer, (VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*));
    MOCK_METHOD(void,
                CmdCopyBufferToImage,
                (VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*));
    MOCK_METHOD(VkResult, EndCommandBuffer, (VkCommandBuffer));
    MOCK_METHOD(VkResult,
                CreateSemaphore,
                (VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore*));
    MOCK_METHOD(VkResult, CreateFence, (VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence*));
    MOCK_METHOD(VkResult, QueueSubmit, (VkQueue, uint32_t, const VkSubmitInfo*, VkFence));
    MOCK_METHOD(VkResult,
                GetDataGraphBindPointRequirements,
                (VkDevice,
                 const VkDataGraphPipelineSessionBindPointRequirementsInfoARM*,
                 uint32_t*,
                 VkDataGraphPipelineSessionBindPointRequirementARM*));
    MOCK_METHOD(void,
                GetDataGraphMemoryRequirements,
                (VkDevice, const VkDataGraphPipelineSessionMemoryRequirementsInfoARM*, VkMemoryRequirements2*));
    MOCK_METHOD(VkResult,
                BindDataGraphSessionMemory,
                (VkDevice, uint32_t, const VkBindDataGraphPipelineSessionMemoryInfoARM*));
};

class MockVmaBackend : public gfxrecon::decode::VulkanRebindAllocatorTestAccess::VmaBackend
{
  public:
    MOCK_METHOD(void,
                GetImageMemoryRequirements,
                (VmaAllocator, VkImage, VkMemoryRequirements&, bool&, bool&),
                (override));
    MOCK_METHOD(VkResult,
                CreateBuffer,
                (VmaAllocator,
                 const VkBufferCreateInfo*,
                 const VmaAllocationCreateInfo*,
                 VkBuffer*,
                 VmaAllocation*,
                 VmaAllocationInfo*),
                (override));
    MOCK_METHOD(VkResult,
                AllocateMemoryForImage,
                (VmaAllocator, VkImage, const VmaAllocationCreateInfo*, VmaAllocation*, VmaAllocationInfo*),
                (override));
    MOCK_METHOD(VkResult, MapMemory, (VmaAllocator, VmaAllocation, void**), (override));
    MOCK_METHOD(
        VkResult,
        AllocateMemory,
        (VmaAllocator, const VkMemoryRequirements*, const VmaAllocationCreateInfo*, VmaAllocation*, VmaAllocationInfo*),
        (override));
    MOCK_METHOD(void, FreeMemory, (VmaAllocator, VmaAllocation), (override));
    MOCK_METHOD(void, FlushAllocation, (VmaAllocator, VmaAllocation, VkDeviceSize, VkDeviceSize), (override));
    MOCK_METHOD(void, UnmapMemory, (VmaAllocator, VmaAllocation), (override));
};

thread_local MockVulkanFunctions* g_mock_vulkan_functions = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL
GetDataGraphBindPointRequirementsThunk(VkDevice                                                      device,
                                       const VkDataGraphPipelineSessionBindPointRequirementsInfoARM* info,
                                       uint32_t*                                                     requirement_count,
                                       VkDataGraphPipelineSessionBindPointRequirementARM*            requirements)
{
    return g_mock_vulkan_functions->GetDataGraphBindPointRequirements(device, info, requirement_count, requirements);
}

VKAPI_ATTR void VKAPI_CALL
GetDataGraphMemoryRequirementsThunk(VkDevice                                                   device,
                                    const VkDataGraphPipelineSessionMemoryRequirementsInfoARM* info,
                                    VkMemoryRequirements2*                                     memory_requirements)
{
    g_mock_vulkan_functions->GetDataGraphMemoryRequirements(device, info, memory_requirements);
}

VKAPI_ATTR VkResult VKAPI_CALL BindDataGraphSessionMemoryThunk(
    VkDevice device, uint32_t bind_info_count, const VkBindDataGraphPipelineSessionMemoryInfoARM* bind_infos)
{
    return g_mock_vulkan_functions->BindDataGraphSessionMemory(device, bind_info_count, bind_infos);
}

VKAPI_ATTR VkResult VKAPI_CALL AllocateCommandBuffersThunk(VkDevice                           device,
                                                           const VkCommandBufferAllocateInfo* allocate_info,
                                                           VkCommandBuffer*                   command_buffer)
{
    return g_mock_vulkan_functions->AllocateCommandBuffers(device, allocate_info, command_buffer);
}

VKAPI_ATTR VkResult VKAPI_CALL BeginCommandBufferThunk(VkCommandBuffer                 command_buffer,
                                                       const VkCommandBufferBeginInfo* begin_info)
{
    return g_mock_vulkan_functions->BeginCommandBuffer(command_buffer, begin_info);
}

VKAPI_ATTR void VKAPI_CALL CmdCopyBufferThunk(VkCommandBuffer     command_buffer,
                                              VkBuffer            src_buffer,
                                              VkBuffer            dst_buffer,
                                              uint32_t            region_count,
                                              const VkBufferCopy* regions)
{
    g_mock_vulkan_functions->CmdCopyBuffer(command_buffer, src_buffer, dst_buffer, region_count, regions);
}

VKAPI_ATTR void VKAPI_CALL CmdCopyBufferToImageThunk(VkCommandBuffer          command_buffer,
                                                     VkBuffer                 src_buffer,
                                                     VkImage                  dst_image,
                                                     VkImageLayout            dst_image_layout,
                                                     uint32_t                 region_count,
                                                     const VkBufferImageCopy* regions)
{
    g_mock_vulkan_functions->CmdCopyBufferToImage(
        command_buffer, src_buffer, dst_image, dst_image_layout, region_count, regions);
}

VKAPI_ATTR VkResult VKAPI_CALL EndCommandBufferThunk(VkCommandBuffer command_buffer)
{
    return g_mock_vulkan_functions->EndCommandBuffer(command_buffer);
}

VKAPI_ATTR VkResult VKAPI_CALL CreateSemaphoreThunk(VkDevice                     device,
                                                    const VkSemaphoreCreateInfo* create_info,
                                                    const VkAllocationCallbacks* allocation_callbacks,
                                                    VkSemaphore*                 semaphore)
{
    return g_mock_vulkan_functions->CreateSemaphore(device, create_info, allocation_callbacks, semaphore);
}

VKAPI_ATTR VkResult VKAPI_CALL CreateFenceThunk(VkDevice                     device,
                                                const VkFenceCreateInfo*     create_info,
                                                const VkAllocationCallbacks* allocation_callbacks,
                                                VkFence*                     fence)
{
    return g_mock_vulkan_functions->CreateFence(device, create_info, allocation_callbacks, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL QueueSubmitThunk(VkQueue             queue,
                                                uint32_t            submit_count,
                                                const VkSubmitInfo* submit_info,
                                                VkFence             fence)
{
    return g_mock_vulkan_functions->QueueSubmit(queue, submit_count, submit_info, fence);
}

inline Matcher<const VkBufferCopy*> BufferCopyRegion(size_t src_offset, size_t dst_offset, size_t size)
{
    return Truly([=](const VkBufferCopy* region) {
        return (region != nullptr) && (region->srcOffset == src_offset) && (region->dstOffset == dst_offset) &&
               (region->size == size);
    });
}

inline Matcher<const VkBufferImageCopy*>
ImageCopyRegion(VkImageAspectFlags aspect_mask, uint32_t width, uint32_t height)
{
    return Truly([=](const VkBufferImageCopy* region) {
        return (region != nullptr) && (region->bufferOffset == 0) &&
               (region->imageSubresource.aspectMask == aspect_mask) && (region->imageExtent.width == width) &&
               (region->imageExtent.height == height) && (region->imageExtent.depth == 1);
    });
}

inline Matcher<const VkSubmitInfo*> SubmitInfoWithSignals(VkCommandBuffer command_buffer, VkSemaphore signal_semaphore)
{
    return Truly([=](const VkSubmitInfo* submit_info) {
        return (submit_info != nullptr) && (submit_info->commandBufferCount == 1) &&
               (submit_info->pCommandBuffers != nullptr) && (submit_info->pCommandBuffers[0] == command_buffer) &&
               (submit_info->waitSemaphoreCount == 0) && (submit_info->signalSemaphoreCount == 1) &&
               (submit_info->pSignalSemaphores != nullptr) && (submit_info->pSignalSemaphores[0] == signal_semaphore);
    });
}

inline Matcher<const VkSubmitInfo*> SubmitInfoWaitingOn(VkSemaphore wait_semaphore)
{
    return Truly([=](const VkSubmitInfo* submit_info) {
        return (submit_info != nullptr) && (submit_info->waitSemaphoreCount == 1) &&
               (submit_info->pWaitSemaphores != nullptr) && (submit_info->pWaitDstStageMask != nullptr) &&
               (submit_info->pWaitSemaphores[0] == wait_semaphore) &&
               (submit_info->pWaitDstStageMask[0] == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    });
}

inline VkPhysicalDeviceMemoryProperties MakeMemoryProperties(std::initializer_list<VkMemoryPropertyFlags> flags)
{
    VkPhysicalDeviceMemoryProperties properties{};
    properties.memoryTypeCount = static_cast<uint32_t>(flags.size());
    properties.memoryHeapCount = std::max<uint32_t>(1, properties.memoryTypeCount);

    uint32_t index = 0;
    for (auto property_flags : flags)
    {
        properties.memoryTypes[index].propertyFlags = property_flags;
        properties.memoryTypes[index].heapIndex     = index;
        properties.memoryHeaps[index].size          = 4096;
        ++index;
    }

    return properties;
}

inline VkMemoryRequirements
MakeMemoryRequirements(uint32_t memory_type_bits, VkDeviceSize size = 64, VkDeviceSize alignment = 16)
{
    VkMemoryRequirements requirements{};
    requirements.size           = size;
    requirements.alignment      = alignment;
    requirements.memoryTypeBits = memory_type_bits;
    return requirements;
}

inline uint32_t MakeMemoryTypeBits(std::initializer_list<uint32_t> memory_type_indices)
{
    uint32_t memory_type_bits = 0;
    for (uint32_t index : memory_type_indices)
    {
        memory_type_bits |= (1u << index);
    }
    return memory_type_bits;
}

inline VkMemoryRequirements MakeMemoryRequirements(std::initializer_list<uint32_t> memory_type_indices,
                                                   VkDeviceSize                    size      = 64,
                                                   VkDeviceSize                    alignment = 16)
{
    return MakeMemoryRequirements(MakeMemoryTypeBits(memory_type_indices), size, alignment);
}

constexpr uint32_t kHostVisibleTypeIndex = 0;
constexpr uint32_t kDeviceLocalTypeIndex = 1;
constexpr uint32_t kLazyTypeIndex        = 2;

struct ImageMemorySelectionFixture
{
    using ResourceAllocInfo = gfxrecon::decode::VulkanRebindAllocatorTestAccess::ResourceAllocInfo;
    using MemoryAllocInfo   = gfxrecon::decode::VulkanRebindAllocatorTestAccess::MemoryAllocInfo;
    using VmaMemoryInfo     = gfxrecon::decode::VulkanRebindAllocatorTestAccess::VmaMemoryInfo;

    StrictMock<MockVmaBackend>              mock_vma_backend;
    gfxrecon::decode::VulkanRebindAllocator allocator;
    VkPhysicalDeviceMemoryProperties        capture_memory_properties =
        MakeMemoryProperties({ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT });
    VkPhysicalDeviceMemoryProperties replay_memory_properties =
        MakeMemoryProperties({ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT });

    ImageMemorySelectionFixture()
    {
        VulkanResourceAllocator::Functions functions{};
        // A fake allocator handle is sufficient because the backend intercepts the VMA calls exercised by these tests.
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::SetState(
            allocator, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, functions, &mock_vma_backend);
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::SetMemoryProperties(
            allocator, capture_memory_properties, replay_memory_properties);
    }
};

struct WriteBoundResourceStagingFixture
{
    using ResourceAllocInfo = gfxrecon::decode::VulkanRebindAllocatorTestAccess::ResourceAllocInfo;
    using MemoryAllocInfo   = gfxrecon::decode::VulkanRebindAllocatorTestAccess::MemoryAllocInfo;
    using VmaMemoryInfo     = gfxrecon::decode::VulkanRebindAllocatorTestAccess::VmaMemoryInfo;
    using StagingResources  = gfxrecon::decode::VulkanRebindAllocatorTestAccess::StagingResources;

    StrictMock<MockVulkanFunctions>                                    mock_vulkan_functions;
    StrictMock<MockVmaBackend>                                         mock_vma_backend;
    gfxrecon::decode::VulkanRebindAllocator                            allocator;
    gfxrecon::decode::VulkanResourceAllocator::Functions               functions{};
    ResourceAllocInfo                                                  resource_alloc_info;
    std::array<uint8_t, 32>                                            original_memory{};
    std::array<uint8_t, 32>                                            staging_memory{};
    MemoryAllocInfo                                                    memory_alloc_info;
    VmaMemoryInfo                                                      bound_memory_info;
    std::vector<uint8_t>                                               write_data{ 0, 1, 2, 3, 4, 5, 6, 7 };
    std::unique_ptr<gfxrecon::decode::VulkanAndroidHardwareBufferInfo> ahb_info;
    VkDevice                                                           device       = MakeHandle<VkDevice>(0x1001);
    VkCommandPool                                                      command_pool = MakeHandle<VkCommandPool>(0x1002);
    VkQueue                                                            staging_queue = MakeHandle<VkQueue>(0x1003);
    VkCommandBuffer command_buffer     = MakeHandle<VkCommandBuffer>(0x1004);
    VkBuffer        staging_buffer     = MakeHandle<VkBuffer>(0x4001);
    VmaAllocation   staging_allocation = reinterpret_cast<VmaAllocation>(static_cast<uintptr_t>(0x5001));
    VkSemaphore     signal_semaphore   = MakeHandle<VkSemaphore>(0x1005);
    VkFence         signal_fence       = MakeHandle<VkFence>(0x1006);

    WriteBoundResourceStagingFixture(VkObjectType object_type,
                                     uint64_t     original_handle,
                                     VkFormat     format = VK_FORMAT_UNDEFINED) :
        resource_alloc_info(
            gfxrecon::decode::VulkanRebindAllocatorTestAccess::MakeResourceAllocInfo(object_type, format)),
        memory_alloc_info(gfxrecon::decode::VulkanRebindAllocatorTestAccess::MakeMemoryAllocInfo(&resource_alloc_info,
                                                                                                 original_handle)),
        bound_memory_info(gfxrecon::decode::VulkanRebindAllocatorTestAccess::MakeVmaMemoryInfo(&memory_alloc_info,
                                                                                               original_memory.data()))
    {
        gfxrecon::util::Log::Init(gfxrecon::util::LoggingSeverity::kError);

        functions.allocate_command_buffers = &AllocateCommandBuffersThunk;
        functions.begin_command_buffer     = &BeginCommandBufferThunk;
        functions.cmd_copy_buffer          = &CmdCopyBufferThunk;
        functions.cmd_copy_buffer_to_image = &CmdCopyBufferToImageThunk;
        functions.end_command_buffer       = &EndCommandBufferThunk;
        functions.create_semaphore         = &CreateSemaphoreThunk;
        functions.create_fence             = &CreateFenceThunk;
        functions.queue_submit             = &QueueSubmitThunk;

        gfxrecon::decode::VulkanRebindAllocatorTestAccess::SetState(
            allocator, device, command_pool, staging_queue, functions, &mock_vma_backend);

        g_mock_vulkan_functions = &mock_vulkan_functions;
    }

    ~WriteBoundResourceStagingFixture()
    {
        g_mock_vulkan_functions = nullptr;
        gfxrecon::util::Log::Release();
    }

    void InvokeWrite(size_t dst_offset)
    {
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::WriteBoundResourceStaging(
            allocator, &resource_alloc_info, &bound_memory_info, 0, dst_offset, write_data.size(), write_data.data());
    }

    void ExpectSuccessfulStagingAllocation()
    {
        EXPECT_CALL(mock_vma_backend, CreateBuffer(_, _, _, _, _, _))
            .WillOnce(Invoke([&](VmaAllocator,
                                 const VkBufferCreateInfo*,
                                 const VmaAllocationCreateInfo*,
                                 VkBuffer*          buffer,
                                 VmaAllocation*     allocation,
                                 VmaAllocationInfo* allocation_info) {
                *buffer          = staging_buffer;
                *allocation      = staging_allocation;
                *allocation_info = {};
                return VK_SUCCESS;
            }));
        EXPECT_CALL(mock_vma_backend, MapMemory(_, staging_allocation, _))
            .WillOnce(Invoke([&](VmaAllocator, VmaAllocation, void** mapped_pointer) {
                *mapped_pointer = staging_memory.data();
                return VK_SUCCESS;
            }));
        EXPECT_CALL(mock_vma_backend, FlushAllocation(_, staging_allocation, 0, VK_WHOLE_SIZE));
        EXPECT_CALL(mock_vma_backend, UnmapMemory(_, staging_allocation));
    }

    void ExpectAllocateAndBeginCommandBuffer()
    {
        EXPECT_CALL(mock_vulkan_functions, AllocateCommandBuffers(device, _, _))
            .WillOnce(
                Invoke([&](VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* allocated_command_buffer) {
                    *allocated_command_buffer = command_buffer;
                    return VK_SUCCESS;
                }));
        EXPECT_CALL(mock_vulkan_functions, BeginCommandBuffer(command_buffer, _)).WillOnce(Return(VK_SUCCESS));
    }

    void ExpectSignalObjectCreation()
    {
        EXPECT_CALL(mock_vulkan_functions, CreateSemaphore(device, _, nullptr, _))
            .WillOnce(Invoke(
                [&](VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore* semaphore) {
                    *semaphore = signal_semaphore;
                    return VK_SUCCESS;
                }));
        EXPECT_CALL(mock_vulkan_functions, CreateFence(device, _, nullptr, _))
            .WillOnce(Invoke([&](VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
                *fence = signal_fence;
                return VK_SUCCESS;
            }));
    }

    void InstallAhbInfo(uint32_t width, const std::vector<uint32_t>& plane_heights)
    {
        const auto original_image = MakeHandle<VkImage>(memory_alloc_info.original_objects.begin()->first);

        ahb_info        = std::make_unique<gfxrecon::decode::VulkanAndroidHardwareBufferInfo>();
        ahb_info->width = width;

        for (auto plane_height : plane_heights)
        {
            gfxrecon::decode::VulkanAndroidHardwareBufferPlaneInfo plane_info{};
            plane_info.height = plane_height;
            ahb_info->plane_info.emplace_back(plane_info);
        }

        memory_alloc_info.original_ahardwarebuffers[original_image] = ahb_info.get();
    }

    void ClearOriginalObjectBindings() { memory_alloc_info.original_objects.clear(); }

    void SeedPriorStagingWrite(size_t dst_offset, VkSemaphore semaphore)
    {
        StagingResources previous_staging{};
        previous_staging.resource_alloc_info = &resource_alloc_info;
        previous_staging.dst_offset          = dst_offset;
        previous_staging.staging_semaphore   = semaphore;
        staging_resources().push_back(previous_staging);
    }

    std::vector<StagingResources>& staging_resources()
    {
        return gfxrecon::decode::VulkanRebindAllocatorTestAccess::GetStagingResources(allocator);
    }
};

} // namespace rebind_allocator_test

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

using namespace gfxrecon::decode::rebind_allocator_test;

constexpr VkImageUsageFlags kLinearMixedUseImageUsage =
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

TEST_CASE("WriteBoundResourceStaging skips image staging writes with non-zero destination offsets", "[decode][rebind]")
{
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_IMAGE, 0x2001, VK_FORMAT_R8G8B8A8_UNORM);

    // Offset-based image staging is intentionally unsupported today, so the allocator must bail out before creating
    // staging resources or mutating the bound mapping.
    fixture.InvokeWrite(4);

    // This protects replay from recording a buffer-to-image copy that does not describe the requested image offset.
    REQUIRE(fixture.staging_resources().empty());
    REQUIRE(fixture.bound_memory_info.mapped_pointer == fixture.original_memory.data());
}

TEST_CASE("WriteBoundResource skips writes to optimally-tiled images", "[decode][rebind]")
{
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_IMAGE, 0x200a, VK_FORMAT_R8G8B8A8_UNORM);
    fixture.resource_alloc_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    gfxrecon::decode::VulkanRebindAllocatorTestAccess::WriteBoundResource(fixture.allocator,
                                                                          &fixture.resource_alloc_info,
                                                                          &fixture.bound_memory_info,
                                                                          0,
                                                                          0,
                                                                          fixture.write_data.size(),
                                                                          fixture.write_data.data());

    CHECK(fixture.staging_resources().empty());
}

TEST_CASE("WriteBoundResourceStaging stages buffer writes and submits a buffer copy", "[decode][rebind]")
{
    const auto                       original_buffer = MakeHandle<VkBuffer>(0x2002);
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_BUFFER, 0x2002);

    // A successful staging write must allocate a transient upload buffer, record one copy, and submit it once.
    fixture.ExpectSuccessfulStagingAllocation();
    fixture.ExpectAllocateAndBeginCommandBuffer();
    EXPECT_CALL(fixture.mock_vulkan_functions,
                CmdCopyBuffer(fixture.command_buffer,
                              fixture.staging_buffer,
                              original_buffer,
                              1,
                              BufferCopyRegion(0, 8, fixture.write_data.size())));
    EXPECT_CALL(fixture.mock_vulkan_functions, EndCommandBuffer(fixture.command_buffer)).WillOnce(Return(VK_SUCCESS));
    fixture.ExpectSignalObjectCreation();
    EXPECT_CALL(fixture.mock_vulkan_functions,
                QueueSubmit(fixture.staging_queue,
                            1,
                            SubmitInfoWithSignals(fixture.command_buffer, fixture.signal_semaphore),
                            fixture.signal_fence))
        .WillOnce(Return(VK_SUCCESS));

    // The call under test should preserve the caller's mapped pointer while copying the source data into staging.
    fixture.InvokeWrite(8);

    // Persisting the staging resource is required so later queue synchronization can wait on the in-flight upload.
    REQUIRE(fixture.bound_memory_info.mapped_pointer == fixture.original_memory.data());
    REQUIRE(std::equal(fixture.write_data.begin(), fixture.write_data.end(), fixture.staging_memory.begin()));
    REQUIRE(fixture.staging_resources().size() == 1);
    REQUIRE(fixture.staging_resources()[0].resource_alloc_info == &fixture.resource_alloc_info);
    REQUIRE(fixture.staging_resources()[0].dst_offset == 8);
}

TEST_CASE("WriteBoundResourceStaging chains overlapping buffer writes using the previous staging semaphore",
          "[decode][rebind]")
{
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_BUFFER, 0x2003);
    const auto                       previous_semaphore = MakeHandle<VkSemaphore>(0x3001);

    gfxrecon::decode::VulkanRebindAllocatorTestAccess::StagingResources previous_staging{};
    previous_staging.resource_alloc_info = &fixture.resource_alloc_info;
    previous_staging.dst_offset          = 8;
    previous_staging.staging_semaphore   = previous_semaphore;
    fixture.staging_resources().push_back(previous_staging);

    // Multiple staged writes to the same resource before submit must serialize to avoid overwriting in-flight data.
    fixture.ExpectSuccessfulStagingAllocation();
    fixture.ExpectAllocateAndBeginCommandBuffer();
    EXPECT_CALL(fixture.mock_vulkan_functions, CmdCopyBuffer(fixture.command_buffer, _, _, _, _));
    EXPECT_CALL(fixture.mock_vulkan_functions, EndCommandBuffer(fixture.command_buffer)).WillOnce(Return(VK_SUCCESS));
    fixture.ExpectSignalObjectCreation();
    EXPECT_CALL(fixture.mock_vulkan_functions,
                QueueSubmit(fixture.staging_queue, 1, SubmitInfoWaitingOn(previous_semaphore), fixture.signal_fence))
        .WillOnce(Return(VK_SUCCESS));

    // The second write overlaps the earlier range, so the new submit must wait on the earlier staging semaphore.
    fixture.InvokeWrite(12);

    REQUIRE(fixture.staging_resources().size() == 2);
}

TEST_CASE("WriteBoundResourceStaging records a buffer to image copy for image resources", "[decode][rebind]")
{
    const auto                       original_image = MakeHandle<VkImage>(0x2004);
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_IMAGE, 0x2004, VK_FORMAT_R8G8B8A8_UNORM);

    // Image staging writes must translate the upload into a buffer-to-image copy with the expected default region.
    fixture.ExpectSuccessfulStagingAllocation();
    fixture.ExpectAllocateAndBeginCommandBuffer();
    EXPECT_CALL(fixture.mock_vulkan_functions,
                CmdCopyBufferToImage(fixture.command_buffer,
                                     fixture.staging_buffer,
                                     original_image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1,
                                     ImageCopyRegion(VK_IMAGE_ASPECT_COLOR_BIT, 1, 1)));
    EXPECT_CALL(fixture.mock_vulkan_functions, EndCommandBuffer(fixture.command_buffer)).WillOnce(Return(VK_SUCCESS));
    fixture.ExpectSignalObjectCreation();
    EXPECT_CALL(fixture.mock_vulkan_functions, QueueSubmit(fixture.staging_queue, 1, _, fixture.signal_fence))
        .WillOnce(Return(VK_SUCCESS));

    // The raw source bytes still have to be staged first because the image copy reads from the upload buffer.
    fixture.InvokeWrite(0);

    REQUIRE(std::equal(fixture.write_data.begin(), fixture.write_data.end(), fixture.staging_memory.begin()));
}

TEST_CASE("WriteBoundResourceStaging uses the AHB image copy path for single-plane images", "[decode][rebind]")
{
    const auto                       original_image = MakeHandle<VkImage>(0x2005);
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_IMAGE, 0x2005, VK_FORMAT_R8G8B8A8_UNORM);

    // Current AHB staging assumes a single plane can be copied as one color aspect image with extent derived from
    // the tracked AHB metadata. This test documents that assumption without endorsing it as correct.
    fixture.InstallAhbInfo(16, { 9 });
    fixture.ExpectSuccessfulStagingAllocation();
    fixture.ExpectAllocateAndBeginCommandBuffer();
    EXPECT_CALL(fixture.mock_vulkan_functions,
                CmdCopyBufferToImage(fixture.command_buffer,
                                     fixture.staging_buffer,
                                     original_image,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     1,
                                     ImageCopyRegion(VK_IMAGE_ASPECT_COLOR_BIT, 16, 9)));
    EXPECT_CALL(fixture.mock_vulkan_functions, EndCommandBuffer(fixture.command_buffer)).WillOnce(Return(VK_SUCCESS));
    fixture.ExpectSignalObjectCreation();
    EXPECT_CALL(fixture.mock_vulkan_functions, QueueSubmit(fixture.staging_queue, 1, _, fixture.signal_fence))
        .WillOnce(Return(VK_SUCCESS));

    // The upload still goes through staging memory first, even though the later copy region is driven entirely by
    // AHB metadata rather than image create information.
    fixture.InvokeWrite(0);

    REQUIRE(std::equal(fixture.write_data.begin(), fixture.write_data.end(), fixture.staging_memory.begin()));
    REQUIRE(fixture.staging_resources().size() == 1);
}

TEST_CASE("WriteBoundResourceStaging uploads AHB multi-plane images without recording an image copy",
          "[decode][rebind]")
{
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_IMAGE, 0x2006, VK_FORMAT_R8G8B8A8_UNORM);

    // The current implementation assumes multi-plane AHB images can be skipped after the staging upload because only
    // the single-plane path records a buffer-to-image copy. This test locks down that unsupported behavior.
    fixture.InstallAhbInfo(12, { 7, 3 });
    fixture.ExpectSuccessfulStagingAllocation();
    fixture.ExpectAllocateAndBeginCommandBuffer();
    EXPECT_CALL(fixture.mock_vulkan_functions, EndCommandBuffer(fixture.command_buffer)).WillOnce(Return(VK_SUCCESS));
    fixture.ExpectSignalObjectCreation();
    EXPECT_CALL(fixture.mock_vulkan_functions, QueueSubmit(fixture.staging_queue, 1, _, fixture.signal_fence))
        .WillOnce(Return(VK_SUCCESS));

    // A passing result here does not mean the image update is correct; it only proves the current code submits work
    // without ever describing how the staged bytes reach the destination image.
    fixture.InvokeWrite(0);

    REQUIRE(std::equal(fixture.write_data.begin(), fixture.write_data.end(), fixture.staging_memory.begin()));
    REQUIRE(fixture.staging_resources().size() == 1);
}

TEST_CASE("WriteBoundResourceStaging records one copy per image aspect for non-AHB depth stencil images",
          "[decode][rebind]")
{
    const auto                       original_image = MakeHandle<VkImage>(0x2007);
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_IMAGE, 0x2007, VK_FORMAT_D24_UNORM_S8_UINT);

    // The implementation currently assumes non-AHB image uploads can be represented as synthetic 1x1x1 copies for
    // each format aspect, which exposes how little real image shape information this path is using.
    fixture.ExpectSuccessfulStagingAllocation();
    fixture.ExpectAllocateAndBeginCommandBuffer();
    EXPECT_CALL(fixture.mock_vulkan_functions,
                CmdCopyBufferToImage(fixture.command_buffer,
                                     fixture.staging_buffer,
                                     original_image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1,
                                     ImageCopyRegion(VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1)));
    EXPECT_CALL(fixture.mock_vulkan_functions,
                CmdCopyBufferToImage(fixture.command_buffer,
                                     fixture.staging_buffer,
                                     original_image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1,
                                     ImageCopyRegion(VK_IMAGE_ASPECT_STENCIL_BIT, 1, 1)));
    EXPECT_CALL(fixture.mock_vulkan_functions, EndCommandBuffer(fixture.command_buffer)).WillOnce(Return(VK_SUCCESS));
    fixture.ExpectSignalObjectCreation();
    EXPECT_CALL(fixture.mock_vulkan_functions, QueueSubmit(fixture.staging_queue, 1, _, fixture.signal_fence))
        .WillOnce(Return(VK_SUCCESS));

    // This verifies the current branching behavior while also making the hard-coded extent assumption explicit.
    fixture.InvokeWrite(0);

    REQUIRE(std::equal(fixture.write_data.begin(), fixture.write_data.end(), fixture.staging_memory.begin()));
}

TEST_CASE("WriteBoundResourceStaging chains repeated image uploads on the prior staging semaphore", "[decode][rebind]")
{
    const auto                       previous_semaphore = MakeHandle<VkSemaphore>(0x3002);
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_IMAGE, 0x2008, VK_FORMAT_R8G8B8A8_UNORM);

    // Image staging only supports dst_offset zero, so the current overlap heuristic always treats repeated image
    // uploads as conflicting writes to the same byte range and serializes them.
    fixture.SeedPriorStagingWrite(0, previous_semaphore);
    fixture.ExpectSuccessfulStagingAllocation();
    fixture.ExpectAllocateAndBeginCommandBuffer();
    EXPECT_CALL(fixture.mock_vulkan_functions, CmdCopyBufferToImage(fixture.command_buffer, _, _, _, _, _));
    EXPECT_CALL(fixture.mock_vulkan_functions, EndCommandBuffer(fixture.command_buffer)).WillOnce(Return(VK_SUCCESS));
    fixture.ExpectSignalObjectCreation();
    EXPECT_CALL(fixture.mock_vulkan_functions,
                QueueSubmit(fixture.staging_queue, 1, SubmitInfoWaitingOn(previous_semaphore), fixture.signal_fence))
        .WillOnce(Return(VK_SUCCESS));

    // The requirement here is only that the current serialization heuristic is preserved until image overlap tracking
    // is modeled more accurately.
    fixture.InvokeWrite(0);

    REQUIRE(fixture.staging_resources().size() == 2);
}

TEST_CASE("WriteBoundResourceStaging submits image staging work even when no destination image handle is found",
          "[decode][rebind]")
{
    WriteBoundResourceStagingFixture fixture(VK_OBJECT_TYPE_IMAGE, 0x2009, VK_FORMAT_R8G8B8A8_UNORM);

    // The implementation currently assumes it is acceptable to submit the staging command buffer even when it cannot
    // resolve a destination image handle and therefore records no image copy command at all.
    fixture.ClearOriginalObjectBindings();
    fixture.ExpectSuccessfulStagingAllocation();
    fixture.ExpectAllocateAndBeginCommandBuffer();
    EXPECT_CALL(fixture.mock_vulkan_functions, EndCommandBuffer(fixture.command_buffer)).WillOnce(Return(VK_SUCCESS));
    fixture.ExpectSignalObjectCreation();
    EXPECT_CALL(fixture.mock_vulkan_functions, QueueSubmit(fixture.staging_queue, 1, _, fixture.signal_fence))
        .WillOnce(Return(VK_SUCCESS));

    // This locks down the current no-op upload behavior so a future correctness fix can change it intentionally.
    fixture.InvokeWrite(0);

    REQUIRE(std::equal(fixture.write_data.begin(), fixture.write_data.end(), fixture.staging_memory.begin()));
    REQUIRE(fixture.staging_resources().size() == 1);
}

TEST_CASE("GetImageMemoryUsage selects expected usage classes for representative image inputs", "[decode][rebind]")
{
    struct TestCase
    {
        const char*           name;
        VkImageUsageFlags     image_usage;
        VkImageTiling         tiling;
        VkMemoryPropertyFlags capture_properties;
        VkMemoryRequirements  replay_requirements;
        VmaMemoryUsage        expected_usage;
    };

    // Keep the selector coverage table-driven so later policy changes can update expectations in one place.
    const std::vector<TestCase> test_cases = {
        { "optimal tiling defaults to gpu-only",
          VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          MakeMemoryRequirements({ kDeviceLocalTypeIndex }),
          VMA_MEMORY_USAGE_GPU_ONLY },
        { "linear transfer src prefers cpu-only staging",
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          VK_IMAGE_TILING_LINEAR,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
          MakeMemoryRequirements({ kHostVisibleTypeIndex }),
          VMA_MEMORY_USAGE_CPU_ONLY },
        { "linear transfer dst prefers gpu-to-cpu readback",
          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          VK_IMAGE_TILING_LINEAR,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
          MakeMemoryRequirements({ kHostVisibleTypeIndex }),
          VMA_MEMORY_USAGE_GPU_TO_CPU },
        { "linear mixed usage falls back to cpu-to-gpu",
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_IMAGE_TILING_LINEAR,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
          MakeMemoryRequirements({ kHostVisibleTypeIndex }),
          VMA_MEMORY_USAGE_CPU_TO_GPU },
        { "host cached capture memory keeps the image in gpu-to-cpu usage",
          VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
          MakeMemoryRequirements({ kHostVisibleTypeIndex }),
          VMA_MEMORY_USAGE_GPU_TO_CPU },
        { "non-device-local capture memory demotes gpu-only to cpu-to-gpu",
          VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
          MakeMemoryRequirements({ kHostVisibleTypeIndex }),
          VMA_MEMORY_USAGE_CPU_TO_GPU },
        { "device-local capture memory can promote a linear upload image back to gpu-only",
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          VK_IMAGE_TILING_LINEAR,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          MakeMemoryRequirements({ kDeviceLocalTypeIndex }),
          VMA_MEMORY_USAGE_GPU_ONLY },
        { "transient lazily allocated images request lazy device memory",
          VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,
          MakeMemoryRequirements({ kLazyTypeIndex }),
          VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED },
        { "amd property bits are ignored when selecting host-cached usage",
          VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
              VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
          MakeMemoryRequirements({ kHostVisibleTypeIndex }),
          VMA_MEMORY_USAGE_GPU_TO_CPU },
        { "optimal sampled images on device-local host-visible memory stay gpu-only",
          VK_IMAGE_USAGE_SAMPLED_BIT,
          VK_IMAGE_TILING_OPTIMAL,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
          MakeMemoryRequirements({ kHostVisibleTypeIndex, kDeviceLocalTypeIndex }),
          VMA_MEMORY_USAGE_GPU_ONLY },
        { "linear mixed-use images on device-local host-visible memory stay cpu-to-gpu",
          kLinearMixedUseImageUsage,
          VK_IMAGE_TILING_LINEAR,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
          MakeMemoryRequirements({ kHostVisibleTypeIndex, kDeviceLocalTypeIndex }),
          VMA_MEMORY_USAGE_CPU_TO_GPU },
    };

    ImageMemorySelectionFixture fixture;

    for (const auto& test_case : test_cases)
    {
        INFO(test_case.name);

        const auto usage =
            gfxrecon::decode::VulkanRebindAllocatorTestAccess::GetImageMemoryUsage(fixture.allocator,
                                                                                   test_case.image_usage,
                                                                                   test_case.tiling,
                                                                                   test_case.capture_properties,
                                                                                   test_case.replay_requirements);

        REQUIRE(usage == test_case.expected_usage);
    }
}

TEST_CASE("AdjustMemoryUsage falls back when replay memory types cannot satisfy the requested class",
          "[decode][rebind]")
{
    ImageMemorySelectionFixture fixture;

    SECTION("gpu-only falls back to host visible when no device-local type is allowed")
    {
        const auto usage = gfxrecon::decode::VulkanRebindAllocatorTestAccess::AdjustMemoryUsage(
            fixture.allocator, VMA_MEMORY_USAGE_GPU_ONLY, MakeMemoryRequirements({ kHostVisibleTypeIndex }));
        REQUIRE(usage == VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    SECTION("host-visible usage falls back to gpu-only when host-visible memory is unavailable")
    {
        const auto usage = gfxrecon::decode::VulkanRebindAllocatorTestAccess::AdjustMemoryUsage(
            fixture.allocator, VMA_MEMORY_USAGE_CPU_TO_GPU, MakeMemoryRequirements({ kDeviceLocalTypeIndex }));
        REQUIRE(usage == VMA_MEMORY_USAGE_GPU_ONLY);
    }

    SECTION("lazy usage survives only when a lazily allocated replay type is available")
    {
        const auto usage = gfxrecon::decode::VulkanRebindAllocatorTestAccess::AdjustMemoryUsage(
            fixture.allocator, VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED, MakeMemoryRequirements({ kLazyTypeIndex }));
        REQUIRE(usage == VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED);
    }

    SECTION("lazy usage falls back to gpu-only when no lazy replay type is allowed")
    {
        const auto usage = gfxrecon::decode::VulkanRebindAllocatorTestAccess::AdjustMemoryUsage(
            fixture.allocator,
            VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED,
            MakeMemoryRequirements({ kDeviceLocalTypeIndex }));
        REQUIRE(usage == VMA_MEMORY_USAGE_GPU_ONLY);
    }
}

TEST_CASE("BuildAllocationRequests prefers captured host-coherent memory with a compatibility fallback",
          "[decode][rebind]")
{
    ImageMemorySelectionFixture fixture;

    VmaAllocationCreateInfo base_request{};
    base_request.flags          = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    base_request.usage          = VMA_MEMORY_USAGE_CPU_TO_GPU;
    base_request.requiredFlags  = VK_MEMORY_PROPERTY_PROTECTED_BIT;
    base_request.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    base_request.memoryTypeBits = MakeMemoryTypeBits({ kHostVisibleTypeIndex });

    SECTION("coherent capture memory adds a strict first request and preserves the original fallback")
    {
        const auto requests = gfxrecon::decode::VulkanRebindAllocatorTestAccess::BuildAllocationRequests(
            fixture.allocator,
            base_request,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        REQUIRE(requests.size() == 2);
        REQUIRE(requests[0].flags == base_request.flags);
        REQUIRE(requests[0].usage == base_request.usage);
        REQUIRE(requests[0].requiredFlags == (base_request.requiredFlags | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        REQUIRE(requests[0].preferredFlags == base_request.preferredFlags);
        REQUIRE(requests[0].memoryTypeBits == base_request.memoryTypeBits);
        REQUIRE(requests[1].requiredFlags == base_request.requiredFlags);
        REQUIRE(requests[1].preferredFlags == base_request.preferredFlags);
    }

    SECTION("non-coherent capture memory leaves the allocation request unchanged")
    {
        const auto requests = gfxrecon::decode::VulkanRebindAllocatorTestAccess::BuildAllocationRequests(
            fixture.allocator, base_request, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        REQUIRE(requests.size() == 1);
        REQUIRE(requests[0].requiredFlags == base_request.requiredFlags);
        REQUIRE(requests[0].preferredFlags == base_request.preferredFlags);
    }

    SECTION("an already coherent request is not duplicated")
    {
        base_request.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const auto requests = gfxrecon::decode::VulkanRebindAllocatorTestAccess::BuildAllocationRequests(
            fixture.allocator,
            base_request,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        REQUIRE(requests.size() == 1);
        REQUIRE(requests[0].requiredFlags == base_request.requiredFlags);
    }
}

TEST_CASE("AllocateMemoryForImage forwards the selected VMA usage and tracks the resulting allocation",
          "[decode][rebind]")
{
    ImageMemorySelectionFixture                    fixture;
    ImageMemorySelectionFixture::ResourceAllocInfo resource_alloc_info =
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::MakeResourceAllocInfo(VK_OBJECT_TYPE_IMAGE);
    ImageMemorySelectionFixture::MemoryAllocInfo memory_alloc_info{};
    ImageMemorySelectionFixture::VmaMemoryInfo*  vma_mem_info = nullptr;
    const auto                                   image        = MakeHandle<VkImage>(0x2100);
    const auto              allocation = reinterpret_cast<VmaAllocation>(static_cast<uintptr_t>(0x5002));
    const auto              replay_req = MakeMemoryRequirements({ kHostVisibleTypeIndex }, 128, 32);
    VmaAllocationCreateInfo captured_create_info{};

    resource_alloc_info.usage            = VK_IMAGE_USAGE_SAMPLED_BIT;
    resource_alloc_info.tiling           = VK_IMAGE_TILING_OPTIMAL;
    resource_alloc_info.capture_mem_reqs = { MakeMemoryRequirements({ kHostVisibleTypeIndex }, 96, 32) };
    memory_alloc_info.original_index     = 0;

    EXPECT_CALL(fixture.mock_vma_backend, GetImageMemoryRequirements(_, image, _, _, _))
        .WillOnce(Invoke([&](VmaAllocator,
                             VkImage,
                             VkMemoryRequirements& out_requirements,
                             bool&                 requires_dedicated,
                             bool&                 prefers_dedicated) {
            out_requirements   = replay_req;
            requires_dedicated = true;
            prefers_dedicated  = false;
        }));
    EXPECT_CALL(fixture.mock_vma_backend, AllocateMemoryForImage(_, image, _, _, _))
        .WillOnce(Invoke([&](VmaAllocator,
                             VkImage,
                             const VmaAllocationCreateInfo* create_info,
                             VmaAllocation*                 out_allocation,
                             VmaAllocationInfo*             out_allocation_info) {
            captured_create_info              = *create_info;
            *out_allocation                   = allocation;
            out_allocation_info->memoryType   = 0;
            out_allocation_info->offset       = 0;
            out_allocation_info->size         = replay_req.size;
            out_allocation_info->deviceMemory = MakeHandle<VkDeviceMemory>(0x4100);
            return VK_SUCCESS;
        }));

    // This locks down the exact VMA request shape so future policy changes can update the tests intentionally.
    const auto result =
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::AllocateMemoryForImage(fixture.allocator,
                                                                                  image,
                                                                                  24,
                                                                                  fixture.capture_memory_properties,
                                                                                  resource_alloc_info,
                                                                                  memory_alloc_info,
                                                                                  &vma_mem_info);

    REQUIRE(result == VK_SUCCESS);
    REQUIRE(vma_mem_info != nullptr);
    REQUIRE(captured_create_info.usage == VMA_MEMORY_USAGE_CPU_TO_GPU);
    REQUIRE(captured_create_info.flags == 0);
    REQUIRE(captured_create_info.requiredFlags == 0);
    REQUIRE(captured_create_info.preferredFlags == 0);
    REQUIRE(captured_create_info.memoryTypeBits == 0);
    REQUIRE(captured_create_info.pool == VmaPool{});
    REQUIRE(memory_alloc_info.vma_mem_infos.size() == 1);
    REQUIRE(vma_mem_info->capture_mem_req.size == 96);
    REQUIRE(vma_mem_info->replay_mem_req.size == replay_req.size);
    REQUIRE(vma_mem_info->offset_from_original_device_memory == 24);
    REQUIRE(vma_mem_info->requires_dedicated_allocation);
    REQUIRE_FALSE(vma_mem_info->prefers_dedicated_allocation);
    REQUIRE(vma_mem_info->allocation == allocation);
}

TEST_CASE("AllocateMemoryForImage falls back when captured host-coherent memory is unavailable", "[decode][rebind]")
{
    ImageMemorySelectionFixture                    fixture;
    ImageMemorySelectionFixture::ResourceAllocInfo resource_alloc_info =
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::MakeResourceAllocInfo(VK_OBJECT_TYPE_IMAGE);
    ImageMemorySelectionFixture::MemoryAllocInfo memory_alloc_info{};
    ImageMemorySelectionFixture::VmaMemoryInfo*  vma_mem_info = nullptr;
    const auto                                   image        = MakeHandle<VkImage>(0x2104);
    const auto                         allocation = reinterpret_cast<VmaAllocation>(static_cast<uintptr_t>(0x5004));
    const auto                         replay_req = MakeMemoryRequirements({ kHostVisibleTypeIndex }, 128, 32);
    std::vector<VkMemoryPropertyFlags> required_flags;

    fixture.capture_memory_properties =
        MakeMemoryProperties({ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT });
    resource_alloc_info.usage            = VK_IMAGE_USAGE_SAMPLED_BIT;
    resource_alloc_info.tiling           = VK_IMAGE_TILING_OPTIMAL;
    resource_alloc_info.capture_mem_reqs = { MakeMemoryRequirements({ kHostVisibleTypeIndex }, 96, 32) };
    memory_alloc_info.original_index     = 0;

    EXPECT_CALL(fixture.mock_vma_backend, GetImageMemoryRequirements(_, image, _, _, _))
        .WillOnce(Invoke([&](VmaAllocator,
                             VkImage,
                             VkMemoryRequirements& out_requirements,
                             bool&                 requires_dedicated,
                             bool&                 prefers_dedicated) {
            out_requirements   = replay_req;
            requires_dedicated = false;
            prefers_dedicated  = false;
        }));
    EXPECT_CALL(fixture.mock_vma_backend, AllocateMemoryForImage(_, image, _, _, _))
        .WillOnce(Invoke(
            [&](VmaAllocator, VkImage, const VmaAllocationCreateInfo* create_info, VmaAllocation*, VmaAllocationInfo*) {
                required_flags.push_back(create_info->requiredFlags);
                return VK_ERROR_FEATURE_NOT_PRESENT;
            }))
        .WillOnce(Invoke([&](VmaAllocator,
                             VkImage,
                             const VmaAllocationCreateInfo* create_info,
                             VmaAllocation*                 out_allocation,
                             VmaAllocationInfo*             out_allocation_info) {
            required_flags.push_back(create_info->requiredFlags);
            *out_allocation                   = allocation;
            out_allocation_info->memoryType   = 0;
            out_allocation_info->size         = replay_req.size;
            out_allocation_info->deviceMemory = MakeHandle<VkDeviceMemory>(0x4104);
            return VK_SUCCESS;
        }));

    const auto result =
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::AllocateMemoryForImage(fixture.allocator,
                                                                                  image,
                                                                                  0,
                                                                                  fixture.capture_memory_properties,
                                                                                  resource_alloc_info,
                                                                                  memory_alloc_info,
                                                                                  &vma_mem_info);

    REQUIRE(result == VK_SUCCESS);
    REQUIRE(vma_mem_info != nullptr);
    REQUIRE(required_flags == std::vector<VkMemoryPropertyFlags>{ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0 });
    REQUIRE(vma_mem_info->alc_create_info.requiredFlags == 0);
}

TEST_CASE("AllocateMemoryForImage reuses a compatible cached VMA allocation before asking the backend",
          "[decode][rebind]")
{
    ImageMemorySelectionFixture                    fixture;
    ImageMemorySelectionFixture::ResourceAllocInfo resource_alloc_info =
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::MakeResourceAllocInfo(VK_OBJECT_TYPE_IMAGE);
    ImageMemorySelectionFixture::MemoryAllocInfo memory_alloc_info{};
    ImageMemorySelectionFixture::VmaMemoryInfo*  vma_mem_info = nullptr;
    const auto                                   image        = MakeHandle<VkImage>(0x2101);
    const auto replay_req = MakeMemoryRequirements({ kHostVisibleTypeIndex }, 128, 32);

    resource_alloc_info.usage            = VK_IMAGE_USAGE_SAMPLED_BIT;
    resource_alloc_info.tiling           = VK_IMAGE_TILING_OPTIMAL;
    resource_alloc_info.capture_mem_reqs = { MakeMemoryRequirements({ kHostVisibleTypeIndex }, 96, 32) };
    memory_alloc_info.original_index     = 0;

    auto cached_memory_info                            = std::make_unique<ImageMemorySelectionFixture::VmaMemoryInfo>();
    cached_memory_info->memory_info                    = &memory_alloc_info;
    cached_memory_info->capture_mem_req                = resource_alloc_info.capture_mem_reqs[0];
    cached_memory_info->replay_mem_req                 = replay_req;
    cached_memory_info->alc_create_info.flags          = 0;
    cached_memory_info->alc_create_info.usage          = VMA_MEMORY_USAGE_CPU_TO_GPU;
    cached_memory_info->alc_create_info.requiredFlags  = 0;
    cached_memory_info->alc_create_info.preferredFlags = 0;
    cached_memory_info->alc_create_info.memoryTypeBits = 0;
    cached_memory_info->alc_create_info.pool           = VK_NULL_HANDLE;
    cached_memory_info->allocation_info.offset         = 0;
    cached_memory_info->offset_from_original_device_memory = 24;

    ImageMemorySelectionFixture::VmaMemoryInfo* cached_memory_info_ptr = cached_memory_info.get();
    memory_alloc_info.vma_mem_infos.emplace_back(std::move(cached_memory_info));

    EXPECT_CALL(fixture.mock_vma_backend, GetImageMemoryRequirements(_, image, _, _, _))
        .WillOnce(Invoke([&](VmaAllocator,
                             VkImage,
                             VkMemoryRequirements& out_requirements,
                             bool&                 requires_dedicated,
                             bool&                 prefers_dedicated) {
            out_requirements   = replay_req;
            requires_dedicated = false;
            prefers_dedicated  = false;
        }));
    EXPECT_CALL(fixture.mock_vma_backend, AllocateMemoryForImage(_, _, _, _, _)).Times(0);

    // Reuse matters because later policy changes should not accidentally regress alias-free image allocation churn.
    const auto result =
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::AllocateMemoryForImage(fixture.allocator,
                                                                                  image,
                                                                                  24,
                                                                                  fixture.capture_memory_properties,
                                                                                  resource_alloc_info,
                                                                                  memory_alloc_info,
                                                                                  &vma_mem_info);

    REQUIRE(result == VK_SUCCESS);
    REQUIRE(vma_mem_info == cached_memory_info_ptr);
    REQUIRE(memory_alloc_info.vma_mem_infos.size() == 1);
}

TEST_CASE("AllocateMemoryForImage keeps trace-like device-local host-visible optimal images on gpu-only memory",
          "[decode][rebind]")
{
    ImageMemorySelectionFixture                    fixture;
    ImageMemorySelectionFixture::ResourceAllocInfo resource_alloc_info =
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::MakeResourceAllocInfo(VK_OBJECT_TYPE_IMAGE);
    ImageMemorySelectionFixture::MemoryAllocInfo memory_alloc_info{};
    ImageMemorySelectionFixture::VmaMemoryInfo*  vma_mem_info = nullptr;
    const auto                                   image        = MakeHandle<VkImage>(0x2102);
    const auto allocation = reinterpret_cast<VmaAllocation>(static_cast<uintptr_t>(0x5003));
    const auto replay_req = MakeMemoryRequirements({ kHostVisibleTypeIndex, kDeviceLocalTypeIndex }, 256, 64);
    VmaAllocationCreateInfo captured_create_info{};

    fixture.capture_memory_properties =
        MakeMemoryProperties({ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT });
    fixture.replay_memory_properties = fixture.capture_memory_properties;
    gfxrecon::decode::VulkanRebindAllocatorTestAccess::SetMemoryProperties(
        fixture.allocator, fixture.capture_memory_properties, fixture.replay_memory_properties);

    resource_alloc_info.usage            = VK_IMAGE_USAGE_SAMPLED_BIT;
    resource_alloc_info.tiling           = VK_IMAGE_TILING_OPTIMAL;
    resource_alloc_info.capture_mem_reqs = { MakeMemoryRequirements(
        { kHostVisibleTypeIndex, kDeviceLocalTypeIndex }, 192, 64) };
    memory_alloc_info.original_index     = 0;

    EXPECT_CALL(fixture.mock_vma_backend, GetImageMemoryRequirements(_, image, _, _, _))
        .WillOnce(Invoke([&](VmaAllocator,
                             VkImage,
                             VkMemoryRequirements& out_requirements,
                             bool&                 requires_dedicated,
                             bool&                 prefers_dedicated) {
            out_requirements   = replay_req;
            requires_dedicated = false;
            prefers_dedicated  = false;
        }));
    EXPECT_CALL(fixture.mock_vma_backend, AllocateMemoryForImage(_, image, _, _, _))
        .WillOnce(Invoke([&](VmaAllocator,
                             VkImage,
                             const VmaAllocationCreateInfo* create_info,
                             VmaAllocation*                 out_allocation,
                             VmaAllocationInfo*             out_allocation_info) {
            captured_create_info              = *create_info;
            *out_allocation                   = allocation;
            out_allocation_info->memoryType   = 0;
            out_allocation_info->offset       = 0;
            out_allocation_info->size         = replay_req.size;
            out_allocation_info->deviceMemory = MakeHandle<VkDeviceMemory>(0x4101);
            return VK_SUCCESS;
        }));

    // This locks down the policy choice that optimal images on hybrid device-local/host-visible memory
    // still request GPU_ONLY from VMA.
    const auto result =
        gfxrecon::decode::VulkanRebindAllocatorTestAccess::AllocateMemoryForImage(fixture.allocator,
                                                                                  image,
                                                                                  32,
                                                                                  fixture.capture_memory_properties,
                                                                                  resource_alloc_info,
                                                                                  memory_alloc_info,
                                                                                  &vma_mem_info);

    REQUIRE(result == VK_SUCCESS);
    REQUIRE(vma_mem_info != nullptr);
    REQUIRE(captured_create_info.usage == VMA_MEMORY_USAGE_GPU_ONLY);
    REQUIRE(captured_create_info.requiredFlags == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

TEST_CASE("Data graph rebind synthesizes the replay session binding set", "[decode][rebind][data-graph]")
{
    using TestAccess = gfxrecon::decode::VulkanRebindAllocatorTestAccess;

    StrictMock<MockVulkanFunctions>         mock_vulkan_functions;
    StrictMock<MockVmaBackend>              mock_vma_backend;
    gfxrecon::decode::VulkanRebindAllocator allocator;

    const VkDevice device      = MakeHandle<VkDevice>(0x1001);
    const auto     session     = MakeHandle<VkDataGraphPipelineSessionARM>(0x2001);
    const auto     allocation0 = reinterpret_cast<VmaAllocation>(static_cast<uintptr_t>(0x3001));
    const auto     allocation1 = reinterpret_cast<VmaAllocation>(static_cast<uintptr_t>(0x3002));
    const auto     memory0     = MakeHandle<VkDeviceMemory>(0x4001);
    const auto     memory1     = MakeHandle<VkDeviceMemory>(0x4002);

    gfxrecon::decode::VulkanResourceAllocator::Functions functions{};
    functions.get_data_graph_pipeline_session_bind_point_requirements = &GetDataGraphBindPointRequirementsThunk;
    functions.get_data_graph_pipeline_session_memory_requirements     = &GetDataGraphMemoryRequirementsThunk;
    functions.bind_data_graph_pipeline_session_memory                 = &BindDataGraphSessionMemoryThunk;

    TestAccess::SetState(allocator, device, VK_NULL_HANDLE, VK_NULL_HANDLE, functions, &mock_vma_backend);
    const auto memory_properties = MakeMemoryProperties({ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT });
    TestAccess::SetMemoryProperties(allocator, memory_properties, memory_properties);
    g_mock_vulkan_functions = &mock_vulkan_functions;

    auto resource_alloc_info = TestAccess::MakeResourceAllocInfo(VK_OBJECT_TYPE_DATA_GRAPH_PIPELINE_SESSION_ARM);

    EXPECT_CALL(mock_vulkan_functions, GetDataGraphBindPointRequirements(device, _, _, nullptr))
        .WillOnce(Invoke([](VkDevice,
                            const VkDataGraphPipelineSessionBindPointRequirementsInfoARM*,
                            uint32_t* requirement_count,
                            VkDataGraphPipelineSessionBindPointRequirementARM*) {
            *requirement_count = 1;
            return VK_SUCCESS;
        }));
    EXPECT_CALL(mock_vulkan_functions, GetDataGraphBindPointRequirements(device, _, _, NotNull()))
        .WillOnce(Invoke([](VkDevice,
                            const VkDataGraphPipelineSessionBindPointRequirementsInfoARM*,
                            uint32_t*                                          requirement_count,
                            VkDataGraphPipelineSessionBindPointRequirementARM* requirements) {
            REQUIRE(*requirement_count == 1);
            requirements[0].bindPoint     = VK_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_OPTICAL_FLOW_CACHE_ARM;
            requirements[0].bindPointType = VK_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_TYPE_MEMORY_ARM;
            requirements[0].numObjects    = 2;
            return VK_SUCCESS;
        }));
    EXPECT_CALL(mock_vulkan_functions, GetDataGraphMemoryRequirements(device, _, _))
        .Times(2)
        .WillRepeatedly(Invoke([](VkDevice,
                                  const VkDataGraphPipelineSessionMemoryRequirementsInfoARM* info,
                                  VkMemoryRequirements2*                                     requirements) {
            REQUIRE(info->bindPoint == VK_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_OPTICAL_FLOW_CACHE_ARM);
            REQUIRE(info->objectIndex < 2);
            requirements->memoryRequirements = MakeMemoryRequirements({ 0 }, 128, 32);
        }));

    uint32_t allocation_index = 0;
    EXPECT_CALL(mock_vma_backend, AllocateMemory(_, _, _, _, _))
        .Times(2)
        .WillRepeatedly(Invoke([&](VmaAllocator,
                                   const VkMemoryRequirements*,
                                   const VmaAllocationCreateInfo* create_info,
                                   VmaAllocation*                 out_allocation,
                                   VmaAllocationInfo*             allocation_info) {
            REQUIRE(create_info->memoryTypeBits == 1);
            const bool first              = (allocation_index++ == 0);
            *out_allocation               = first ? allocation0 : allocation1;
            allocation_info->deviceMemory = first ? memory0 : memory1;
            allocation_info->memoryType   = 0;
            allocation_info->offset       = first ? 0 : 32;
            allocation_info->size         = 128;
            return VK_SUCCESS;
        }));
    EXPECT_CALL(mock_vulkan_functions, BindDataGraphSessionMemory(device, 2, _))
        .WillOnce(Invoke([&](VkDevice, uint32_t, const VkBindDataGraphPipelineSessionMemoryInfoARM* bind_infos) {
            REQUIRE(bind_infos[0].session == session);
            REQUIRE(bind_infos[0].bindPoint == VK_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_OPTICAL_FLOW_CACHE_ARM);
            REQUIRE(bind_infos[0].objectIndex == 0);
            REQUIRE(bind_infos[0].memory == memory0);
            REQUIRE(bind_infos[1].objectIndex == 1);
            REQUIRE(bind_infos[1].memory == memory1);
            return VK_SUCCESS;
        }));

    VkBindDataGraphPipelineSessionMemoryInfoARM captured_bind{
        VK_STRUCTURE_TYPE_BIND_DATA_GRAPH_PIPELINE_SESSION_MEMORY_INFO_ARM
    };
    captured_bind.session     = session;
    captured_bind.bindPoint   = VK_DATA_GRAPH_PIPELINE_SESSION_BIND_POINT_TRANSIENT_ARM;
    captured_bind.objectIndex = 0;
    captured_bind.memory      = MakeHandle<VkDeviceMemory>(0x9999);

    REQUIRE(TestAccess::InitializeDataGraphSessionMemory(allocator, session, resource_alloc_info) == VK_SUCCESS);

    // Captured bind calls are completely ignored: no arguments are read and output storage is not modified.
    VkMemoryPropertyFlags memory_property_flags = 0x1234;
    REQUIRE(allocator.BindDataGraphPipelineSessionMemory(1, &captured_bind, nullptr, nullptr, &memory_property_flags) ==
            VK_SUCCESS);
    REQUIRE(memory_property_flags == 0x1234);
    REQUIRE(allocator.BindDataGraphPipelineSessionMemory(99, nullptr, nullptr, nullptr, nullptr) == VK_SUCCESS);

    EXPECT_CALL(mock_vma_backend, FreeMemory(_, allocation0));
    EXPECT_CALL(mock_vma_backend, FreeMemory(_, allocation1));
    TestAccess::RemoveDataGraphSessionMemory(allocator, resource_alloc_info, session);
    g_mock_vulkan_functions = nullptr;
}
