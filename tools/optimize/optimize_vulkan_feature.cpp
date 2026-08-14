/*
** Copyright (c) 2020-2026 LunarG, Inc.
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

#include "optimize_vulkan_feature.h"

#include "file_optimizer.h"
#include "replay_options_editor.h"
#include "vulkan_file_optimizer.h"
#include "vulkan_micromap_modifier.h"
#include "generated/generated_vulkan_skiavk_modifier.h"
#include "vulkan_raytracing_modifier.h"
#include "vulkan_descriptor_buffer_modifier.h"
#include "resource_memory_requirements_modifier.h"
#include "vulkan_shader_replacement_modifier.h"
#include "vulkan_arm_trace_helpers_modifier.h"

#include "decode/file_processor.h"
#include "decode/vulkan_feature_tracker_consumer_base.h"
#include "generated/generated_vulkan_referenced_block_consumer.h"
#include "generated/generated_vulkan_referenced_resource_consumer.h"
#include "util/api_version_info.h"
#include "util/feature_module_registry.h"
#include "util/logging.h"

#include <cinttypes>
#include <memory>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(optimize)

GFXR_UTIL_REGISTER_FEATURE_CREATOR(OptimizeFeature, OptimizeVulkanFeature)

// Vulkan-specific CLI flag names, private to this translation unit.
constexpr char kRemoveThreadArgument[]               = "--remove-thread";
constexpr char kVulkanSetReplayOptionsArgument[]     = "--set-replay-options";
constexpr char kVulkanRemoveDeviceInstanceArgument[] = "--remove-device-instance";
constexpr char kVulkanKeepDeviceInstanceArgument[]   = "--keep-device-instance";
constexpr char kVulkanRemoveRtOption[]               = "--vk-remove-rt";
constexpr char kVulkanReplaceShadersArgument[]       = "--replace-shaders";

std::string OptimizeVulkanFeature::CompiledHeaderVersionString() const
{
#if defined(GFXRECON_ENABLE_VULKAN)
    return util::GetVulkanHeaderVersionString();
#else
    return "";
#endif
}

void OptimizeVulkanFeature::RegisterDetectionDecoder(decode::FileProcessor& file_processor, uint64_t block_limit)
{
    // Destroy decoder before consumer: decoder holds a raw pointer to consumer.
    detection_decoder_.reset();
    detection_consumer_ = std::make_unique<decode::VulkanDetectionConsumer>(block_limit);
    detection_decoder_  = std::make_unique<decode::VulkanDecoder>();
    detection_decoder_->AddConsumer(detection_consumer_.get());
    file_processor.AddDecoder(detection_decoder_.get());
}

bool OptimizeVulkanFeature::WasDetected() const
{
    return detection_consumer_ != nullptr && detection_consumer_->WasVulkanAPIDetected();
}

bool OptimizeVulkanFeature::ShouldRun(const util::ArgumentParser& args) const
{
    bool manual_mode = args.IsArgumentSet(kRemoveThreadArgument) ||
                       args.IsArgumentSet(kVulkanSetReplayOptionsArgument) ||
                       args.IsArgumentSet(kVulkanRemoveDeviceInstanceArgument) ||
                       args.IsArgumentSet(kVulkanKeepDeviceInstanceArgument) ||
                       args.IsOptionSet(kVulkanRemoveRtOption) || args.IsArgumentSet(kVulkanReplaceShadersArgument);
    return manual_mode || WasDetected();
}

std::string OptimizeVulkanFeature::GetOptions() const
{
    return "--vk-remove-rt";
}

std::string OptimizeVulkanFeature::GetArguments() const
{
    return "--set-replay-options,--remove-device-instance,--keep-device-instance,--replace-shaders";
}

std::string OptimizeVulkanFeature::GetSynopsisFragment() const
{
    return "[--set-replay-options <options>] [--remove-device-instance <names>] [--keep-device-instance <names>] "
           "[--vk-remove-rt] [--replace-shaders <dir>]";
}

void OptimizeVulkanFeature::PrintUsage() const
{
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE(" // Vulkan-only options:");
    GFXRECON_WRITE_CONSOLE(" // -------------------");
    GFXRECON_WRITE_CONSOLE("  --set-replay-options <options>");
    GFXRECON_WRITE_CONSOLE("      Add default playback options to the trace. Use quotation marks for multiple");
    GFXRECON_WRITE_CONSOLE("      arguments. Do NOT combine this option with any other option.");
    GFXRECON_WRITE_CONSOLE("  --remove-device-instance <names>");
    GFXRECON_WRITE_CONSOLE("      Remove redundant instance/device and corresponding APIs. Use comma marks for");
    GFXRECON_WRITE_CONSOLE("      multiple arguments. the default value is \"android framework\".");
    GFXRECON_WRITE_CONSOLE("  --keep-device-instance <names>");
    GFXRECON_WRITE_CONSOLE("      Keep only the specified instance/device and corresponding APIs. Use comma marks for");
    GFXRECON_WRITE_CONSOLE("      multiple arguments.");
    GFXRECON_WRITE_CONSOLE("  --vk-remove-rt");
    GFXRECON_WRITE_CONSOLE("      Remove ray-tracing related API calls from the trace.");
    GFXRECON_WRITE_CONSOLE("  --replace-shaders <dir>");
    GFXRECON_WRITE_CONSOLE("      Replace the shader code in each `VkShaderModuleCreateInfo`");
    GFXRECON_WRITE_CONSOLE("      with the content of the matching file in <dir> if found.");
    GFXRECON_WRITE_CONSOLE("      See gfxrecon-extract.");
}

decode::VulkanOptimizationOptions OptimizeVulkanFeature::BuildOptions(const util::ArgumentParser& args) const
{
    if (args.IsArgumentSet(kVulkanSetReplayOptionsArgument))
    {
        if (args.GetOptionCount() != 0 || args.GetArgumentCount() != 1)
        {
            throw std::runtime_error("Option --set-replay-options cannot be used with any other option. Exiting.");
        }
    }

    if (args.IsArgumentSet(kVulkanRemoveDeviceInstanceArgument) &&
        args.IsArgumentSet(kVulkanKeepDeviceInstanceArgument))
    {
        throw std::runtime_error(
            "Options --remove-device-instance and --keep-device-instance cannot be used together.");
    }

    decode::VulkanOptimizationOptions options;
    options.remove_rt              = args.IsOptionSet(kVulkanRemoveRtOption);
    options.remove_device_instance = args.IsArgumentSet(kVulkanRemoveDeviceInstanceArgument);
    options.keep_device_instance   = args.IsArgumentSet(kVulkanKeepDeviceInstanceArgument);
    options.replace_shader_dir     = args.GetArgumentValue(kVulkanReplaceShadersArgument);

    std::string remove_app_string;
    if (options.remove_device_instance)
    {
        remove_app_string = args.GetArgumentValue(kVulkanRemoveDeviceInstanceArgument);
    }
    else if (options.keep_device_instance)
    {
        remove_app_string = args.GetArgumentValue(kVulkanKeepDeviceInstanceArgument);
    }
    else
    {
        remove_app_string = "android framework";
    }
    options.remove_app_name = args.SplitStringByFlag(remove_app_string, ',');

    if (args.IsArgumentSet(kRemoveThreadArgument))
    {
        std::string remove_thread_string = args.GetArgumentValue(kRemoveThreadArgument);
        for (const std::string& thread_string : args.SplitStringByFlag(remove_thread_string, ','))
        {
            options.removed_threads_ids.insert(std::stoi(thread_string));
        }
    }

    return options;
}

bool OptimizeVulkanFeature::GetUnreferencedResources(const std::string&                    input_filename,
                                                     std::unordered_set<format::HandleId>& unreferenced_ids)
{
    decode::FileProcessor file_processor;
    if (!file_processor.Initialize(input_filename))
    {
        return false;
    }

    decode::VulkanDecoder                    decoder;
    decode::VulkanReferencedResourceConsumer resref_consumer;
    decoder.AddConsumer(&resref_consumer);
    file_processor.AddDecoder(&decoder);
    file_processor.ProcessAllFrames();

    if (file_processor.GetCurrentFrameNumber() == 0)
    {
        GFXRECON_WRITE_CONSOLE("File did not contain any frames");
        return false;
    }

    if (file_processor.GetErrorState() != decode::BlockIOError::kErrorNone)
    {
        GFXRECON_WRITE_CONSOLE("A failure has occurred during file processing");
        return false;
    }

    resref_consumer.GetReferencedHandleIds(nullptr, &unreferenced_ids);
    return true;
}

bool OptimizeVulkanFeature::FilterUnreferencedResources(const std::string&                          input_filename,
                                                        const std::string&                          output_filename,
                                                        const std::unordered_set<format::HandleId>& unreferenced_ids)
{
    // Collect the block indices that correspond to unreferenced resources.
    decode::FileProcessor file_processor;
    if (!file_processor.Initialize(input_filename))
    {
        return false;
    }

    decode::VulkanDecoder                 decoder;
    decode::VulkanReferencedBlockConsumer block_ref_consumer(unreferenced_ids);
    decoder.AddConsumer(&block_ref_consumer);
    file_processor.AddDecoder(&decoder);
    file_processor.ProcessAllFrames();

    if (file_processor.GetErrorState() != decode::BlockIOError::kErrorNone)
    {
        GFXRECON_WRITE_CONSOLE("A failure has occurred during file processing");
        return false;
    }

    uint64_t                     num_blocks          = file_processor.GetCurrentBlockIndex();
    std::unordered_set<uint64_t> unreferenced_blocks = block_ref_consumer.GetUnreferencedBlocks();

    // Stream the input to the output, dropping unreferenced blocks.
    FileOptimizer file_optimizer(unreferenced_ids, unreferenced_blocks, {});
    if (!file_optimizer.Initialize(input_filename, output_filename))
    {
        return false;
    }

    file_optimizer.Process();

    if (file_optimizer.GetErrorState() != decode::BlockIOError::kErrorNone)
    {
        GFXRECON_WRITE_CONSOLE("A failure has occurred during file processing");
        return false;
    }

    GFXRECON_WRITE_CONSOLE(
        "Resource filtering complete - Removed %zu / %" PRIu64 " blocks", unreferenced_blocks.size(), num_blocks);
    GFXRECON_WRITE_CONSOLE("\tOriginal file size: %" PRIu64 " bytes", file_optimizer.GetNumBytesRead());
    GFXRECON_WRITE_CONSOLE("\tOptimized file size: %" PRIu64 " bytes", file_optimizer.GetNumBytesWritten());
    return true;
}

bool OptimizeVulkanFeature::Optimize(const std::string&          input_filename,
                                     const std::string&          output_filename,
                                     const util::ArgumentParser& args)
{
    decode::VulkanOptimizationOptions options = BuildOptions(args);

    if (args.IsArgumentSet(kVulkanSetReplayOptionsArgument))
    {
        const auto& replay_options = args.GetArgumentValue(kVulkanSetReplayOptionsArgument);

        gfxrecon::ReplayOptionsEditor file_transformer;
        if (file_transformer.Initialize(input_filename, output_filename, "replay_options"))
        {
            file_transformer.SetReplayOptions(replay_options);
            file_transformer.Process();

            if (file_transformer.GetErrorState() != gfxrecon::decode::kErrorNone)
            {
                GFXRECON_LOG_FATAL("A failure has occurred during file processing");
            }
            else
            {
                GFXRECON_WRITE_CONSOLE((std::string("Replay options added: ") + replay_options).c_str());
            }
        }

        return true;
    }

    /* ========== LUNARG ONLY ==========

    GFXRECON_WRITE_CONSOLE("Scanning Vulkan file %s for unreferenced resources.", input_filename.c_str());

    std::unordered_set<format::HandleId> unreferenced_ids;
    if (!GetUnreferencedResources(input_filename, unreferenced_ids))
    {
        return false;
    }

    if (unreferenced_ids.empty())
    {
        GFXRECON_WRITE_CONSOLE("No unused resources detected. A new file will not be created.");
        return true;
    }

    GFXRECON_WRITE_CONSOLE("Writing optimized file, removing initialization data for %" PRIu64 " unused resources.",
                           unreferenced_ids.size());
    return FilterUnreferencedResources(input_filename, output_filename, unreferenced_ids);

    ========== LUNARG ONLY ========== */

    GFXRECON_WRITE_CONSOLE("Scanning vulkan trace %s for optimizations...", input_filename.c_str());

    // First (and maybe second) pass - get the optimization data

    auto vulkan_opt_data = std::make_unique<gfxrecon::VulkanFileOptimizer::VulkanOptimizationData>();

    gfxrecon::decode::FileProcessor file_processor;
    if (file_processor.Initialize(input_filename))
    {
        gfxrecon::decode::VulkanDecoder decoder;
        // gfxrecon::decode::VulkanReferencedResourceConsumer resref_consumer;
        auto feature_tracker_consumer      = std::make_unique<gfxrecon::decode::VulkanFeatureTrackerConsumerBase>();
        auto micromap_modifier_consumer    = std::make_unique<gfxrecon::decode::VulkanMicromapModifier>();
        auto vulkan_skia_modifier_consumer = std::make_unique<gfxrecon::decode::VulkanSkiaModifier>();
        auto raytracing_modifier_consumer  = std::make_unique<gfxrecon::decode::VulkanRayTracingModifier>(options);
        auto resource_memory_requirements_modifier =
            std::make_unique<gfxrecon::decode::ResourceMemoryRequirementsModifier>();
        auto descriptor_buffer_modifier_consumer =
            std::make_unique<gfxrecon::decode::VulkanDescriptorBufferModifier>(options);
        auto trace_helpers_modifier_consumer = std::make_unique<gfxrecon::decode::VulkanArmTraceHelpersModifier>();

        auto shader_replacement_modifier_consumer =
            std::make_unique<gfxrecon::decode::VulkanShaderReplacementModifier>(options.replace_shader_dir);

        // decoder.AddConsumer(&resref_consumer);
        decoder.AddConsumer(feature_tracker_consumer.get());
        decoder.AddConsumer(micromap_modifier_consumer.get());
        decoder.AddConsumer(vulkan_skia_modifier_consumer.get());
        decoder.AddConsumer(descriptor_buffer_modifier_consumer.get());
        decoder.AddConsumer(raytracing_modifier_consumer.get());
        decoder.AddConsumer(resource_memory_requirements_modifier.get());
        if (!options.replace_shader_dir.empty())
        {
            decoder.AddConsumer(shader_replacement_modifier_consumer.get());
        }
        decoder.AddConsumer(trace_helpers_modifier_consumer.get());

        vulkan_skia_modifier_consumer.get()->SetAppName(options.remove_app_name);
        vulkan_skia_modifier_consumer.get()->SetKeepDeviceInstanceMode(options.keep_device_instance);
        file_processor.AddDecoder(&decoder);
        file_processor.ProcessAllFrames();

        if (file_processor.GetErrorState() != gfxrecon::decode::kErrorNone)
        {
            throw std::runtime_error("Failed to scan input file for optimizations");
        }

        // resref_consumer.GetReferencedHandleIds(nullptr, &vulkan_opt_data->unreferenced_ids);

        if (feature_tracker_consumer->CanOptimize())
        {
            vulkan_opt_data->modifiers.push_back(std::move(feature_tracker_consumer));
        }
        if (micromap_modifier_consumer->CanOptimize())
        {
            vulkan_opt_data->modifiers.push_back(std::move(micromap_modifier_consumer));
        }
        if (vulkan_skia_modifier_consumer->CanOptimize())
        {
            vulkan_opt_data->modifiers.push_back(std::move(vulkan_skia_modifier_consumer));
        }
        if (descriptor_buffer_modifier_consumer->CanOptimize())
        {
            vulkan_opt_data->modifiers.push_back(std::move(descriptor_buffer_modifier_consumer));
        }
        if (raytracing_modifier_consumer->CanOptimize())
        {
            vulkan_opt_data->modifiers.push_back(std::move(raytracing_modifier_consumer));
        }
        if (resource_memory_requirements_modifier->CanOptimize())
        {
            vulkan_opt_data->modifiers.push_back(std::move(resource_memory_requirements_modifier));
        }
        if (shader_replacement_modifier_consumer->CanOptimize())
        {
            vulkan_opt_data->modifiers.push_back(std::move(shader_replacement_modifier_consumer));
        }
        if (trace_helpers_modifier_consumer->CanOptimize())
        {
            vulkan_opt_data->modifiers.push_back(std::move(trace_helpers_modifier_consumer));
        }
    }

    // if (!vulkan_opt_data->unreferenced_ids.empty())
    // {
    //     auto block_result           = GetUnreferencedBlocks(input_filename, result->unreferenced_ids);
    //     vulkan_opt_data->unreferenced_blocks = std::move(block_result.unreferenced_blocks);
    // }

    // Check if any optimization can be done
    const bool can_remove_unused_resources = !vulkan_opt_data->unreferenced_ids.empty();

    // Early exit if no optimization can be done
    if (vulkan_opt_data->modifiers.empty() && !can_remove_unused_resources)
    {
        GFXRECON_WRITE_CONSOLE("Nothing to optimize. Exiting.");
        return true;
    }

    // Modification pass. Implement all identified optimizations in output file
    gfxrecon::VulkanFileOptimizer file_optimizer(vulkan_opt_data.get(), options.removed_threads_ids);
    if (file_optimizer.Initialize(input_filename, output_filename, "optimize"))
    {
        file_optimizer.Process();

        if (file_optimizer.GetErrorState() != gfxrecon::decode::kErrorNone &&
            file_optimizer.GetErrorState() != gfxrecon::decode::FileTransformer::Error::kErrorReadingBlockHeader)
        {
            throw std::runtime_error("A failure has occurred during file processing");
        }

        GFXRECON_WRITE_CONSOLE("Resource filtering complete - Removed %d blocks", file_optimizer.GetNumRemovedBlocks());
        GFXRECON_WRITE_CONSOLE("Vulkan optimizations complete.");
        GFXRECON_WRITE_CONSOLE("\tOriginal file size: %" PRIu64 " bytes", file_optimizer.GetNumBytesRead());
        GFXRECON_WRITE_CONSOLE("\tOptimized file size: %" PRIu64 " bytes", file_optimizer.GetNumBytesWritten());
    }

    return true;
}

GFXRECON_END_NAMESPACE(optimize)
GFXRECON_END_NAMESPACE(gfxrecon)
