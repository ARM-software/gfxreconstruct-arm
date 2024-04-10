//#pragma once
#include "encode/vulkan_device_address_tracker.h"
#include "encode/vulkan_capture_manager.h"
GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)

void VulkanDeviceAddressTracker::TrackBufferDeviceAddress(format::HandleId id,
                                                          uint64_t         buffer_size,
                                                          VkDeviceAddress  address)
{
    lock();
    assert(address != 0);
    format::AddressLocationInfo loc{};
    loc.id               = id;
    loc.original_address = address;
    loc.size             = buffer_size;
    tracked_objects[id]  = loc;
    unlock();
}

void VulkanDeviceAddressTracker::TrackAccelerationStructureDeviceAddress(format::HandleId id, VkDeviceAddress address)
{
    lock();
    assert(address != 0);
    format::AddressLocationInfo loc{};
    loc.id               = id;
    loc.original_address = address;
    loc.adjusted_address = address;
    tracked_objects[id]  = loc;
    unlock();
}

std::vector<format::AddressLocationInfo>
VulkanDeviceAddressTracker::GetAddressesInMemoryRange(void* start_address, size_t offset, size_t size)
{
    std::vector<format::AddressLocationInfo> locations;
    uint64_t*                                start       = (uint64_t*)((uint8_t*)start_address + offset);
    uint64_t*                                end         = (uint64_t*)((uint8_t*)start_address + offset + size);
    gfxrecon::encode::VulkanCaptureManager*  cap_manager = gfxrecon::encode::VulkanCaptureManager::Get();

    lock();
    if (tracked_objects.empty())
    {
        unlock();
        return locations;
    }
    auto [min, max] =
        std::minmax_element(tracked_objects.begin(), tracked_objects.end(), [](const auto& a, const auto& b) {
            return a.second.original_address < b.second.original_address;
        });
    const VkDeviceAddress min_addr = min->second.original_address;
    const VkDeviceAddress max_addr = max->second.original_address + max->second.size;
    for (uint64_t* ptr = start; ptr != end; ptr++)
    {
        const uint64_t value = *ptr;
        if (value >= min_addr && value <= max_addr)
        {
            for (auto& [entry, loc] : tracked_objects)
            {
                bool is_value_in_range = (value >= loc.original_address) && (value <= loc.original_address + loc.size);
                if (is_value_in_range)
                {
                    loc.adjusted_address = value;
                    loc.offset_in_memory = (uint64_t)ptr - (uint64_t)start;
                    locations.push_back(loc);
                    break;
                }
            }
        }
    }
    unlock();
    return locations;
}
GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)