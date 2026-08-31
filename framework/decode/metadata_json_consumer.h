/*
** Copyright (c) 2023 Valve Corporation
** Copyright (c) 2022-2023 LunarG, Inc.
** Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef GFXRECON_DECODE_METADATA_JSON_CONSUMER_H
#define GFXRECON_DECODE_METADATA_JSON_CONSUMER_H

#include "decode/custom_vulkan_struct_to_json.h"
#include "format/format_json.h"
#include "util/defines.h"
#include "util/file_path.h"
#include "util/strings.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

using util::FieldToJson;
using util::HandleToJson;
using util::WriteChecksumToJson;

/// Template shim for turning metadata (metacommands) into JSON using a
/// JsonWriter pointed to by a protected pointer called writer_ in a base class.
/// This can be shimmed into the inheritance hierarchy for any JSON consumer as
/// is done by gfxrecon-convert's main.cpp for both Vulkan and DX12 consumers to
/// provide metacommand support.
/// @note Any API-specific metablocks/metadata/metacommands are best handled directly by the JSON consumer of that API.
/// @note An alternative implementation would make MetadataConsumerBase a virtual base class of the base
/// Vulkan and DX12 consumers, and also a virtual base of a non-template version of MetadataJsonConsumer,
/// and VulkanJSONConsumer and DX12JsonConsumer would both use multiple inheritance to mix-in this posited non-template
/// MetadataJsonConsumer. The advantage would be code space saved due to avoiding two template instantiations with
/// identical code. The downside would be the slower virtual function dispatch due to diamond inheritance from the
/// use of virtual base classes (See Stroustrup, Design and Evolution of C++ for overheads of virtual base classes).
template <class Base>
class MetadataJsonConsumer : public Base
{
    inline nlohmann::ordered_json& WriteMetaCommandStart(const std::string& command_name) const
    {
        this->writer_->SetCurrentBlockIndex(this->block_index_);
        return this->writer_->WriteMetaCommandStart(command_name);
    }
    inline void WriteBlockEnd() { this->writer_->WriteBlockEnd(); }

  public:
    /// @defGroup ApiAgnosticMetaBlocks Metablocks used by both Vulkan and DX12.
    /// @{
    virtual void ProcessDisplayMessageCommand(const std::string& message) override
    {
        auto& jdata = WriteMetaCommandStart("DisplayMessageCommand");
        FieldToJson(jdata["message"], message);
        WriteBlockEnd();
    }

    virtual void
    ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override
    {
        auto& jdata = WriteMetaCommandStart("FillMemoryCommand");
        HandleToJson(jdata["memory_id"], memory_id);
        jdata["offset"] = offset;
        jdata["size"]   = size;
        WriteChecksumToJson(jdata, data, size);
        RepresentBinaryFile(*(this->writer_), jdata[format::kNameData], "fill_memory.bin", size, data);
        WriteBlockEnd();
    }

    virtual void ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader&    header,
                                                const std::vector<format::AddressLocationInfo>& infos) override
    {
        using namespace util;
        auto& jdata = WriteMetaCommandStart("FixDeviceAddressCommand");
        HandleToJson(jdata["relation_id"], header.relation_id);
        if (util::JsonOptions::verbose)
        {
            for (int i = 0; i < header.num_of_locations; i++)
            {
                HandleToJson(jdata["location"][i]["object_id"], infos[i].id);
                jdata["location"][i]["original_address"] = infos[i].original_address;
                jdata["location"][i]["adjusted_address"] = infos[i].adjusted_address;
                jdata["location"][i]["offset_in_memory"] = infos[i].offset_in_memory;
            }
        }
        WriteBlockEnd();
    }

    virtual void ProcessFixShaderGroupHandleCommand(const format::FixShaderGroupHandleCommandHeader&     header,
                                                    const std::vector<format::ShaderHandleLocationInfo>& infos) override
    {
        using namespace util;
        auto& jdata = WriteMetaCommandStart("FixShaderGroupHandleCommand");
        HandleToJson(jdata["relation_id"], header.relation_id);
        if (util::JsonOptions::verbose)
        {
            for (int i = 0; i < header.num_of_locations; i++)
            {
                HandleToJson(jdata["location"][i]["pipeline_id"], infos[i].id);
                jdata["location"][i]["group"]            = infos[i].group;
                jdata["location"][i]["group_size"]       = infos[i].group_size;
                jdata["location"][i]["offset_in_memory"] = infos[i].offset_in_memory;
            }
        }
        WriteBlockEnd();
    }

    virtual void ProcessTraceHelpersDataCommand(const format::TraceHelpersDataCommandHeader&      header,
                                                const std::vector<format::TraceHelpersDataInfos>& infos) override
    {
        using namespace util;
        auto& jdata = WriteMetaCommandStart("TraceHelpersDataCommand");
        HandleToJson(jdata["count"], header.count);

        for (int i = 0; i < header.count; i++)
        {
            jdata["infos"][i]["markingTypes"] = infos[i].markingTypes;
            jdata["infos"][i]["subTypes"]     = infos[i].subTypes;
            jdata["infos"][i]["offsets"]      = infos[i].offsets;
        }

        WriteBlockEnd();
    }

    virtual void ProcessFixDescriptorDataCommand(const format::FixDescriptorDataCommandHeader&          header,
                                                 const std::vector<format::DescriptorDataLocationInfo>& infos) override
    {
        using namespace util;
        auto& jdata = WriteMetaCommandStart("FixDescriptorDataCommand");
        HandleToJson(jdata["memory_id"], header.memory_id);
        if (util::JsonOptions::verbose)
        {
            for (int i = 0; i < header.num_of_locations; i++)
            {
                jdata["location"][i]["offset_in_mapped_memory"] = infos[i].descriptor_offset_in_mapped_memory;
                jdata["location"][i]["offset_in_buffer"]        = infos[i].descriptor_offset_in_buffer;
                jdata["location"][i]["offset_in_memory"]        = infos[i].descriptor_offset_in_memory;
                jdata["location"][i]["descriptor_address"]      = infos[i].descriptor_addr;
                jdata["location"][i]["orig_size"]               = infos[i].orig_size;
                jdata["location"][i]["generation"]              = infos[i].descriptor_generation;
                jdata["location"][i]["is_descriptor_buffer"]    = infos[i].is_descriptor_buffer;
            }
        }
        WriteBlockEnd();
    }

    virtual void
    ProcessFixShadowMemoryCommand(format::HandleId memory_id, uint64_t map_memory, uint64_t shadow_memory) override
    {
        using namespace util;
        auto& jdata = WriteMetaCommandStart("FixShadowMemoryCommand");
        HandleToJson(jdata["memory_id"], memory_id);
        FieldToJsonAsHex(jdata["map_memory"], map_memory);
        FieldToJsonAsHex(jdata["shadow_memory"], shadow_memory);

        WriteBlockEnd();
    }

    virtual void Process_ExeFileInfo(const gfxrecon::util::filepath::FileInfo& info) override
    {
        auto& jdata              = WriteMetaCommandStart("ExeFileInfo");
        jdata["product_version"] = info.ProductVersion;
        jdata["file_version"]    = info.FileVersion;
        FieldToJson(jdata["app_version"], info.AppVersion);
        jdata["app_name"]          = info.AppName;
        jdata["company_name"]      = info.CompanyName;
        jdata["file_description"]  = info.FileDescription;
        jdata["internal_name"]     = info.InternalName;
        jdata["original_filename"] = info.OriginalFilename;
        jdata["product_name"]      = info.ProductName;
        WriteBlockEnd();
    }

    virtual void ProcessResizeWindowCommand(format::HandleId surface_id, uint32_t width, uint32_t height) override
    {
        using namespace util;
        auto& jdata = WriteMetaCommandStart("ResizeWindowCommand");
        HandleToJson(jdata["surface_id"], surface_id);
        jdata["width"]  = width;
        jdata["height"] = height;
        WriteBlockEnd();
    }

    virtual void ProcessResizeWindowCommand2(format::HandleId surface_id,
                                             uint32_t         width,
                                             uint32_t         height,
                                             uint32_t         pre_transform) override
    {
        auto& jdata = WriteMetaCommandStart("ResizeWindowCommand2");
        HandleToJson(jdata["surface_id"], surface_id);
        jdata["width"]         = width;
        jdata["height"]        = height;
        jdata["pre_transform"] = pre_transform;
        WriteBlockEnd();
    }

    virtual void
    ProcessCreateHardwareBufferCommand(format::HandleId                                    device_id,
                                       format::HandleId                                    memory_id,
                                       uint64_t                                            buffer_id,
                                       uint32_t                                            format,
                                       uint32_t                                            width,
                                       uint32_t                                            height,
                                       uint32_t                                            stride,
                                       uint64_t                                            usage,
                                       uint32_t                                            layers,
                                       const std::vector<format::HardwareBufferPlaneInfo>& plane_info) override
    {
        auto& jdata = WriteMetaCommandStart("CreateHardwareBufferCommand");
        HandleToJson(jdata["device_id"], device_id);
        HandleToJson(jdata["memory_id"], memory_id);
        HandleToJson(jdata["buffer_id"], buffer_id);
        jdata["format"] = format;
        jdata["width"]  = width;
        jdata["height"] = height;
        jdata["stride"] = stride;
        jdata["usage"]  = usage;
        jdata["layers"] = layers;
        WriteBlockEnd();
    }

    virtual void ProcessDestroyHardwareBufferCommand(const uint64_t buffer_id) override
    {
        auto& jdata = WriteMetaCommandStart("DestroyHardwareBufferCommand");
        HandleToJson(jdata["buffer_id"], buffer_id);
        WriteBlockEnd();
    }

    virtual void ProcessSetDevicePropertiesCommand(format::HandleId   physical_device_id,
                                                   uint32_t           api_version,
                                                   uint32_t           driver_version,
                                                   uint32_t           vendor_id,
                                                   uint32_t           device_id,
                                                   uint32_t           device_type,
                                                   const uint8_t      pipeline_cache_uuid[format::kUuidSize],
                                                   const std::string& device_name) override
    {
        auto& jdata = WriteMetaCommandStart("SetDevicePropertiesCommand");
        HandleToJson(jdata["physical_device_id"], physical_device_id);
        jdata["api_version"]    = api_version;
        jdata["driver_version"] = driver_version;
        jdata["vendor_id"]      = vendor_id;
        jdata["device_id"]      = device_id;
        jdata["device_type"]    = device_type;
        FieldToJson(jdata["pipeline_cache_uuid"], util::uuid_to_string(format::kUuidSize, pipeline_cache_uuid));
        FieldToJson(jdata["device_name"], device_name);
        WriteBlockEnd();
    }

    virtual void
    ProcessSetOpaqueAddressCommand(format::HandleId device_id, format::HandleId object_id, uint64_t address) override
    {
        auto& jdata = WriteMetaCommandStart("SetOpaqueAddressCommand");
        HandleToJson(jdata["device_id"], device_id);
        HandleToJson(jdata["object_id"], object_id);
        FieldToJson(jdata["address"], util::to_hex_variable_width(address));
        WriteBlockEnd();
    }

    void ProcessSetOpaqueDescriptorDataCommand(format::HandleId device_id,
                                               format::HandleId object_id,
                                               uint32_t         data_size,
                                               const uint8_t*   data) override
    {
        auto& jdata = WriteMetaCommandStart("SetOpaqueDescriptorDataCommand");
        HandleToJson(jdata["device_id"], device_id);
        HandleToJson(jdata["object_id"], object_id);
        jdata["data_size"] = data_size;
        RepresentBinaryFile(*(this->writer_), jdata[format::kNameData], "opaque_descriptor_data.bin", data_size, data);
        WriteBlockEnd();
    }

    virtual void ProcessSetRayTracingShaderGroupHandlesCommand(format::HandleId device_id,
                                                               format::HandleId pipeline_id,
                                                               size_t           data_size,
                                                               const uint8_t*   data) override
    {
        auto& jdata = WriteMetaCommandStart("SetRayTracingShaderGroupHandlesCommand");
        HandleToJson(jdata["device_id"], device_id);
        HandleToJson(jdata["pipeline_id"], pipeline_id);
        jdata["data_size"] = data_size;
        WriteChecksumToJson(jdata, data, data_size);
        RepresentBinaryFile(
            *(this->writer_), jdata[format::kNameData], "set_raytracing_shader_group_handles.bin", data_size, data);
        WriteBlockEnd();
    }

    virtual void
    ProcessSetSwapchainImageStateCommand(format::HandleId                                    device_id,
                                         format::HandleId                                    swapchain_id,
                                         uint32_t                                            last_presented_image,
                                         const std::vector<format::SwapchainImageStateInfo>& image_state) override
    {
        auto& jdata = WriteMetaCommandStart("SetSwapchainImageStateCommand");
        HandleToJson(jdata["device_id"], device_id);
        HandleToJson(jdata["swapchain_id"], swapchain_id);
        jdata["last_presented_image"] = last_presented_image;
        FieldToJson(jdata["image_state"], "not available");
        WriteBlockEnd();
    }

    virtual void ProcessInitializeMetaCommand(const format::InitializeMetaCommand& command_header,
                                              const uint8_t*                       parameters_data) override
    {
        auto& jdata = WriteMetaCommandStart("InitializeMetaCommand");
        HandleToJson(jdata["MetaCommand_id"], command_header.capture_id);
        jdata["InitializationParametersDataSizeInBytes"] = command_header.data_size;
        WriteBlockEnd();
    }

    virtual void ProcessBeginResourceInitCommand(format::HandleId device_id,
                                                 uint64_t         max_resource_size,
                                                 uint64_t         max_copy_size) override
    {
        auto& jdata = WriteMetaCommandStart("BeginResourceInitCommand");
        HandleToJson(jdata["device_id"], device_id);

        // TODO: should be "total_copy_size"
        jdata["max_resource_size"] = max_resource_size;
        jdata["max_copy_size"]     = max_copy_size;
        WriteBlockEnd();
    }

    virtual void ProcessEndResourceInitCommand(format::HandleId device_id) override
    {
        auto& jdata = WriteMetaCommandStart("EndResourceInitCommand");
        HandleToJson(jdata["device_id"], device_id);
        WriteBlockEnd();
    }

    virtual void ProcessInitBufferCommand(format::HandleId device_id,
                                          format::HandleId buffer_id,
                                          uint64_t         data_size,
                                          const uint8_t*   data) override
    {
        auto& jdata = WriteMetaCommandStart("InitBufferCommand");
        HandleToJson(jdata["device_id"], device_id);
        HandleToJson(jdata["buffer_id"], buffer_id);
        jdata["data_size"] = data_size;
        WriteChecksumToJson(jdata, data, data_size);
        RepresentBinaryFile(*(this->writer_), jdata[format::kNameData], "init_buffer.bin", data_size, data);
        WriteBlockEnd();
    }

    virtual void ProcessInitTensorCommand(format::HandleId device_id,
                                          format::HandleId tensor_id,
                                          uint64_t         data_size,
                                          const uint8_t*   data) override
    {
        auto& jdata = WriteMetaCommandStart("InitTensorCommand");
        HandleToJson(jdata["device_id"], device_id);
        HandleToJson(jdata["tensor_id"], tensor_id);
        jdata["data_size"] = data_size;
        WriteChecksumToJson(jdata, data, data_size);
        RepresentBinaryFile(*(this->writer_), jdata[format::kNameData], "init_tensor.bin", data_size, data);
        WriteBlockEnd();
    }

    virtual void ProcessInitImageCommand(format::HandleId             device_id,
                                         format::HandleId             image_id,
                                         uint64_t                     data_size,
                                         uint32_t                     aspect,
                                         uint32_t                     layout,
                                         const std::vector<uint64_t>& level_sizes,
                                         const uint8_t*               data) override
    {
        auto& jdata = WriteMetaCommandStart("InitImageCommand");
        HandleToJson(jdata["device_id"], device_id);
        HandleToJson(jdata["image_id"], image_id);
        jdata["data_size"] = data_size;
        WriteChecksumToJson(jdata, data, data_size);
        jdata["aspect"] = aspect;
        jdata["layout"] = layout;
        FieldToJson(jdata["level_sizes"], "not available");
        RepresentBinaryFile(*(this->writer_), jdata[format::kNameData], "init_image.bin", data_size, data);
        WriteBlockEnd();
    }

    void ProcessSetTlasToBlasRelationCommand(format::HandleId tlas, const std::vector<format::HandleId>& blases)
    {
        auto& jdata = WriteMetaCommandStart("SetTLAStoBLASRelationCommand");
        HandleToJson(jdata["TLAS_id"], tlas);
        for (const auto& blas_id : blases)
        {
            jdata["BLAS_ids"].push_back(blas_id);
        }
        WriteBlockEnd();
    }

    void ProcessMicromapCompactionDependencyCommand(format::HandleId                     parent,
                                                    const std::vector<format::HandleId>& children)
    {
        auto& jdata = WriteMetaCommandStart("MicromapCompactionDependencyCommand");
        HandleToJson(jdata["Parent"], parent);
        for (const auto& child : children)
        {
            jdata["Children"].push_back(child);
        }
        WriteBlockEnd();
    }

    void ProcessAccelerationStructureCompactionDependencyCommand(format::HandleId                     parent,
                                                                 const std::vector<format::HandleId>& children)
    {
        auto& jdata = WriteMetaCommandStart("AccelerationStructureCompactionDependencyCommand");
        HandleToJson(jdata["Parent"], parent);
        for (const auto& child : children)
        {
            jdata["Children"].push_back(child);
        }
        WriteBlockEnd();
    }

    virtual void ProcessSetEnvironmentVariablesCommand(const format::SetEnvironmentVariablesCommand& header,
                                                       const char* env_string) override
    {
        auto& json_data = WriteMetaCommandStart("SetEnvironmentVariablesCommand");

        std::vector<std::string> env_vars =
            util::strings::SplitString(std::string_view(env_string), format::kEnvironmentStringDelimeter);
        for (std::string& e : env_vars)
        {
            std::vector<std::string> var_plus_val = util::strings::SplitString(e, '=');
            if (var_plus_val.size() == 2)
            {
                const char* var = var_plus_val[0].c_str();
                const char* val = var_plus_val[1].c_str();
                json_data[var]  = val;
            }
        }
        WriteBlockEnd();
    }

    virtual void ProcessExecuteBlocksFromFile(uint32_t n_blocks, int64_t offset, const std::string& filename) override
    {
        auto& jdata       = WriteMetaCommandStart("ExecuteBlocksFromFile");
        jdata["n_blocks"] = n_blocks;
        jdata["offset"]   = offset;
        FieldToJson(jdata["filename"], filename);
        WriteBlockEnd();
    }

    virtual void ProcessVulkanBuildAccelerationStructuresCommand(
        format::HandleId                                                           device_id,
        uint32_t                                                                   info_count,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* geometry_infos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   range_infos,
        std::vector<std::vector<VkAccelerationStructureInstanceKHR>>&              instance_buffers_data) override
    {
        auto& jdata = WriteMetaCommandStart("ProcessVulkanBuildAccelerationStructuresCommand");
        HandleToJson(jdata["device_id"], device_id);
        jdata["infoCount"] = info_count;
        FieldToJson(jdata["pInfos"], geometry_infos);
        FieldToJson(jdata["ppBuildRangeInfos"], range_infos);
        WriteBlockEnd();
    }

    void ProcessVulkanCopyAccelerationStructuresCommand(
        format::HandleId                                                  device_id,
        StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* copy_infos) override
    {
        auto& jdata = WriteMetaCommandStart("VulkanCopyAccelerationStructuresCommand");
        HandleToJson(jdata["device_id"], device_id);
        FieldToJson(jdata["pInfo"], copy_infos);
        WriteBlockEnd();
    }

    virtual void ProcessVulkanWriteAccelerationStructuresPropertiesCommand(
        format::HandleId device_id, VkQueryType query_type, format::HandleId acceleration_structure_id) override
    {
        auto& jdata = WriteMetaCommandStart("VulkanWriteAccelerationStructuresPropertiesCommand");
        HandleToJson(jdata["device_id"], device_id);
        jdata["queryType"] = query_type;
        HandleToJson(jdata["acceleration_structure_id"], acceleration_structure_id);
        WriteBlockEnd();
    }

    virtual void ProcessResourceMemoryRequirements(
        const format::arm::ResourceMemoryRequirementsCommandHeader&     command_header,
        const std::vector<format::arm::ResourceMemoryRequirementsInfo>& resources) override
    {
        auto& jdata        = WriteMetaCommandStart("ResourceMemoryRequirements");
        using Property     = format::arm::ResourceMemoryRequirementsProperties;
        using ResourceType = format::arm::ResourceMemoryRequirementsPropertiesResourceType;
        HandleToJson(jdata["device_id"], command_header.device_id);
        jdata["resources_count"] = command_header.resources_count;

        const size_t resource_count = resources.size();
        for (size_t i = 0; i < resource_count; ++i)
        {
            const auto& resource  = resources[i];
            auto&       jresource = jdata["resources"][i];

            auto property_iter = resource.find(Property::kResourceType);
            if (property_iter != resource.end())
            {
                if (const auto* value = std::any_cast<ResourceType>(&(property_iter->second)))
                {
                    jresource["resource_type"] = static_cast<uint32_t>(*value);
                }
            }

            property_iter = resource.find(Property::kResourceHandle);
            if (property_iter != resource.end())
            {
                if (const auto* value = std::any_cast<format::HandleId>(&(property_iter->second)))
                {
                    HandleToJson(jresource["resource_handle"], *value);
                }
            }

            property_iter = resource.find(Property::kAliasingGroup);
            if (property_iter != resource.end())
            {
                if (const auto* value = std::any_cast<uint8_t>(&(property_iter->second)))
                {
                    jresource["aliasing_group"] = *value;
                }
            }

            property_iter = resource.find(Property::kCreateInfo);
            if (property_iter != resource.end())
            {
                if (const auto* value = std::any_cast<Decoded_VkBufferCreateInfo>(&(property_iter->second)))
                {
                    FieldToJson(jresource["create_info"], value);
                }
                else if (const auto* value = std::any_cast<Decoded_VkImageCreateInfo>(&(property_iter->second)))
                {
                    FieldToJson(jresource["create_info"], value);
                }
                else if (const auto* value = std::any_cast<Decoded_VkTensorCreateInfoARM>(&(property_iter->second)))
                {
                    FieldToJson(jresource["create_info"], value);
                }
            }
        }
        WriteBlockEnd();
    }

    virtual void
    ProcessDx12ResourceAliasingCommand(const format::arm::Dx12ResourceAliasingCommandHeader& command_header,
                                       const uint8_t*                                        data) override
    {
        auto& jdata = WriteMetaCommandStart("Dx12ResourceAliasingCommand");
        HandleToJson(jdata["thread_id"], command_header.thread_id);
        jdata["resources_count"] = command_header.resources_count;

        const auto resource_count = static_cast<size_t>(command_header.resources_count);
        auto*      aliasing_infos = reinterpret_cast<const format::arm::Dx12ResourceAliasingInfo*>(data);
        for (size_t i = 0; i < resource_count; ++i)
        {
            auto& jresource = jdata["resources"][i];
            HandleToJson(jresource["resource_id"], aliasing_infos[i].resource_id);
            HandleToJson(jresource["heap_id"], aliasing_infos[i].heap_id);
            jresource["heap_offset"] = aliasing_infos[i].heap_offset;
        }
        WriteBlockEnd();
    }

    /// @}
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_METADATA_JSON_CONSUMER_H
