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

#include "generated/generated_vulkan_dispatch_table.h"

#ifndef GFXRECON_GENERATED_VULKAN_DEVICE_TABLE_DISPATCHER_H
#define GFXRECON_GENERATED_VULKAN_DEVICE_TABLE_DISPATCHER_H

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(util)

struct VulkanDeviceTableDispatcher
{
    const encode::VulkanDeviceTable* table_;

    VulkanDeviceTableDispatcher() : table_(nullptr) {}

    VulkanDeviceTableDispatcher(const encode::VulkanDeviceTable* table) : table_(table) {}

    VkDeviceAddress GetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo)
    {
        using namespace encode;
        if (table_->GetBufferDeviceAddress != noop::GetBufferDeviceAddress)
        {
            return table_->GetBufferDeviceAddress(device, pInfo);
        }
        else if (table_->GetBufferDeviceAddressKHR != noop::GetBufferDeviceAddressKHR)
        {
            return table_->GetBufferDeviceAddressKHR(device, pInfo);
        }
        else if (table_->GetBufferDeviceAddressEXT != noop::GetBufferDeviceAddressEXT)
        {
            return table_->GetBufferDeviceAddressEXT(device, pInfo);
        }
        else
        {
            return noop::GetBufferDeviceAddress(device, pInfo);
        }
    }
};

GFXRECON_END_NAMESPACE(util)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_GENERATED_VULKAN_DEVICE_TABLE_DISPATCHER_H