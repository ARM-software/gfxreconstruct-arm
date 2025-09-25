
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

#ifndef GFXRECON_UTIL_UNDEFINED_GUID_H
#define GFXRECON_UTIL_UNDEFINED_GUID_H

#include <initguid.h>

// This IID is not defined in d3dcommon.h or dxguid.lib
DEFINE_GUID(IID_ID3DDestructionNotifier, 0xa06eb39a, 0x50da, 0x425b, 0x8c, 0x31, 0x4e, 0xec, 0xd6, 0xc2, 0x70, 0xf3);

// DEFINE_GUID(IID_ID3D12DSRDeviceFactory, 0x51ee7783,0x6426,0x4428,0xb1,0x82,0x42,0xf3,0x54,0x1f,0xca,0x71);

#endif // GFXRECON_UTIL_UNDEFINED_GUID_H
