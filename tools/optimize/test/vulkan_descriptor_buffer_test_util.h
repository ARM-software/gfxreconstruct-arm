#ifndef GFXRECON_TOOLS_OPTIMIZE_TEST_VULKAN_DESCRIPTOR_BUFFER_TEST_UTIL_H
#define GFXRECON_TOOLS_OPTIMIZE_TEST_VULKAN_DESCRIPTOR_BUFFER_TEST_UTIL_H

#include "util/call_modifier_base.h"
#include "format/format_arm.h"
#include "util/memory_output_stream.h"
#include "vulkan_descriptor_buffer_modifier.h"
#include "vulkan_spirv_tracker_fixup_location_builder.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

struct ParsedFixDescriptorDataCommand
{
    format::FixDescriptorDataCommandHeader          header{};
    std::vector<format::DescriptorDataLocationInfo> locations;
};

class VulkanDescriptorBufferModifierTestAccess
{
  public:
    static void EnableModificationPass(VulkanDescriptorBufferModifier& modifier, encode::ParameterBuffer& buffer)
    {
        modifier.SetParameterBuffer(&buffer);
    }

    static void DisableModificationPass(VulkanDescriptorBufferModifier& modifier)
    {
        modifier.SetParameterBuffer(nullptr);
    }

    static void SetBuffer(VulkanDescriptorBufferModifier& modifier,
                          format::HandleId                buffer_id,
                          uint64_t                        size,
                          VkBufferUsageFlags              usage,
                          uint64_t                        creation_index,
                          uint64_t                        destruction_index = UINT64_MAX)
    {
        auto& entry             = modifier.buffer_entries_[buffer_id];
        entry.handle            = buffer_id;
        entry.size              = size;
        entry.usage             = usage;
        entry.flags             = 0;
        entry.creation_index    = creation_index;
        entry.destruction_index = destruction_index;
    }

    static void SetMemory(VulkanDescriptorBufferModifier& modifier,
                          format::HandleId                memory_id,
                          uint64_t                        size,
                          VkDeviceSize                    map_offset,
                          VkDeviceSize                    map_size)
    {
        auto& entry                 = modifier.memory_binding_entries_[memory_id];
        entry.handle                = memory_id;
        entry.size                  = size;
        entry.type_index            = 0;
        entry.flags                 = 0;
        entry.creation_index        = 0;
        entry.destruction_index     = UINT64_MAX;
        entry.mapping.offset        = map_offset;
        entry.mapping.size          = map_size;
        entry.mapping.map_memory    = 0;
        entry.mapping.shadow_memory = 0;
    }

    static void BindBuffer(VulkanDescriptorBufferModifier& modifier,
                           format::HandleId                memory_id,
                           format::HandleId                buffer_id,
                           uint64_t                        offset)
    {
        modifier.memory_binding_entries_.at(memory_id).memory_binding_records_.push_back({ buffer_id, true, offset });
    }

    static void RecordDescriptor(VulkanDescriptorBufferModifier& modifier,
                                 uint64_t                        call_index,
                                 uint64_t                        descriptor_addr,
                                 const std::vector<uint8_t>&     payload)
    {
        modifier.RecordDescriptor(call_index, descriptor_addr, payload.size(), payload.data());
    }

    static void SetSeenVersion(VulkanDescriptorBufferModifier& modifier, uint64_t descriptor_addr, uint64_t version)
    {
        modifier.descriptor_versions_seen_[descriptor_addr] = version;
    }

    static void AddKnownLocation(VulkanDescriptorBufferModifier& modifier,
                                 format::HandleId                memory_id,
                                 uint64_t                        descriptor_addr,
                                 uint64_t                        mapped_offset,
                                 uint64_t                        buffer_offset,
                                 uint64_t                        generation,
                                 uint64_t                        size)
    {
        format::DescriptorDataLocationInfo location{};
        location.descriptor_offset_in_mapped_memory = mapped_offset;
        location.descriptor_offset_in_buffer        = buffer_offset;
        location.descriptor_offset_in_memory        = 0;
        location.descriptor_addr                    = descriptor_addr;
        location.orig_size                          = size;
        location.descriptor_generation              = generation;
        modifier.device_memory_descriptor_locations[memory_id][descriptor_addr].push_back(location);
    }

    static std::vector<format::DescriptorDataLocationInfo> GetDescriptorsInFillMemory(
        VulkanDescriptorBufferModifier& modifier, uint64_t memory_id, uint64_t offset, const std::vector<uint8_t>& data)
    {
        return modifier.GetDescriptorsInFillMemory(memory_id, offset, data.size(), data.data());
    }

    static std::vector<ParsedFixDescriptorDataCommand>
    TakeFixDescriptorDataCommands(VulkanDescriptorBufferModifier& modifier)
    {
        std::vector<std::unique_ptr<util::CallModifierBase::NewCallData>> pre_calls;
        modifier.AppendPreCalls(pre_calls);

        std::vector<ParsedFixDescriptorDataCommand> parsed_calls;
        parsed_calls.reserve(pre_calls.size());
        for (const auto& call : pre_calls)
        {
            parsed_calls.push_back(ParseFixDescriptorDataCommand(call->parameter_buffer));
        }
        return parsed_calls;
    }

  private:
    static ParsedFixDescriptorDataCommand ParseFixDescriptorDataCommand(const util::MemoryOutputStream& buffer)
    {
        ParsedFixDescriptorDataCommand parsed{};
        const uint8_t*                 data = buffer.GetData();
        std::memcpy(&parsed.header, data, sizeof(parsed.header));

        const auto* locations =
            reinterpret_cast<const format::DescriptorDataLocationInfo*>(data + sizeof(parsed.header));
        parsed.locations.assign(locations, locations + parsed.header.num_of_locations);
        return parsed;
    }
};

class FixupLocationBuilderTestAccess
{
  public:
    static void SetDescriptorDataLocations(FixupLocationBuilder&                                  builder,
                                           ProvenanceRootType                                     type,
                                           uint64_t                                               source_index,
                                           const std::vector<format::DescriptorDataLocationInfo>& locations)
    {
        builder.fixup_locations_by_root_[type][source_index].descriptor_data_locations = locations;
    }
};

inline std::vector<uint8_t> MakeSequentialBytes(uint8_t first_value, size_t count)
{
    std::vector<uint8_t> bytes(count);
    for (size_t i = 0; i < count; ++i)
    {
        bytes[i] = static_cast<uint8_t>(first_value + i);
    }
    return bytes;
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif
