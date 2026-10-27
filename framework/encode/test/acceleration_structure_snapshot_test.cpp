// Copyright (c) 2026 Arm Limited and/or its affiliates
// SPDX-License-Identifier: MIT
#include <catch2/catch.hpp>

#include "encode/vulkan_state_tracker.h"
#include "encode/vulkan_handle_wrapper_util.h"

#include <array>
#include <map>
#include <vulkan/vulkan_core.h>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)

// Exercise the real tracker with mock device calls: recording a GPU copy must not make its bytes readable.
class AccelerationStructureSnapshotTest
{
  public:
    AccelerationStructureSnapshotTest()
    {
        active                                     = this;
        device.handle                              = format::FromHandleId<VkDevice>(100);
        device.handle_id                           = 100;
        device.physical_device                     = &physical;
        physical.memory_properties.memoryTypeCount = 1;
        physical.memory_properties.memoryTypes[0].propertyFlags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        device.layer_table.CreateBuffer =
            [](VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*, VkBuffer* out) {
                *out = format::FromHandleId<VkBuffer>(++active->next_handle);
                return VK_SUCCESS;
            };
        device.layer_table.GetBufferMemoryRequirements = [](VkDevice, VkBuffer, VkMemoryRequirements* out) {
            *out = { 64, 64, 1 };
        };
        device.layer_table.AllocateMemory =
            [](VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory* out) {
                *out = format::FromHandleId<VkDeviceMemory>(++active->next_handle);
                active->memory[*out].fill(0xcd);
                return VK_SUCCESS;
            };
        device.layer_table.BindBufferMemory = [](VkDevice, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize) {
            active->bindings[buffer] = mem;
            return VK_SUCCESS;
        };
        device.layer_table.CmdPipelineBarrier = [](VkCommandBuffer,
                                                   VkPipelineStageFlags,
                                                   VkPipelineStageFlags,
                                                   VkDependencyFlags,
                                                   uint32_t,
                                                   const VkMemoryBarrier*,
                                                   uint32_t,
                                                   const VkBufferMemoryBarrier*,
                                                   uint32_t,
                                                   const VkImageMemoryBarrier*) {};
        device.layer_table.CmdCopyBuffer = [](VkCommandBuffer, VkBuffer, VkBuffer dst, uint32_t, const VkBufferCopy*) {
            active->copies.push_back({ active->bindings.at(dst), active->input_value });
        };
        device.layer_table.MapMemory =
            [](VkDevice, VkDeviceMemory mem, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void** out) {
                ++active->maps;
                if (active->map_result != VK_SUCCESS)
                    return active->map_result;
                *out = active->memory.at(mem).data();
                return VK_SUCCESS;
            };
        device.layer_table.UnmapMemory   = [](VkDevice, VkDeviceMemory) {};
        device.layer_table.DestroyBuffer = [](VkDevice, VkBuffer, const VkAllocationCallbacks*) {
            ++active->destroyed;
        };
        device.layer_table.FreeMemory     = [](VkDevice, VkDeviceMemory, const VkAllocationCallbacks*) {};
        device.layer_table.DeviceWaitIdle = [](VkDevice) {
            ++active->waits;
            if (active->wait_result == VK_SUCCESS)
                active->ExecuteCopies();
            return active->wait_result;
        };
        pool.device = &device;
        for (size_t i = 0; i < command_buffers.size(); ++i)
        {
            auto& cb       = command_buffers[i];
            cb.handle      = format::FromHandleId<VkCommandBuffer>(200 + i);
            cb.parent_pool = &pool;
            vulkan_wrappers::state_handle_table_.InsertWrapper(&cb);
        }
        input.handle      = format::FromHandleId<VkBuffer>(300);
        input.handle_id   = 300;
        input.device      = &device;
        input.address     = 0x10000;
        input.size        = 64;
        input.usage       = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        storage.handle    = format::FromHandleId<VkBuffer>(301);
        storage.handle_id = 301;
        storage.device    = &device;
        storage.size      = 64;
        as.handle         = format::FromHandleId<VkAccelerationStructureKHR>(302);
        as.handle_id      = 302;
        as.device         = &device;
        as.buffer         = storage.handle_id;
        as.type           = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        as.address        = 0x20000;
        as.size           = 64;
        state             = std::make_shared<AccelerationStructureBuildState>();
        state->id         = as.handle_id;
        state->type       = as.type;
        storage.acceleration_structures[as.handle_id] = state;
        vulkan_wrappers::state_handle_table_.InsertWrapper(&as);
        vulkan_wrappers::state_handle_table_.InsertWrapper(&input);
        tracker.state_table_.InsertWrapper(storage.handle_id, &storage);
        tracker.state_table_.InsertWrapper(device.handle_id, &device);
        tracker.device_address_trackers_[device.handle].TrackBuffer(&input);
    }

    ~AccelerationStructureSnapshotTest()
    {
        for (auto& cb : command_buffers)
        {
            tracker.ReleaseAccelerationStructureSnapshots(&cb);
            vulkan_wrappers::state_handle_table_.RemoveWrapper(&cb);
        }
        vulkan_wrappers::state_handle_table_.RemoveWrapper(&as);
        vulkan_wrappers::state_handle_table_.RemoveWrapper(&input);
    }

    void Record(uint8_t value, size_t cb = 0)
    {
        input_value = value;
        VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geometry.geometryType             = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.data.deviceAddress = input.address;
        VkAccelerationStructureBuildGeometryInfoKHR info{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
        };
        info.type                     = as.type;
        info.dstAccelerationStructure = as.handle;
        info.geometryCount            = 1;
        info.pGeometries              = &geometry;
        VkAccelerationStructureBuildRangeInfoKHR range{ 1, 0, 0, 0 };
        const auto*                              ranges = &range;
        tracker.TrackAccelerationStructureBuildCommand(command_buffers[cb].handle, 1, &info, &ranges);
    }

    void Submit(size_t cb = 0, uintptr_t queue = 400, uintptr_t fence = 500)
    {
        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &command_buffers[cb].handle;
        tracker.TrackAccelerationStructureSnapshotSubmission(
            format::FromHandleId<VkQueue>(queue), 1, &submit, format::FromHandleId<VkFence>(fence));
    }
    void Complete(uintptr_t fence = 500)
    {
        ExecuteCopies();
        tracker.CompleteAccelerationStructureSnapshotFence(format::FromHandleId<VkFence>(fence));
    }
    void ExecuteCopies()
    {
        for (const auto& [mem, value] : copies) memory.at(mem).fill(value);
        copies.clear();
    }
    void SetSnapshotData(uint8_t value) { memory.at(Snapshot()->memory).fill(value); }
    void Release(size_t cb = 0) { tracker.ReleaseAccelerationStructureSnapshots(&command_buffers[cb]); }
    bool Drain() { return tracker.DrainAccelerationStructureSnapshotSubmissions(); }
    void ExecuteSecondary(size_t primary, size_t secondary)
    {
        tracker.TrackExecuteCommands(command_buffers[primary].handle, 1, &command_buffers[secondary].handle);
    }
    uint8_t RetainedByte()
    {
        REQUIRE(state->latest_build_command.has_value());
        return state->latest_build_command->input_buffers.begin()->build_copy_snapshot->bytes.at(0);
    }
    std::shared_ptr<AccelerationStructureInputBufferSnapshot> Snapshot(size_t cb = 0)
    {
        return command_buffers[cb].acceleration_structure_input_snapshots.back();
    }

    std::shared_ptr<AccelerationStructureBuildState> AddCompletedAlias()
    {
        auto alias  = std::make_shared<AccelerationStructureBuildState>();
        alias->id   = 303;
        alias->type = as.type;
        alias->latest_build_command.emplace();
        alias->latest_build_command->size          = 64;
        storage.acceleration_structures[alias->id] = alias;
        return alias;
    }

    VulkanStateTracker                               tracker;
    std::shared_ptr<AccelerationStructureBuildState> state;
    size_t                                           maps = 0, destroyed = 0, waits = 0;
    VkResult                                         wait_result = VK_SUCCESS;
    VkResult                                         map_result  = VK_SUCCESS;

  private:
    inline static AccelerationStructureSnapshotTest*     active      = nullptr;
    uint64_t                                             next_handle = 1000;
    uint8_t                                              input_value = 0;
    vulkan_wrappers::PhysicalDeviceWrapper               physical{};
    vulkan_wrappers::DeviceWrapper                       device{};
    vulkan_wrappers::CommandPoolWrapper                  pool{};
    std::array<vulkan_wrappers::CommandBufferWrapper, 3> command_buffers{};
    vulkan_wrappers::BufferWrapper                       input{}, storage{};
    vulkan_wrappers::AccelerationStructureKHRWrapper     as{};
    std::map<VkDeviceMemory, std::array<uint8_t, 64>>    memory;
    std::map<VkBuffer, VkDeviceMemory>                   bindings;
    std::vector<std::pair<VkDeviceMemory, uint8_t>>      copies;
};

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Discarded AS recordings are never read or published",
                 "[as_snapshot]")
{
    Record(1);
    REQUIRE_FALSE(state->latest_build_command);
    Release();
    REQUIRE(maps == 0);
    REQUIRE(destroyed == 1);
    REQUIRE_FALSE(state->latest_build_command);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "AS state changes on successful submission and survives discarded recordings",
                 "[as_snapshot]")
{
    Record(1);
    Submit();
    REQUIRE(state->latest_build_command.has_value());
    REQUIRE(state->latest_build_command->input_buffers.begin()->build_copy_snapshot == Snapshot());
    REQUIRE(Snapshot()->bytes.empty());
    REQUIRE(maps == 0);
    Complete();
    REQUIRE(RetainedByte() == 1);
    Release();
    Record(2); // Also models a failed submit: the successful-submit hook is not called.
    REQUIRE(RetainedByte() == 1);
    REQUIRE(Drain()); // An unsubmitted replacement must not interfere with recreation of the submitted build.
    REQUIRE(waits == 0);
    Release();
    REQUIRE(maps == 1);
    REQUIRE(RetainedByte() == 1);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Discarding a submitted recording preserves submission ownership",
                 "[as_snapshot]")
{
    Record(3);
    Submit();
    Release();
    REQUIRE(destroyed == 0);
    Complete();
    REQUIRE(RetainedByte() == 3);
    REQUIRE(destroyed == 1);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Late older completions cannot replace newer AS state",
                 "[as_snapshot]")
{
    Record(1, 0);
    Submit(0, 400, 500);
    Record(2, 1);
    Submit(1, 401, 501);
    Complete(501);
    REQUIRE(RetainedByte() == 2);
    Complete(500);
    REQUIRE(RetainedByte() == 2);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "AS builds preserve primary and secondary execution order",
                 "[as_snapshot]")
{
    Record(1, 0);
    Record(2, 1);
    ExecuteSecondary(0, 1);
    Record(3, 0);
    Submit();
    Complete();
    REQUIRE(RetainedByte() == 3);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Trim drains AS submissions without an application fence",
                 "[as_snapshot]")
{
    Record(4);
    Submit(0, 400, 0);
    REQUIRE(Drain());
    REQUIRE(waits == 1);
    REQUIRE(RetainedByte() == 4);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Failed trim waits reject serialization of pending AS inputs",
                 "[as_snapshot]")
{
    Record(1);
    Submit();
    Complete();
    Release();
    Record(2);
    Submit();
    wait_result = VK_ERROR_DEVICE_LOST;
    REQUIRE_FALSE(Drain());
    REQUIRE(maps == 1);
    REQUIRE(state->latest_build_command->input_buffers.begin()->build_copy_snapshot == Snapshot());
    REQUIRE_FALSE(Snapshot()->readback_succeeded);
    wait_result = VK_SUCCESS;
    REQUIRE(Drain());
    REQUIRE(RetainedByte() == 2);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Failed AS readback rejects trim even when snapshot bytes have the expected size",
                 "[as_snapshot]")
{
    Record(1);
    Submit();
    Complete();
    Release();
    Record(2);
    Submit();
    Snapshot()->bytes.assign(64, 0xcd);
    map_result = VK_ERROR_MEMORY_MAP_FAILED;
    Complete();
    REQUIRE_FALSE(Snapshot()->readback_succeeded);
    REQUIRE_FALSE(Drain());
    REQUIRE(maps == 2);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Resubmission refreshes shared AS input snapshots after completion",
                 "[as_snapshot]")
{
    Record(7);
    Submit();
    Complete();
    auto retained = state->latest_build_command->input_buffers.begin()->build_copy_snapshot;
    REQUIRE(retained == Snapshot());
    REQUIRE(retained->readback_succeeded);
    Submit();
    REQUIRE_FALSE(retained->readback_succeeded);
    REQUIRE_FALSE(retained->copy_completed);
    SetSnapshotData(9);
    Complete();
    REQUIRE(RetainedByte() == 9);
    REQUIRE(retained->readback_succeeded);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Failed resubmission readback cannot reuse previous AS input bytes",
                 "[as_snapshot]")
{
    Record(7);
    Submit();
    Complete();
    REQUIRE(RetainedByte() == 7);
    Submit();
    map_result = VK_ERROR_MEMORY_MAP_FAILED;
    Complete();
    REQUIRE_FALSE(Snapshot()->readback_succeeded);
    REQUIRE_FALSE(Drain());
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Repeated secondary executions own one snapshot per submission",
                 "[as_snapshot]")
{
    Record(8, 1);
    ExecuteSecondary(0, 1);
    ExecuteSecondary(0, 1);
    Submit();
    REQUIRE(Snapshot(1)->pending_submissions == 1);
    Complete();
    REQUIRE(Snapshot(1)->pending_submissions == 0);
    REQUIRE(maps == 1);
    REQUIRE(RetainedByte() == 8);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Overlapping uses wait for the final snapshot-copy completion",
                 "[as_snapshot]")
{
    Record(8);
    Submit(0, 400, 500);
    Submit(0, 401, 501);
    Complete(500);
    REQUIRE(maps == 0);
    REQUIRE(state->latest_build_command.has_value());
    REQUIRE_FALSE(Snapshot()->readback_succeeded);
    Complete(501);
    REQUIRE(maps == 1);
    REQUIRE(RetainedByte() == 8);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Aliased AS state survives discarded replacing builds",
                 "[as_snapshot]")
{
    auto alias = AddCompletedAlias();
    Record(1);
    REQUIRE(alias->latest_build_command.has_value());
    Release();
    REQUIRE(alias->latest_build_command.has_value());
    Record(2);
    Submit();
    REQUIRE_FALSE(alias->latest_build_command);
    Complete();
    REQUIRE_FALSE(alias->latest_build_command);
}

TEST_CASE_METHOD(AccelerationStructureSnapshotTest,
                 "Writer address changes cannot corrupt a reusable AS recording",
                 "[as_snapshot]")
{
    Record(1);
    Submit();
    Complete();
    auto* geometry =
        const_cast<VkAccelerationStructureGeometryKHR*>(state->latest_build_command->geometry_info.pGeometries);
    geometry->geometry.instances.data.deviceAddress = 0xdead;
    Submit();
    Complete();
    REQUIRE(state->latest_build_command->geometry_info.pGeometries[0].geometry.instances.data.deviceAddress == 0x10000);
}

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)
