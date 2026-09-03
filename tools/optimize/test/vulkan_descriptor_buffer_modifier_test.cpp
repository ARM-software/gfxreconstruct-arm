#include <nlohmann/json.hpp>
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "vulkan_descriptor_buffer_test_util.h"
#include "util/logging.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace
{

using gfxrecon::decode::ParsedFixDescriptorDataCommand;
using gfxrecon::decode::VulkanDescriptorBufferModifier;
using gfxrecon::decode::VulkanDescriptorBufferModifierTestAccess;

struct ModifierFixture
{
    static constexpr gfxrecon::format::HandleId kMemoryId = 7;
    static constexpr gfxrecon::format::HandleId kBufferId = 9;

    ModifierFixture()
    {
        gfxrecon::util::Log::Init(gfxrecon::util::LoggingSeverity::kError);
        VulkanDescriptorBufferModifierTestAccess::SetMemory(modifier, kMemoryId, 256, 0, 256);
        VulkanDescriptorBufferModifierTestAccess::SetBuffer(
            modifier, kBufferId, 256, VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT, 1);
        VulkanDescriptorBufferModifierTestAccess::BindBuffer(modifier, kMemoryId, kBufferId, 0);
        modifier.SetCurrentBlockIndex(8);
        VulkanDescriptorBufferModifierTestAccess::EnableModificationPass(modifier, parameter_buffer);
    }

    ~ModifierFixture()
    {
        VulkanDescriptorBufferModifierTestAccess::DisableModificationPass(modifier);
        gfxrecon::util::Log::Release();
    }

    std::vector<ParsedFixDescriptorDataCommand> EmitFixups(const std::vector<uint8_t>& fill_data, uint64_t offset = 0)
    {
        modifier.ProcessFillMemoryCommand(kMemoryId, offset, fill_data.size(), fill_data.data());
        return VulkanDescriptorBufferModifierTestAccess::TakeFixDescriptorDataCommands(modifier);
    }

    VulkanDescriptorBufferModifier    modifier;
    gfxrecon::encode::ParameterBuffer parameter_buffer;
};

} // namespace

TEST_CASE_METHOD(ModifierFixture,
                 "reused descriptor address emits fixup for older payload generation",
                 "[descriptor-buffer][regression]")
{
    const uint64_t descriptor_addr = 0x1000;
    auto           payload_v1      = gfxrecon::decode::MakeSequentialBytes(0x10, 16);
    auto           payload_v2      = gfxrecon::decode::MakeSequentialBytes(0x40, 16);

    // Scenario:
    // 1. vkGetDescriptorEXT writes payload_v1 to captured pointer 0x1000.
    // 2. Later, the same captured pointer 0x1000 is reused and now contains payload_v2.
    // 3. A FillMemory block that happened between those two logical generations still contains payload_v1 at offset 24.
    //
    // Before this change, the modifier kept only the latest payload per descriptor address. Once payload_v2 was seen,
    // payload_v1 was lost, so a later scan of FillMemory bytes containing payload_v1 could not match anything and no
    // fixup metadata was emitted.
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 0, descriptor_addr, payload_v1);
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 1, descriptor_addr, payload_v2);
    VulkanDescriptorBufferModifierTestAccess::AddKnownLocation(
        modifier, kMemoryId, descriptor_addr, 24, 24, 1, payload_v1.size());
    VulkanDescriptorBufferModifierTestAccess::AddKnownLocation(
        modifier, kMemoryId, descriptor_addr, 24, 24, 2, payload_v2.size());
    VulkanDescriptorBufferModifierTestAccess::SetSeenVersion(modifier, descriptor_addr, 1);

    std::vector<uint8_t> fill_data(64, 0);
    std::copy(payload_v1.begin(), payload_v1.end(), fill_data.begin() + 24);

    auto calls = EmitFixups(fill_data);
    REQUIRE(calls.size() == 1);
    REQUIRE(calls[0].header.memory_id == kMemoryId);
    REQUIRE(calls[0].locations.size() == 1);

    const auto& location = calls[0].locations[0];
    REQUIRE(location.descriptor_addr == descriptor_addr);
    REQUIRE(location.descriptor_generation == 1);
    REQUIRE(location.descriptor_offset_in_memory == 24);
    REQUIRE(location.orig_size == payload_v1.size());
}

TEST_CASE_METHOD(ModifierFixture,
                 "copied descriptor payload in other memory emits fixup",
                 "[descriptor-buffer][regression]")
{
    const uint64_t descriptor_addr = 0x2000;
    auto           payload         = gfxrecon::decode::MakeSequentialBytes(0x70, 16);

    // Scenario:
    // 1. vkGetDescriptorEXT produces one descriptor payload at captured pointer 0x2000.
    // 2. The original write provenance is lost because the bytes are copied elsewhere with a raw CPU memcpy-like write.
    // 3. A later FillMemory block contains those copied bytes starting at offset 16.
    //
    // Before this change, the modifier only recognized descriptors at the original mapped address recorded for the
    // vkGetDescriptorEXT destination. A byte-identical copy in another location was invisible, so no descriptor fixup
    // was emitted for the copied descriptor payload.
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 0, descriptor_addr, payload);
    VulkanDescriptorBufferModifierTestAccess::SetSeenVersion(modifier, descriptor_addr, 1);

    std::vector<uint8_t> fill_data(64, 0);
    std::copy(payload.begin(), payload.end(), fill_data.begin() + 16);

    auto calls = EmitFixups(fill_data);
    REQUIRE(calls.size() == 1);
    REQUIRE(calls[0].locations.size() == 1);

    const auto& location = calls[0].locations[0];
    REQUIRE(location.descriptor_addr == descriptor_addr);
    REQUIRE(location.descriptor_generation == 1);
    REQUIRE(location.descriptor_offset_in_memory == 16);
    REQUIRE(location.descriptor_offset_in_buffer == 16);
}

TEST_CASE_METHOD(ModifierFixture,
                 "descriptor selection prefers latest seen generation",
                 "[descriptor-buffer][regression]")
{
    const uint64_t descriptor_addr = 0x3000;
    auto           payload_v1      = gfxrecon::decode::MakeSequentialBytes(0x11, 16);
    auto           payload_v2      = gfxrecon::decode::MakeSequentialBytes(0x31, 16);
    auto           payload_v3      = gfxrecon::decode::MakeSequentialBytes(0x51, 16);

    // Scenario:
    // 1. The same captured descriptor address is reused three times, creating generations 1, 2, and 3.
    // 2. The FillMemory block being scanned belongs to a point in the trace where only generations 1 and 2 had
    //    happened yet.
    // 3. The actual bytes in the FillMemory block are payload_v2 at offset 40.
    //
    // The intended behavior is not merely "pick the newest descriptor ever seen", but "pick the newest generation that
    // existed at this point in the trace". This test makes generation 3 globally available while proving generation 2
    // must still win because generation 3 has not been marked seen yet.
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 0, descriptor_addr, payload_v1);
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 1, descriptor_addr, payload_v2);
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 2, descriptor_addr, payload_v3);
    VulkanDescriptorBufferModifierTestAccess::AddKnownLocation(
        modifier, kMemoryId, descriptor_addr, 40, 40, 1, payload_v1.size());
    VulkanDescriptorBufferModifierTestAccess::AddKnownLocation(
        modifier, kMemoryId, descriptor_addr, 40, 40, 2, payload_v2.size());
    VulkanDescriptorBufferModifierTestAccess::AddKnownLocation(
        modifier, kMemoryId, descriptor_addr, 40, 40, 3, payload_v3.size());
    VulkanDescriptorBufferModifierTestAccess::SetSeenVersion(modifier, descriptor_addr, 2);

    std::vector<uint8_t> fill_data(80, 0);
    std::copy(payload_v2.begin(), payload_v2.end(), fill_data.begin() + 40);

    auto calls = EmitFixups(fill_data);
    REQUIRE(calls.size() == 1);
    REQUIRE(calls[0].locations.size() == 1);
    REQUIRE(calls[0].locations[0].descriptor_generation == 2);
}

TEST_CASE_METHOD(ModifierFixture,
                 "copied descriptor scan should not emit overlapping replacements",
                 "[descriptor-buffer][regression]")
{
    const uint64_t descriptor_a = 0x4000;
    const uint64_t descriptor_b = 0x5000;

    auto                 payload_a = gfxrecon::decode::MakeSequentialBytes(0x01, 16);
    std::vector<uint8_t> payload_b(payload_a.begin() + 8, payload_a.end());

    // Scenario:
    // 1. descriptor_a is a 16-byte payload: bytes 0..15.
    // 2. descriptor_b is an 8-byte payload equal to descriptor_a's trailing half: bytes 8..15.
    // 3. The FillMemory data contains only descriptor_a, starting at offset 0.
    //
    // A correct copied-descriptor scan should emit one replacement covering the outer 16-byte match at offset 0.
    // The inner 8-byte sequence at offset 8 is not a separate safe replacement because it overlaps bytes already
    // claimed by descriptor_a.
    //
    // The current implementation walks the FillMemory range one byte at a time and only records the matched *start*
    // offset as occupied. After matching descriptor_a at offset 0, offset 8 is still considered free, so descriptor_b
    // is matched inside descriptor_a and an overlapping second replacement is emitted.
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 0, descriptor_a, payload_a);
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 1, descriptor_b, payload_b);
    VulkanDescriptorBufferModifierTestAccess::SetSeenVersion(modifier, descriptor_a, 1);
    VulkanDescriptorBufferModifierTestAccess::SetSeenVersion(modifier, descriptor_b, 1);

    auto calls = EmitFixups(payload_a);
    REQUIRE(calls.size() == 1);
    REQUIRE(calls[0].locations.size() == 1);
}

TEST_CASE_METHOD(ModifierFixture,
                 "copied descriptor scan should prefer newest candidate for shared prefix payloads",
                 "[descriptor-buffer][regression]")
{
    const uint64_t descriptor_long  = 0x7000;
    const uint64_t descriptor_short = 0x8000;

    auto                 payload_long = gfxrecon::decode::MakeSequentialBytes(0x20, 16);
    std::vector<uint8_t> payload_short(payload_long.begin(), payload_long.begin() + 12);

    // Scenario:
    // 1. descriptor_long is recorded first with a 16-byte payload.
    // 2. Later, descriptor_short is recorded with a 12-byte payload that matches the prefix of descriptor_long.
    // 3. FillMemory contains the full 16-byte payload_long bytes at offset 0.
    //
    // The pre-trie implementation iterated matching candidates in reverse insertion order, so the newer descriptor
    // payload won even when an older, longer payload shared the same leading bytes. The trie implementation must
    // preserve that precedence instead of preferring the deepest/longest match.
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 0, descriptor_long, payload_long);
    VulkanDescriptorBufferModifierTestAccess::RecordDescriptor(modifier, 1, descriptor_short, payload_short);
    VulkanDescriptorBufferModifierTestAccess::SetSeenVersion(modifier, descriptor_long, 1);
    VulkanDescriptorBufferModifierTestAccess::SetSeenVersion(modifier, descriptor_short, 1);

    auto calls = EmitFixups(payload_long);
    REQUIRE(calls.size() == 1);
    REQUIRE(calls[0].locations.size() == 1);
    REQUIRE(calls[0].locations[0].descriptor_addr == descriptor_short);
    REQUIRE(calls[0].locations[0].orig_size == payload_short.size());
}

TEST_CASE("rewrite plan json should include descriptor generation", "[descriptor-buffer][regression]")
{
    gfxrecon::util::Log::Init(gfxrecon::util::LoggingSeverity::kError);

    gfxrecon::decode::FixupLocationBuilder       builder;
    gfxrecon::format::DescriptorDataLocationInfo location{};
    location.descriptor_offset_in_mapped_memory = 12;
    location.descriptor_offset_in_buffer        = 8;
    location.descriptor_offset_in_memory        = 4;
    location.descriptor_addr                    = 0x6000;
    location.orig_size                          = 16;
    location.descriptor_generation              = 3;

    // Scenario:
    // 1. A descriptor location already carries generation = 3, which is required to disambiguate reused descriptor
    //    addresses.
    // 2. The rewrite-plan JSON is written from that in-memory DescriptorDataLocationInfo.
    //
    // The metadata JSON path for FixDescriptorDataCommand already serializes this field, but the SPIR-V rewrite-plan
    // JSON path does not. As a result, a consumer of the rewrite plan cannot tell which descriptor generation was
    // selected when a captured descriptor address has been reused multiple times.
    gfxrecon::decode::FixupLocationBuilderTestAccess::SetDescriptorDataLocations(
        builder, gfxrecon::decode::ProvenanceRootType::FillMemory, 23, { location });

    const auto path = std::filesystem::temp_directory_path() / "gfxrecon_optimize_rewrite_plan_regression.json";

    builder.WriteRewritePlanJson(path);

    std::ifstream input(path);
    REQUIRE(input.good());

    nlohmann::ordered_json json;
    input >> json;

    REQUIRE(json["root_fixups"].size() == 1);
    const auto& json_location = json["root_fixups"][0]["descriptor_data_locations"][0];
    REQUIRE(json_location.contains("generation"));
    REQUIRE(json_location["generation"] == location.descriptor_generation);

    input.close();
    REQUIRE(std::filesystem::remove(path));
    gfxrecon::util::Log::Release();
}
