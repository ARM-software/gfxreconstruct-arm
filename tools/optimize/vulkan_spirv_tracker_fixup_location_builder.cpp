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
#include <algorithm>
#include <fstream>
#include <vector>

#include "vulkan_spirv_tracker_fixup_location_builder.h"
#include "util/json_util.h"

namespace
{

const char* ProvenanceRootTypeToString(gfxrecon::decode::ProvenanceRootType type)
{
    switch (type)
    {
        case gfxrecon::decode::ProvenanceRootType::FillMemory:
            return "FillMemory";
        case gfxrecon::decode::ProvenanceRootType::UpdateBuffer:
            return "UpdateBuffer";
        case gfxrecon::decode::ProvenanceRootType::PushConstants:
            return "PushConstants";
        default:
            return "Unknown";
    }
}

nlohmann::ordered_json AddressLocationInfoToJson(const gfxrecon::format::AddressLocationInfo& location)
{
    nlohmann::ordered_json jdata = nlohmann::ordered_json::object();

    gfxrecon::util::HandleToJson(jdata["object_id"], location.id);
    jdata["object_size"] = location.size;
    gfxrecon::util::FieldToJsonAsHex(jdata["original_address"], location.original_address);
    gfxrecon::util::FieldToJsonAsHex(jdata["adjusted_address"], location.adjusted_address);
    jdata["offset_in_memory"] = location.offset_in_memory;
    return jdata;
}

nlohmann::ordered_json DescriptorDataLocationInfoToJson(const gfxrecon::format::DescriptorDataLocationInfo& location)
{
    nlohmann::ordered_json jdata = nlohmann::ordered_json::object();

    jdata["offset_in_mapped_memory"] = location.descriptor_offset_in_mapped_memory;
    jdata["offset_in_buffer"]        = location.descriptor_offset_in_buffer;
    jdata["offset_in_memory"]        = location.descriptor_offset_in_memory;
    gfxrecon::util::FieldToJsonAsHex(jdata["descriptor_address"], location.descriptor_addr);
    jdata["orig_size"]            = location.orig_size;
    jdata["generation"]           = location.descriptor_generation;
    return jdata;
}

} // namespace

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

void FixupLocationBuilder::Reset()
{
    fixup_locations_by_root_.clear();
}

void FixupLocationBuilder::Build(const std::vector<DeviceAddressFixupInput>& inputs)
{
    Reset();

    for (const auto& input : inputs)
    {
        ProvenanceRootType root_type    = input.resolved_root.type;
        uint64_t           source_index = input.resolved_root.root.source_index;

        auto& locations = fixup_locations_by_root_[root_type][source_index].address_locations;
        locations.push_back(MakeFixDeviceAddressLocation(input));
    }

    for (auto& [root_type, locations_by_source_index] : fixup_locations_by_root_)
    {
        (void)root_type;
        for (auto& [source_index, locations] : locations_by_source_index)
        {
            (void)source_index;
            DeduplicateAddressLocations(locations.address_locations);
        }
    }
}

bool FixupLocationBuilder::HasFixups() const
{
    return !fixup_locations_by_root_.empty();
}

void FixupLocationBuilder::WriteRewritePlanJson(const std::filesystem::path& path) const
{
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file)
    {
        GFXRECON_LOG_WARNING("Failed to open SPIR-V tracker rewrite plan JSON output: %s", path.c_str());
        return;
    }

    nlohmann::ordered_json jdata            = nlohmann::ordered_json::object();
    auto&                  root_fixups_json = jdata["root_fixups"] = nlohmann::ordered_json::array();

    std::vector<ProvenanceRootType> root_types;
    root_types.reserve(fixup_locations_by_root_.size());
    for (const auto& [root_type, locations_by_source_index] : fixup_locations_by_root_)
    {
        (void)locations_by_source_index;
        root_types.push_back(root_type);
    }
    std::sort(root_types.begin(), root_types.end(), [](ProvenanceRootType lhs, ProvenanceRootType rhs) {
        return static_cast<uint32_t>(lhs) < static_cast<uint32_t>(rhs);
    });

    for (ProvenanceRootType root_type : root_types)
    {
        const auto type_iter = fixup_locations_by_root_.find(root_type);
        if (type_iter == fixup_locations_by_root_.end())
        {
            continue;
        }

        std::vector<uint64_t> source_indices;
        source_indices.reserve(type_iter->second.size());
        for (const auto& [source_index, locations] : type_iter->second)
        {
            (void)locations;
            source_indices.push_back(source_index);
        }
        std::sort(source_indices.begin(), source_indices.end());

        for (uint64_t source_index : source_indices)
        {
            const auto source_iter = type_iter->second.find(source_index);
            if (source_iter == type_iter->second.end())
            {
                continue;
            }

            nlohmann::ordered_json root_fixup = nlohmann::ordered_json::object();
            root_fixup["root_type"]           = ProvenanceRootTypeToString(root_type);
            root_fixup["source_index"]        = source_index;

            if (!source_iter->second.address_locations.empty())
            {
                auto& address_locations_json = root_fixup["address_locations"] = nlohmann::ordered_json::array();
                for (const auto& location : source_iter->second.address_locations)
                {
                    address_locations_json.push_back(AddressLocationInfoToJson(location));
                }
            }

            if (!source_iter->second.descriptor_data_locations.empty())
            {
                auto& descriptor_data_locations_json = root_fixup["descriptor_data_locations"] =
                    nlohmann::ordered_json::array();
                for (const auto& location : source_iter->second.descriptor_data_locations)
                {
                    descriptor_data_locations_json.push_back(DescriptorDataLocationInfoToJson(location));
                }
            }

            root_fixups_json.push_back(std::move(root_fixup));
        }
    }

    file << jdata.dump(util::kJsonIndentWidth) << '\n';
    if (!file)
    {
        GFXRECON_LOG_WARNING("Failed to write SPIR-V tracker rewrite plan JSON output: %s", path.c_str());
    }
}

const FixupLocations* FixupLocationBuilder::GetFixupLocations(ProvenanceRootType type, uint64_t source_index) const
{
    const auto type_iter = fixup_locations_by_root_.find(type);
    if (type_iter == fixup_locations_by_root_.end())
    {
        return nullptr;
    }

    const auto source_iter = type_iter->second.find(source_index);
    if (source_iter == type_iter->second.end())
    {
        return nullptr;
    }

    return &source_iter->second;
}

format::AddressLocationInfo
FixupLocationBuilder::MakeFixDeviceAddressLocation(const DeviceAddressFixupInput& input) const
{
    format::AddressLocationInfo location = {};

    location.id               = input.referenced_object;
    location.size             = input.referenced_object_size;
    location.original_address = input.original_address;
    location.adjusted_address = input.adjusted_address;
    location.offset_in_memory = input.resolved_root.root.payload_offset;
    location.new_address      = 0;
    return location;
}

void FixupLocationBuilder::DeduplicateAddressLocations(std::vector<format::AddressLocationInfo>& locations) const
{
    std::sort(locations.begin(),
              locations.end(),
              [](const format::AddressLocationInfo& lhs, const format::AddressLocationInfo& rhs) {
                  if (lhs.id != rhs.id)
                  {
                      return lhs.id < rhs.id;
                  }
                  if (lhs.original_address != rhs.original_address)
                  {
                      return lhs.original_address < rhs.original_address;
                  }
                  if (lhs.adjusted_address != rhs.adjusted_address)
                  {
                      return lhs.adjusted_address < rhs.adjusted_address;
                  }
                  return lhs.offset_in_memory < rhs.offset_in_memory;
              });

    locations.erase(std::unique(locations.begin(),
                                locations.end(),
                                [](const format::AddressLocationInfo& lhs, const format::AddressLocationInfo& rhs) {
                                    return (lhs.id == rhs.id) && (lhs.original_address == rhs.original_address) &&
                                           (lhs.adjusted_address == rhs.adjusted_address) &&
                                           (lhs.offset_in_memory == rhs.offset_in_memory);
                                }),
                    locations.end());
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)