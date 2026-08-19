/*
** Copyright (c) 2024 LunarG, Inc.
** Copyright (c) 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "decode/vulkan_device_address_tracker.h"
#include "decode/vulkan_object_info.h"
#include "format/format.h"
#include "util/logging.h"
#include <vector>
#include <vulkan/vulkan_core.h>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

VulkanDeviceAddressTracker::VulkanDeviceAddressTracker(VulkanObjectInfoTable& object_info_table) :
    object_info_table_(object_info_table)
{}

void decode::VulkanDeviceAddressTracker::TrackBuffer(const decode::VulkanBufferInfo* buffer_info)
{
    if (buffer_info != nullptr)
    {
        // track capture device address
        if (buffer_info->capture_address != 0)
        {
            buffer_capture_addresses_[buffer_info->capture_address] = buffer_info->capture_id;

            // if a capture-address is known, we also know the replay-address
            address_lookup_helper_map_[buffer_info->capture_address] = { buffer_info->replay_address,
                                                                         buffer_info->replay_size };
        }

        // track replay device address
        if (buffer_info->replay_address != 0)
        {
            buffer_replay_addresses_[buffer_info->replay_address] = buffer_info->capture_id;
        }

        // track vulkan-handle
        if (buffer_info->handle != VK_NULL_HANDLE)
        {
            buffer_handles_[buffer_info->handle] = buffer_info->capture_id;
        }

        if (buffer_info->capture_id == format::kNullHandleId)
        {
            shadow_info_table_[buffer_info->handle]           = const_cast<VulkanBufferInfo*>(buffer_info);
            shadow_address_map_[buffer_info->capture_address] = buffer_info->handle;
            shadow_address_map_[buffer_info->replay_address]  = buffer_info->handle;
        }
    }
}

void VulkanDeviceAddressTracker::RemoveBuffer(const VulkanBufferInfo* buffer_info)
{
    if (buffer_info != nullptr)
    {
        buffer_capture_addresses_.erase(buffer_info->capture_address);
        buffer_replay_addresses_.erase(buffer_info->replay_address);
        buffer_handles_.erase(buffer_info->handle);
        address_lookup_helper_map_.erase(buffer_info->capture_address);
        shadow_address_map_.erase(buffer_info->replay_address);
        shadow_info_table_.erase(buffer_info->handle);

        for (const auto& [capture_address, as_info] : buffer_info->acceleration_structures)
        {
            acceleration_structure_addresses_.erase(capture_address);
        }
    }
}

void VulkanDeviceAddressTracker::TrackAccelerationStructure(
    const VulkanAccelerationStructureKHRInfo* acceleration_structure_info)
{
    if (acceleration_structure_info != nullptr)
    {
        auto* buffer_info = GetBufferByHandle(acceleration_structure_info->buffer);

        // track capture device address
        if (acceleration_structure_info->capture_address != 0)
        {
            // if a capture-address is known, we also know the replay-address
            acceleration_structure_addresses_[acceleration_structure_info->capture_address] =
                acceleration_structure_info->replay_address;

            // we can derive the buffer-device address from AS device-address, if necessary
            if (buffer_info != nullptr && buffer_info->capture_address == 0)
            {
                buffer_info->capture_address =
                    acceleration_structure_info->capture_address - acceleration_structure_info->offset;
            }
        }

        if (acceleration_structure_info->replay_address != 0)
        {
            // we can derive the buffer-device address from AS device-address, if necessary
            if (buffer_info != nullptr)
            {
                // if not already present, keep track of AS<->VkBuffer association
                buffer_info->acceleration_structures[buffer_info->capture_address + acceleration_structure_info->offset]
                    .insert(acceleration_structure_info->capture_id);

                buffer_info->replay_address =
                    acceleration_structure_info->replay_address - acceleration_structure_info->offset;
            }
        }

        // track vulkan-handle
        if (acceleration_structure_info->handle != VK_NULL_HANDLE)
        {
            acceleration_structure_handles_[acceleration_structure_info->handle] =
                acceleration_structure_info->capture_id;
        }

        // potentially update tracked address-information for linked buffer
        TrackBuffer(buffer_info);
    }
}

void VulkanDeviceAddressTracker::RemoveAccelerationStructure(
    const VulkanAccelerationStructureKHRInfo* acceleration_structure_info)
{
    if (acceleration_structure_info != nullptr)
    {
        acceleration_structure_handles_.erase(acceleration_structure_info->handle);
        auto* buffer_info = GetBufferByHandle(acceleration_structure_info->buffer);

        if (buffer_info != nullptr)
        {
            auto aliases = buffer_info->acceleration_structures.find(acceleration_structure_info->capture_address);
            if (aliases != buffer_info->acceleration_structures.end())
            {
                aliases->second.erase(acceleration_structure_info->capture_id);
                if (aliases->second.empty())
                {
                    buffer_info->acceleration_structures.erase(aliases);
                }
            }
            else
            {
                GFXRECON_LOG_ERROR("Device address tracking error.\n\tbuffer %" PRIu64
                                   "does not contain acceleration structure %" PRIu64
                                   "but it was created with this buffer as storage",
                                   buffer_info->capture_id,
                                   acceleration_structure_info->capture_id);
            }
        }
    }
}

const decode::VulkanBufferInfo*
decode::VulkanDeviceAddressTracker::GetBufferByCaptureDeviceAddress(VkDeviceAddress capture_address,
                                                                    size_t*         offset) const
{
    return GetBufferInfo(capture_address, buffer_capture_addresses_, offset);
}

const decode::VulkanBufferInfo*
decode::VulkanDeviceAddressTracker::GetBufferByReplayDeviceAddress(VkDeviceAddress replay_address, size_t* offset) const
{
    return GetBufferInfo(replay_address, buffer_replay_addresses_, offset);
}

std::vector<VkDeviceAddressRangeKHR> VulkanDeviceAddressTracker::TranslateCaptureToReplayDeviceAddressRanges(
    const VkDeviceAddressRangeKHR& capture_range, std::vector<const VulkanBufferInfo*>* optional_buffer_info) const
{
    std::vector<VkDeviceAddressRangeKHR> replay_ranges;
    VkDeviceAddress                      current_capture_address = capture_range.address;
    VkDeviceSize                         remaining_size          = capture_range.size;

    std::vector<const VulkanBufferInfo*> tmp_optional_buffer_info;

    while (remaining_size > 0)
    {
        size_t                  range_offset = 0;
        const VulkanBufferInfo* buffer_info  = GetBufferByCaptureDeviceAddress(current_capture_address, &range_offset);
        const VkDeviceSize      offset       = range_offset;

        if ((buffer_info == nullptr) || (buffer_info->replay_address == 0))
        {
            return {};
        }

        if (buffer_info->capture_size != buffer_info->replay_size)
        {
            GFXRECON_LOG_WARNING("Potential corruption as of result of buffer capture and replay time size difference");
        }
        const VkDeviceSize remaining_capture_size = buffer_info->capture_size - offset;
        const VkDeviceSize remaining_replay_size  = buffer_info->replay_size - offset;
        const VkDeviceSize segment_size =
            std::min(remaining_size, std::min(remaining_capture_size, remaining_replay_size));

        if (segment_size == 0)
        {
            return {};
        }

        replay_ranges.push_back({ buffer_info->replay_address + offset, segment_size });
        tmp_optional_buffer_info.push_back(buffer_info);

        current_capture_address += segment_size;
        remaining_size -= segment_size;
    }

    if (optional_buffer_info != nullptr)
    {
        *optional_buffer_info = tmp_optional_buffer_info;
    }

    return replay_ranges;
}

VulkanBufferInfo* VulkanDeviceAddressTracker::GetBufferByHandle(VkBuffer handle)
{
    auto handle_it = buffer_handles_.find(handle);
    if (handle_it != buffer_handles_.end())
    {
        const auto& [h, handle_id] = *handle_it;
        if (handle_id != format::kNullHandleId)
        {
            return object_info_table_.GetVkBufferInfo(handle_id);
        }
        else
        {
            return shadow_info_table_[handle];
        }
    }
    return nullptr;
}

const VulkanBufferInfo* VulkanDeviceAddressTracker::GetBufferByHandle(VkBuffer handle) const
{
    auto handle_it = buffer_handles_.find(handle);
    if (handle_it != buffer_handles_.end())

    {
        const auto& [h, handle_id] = *handle_it;
        if (handle_id != format::kNullHandleId)
        {
            return object_info_table_.GetVkBufferInfo(handle_id);
        }
        else
        {
            return shadow_info_table_.at(handle);
        }
    }
    return nullptr;
}

const VulkanBufferInfo*
VulkanDeviceAddressTracker::GetBufferInfo(VkDeviceAddress                                         device_address,
                                          const VulkanDeviceAddressTracker::buffer_address_map_t& address_map,
                                          size_t*                                                 offset) const
{
    if (!address_map.empty())
    {
        // find first address equal or greater
        auto address_it = address_map.lower_bound(device_address);

        if (address_it == address_map.end() || address_it->first > device_address)
        {
            // not found
            if (address_it == address_map.begin())
            {
                return nullptr;
            }

            // decrement iterator, now pointing to the first VkDeviceAddress that is lower than device_address
            address_it--;
        }
        // found_address is lower or equal to device_address
        const auto& [found_address, buffer_handle] = *address_it;

        const VulkanBufferInfo* found_buffer = nullptr;
        if (buffer_handle != format::kNullHandleId)
        {
            found_buffer = object_info_table_.GetVkBufferInfo(buffer_handle);
        }
        else
        {
            found_buffer = GetShadowBufferInfo(device_address);
        }

        if (found_buffer != nullptr)
        {
            if (device_address < found_address + found_buffer->capture_size)
            {
                if (offset != nullptr)
                {
                    *offset = device_address - found_address;
                }

                return found_buffer;
            }
        }
    }
    return nullptr;
}

[[nodiscard]] const VulkanBufferInfo*
VulkanDeviceAddressTracker::GetShadowBufferInfo(VkDeviceAddress device_address) const
{
    if (!shadow_address_map_.empty())
    {
        auto address_it = shadow_address_map_.lower_bound(device_address);

        if (address_it == shadow_address_map_.end() || address_it->first > device_address)
        {
            if (address_it == shadow_address_map_.begin())
            {
                return nullptr;
            }

            address_it--;
        }
        const auto& [found_address, buffer_handle] = *address_it;
        const VulkanBufferInfo* found_buffer       = nullptr;

        if (auto result = shadow_info_table_.find(buffer_handle); result != shadow_info_table_.end())
        {
            found_buffer = result->second;
        }

        if (found_buffer != nullptr)
        {
            if (device_address < found_address + found_buffer->capture_size)
            {
                return found_buffer;
            }
        }
    }
    return nullptr;
}

const std::unordered_set<format::HandleId>&
VulkanDeviceAddressTracker::GetAccelerationStructuresByReplayDeviceAddress(VkDeviceAddress replay_address) const
{
    // delegate query to buffer
    const auto* buffer_info = GetBufferByReplayDeviceAddress(replay_address);
    if (buffer_info != nullptr)
    {
        GFXRECON_ASSERT(replay_address >= buffer_info->replay_address);
        const VkDeviceAddress as_offset = replay_address - buffer_info->replay_address;
        auto handle_set_it = buffer_info->acceleration_structures.find(buffer_info->capture_address + as_offset);
        if (handle_set_it != buffer_info->acceleration_structures.end())
        {
            return handle_set_it->second;
        }
    }
    static const std::unordered_set<format::HandleId> empty_set;
    return empty_set;
}

const std::unordered_set<format::HandleId>&
VulkanDeviceAddressTracker::GetAccelerationStructuresByCaptureDeviceAddress(VkDeviceAddress capture_address) const
{
    // delegate query to buffer
    const auto* buffer_info = GetBufferByCaptureDeviceAddress(capture_address);
    if (buffer_info != nullptr)
    {
        auto handle_set_it = buffer_info->acceleration_structures.find(capture_address);
        if (handle_set_it != buffer_info->acceleration_structures.end())
        {
            return handle_set_it->second;
        }
    }
    static const std::unordered_set<format::HandleId> empty_set;
    return empty_set;
}

[[nodiscard]] const VulkanAccelerationStructureKHRInfo*
VulkanDeviceAddressTracker::GetAccelerationStructureByHandle(VkAccelerationStructureKHR handle) const
{
    auto handle_it = acceleration_structure_handles_.find(handle);
    if (handle_it != acceleration_structure_handles_.end())
    {
        const auto& [h, handle_id] = *handle_it;
        return object_info_table_.GetVkAccelerationStructureKHRInfo(handle_id);
    }
    return nullptr;
}

const std::unordered_map<VkDeviceAddress, VkDeviceAddress>&
VulkanDeviceAddressTracker::GetAccelerationStructureDeviceAddressMap() const
{
    return acceleration_structure_addresses_;
}

const std::unordered_map<VkDeviceAddress, VulkanDeviceAddressTracker::device_address_range_t>&
VulkanDeviceAddressTracker::GetBufferDeviceAddressMap() const
{
    return address_lookup_helper_map_;
}

[[nodiscard]] const VulkanAccelerationStructureKHRInfo*
VulkanDeviceAddressTracker::GetAccelerationStructureByID(format::HandleId handle_id) const
{
    return object_info_table_.GetVkAccelerationStructureKHRInfo(handle_id);
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
