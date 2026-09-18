/*
** Copyright (c) 2025 LunarG, Inc.
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

#ifndef GFXRECON_ENCODE_VULKAN_ACCELERATION_STRUCTURE_BUILD_STATE_H
#define GFXRECON_ENCODE_VULKAN_ACCELERATION_STRUCTURE_BUILD_STATE_H

#include "util/defines.h"
#include "format/format.h"
#include "vulkan/vulkan_core.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)

namespace vulkan_wrappers
{
struct DeviceWrapper;
struct AccelerationStructureKHRWrapper;
} // namespace vulkan_wrappers

struct RestoreableBuffer
{
    // Required data to correctly create a buffer
    VkBuffer                              handle{ VK_NULL_HANDLE };
    format::HandleId                      handle_id{ format::kNullHandleId };
    const vulkan_wrappers::DeviceWrapper* device{ nullptr };
    uint32_t                              queue_family_index{ 0 };
    VkDeviceSize                          created_size{ 0 };
    VkBufferUsageFlags                    usage{ 0 };

    VkDeviceAddress capture_address{ 0 };
    VkDeviceAddress actual_address{ 0 };

    VkMemoryRequirements memory_requirements{};
    format::HandleId     bind_memory{};
    VkDeviceMemory       bind_memory_handle{ VK_NULL_HANDLE };
};

struct AccelerationStructureInputBufferSnapshot
{
    VkBuffer                        buffer{ VK_NULL_HANDLE };
    VkDeviceMemory                  memory{ VK_NULL_HANDLE };
    vulkan_wrappers::DeviceWrapper* device{ nullptr };
    VkDeviceSize                    size{ 0 };
    uint32_t                        queue_family_index{ 0 };
    VkMemoryRequirements            memory_requirements{};
    VkMemoryPropertyFlags           memory_properties{ 0 };
    std::vector<uint8_t>            bytes;
    uint32_t                        pending_submissions{ 0 };
    bool                            recording_alive{ true };
    bool                            release_on_completion{ false };
    bool                            resources_destroyed{ false };
};

struct AccelerationStructureInputBuffer : public RestoreableBuffer
{
    VkDeviceAddress build_copy_source_address{ 0 };
    VkDeviceSize    build_copy_source_offset{ 0 };

    std::shared_ptr<AccelerationStructureInputBufferSnapshot> build_copy_snapshot;
};

struct AccelerationStructureKHRBuildCommandData
{
    format::HandleId               device_id          = format::kNullHandleId;
    format::HandleId               handle_id          = format::kNullHandleId;
    VkAccelerationStructureTypeKHR type               = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
    VkDeviceSize                   size               = 0;
    VkDeviceSize                   offset             = 0;
    format::HandleId               replaced_handle_id = format::kNullHandleId;
    VkAccelerationStructureKHR     replaced_handle    = VK_NULL_HANDLE;

    VkAccelerationStructureBuildGeometryInfoKHR                            geometry_info;
    std::vector<uint8_t>                                                   geometry_info_memory;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR>                  build_range_infos;
    std::unordered_map<format::HandleId, AccelerationStructureInputBuffer> input_buffers;
    RestoreableBuffer                                                      storage_buffer;
};

struct AccelerationStructureCopyCommandData
{
    format::HandleId                   device = format::kNullHandleId;
    format::HandleId                   src    = format::kNullHandleId;
    format::HandleId                   dst    = format::kNullHandleId;
    VkCopyAccelerationStructureModeKHR mode   = VK_COPY_ACCELERATION_STRUCTURE_MODE_MAX_ENUM_KHR;
};

struct AccelerationStructureWritePropertiesCommandData
{
    format::HandleId device     = format::kNullHandleId;
    VkQueryType      query_type = VK_QUERY_TYPE_MAX_ENUM;
};

struct AccelerationStructureBuildState
{
    format::HandleId                                               id{ format::kNullHandleId };
    VkAccelerationStructureTypeKHR                                 type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
    std::optional<AccelerationStructureKHRBuildCommandData>        latest_build_command{ std::nullopt };
    std::optional<AccelerationStructureCopyCommandData>            latest_copy_command{ std::nullopt };
    std::optional<AccelerationStructureWritePropertiesCommandData> latest_write_properties_command{ std::nullopt };

    std::vector<format::HandleId>                                 copy_destinations;
    std::vector<std::shared_ptr<AccelerationStructureBuildState>> dependencies{};
};

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)
#endif // GFXRECON_ENCODE_VULKAN_ACCELERATION_STRUCTURE_BUILD_STATE_H
