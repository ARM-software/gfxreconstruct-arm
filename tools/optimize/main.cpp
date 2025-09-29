/*
** Copyright (c) 2020 LunarG, Inc.
** Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
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

#include PROJECT_VERSION_HEADER_FILE
#include "file_optimizer.h"
#include "replay_options_editor.h"
#include "vulkan_file_optimizer.h"
#include "vulkan_micromap_modifier.h"
#include "generated/generated_vulkan_skiavk_modifier.h"
#include "vulkan_raytracing_modifier.h"
#include "vulkan_descriptor_buffer_modifier.h"

#include "../tool_settings.h"

#if defined(D3D12_SUPPORT)
#include "dx12_optimize_util.h"
#endif

#include "decode/decode_api_detection.h"
#include "decode/dx12_optimize_options.h"
#include "vulkan_optimize_options.h"
#include "decode/file_processor.h"
#include "format/format.h"
#include "format/format_util.h"
#include "generated/generated_vulkan_decoder.h"
#include "generated/generated_vulkan_referenced_resource_consumer.h"
#include "decode/vulkan_feature_tracker_consumer_base.h"
#include "util/argument_parser.h"
#include "util/logging.h"
#include "util/date_time.h"

// TODO add vulkan_raytracing_optimizer
// #include "vulkan_raytracing_optimizer.h"

#include "vulkan/vulkan.h"

#include <filesystem>
#include <cassert>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#if defined(WIN32)
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = 616;
}
extern "C"
{
    __declspec(dllexport) extern const char* D3D12SDKPath = reinterpret_cast<const char*>(u8".\\D3D12\\");
}
#endif

const char kOptions[] =
    "-h|--help,--version,--no-debug-popup,--d3d12-pso-removal,--dxr,--dxr-offline,--dxr-experimental,--vk-remove-rt";
const char kArguments[] = "--gpu,--set-replay-options,--set-replay-options,--remove-device-instance,--remove-thread";

const char kD3d12PsoRemoval[]             = "--d3d12-pso-removal";
const char kDx12OptimizeDxr[]             = "--dxr";
const char kDx12OptimizeDxrExperimental[] = "--dxr-experimental";
const char kDx12OptimizeDxrOffline[]      = "--dxr-offline";
const char kReplayOptions[]               = "--set-replay-options";
const char kVulkanDevInsRemoval[]         = "--remove-device-instance";
const char kThreadRemoval[]               = "--remove-thread";

std::vector<std::string>                       remove_app_name;
std::unordered_set<gfxrecon::format::ThreadId> removed_threads_ids;
const char                                     kVulkanRTRemoval[] = "--vk-remove-rt";

static void PrintUsage(const char* exe_name)
{
    std::string app_name     = exe_name;
    size_t      dir_location = app_name.find_last_of("/\\");
    if (dir_location >= 0)
    {
        app_name.replace(0, dir_location + 1, "");
    }
    GFXRECON_WRITE_CONSOLE("\n%s - Produce new captures with enhanced performance characteristics", app_name.c_str());

    GFXRECON_WRITE_CONSOLE("\t\t\tFor Vulkan, the optimizer will remove unused buffer and image initialization data "
                           "(for trimmed captures)");
    GFXRECON_WRITE_CONSOLE(
        "\t\t\tFor D3D12, the optimizer will improve DXR replay performance and remove unused PSOs (for all captures)");
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("Usage:");
    GFXRECON_WRITE_CONSOLE(
        "  %s [-h | --help] [--version] [--d3d12-pso-removal] [--dxr] [--dxr-offline] [--gpu <index>] "
        "[--set-replay-options] [--remove-device-instance] "
        "<input-file> <output-file>",
        app_name.c_str());
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("Required arguments:");
    GFXRECON_WRITE_CONSOLE("  <input-file>\t\tThe path to input GFXReconstruct capture file to be processed.");
    GFXRECON_WRITE_CONSOLE("  <output-file>\t\tThe path to output GFXReconstruct capture file to be created.");
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("Optional arguments:");
    GFXRECON_WRITE_CONSOLE(
        "  --set-replay-options <options>\t\tAdd default playback options to the trace. Use quotation marks "
        "for multiple arguments. Do NOT combine this option with any other option.");
    GFXRECON_WRITE_CONSOLE(
        "  --remove-device-instance <options>\t\tRemove redundant instance/device and corresponding APIs. Use "
        "comma marks for multiple arguments. the default value is \"android framework\".");
    GFXRECON_WRITE_CONSOLE("  --vk-remove-rt\t\tRemove ray-tracing related API calls from the trace");
    GFXRECON_WRITE_CONSOLE("  --remove-thread <threads>\t\tRemove the specified threads from the trace.");
    GFXRECON_WRITE_CONSOLE("  -h\t\t\tPrint usage information and exit (same as --help).");
    GFXRECON_WRITE_CONSOLE("  --version\t\tPrint version information and exit.");
#if defined(WIN32)
#if defined(_DEBUG)
    GFXRECON_WRITE_CONSOLE("  --no-debug-popup\tDisable the 'Abort, Retry, Ignore' message box");
    GFXRECON_WRITE_CONSOLE("        \t\tdisplayed when abort() is called (Windows debug only).");
#endif
    GFXRECON_WRITE_CONSOLE("  --d3d12-pso-removal\tD3D12-only: Remove creation of unreferenced PSOs.");
    GFXRECON_WRITE_CONSOLE("  --dxr\t\t\tD3D12-only: Optimize for DXR and ExecuteIndirect replay.");
    GFXRECON_WRITE_CONSOLE("  --dxr-offline\t\t\tD3D12-only: Optimize for ray tracing with offline.");
    GFXRECON_WRITE_CONSOLE("  --gpu <index>\t\tUse the specified device for the optimizer replay, where index");
    GFXRECON_WRITE_CONSOLE("          \t\tis the zero-based index to the array of physical devices");
    GFXRECON_WRITE_CONSOLE("          \t\treturned by vkEnumeratePhysicalDevices or IDXGIFactory1::EnumAdapters1.");
    GFXRECON_WRITE_CONSOLE(
        "          \t\tThe optimizer replay may fail if the specified device is not compatible with the");
    GFXRECON_WRITE_CONSOLE("          \t\toriginal capture devices.");
    GFXRECON_WRITE_CONSOLE("");
    GFXRECON_WRITE_CONSOLE("Note: running without optional arguments will instruct the optimizer to detect API and run "
                           "all available optimizations.");
#endif
}

void RunDx12Optimizations(const std::string&                        input_filename,
                          const std::string&                        output_filename,
                          gfxrecon::decode::Dx12OptimizationOptions dx12_options)
{
#if defined(D3D12_SUPPORT)
    bool result = gfxrecon::Dx12OptimizeFile(input_filename, output_filename, dx12_options);
    if (!result)
    {
        gfxrecon::util::Log::Release();
        exit(-1);
    }
#endif
}

std::unique_ptr<gfxrecon::VulkanFileOptimizer::VulkanOptimizationData>
GetVulkanOptimizationData(const std::string& input_filename, const gfxrecon::VulkanOptimizationOptions& options)
{
    auto result = std::make_unique<gfxrecon::VulkanFileOptimizer::VulkanOptimizationData>();

    gfxrecon::decode::FileProcessor file_processor;
    if (file_processor.Initialize(input_filename))
    {
        gfxrecon::decode::VulkanDecoder                    decoder;
        gfxrecon::decode::VulkanReferencedResourceConsumer resref_consumer;
        auto feature_tracker_consumer      = std::make_unique<gfxrecon::decode::VulkanFeatureTrackerConsumerBase>();
        auto micromap_modifier_consumer    = std::make_unique<gfxrecon::decode::VulkanMicromapModifier>();
        auto vulkan_skia_modifier_consumer = std::make_unique<gfxrecon::decode::VulkanSkiaModifier>();
        auto raytracing_modifier_consumer  = std::make_unique<gfxrecon::decode::VulkanRayTracingModifier>(options);
        auto descriptor_buffer_modifier_consumer =
            std::make_unique<gfxrecon::decode::VulkanDescriptorBufferModifier>(options);

        decoder.AddConsumer(&resref_consumer);
        decoder.AddConsumer(feature_tracker_consumer.get());
        decoder.AddConsumer(micromap_modifier_consumer.get());
        decoder.AddConsumer(vulkan_skia_modifier_consumer.get());
        decoder.AddConsumer(descriptor_buffer_modifier_consumer.get());
        decoder.AddConsumer(raytracing_modifier_consumer.get());

        vulkan_skia_modifier_consumer.get()->SetAppName(remove_app_name);
        file_processor.AddDecoder(&decoder);
        file_processor.ProcessAllFrames();

        if (file_processor.GetErrorState() != gfxrecon::decode::FileProcessor::kErrorNone)
        {
            throw std::runtime_error("Failed to scan input file for optimizations");
        }

        resref_consumer.GetReferencedResourceIds(nullptr, &result->unreferenced_ids);

        if (feature_tracker_consumer->CanOptimize())
        {
            result->modifiers.push_back(std::move(feature_tracker_consumer));
        }
        if (micromap_modifier_consumer->CanOptimize())
        {
            result->modifiers.push_back(std::move(micromap_modifier_consumer));
        }
        if (vulkan_skia_modifier_consumer->CanOptimize())
        {
            result->modifiers.push_back(std::move(vulkan_skia_modifier_consumer));
        }
        if (descriptor_buffer_modifier_consumer->CanOptimize())
        {
            result->modifiers.push_back(std::move(descriptor_buffer_modifier_consumer));
        }
        if (raytracing_modifier_consumer->CanOptimize())
        {
            result->modifiers.push_back(std::move(raytracing_modifier_consumer));
        }
    }
    return result;
}

void RunVulkanOptimizations(const std::string&                         input_filename,
                            const std::string&                         output_filename,
                            const gfxrecon::VulkanOptimizationOptions& options)
{
    GFXRECON_WRITE_CONSOLE("Scanning vulkan trace %s for optimizations...", input_filename.c_str());

    // First pass - get the optimization data
    auto vulkan_opt_data = GetVulkanOptimizationData(input_filename, options);

    // Check if any optimization can be done
    const bool can_remove_unused_resources = !vulkan_opt_data->unreferenced_ids.empty();

    // Early exit if no optimization can be done
    if (vulkan_opt_data->modifiers.empty() && !can_remove_unused_resources)
    {
        GFXRECON_WRITE_CONSOLE("Nothing to optimize. Exiting.");
        return;
    }

    // Modification pass. Implement all identified optimizations in output file
    gfxrecon::VulkanFileOptimizer file_optimizer(vulkan_opt_data.get());
    if (file_optimizer.Initialize(input_filename, output_filename, "optimize"))
    {
        file_optimizer.SetRemovedThreads(removed_threads_ids);

        file_optimizer.Process();

        if (file_optimizer.GetErrorState() != gfxrecon::FileOptimizer::kErrorNone &&
            file_optimizer.GetErrorState() != gfxrecon::decode::FileTransformer::Error::kErrorReadingBlockHeader)
        {
            throw std::runtime_error("A failure has occurred during file processing");
        }

        GFXRECON_WRITE_CONSOLE("Vulkan optimizations complete.");
        GFXRECON_WRITE_CONSOLE("\tOriginal file size: %" PRIu64 " bytes", file_optimizer.GetNumBytesRead());
        GFXRECON_WRITE_CONSOLE("\tOptimized file size: %" PRIu64 " bytes", file_optimizer.GetNumBytesWritten());
    }
}

void SetReplayOptions(std::string input_filename, std::string output_filename, std::string replay_options)
{
    gfxrecon::ReplayOptionsEditor file_transformer;
    if (file_transformer.Initialize(input_filename, output_filename, "replay_options"))
    {
        file_transformer.SetReplayOptions(replay_options);
        file_transformer.Process();

        if (file_transformer.GetErrorState() != gfxrecon::FileOptimizer::kErrorNone)
        {
            GFXRECON_WRITE_CONSOLE("A failure has occurred during file processing");
            gfxrecon::util::Log::Release();
            exit(-1);
        }

        GFXRECON_WRITE_CONSOLE((std::string("Replay options added: ") + replay_options).c_str());
    }
}

int main(int argc, const char** argv)
{
    int64_t start_time = gfxrecon::util::datetime::GetTimestamp();

    gfxrecon::util::Log::Init();

    gfxrecon::util::ArgumentParser arg_parser(argc, argv, kOptions, kArguments);

    if (CheckOptionPrintUsage(argv[0], arg_parser) || CheckOptionPrintVersion(argv[0], arg_parser))
    {
        gfxrecon::util::Log::Release();
        exit(0);
    }
    else if (arg_parser.IsInvalid() || (arg_parser.GetPositionalArgumentsCount() != 2))
    {
        PrintUsage(argv[0]);
        gfxrecon::util::Log::Release();
        exit(-1);
    }
    else
    {
#if defined(WIN32) && defined(_DEBUG)
        if (arg_parser.IsOptionSet(kNoDebugPopup))
        {
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        }
#endif
    }

    try
    {
        std::string                     input_filename;
        std::string                     output_filename;
        std::string                     remove_app_string;
        const std::vector<std::string>& positional_arguments = arg_parser.GetPositionalArguments();
        input_filename                                       = positional_arguments[0];
        output_filename                                      = positional_arguments[1];

        const bool set_replay_options     = arg_parser.IsArgumentSet(kReplayOptions);
        const bool remove_device_instance = arg_parser.IsArgumentSet(kVulkanDevInsRemoval);
        const bool remove_thread          = arg_parser.IsArgumentSet(kThreadRemoval);

        // Parameter checking and API detection
        gfxrecon::decode::Dx12OptimizationOptions dx12_options;
        dx12_options.optimize_resource_values              = arg_parser.IsOptionSet(kDx12OptimizeDxr);
        dx12_options.optimize_resource_values_experimental = arg_parser.IsOptionSet(kDx12OptimizeDxrExperimental);
        dx12_options.optimize_resource_values_offline      = arg_parser.IsOptionSet(kDx12OptimizeDxrOffline);
        dx12_options.remove_redundant_psos                 = arg_parser.IsOptionSet(kD3d12PsoRemoval);
        const auto& override_gpu                           = arg_parser.GetArgumentValue(kOverrideGpuArgument);

        gfxrecon::VulkanOptimizationOptions vulkan_options{};
        vulkan_options.remove_rt = arg_parser.IsOptionSet(kVulkanRTRemoval);

        // Quick validation
        if (set_replay_options)
        {
            if (dx12_options.optimize_resource_values || dx12_options.optimize_resource_values_experimental ||
                dx12_options.remove_redundant_psos || dx12_options.optimize_resource_values_offline ||
                !override_gpu.empty())
            {
                throw std::runtime_error("Option --set-replay-options cannot be used with any other option. Exiting.");
            }
        }

        if (remove_device_instance)
        {
            remove_app_string = arg_parser.GetArgumentValue(kVulkanDevInsRemoval);
        }
        else
        {
            remove_app_string = "android framework";
        }
        remove_app_name = arg_parser.SplitStringByFlag(remove_app_string, ',');

        if (!override_gpu.empty())
        {
            dx12_options.override_gpu_index = std::stoi(override_gpu);
        }

        if (dx12_options.optimize_resource_values_experimental)
        {
            GFXRECON_WRITE_CONSOLE("Running experimental DXR optimization. This mode is experimental, and should only "
                                   "be used if --dxr did not produce a valid capture file.");
            dx12_options.optimize_resource_values = true;
        }

        if (remove_thread)
        {
            std::string remove_thread_string = arg_parser.GetArgumentValue(kThreadRemoval);
            for (const std::string& thread_string : arg_parser.SplitStringByFlag(remove_thread_string, ','))
            {
                removed_threads_ids.insert(std::stoi(thread_string));
            }
        }

        // Setting default replay options only, skip all other optimizations
        if (set_replay_options)
        {
            const auto& replay_options = arg_parser.GetArgumentValue(kReplayOptions);
            SetReplayOptions(input_filename, output_filename, replay_options);
        }
        // Perform user selected DX12 optimizations
        else if (dx12_options.optimize_resource_values || dx12_options.optimize_resource_values_offline ||
                 dx12_options.remove_redundant_psos || !override_gpu.empty())
        {
            RunDx12Optimizations(input_filename, output_filename, dx12_options);
        }
        else if (vulkan_options.remove_rt)
        {
            RunVulkanOptimizations(input_filename, output_filename, vulkan_options);
        }
        // Automatic mode - user specified no options, detect api and perform default optimizations
        else
        {
            bool detected_d3d12  = false;
            bool detected_vulkan = false;
            bool detected_openxr = false;
            gfxrecon::decode::DetectAPIs(input_filename, detected_d3d12, detected_vulkan, detected_openxr);

            if ((!detected_d3d12) && (!detected_vulkan))
            {
                // Detect with no block limit
                gfxrecon::decode::DetectAPIs(input_filename, detected_d3d12, detected_vulkan, detected_openxr, true);
            }

            if (detected_d3d12)
            {
                dx12_options.optimize_resource_values         = true;
                dx12_options.remove_redundant_psos            = true;
                dx12_options.optimize_resource_values_offline = true;
                RunDx12Optimizations(input_filename, output_filename, dx12_options);
            }
            else if (detected_vulkan)
            {
                // Run all vulkan optimizations
                RunVulkanOptimizations(input_filename, output_filename, vulkan_options);
            }
#if ENABLE_OPENXR_SUPPORT
            else if (detected_openxr)
            {
                GFXRECON_LOG_INFO("No optimizations defined for OpenXR capture files");
            }
#endif
            else
            {
                GFXRECON_LOG_ERROR("Could not detect graphics API. Aborting optimization.")
            }
        }
    }
    catch (const std::runtime_error& error)
    {
        GFXRECON_WRITE_CONSOLE("File processing has encountered a fatal error and cannot continue: %s", error.what());
        gfxrecon::util::Log::Release();
        return -1;
    }
    catch (...)
    {
        GFXRECON_WRITE_CONSOLE("File processing failed due to an unhandled exception");
        gfxrecon::util::Log::Release();
        return -1;
    }

    int64_t end_time        = gfxrecon::util::datetime::GetTimestamp();
    int     time_in_seconds = static_cast<int>(gfxrecon::util::datetime::ConvertTimestampToSeconds(
        gfxrecon::util::datetime::DiffTimestamps(start_time, end_time)));
    GFXRECON_WRITE_CONSOLE("File processing time: %d seconds", time_in_seconds);

    gfxrecon::util::Log::Release();
    return 0;
}
