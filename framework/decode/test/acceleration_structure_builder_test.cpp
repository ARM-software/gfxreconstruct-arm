/*
** Copyright (c) 2025 LunarG, Inc.
** Copyright (c) 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "decode/vulkan_acceleration_structure_builder.h"
#include "decode/vulkan_handle_mapping_util.h"
#include "decode/vulkan_object_info.h"
#include "vulkan_resource_allocator_mock.h"

#include <vector>

using namespace gfxrecon;

// Contains reusable test objects, recreated for each scenario
struct TestFixture
{
    encode::VulkanDeviceTable            device_table;
    decode::VulkanPhysicalDeviceInfo*    physical_device_info;
    VkDevice                             device;
    VkPhysicalDeviceMemoryProperties     properties;
    decode::VulkanObjectInfoTable        object_info_table;
    decode::VulkanDeviceInfo             device_info;
    decode::VulkanDeviceAddressTracker   buffer_tracker;
    decode::VulkanResourceAllocatorMock* mock_allocator;
    decode::VulkanBufferInfo             storage_buffer_info{};
    VkAccelerationStructureCreateInfoKHR acceleration_structure_create_info{};
    format::HandleId                     acceleration_structure_capture_id{};
    VkAccelerationStructureKHR           acceleration_structure_handle{};

    // used for buffer identification in mock allocator
    const decode::VulkanResourceAllocator::ResourceData kInputStorageAllocatorData = 1;

    TestFixture() : buffer_tracker(object_info_table)
    {
        util::Log::Init(util::Log::kErrorSeverity);
        device_info.handle         = device;
        device_info.allocator      = std::make_unique<decode::VulkanResourceAllocatorMock>();
        mock_allocator             = dynamic_cast<decode::VulkanResourceAllocatorMock*>(device_info.allocator.get());
        physical_device_info       = nullptr;
        properties.memoryTypeCount = 2;
        storage_buffer_info.allocator_data = kInputStorageAllocatorData;
    }
    ~TestFixture() { util::Log::Release(); }
};

struct AccelerationStructureSizeTestParameters
{
    size_t captured_as_size;
    size_t replay_as_size;
    size_t storage_size;
    size_t expected_storage_size;
    size_t expected_as_size;
};

// Contains scenarios of acceleration stucture size to be tested
std::vector<AccelerationStructureSizeTestParameters> acceleration_structure_size_test_cases = {
    // clang-format off
    {.captured_as_size = 256, .replay_as_size = 128, .storage_size = 256, .expected_storage_size = 256, .expected_as_size = 128},
    {.captured_as_size = 256, .replay_as_size = 256, .storage_size = 256, .expected_storage_size = 512, .expected_as_size = 256},
    {.captured_as_size = 256, .replay_as_size = 512, .storage_size = 256, .expected_storage_size = 768, .expected_as_size = 512},
    // clang-format on
};

thread_local AccelerationStructureSizeTestParameters scenario;

SCENARIO_METHOD(TestFixture, "Create single AS object with valid sizes")
{
    scenario = GENERATE(
        from_range(acceleration_structure_size_test_cases.begin(), acceleration_structure_size_test_cases.end()));

    GIVEN("Capture size was " + std::to_string(scenario.captured_as_size) + " and replay size is " +
          std::to_string(scenario.replay_as_size))
    {
        // acceleration structure size set by acceleration structure builder in CreateAccelerationStructure call
        thread_local size_t output_acceleration_structure_size = 0;
        // storage size may be changed by ASB - initialize with test parameter value
        thread_local size_t storage_size = scenario.storage_size;

        // Device table function pointers need to be set before ASB is created since it will copy needed pointers to
        // internal table

        // GetAccelerationStructureBuildSizesKHR should return input_acceleration_structure_size
        device_table.GetAccelerationStructureBuildSizesKHR =
            [](VkDevice                                           device,
               VkAccelerationStructureBuildTypeKHR                buildType,
               const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo,
               const uint32_t*                                    pMaxPrimitiveCounts,
               VkAccelerationStructureBuildSizesInfoKHR*          pSizeInfo) {
                pSizeInfo->accelerationStructureSize = scenario.replay_as_size;
            };

        // CreateAccelerationStructureKHR should report back the output size generated by ASB
        device_table.CreateAccelerationStructureKHR = [](VkDevice                                    device,
                                                         const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
                                                         const VkAllocationCallbacks*                pAllocator,
                                                         VkAccelerationStructureKHR* pAccelerationStructure) {
            output_acceleration_structure_size = pCreateInfo->size;
            return VK_SUCCESS;
        };

        // Create tested object
        decode::VulkanAccelerationStructureBuilder asb(
            &device_table, physical_device_info, device, mock_allocator, properties, buffer_tracker);

        // Simulate GetAccelerationStructureBuildSizes expected before each create call
        VkAccelerationStructureBuildSizesInfoKHR size_info;
        asb.OnGetAccelerationStructureBuildSizes(
            &device_info, (VkAccelerationStructureBuildTypeKHR)0, nullptr, 0, &size_info);

        WHEN("OnCreateAccelerationStructure is called")
        {
            // ASB may attempt to create new storage - record its size
            mock_allocator->OnCreateBufferDirect = [&](const VkBufferCreateInfo* info) { storage_size = info->size; };
            // ASB will attempt to fetch size of the storage buffer from allocator
            // This may be called either on the input storage buffer or the recreated storage buffer
            mock_allocator->OnGetBufferSize = [&](decode::VulkanResourceAllocator::ResourceData alloc_data) {
                if (alloc_data == kInputStorageAllocatorData)
                    return scenario.storage_size;
                else
                    return storage_size;
            };
            // ASB will check the original captured size of AS
            acceleration_structure_create_info.size = scenario.captured_as_size;
            storage_buffer_info.replay_size         = scenario.storage_size;
            storage_buffer_info.allocator_data      = kInputStorageAllocatorData;

            asb.OnCreateAccelerationStructure(&device_info,
                                              &acceleration_structure_create_info,
                                              nullptr,
                                              &storage_buffer_info,
                                              acceleration_structure_capture_id,
                                              &acceleration_structure_handle);
            THEN("Acceleration structure of size " + std::to_string(scenario.expected_as_size) + " is created")
            {
                REQUIRE(output_acceleration_structure_size == scenario.expected_as_size);
            }
            THEN("Storage buffer size is " + std::to_string(scenario.expected_storage_size))
            {
                REQUIRE(storage_size == scenario.expected_storage_size);
            }
        }
    }
}

// TODO:
// Scenario: Create two acceleration structures in a single buffer next to eachother
//  Given: they didn't overlap capture time and don't overlap replay time
//  Given: they don't overlap capture time, but overlap replay time
//  Given: they overlapped capture time, but don't overlap replay time
//  Given: they overlapped capture time and overlap replay time

// TODO:
// Create two acceleration structures at the same address
// Test build process
// Test compression
// Test meta-commands