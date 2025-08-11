/*
** Copyright (c) 2025 LunarG, Inc.
** Copyright (c) 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
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

#include "util/defines.h"
#include "util/logging.h"
#include <string>
#include <regex>

#ifndef GFXRECON_REPLAY_SETTINGS_ARM_H
#define GFXRECON_REPLAY_SETTINGS_ARM_H

static constexpr const char kArmOptions[] = ",--dsf|--disable-subpass-fusion"
                                            ",--use-ext-frame-boundary";

static constexpr const char kArmArguments[] = ",--tsp|--trigger-script-path"
                                              ",--tsf|--trigger-script-frame"
                                              ",--marking-layers";

inline const char* GetArmOptionString(const char* options)
{
    // add alternative name for --use-colorspace-fallback
    auto modified_options = std::regex_replace(
        options, std::regex("--use-colorspace-fallback"), "--use-colorspace-fallback|--colorspace-fallback");

    static const std::string combined_opts = modified_options + kArmOptions;
    return combined_opts.c_str();
}

inline const char* GetArmArgumentsString(const char* arguments)
{
    static const std::string combined_args = std::string(arguments) + kArmArguments;
    return combined_args.c_str();
}

// Part of the help message displaying short usage of cross-API options
inline void PrintUsageArmShort()
{
    GFXRECON_WRITE_CONSOLE("\t\t\t[--dsf | --disable-subpass-fusion]");
    GFXRECON_WRITE_CONSOLE("\t\t\t[--use-ext-frame-boundary]");
#if !defined(WIN32)
    GFXRECON_WRITE_CONSOLE("\t\t\t[--tsp | --trigger-script-path <script-file>]");
    GFXRECON_WRITE_CONSOLE("\t\t\t[--tsf | --trigger-script-frame <frame-ranges>]");
#endif // WIN32
    GFXRECON_WRITE_CONSOLE("\t\t\t[--marking-layers <N1,...>]");
}

// Part of the help message displaying detailed usage of cross-API options
inline void PrintUsageArmDetailedCommon()
{
#if !defined(WIN32)
    GFXRECON_WRITE_CONSOLE(" --trigger-script-path <script-file>");
    GFXRECON_WRITE_CONSOLE("          \t\tPath to script file.");
    GFXRECON_WRITE_CONSOLE(" --trigger-script-frame <frame-ranges>");
    GFXRECON_WRITE_CONSOLE("          \t\tTrigger script for the specified frames.");
    GFXRECON_WRITE_CONSOLE("          \t\tTarget frames are specified as a comma separated");
    GFXRECON_WRITE_CONSOLE("          \t\tlist of frame ranges. * is for all frames");
#endif
}

// Part of the help message displaying detailed usage of Vulkan-specific options
inline void PrintUsageArmDetailedVulkanOnly()
{
    GFXRECON_WRITE_CONSOLE("  --use-ext-frame-boundary");
    GFXRECON_WRITE_CONSOLE("          \t\tConvert all offscreen frame boundaries to `VK_EXT_frame_boundary`");
    GFXRECON_WRITE_CONSOLE("          \t\tframe boundaries.");
    GFXRECON_WRITE_CONSOLE("  --dsf   \t\tForce disable subpass fusion.");
    GFXRECON_WRITE_CONSOLE("          \t\tTry to nudge the driver to \"fuse\" subpasses of the render pass");
    GFXRECON_WRITE_CONSOLE("          \t\tinto 1 pass,");
    GFXRECON_WRITE_CONSOLE("  --marking-layers <N1[,...]>");
    GFXRECON_WRITE_CONSOLE("          \t\t Specifies the tools that are used to mark API calls injected by replayer");
}

#endif /* GFXRECON_REPLAY_SETTINGS_ARM_H */
