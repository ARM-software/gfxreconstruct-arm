/*
** Copyright (c) 2024 LunarG, Inc
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

#ifndef GFXRECON_UTIL_MARKING_LAYER_H
#define GFXRECON_UTIL_MARKING_LAYER_H

#include "vulkan/vulkan.h"
#include "util/defines.h"
#include "format/format.h"

#include <string>
#include <set>
#include <unordered_map>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(util)

class MarkingLayersUtil
{
  private:
    MarkingLayersUtil() = delete;

    using MarkInjectedCallback = void(void*);

    struct MarkInjectedCallbacks
    {
        void*                 user_data;
        MarkInjectedCallback* begin_injected = nullptr;
        MarkInjectedCallback* end_injected   = nullptr;
    };

    // Takes as input any Vulkan dispatchable handle (VkDevice, VkQueue, VkCommandBuffer...)
    // Returns a key that is the same for every handle created from a same VkDevice
    // See
    // https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDriverInterface.md#driver-dispatchable-object-creation
    static uintptr_t GetKey(const void* handle) { return *reinterpret_cast<const uintptr_t*>(handle); }

    inline static std::unordered_map<uintptr_t, std::vector<MarkInjectedCallbacks>> callbacks_;

  public:
    static void BeginInjected(const void* handle)
    {
        const uintptr_t key = GetKey(handle);
        auto            it  = callbacks_.find(key);

        if (it != callbacks_.end())
        {
            for (const MarkInjectedCallbacks& callback : it->second)
            {
                callback.begin_injected(callback.user_data);
            }
        }
    }

    static void EndInjected(const void* handle)
    {
        const uintptr_t key = GetKey(handle);
        auto            it  = callbacks_.find(key);

        if (it != callbacks_.end())
        {
            for (const MarkInjectedCallbacks& callback : it->second)
            {
                callback.end_injected(callback.user_data);
            }
        }
    }

    static void AddCallbacks(const void* handle, const VkPhysicalDeviceToolProperties& tool_properties)
    {
        callbacks_[GetKey(handle)].emplace_back(*reinterpret_cast<MarkInjectedCallbacks*>(tool_properties.pNext));
    }
};

GFXRECON_END_NAMESPACE(util)
GFXRECON_END_NAMESPACE(gfxrecon)
#endif