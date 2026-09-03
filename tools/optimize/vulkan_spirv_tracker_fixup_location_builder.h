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
#ifndef GFXRECON_TOOLS_OPTIMIZE_VULKAN_SPIRV_TRACKER_FIXUP_LOCATION_BUILDER_H
#define GFXRECON_TOOLS_OPTIMIZE_VULKAN_SPIRV_TRACKER_FIXUP_LOCATION_BUILDER_H

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include "util/defines.h"
#include "vulkan_spirv_tracker_types.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

class FixupLocationBuilderTestAccess;

// Holds root-source-local fixup locations built from analysis results and later
// consumed by the modification pass.
class FixupLocationBuilder
{
    friend class FixupLocationBuilderTestAccess;

  public:
    void Reset();
    // Lower one batch of finalized device-address fixup inputs into root-source-local
    // fixup locations owned by this builder.
    void Build(const std::vector<DeviceAddressFixupInput>& inputs);
    bool HasFixups() const;
    void WriteRewritePlanJson(const std::filesystem::path& path) const;
    // Look up the fixup locations attached to one resolved root source.
    const FixupLocations* GetFixupLocations(ProvenanceRootType type, uint64_t source_index) const;

  private:
    format::AddressLocationInfo MakeFixDeviceAddressLocation(const DeviceAddressFixupInput& input) const;
    void                        DeduplicateAddressLocations(std::vector<format::AddressLocationInfo>& locations) const;

    std::unordered_map<ProvenanceRootType, FixupLocationsBySourceIndex> fixup_locations_by_root_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_VULKAN_SPIRV_TRACKER_FIXUP_LOCATION_BUILDER_H