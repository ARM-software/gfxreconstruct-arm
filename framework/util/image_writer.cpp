/*
** Copyright (c) 2020 LunarG, Inc.
** Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
*
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

#include "image_writer.h"

#include "platform.h"
#include "util/file_path.h"
#include "util/logging.h"

#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <inttypes.h>
#include <limits>
#include <math.h>
#include <memory>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#if defined(GFXRECON_ENABLE_ZLIB_COMPRESSION) && defined(GFXRECON_ENABLE_PNG_SCREENSHOT)
#include <zlib.h>

// This function re-formats the call to the Zlib compress2 function so that we can
// use it in the STB PNG generation code.  Using the default STB PNG compression
// resulted in a 20% reduction versus the original image in tests, while using the
// zlib compress2 function resulted in a 40+% reduction versus the original image.
uint8_t* GFXRECON_zlib_compress2(uint8_t* data, int32_t data_len, int32_t* out_len, int32_t quality)
{
    unsigned long alloc_len = compressBound(static_cast<uLong>(data_len));
    auto*         target    = reinterpret_cast<uint8_t*>(malloc(alloc_len));
    if (nullptr != target)
    {
        unsigned long ret_len = alloc_len;
        if (Z_OK == compress2(target, &ret_len, data, static_cast<uLong>(data_len), quality))
        {
            *out_len = static_cast<int32_t>(ret_len);
            return target;
        }
        free(target);
    }
    return nullptr;
}
#define STBIW_ZLIB_COMPRESS GFXRECON_zlib_compress2

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC

#if defined(_WIN32)
#define __STDC_LIB_EXT1__
#endif

#include <stb_image_write.h>
#endif // GFXRECON_ENABLE_ZLIB_COMPRESSION && GFXRECON_ENABLE_PNG_SCREENSHOT
#include <algorithm>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(util)
GFXRECON_BEGIN_NAMESPACE(imagewriter)

const uint16_t kBmpBitCount = 32; // Expecting 32-bit BGRA bitmap data.
const uint32_t kImageBpp    = 4;  // Expecting 4 bytes per pixel for 32-bit BGRA bitmap data.

const uint16_t kBmpBitCountNoAlpha = 24; // Expecting 24-bit BGR bitmap data.
const uint32_t kImageBppNoAlpha    = 3;  // Expecting 3 bytes per pixel for 32-bit BGRA bitmap data; alpha removed.

static size_t               temporary_buffer_size = 0;
static std::vector<uint8_t> temporary_buffer;

// This function is a copy from Renderdoc sources
inline float ConvertFromHalf(uint16_t comp)
{
    bool sign     = (comp & 0x8000) != 0;
    int  exponent = (comp & 0x7C00) >> 10;
    int  mantissa = comp & 0x03FF;

    if (exponent == 0x00)
    {
        if (mantissa == 0)
            return sign ? -0.0f : 0.0f;

        // subnormal
        float ret   = (float)mantissa;
        int*  alias = (int*)&ret;

        // set sign bit and set exponent to 2^-24
        // (2^-14 from spec for subnormals * 2^-10 to convert (float)mantissa to 0.mantissa)
        *alias = (sign ? 0x80000000 : 0) | (*alias - (24 << 23));

        return ret;
    }
    else if (exponent < 0x1f)
    {
        exponent -= 15;

        float ret   = 0.0f;
        int*  alias = (int*)&ret;

        // convert to float. Put sign bit in the right place, convert exponent to be
        // [-128,127] and put in the right place, then shift mantissa up.
        *alias = (sign ? 0x80000000 : 0) | (exponent + 127) << 23 | (mantissa << 13);

        return ret;
    }
    else // if(exponent = 0x1f)
    {
        union
        {
            int   i;
            float f;
        } ret;

        if (mantissa == 0)
            ret.i = (sign ? 0x80000000 : 0) | 0x7F800000;
        else
            ret.i = 0x7F800001;

        return ret.f;
    }
}

// This function is a copy from Renderdoc sources
static float Ufloat11ToFloat(uint16_t val)
{
    const uint32_t        mantissa = val & 0x3f;
    const uint32_t        exponent = (val >> 6) & 0x1f;
    static const uint32_t lead_bit = 0x40;

    if (mantissa == 0 && exponent == 0)
    {
        return 0.0f;
    }
    else
    {
        if (exponent == 0x1f)
        {
            if (mantissa == 0)
            {
                // +inf
                return std::numeric_limits<float>::max();
            }
            else
            {
                // NaN... I don't think we want a Nan in a pixel
                return 0.0f;
            }
        }
        else if (exponent != 0)
        {
            uint32_t combined = lead_bit | mantissa;
            return (static_cast<float>(combined) / static_cast<float>(lead_bit)) *
                   pow(2.0f, static_cast<float>(exponent) - 15.0f);
        }
        else /* if (exponent == 0) */
        {
            return static_cast<float>(mantissa) / static_cast<float>(lead_bit) * pow(2.0f, 1.0f - 15.0f);
        }
    }
}

// This function is a copy from Renderdoc sources
static float Ufloat10ToFloat(uint16_t val)
{
    const uint32_t        mantissa = val & 0x1f;
    const uint32_t        exponent = (val >> 5) & 0x1f;
    static const uint32_t lead_bit = 0x20;

    if (mantissa == 0 && exponent == 0)
    {
        return 0.0f;
    }
    else
    {
        if (exponent == 0x1f)
        {
            if (mantissa == 0)
            {
                // +inf
                return std::numeric_limits<float>::max();
            }
            else
            {
                // NaN... I don't think we want a Nan in a pixel
                return 0.0f;
            }
        }
        else if (exponent != 0)
        {
            uint32_t combined = lead_bit | mantissa;
            return (static_cast<float>(combined) / static_cast<float>(lead_bit)) *
                   pow(2.0f, static_cast<float>(exponent) - 15.0f);
        }
        else /* if (exponent == 0) */
        {
            return static_cast<float>(mantissa) / static_cast<float>(lead_bit) * pow(2.0f, 1.0f - 15.0f);
        }
    }
}

#define CheckFwriteRetVal(_val_, _file_)                                                              \
    {                                                                                                 \
        if (!_val_)                                                                                   \
        {                                                                                             \
            GFXRECON_LOG_ERROR("%s() (%u): fwrite failed (%s)", __func__, __LINE__, strerror(errno)); \
            util::platform::FileClose(_file_);                                                        \
                                                                                                      \
            return false;                                                                             \
        }                                                                                             \
    }

// This function is a copy from Renderdoc sources
static float ConvertLinearToSRGB(float linear)
{
    if (linear <= 0.0031308f)
        return 12.92f * linear;

    if (linear < 0.0f)
        linear = 0.0f;
    else if (linear > 1.0f)
        linear = 1.0f;

    return 1.055f * powf(linear, 1.0f / 2.4f) - 0.055f;
}

static const uint8_t* ConvertIntoTemporaryBuffer(uint32_t    width,
                                                 uint32_t    height,
                                                 const void* data,
                                                 uint32_t    data_pitch,
                                                 DataFormats format,
                                                 bool        is_png,
                                                 bool        write_alpha)
{
    assert(data_pitch);

    uint32_t output_pitch = width * (write_alpha ? kImageBpp : kImageBppNoAlpha);
    if (!is_png)
    {
        output_pitch = static_cast<uint32_t>(util::platform::GetAlignedSize(output_pitch, 4));
    }

    const uint32_t output_size = height * output_pitch;
    if (temporary_buffer_size < output_size)
    {
        temporary_buffer_size = output_size;
        temporary_buffer.resize(output_size);
    }

    uint8_t* temp_buffer = temporary_buffer.data();

    switch (format)
    {
        case kFormat_R8:
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint8_t r = bytes[x];

                    *(temp_buffer++) = r;
                    *(temp_buffer++) = r;
                    *(temp_buffer++) = r;

                    if (write_alpha)
                    {
                        *(temp_buffer++) = 0xff;
                    }
                }

                bytes += data_pitch;
                temp_buffer = reinterpret_cast<uint8_t*>(temporary_buffer.data()) + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_RGB:
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint8_t r = bytes[(3 * x) + 0];
                    const uint8_t g = bytes[(3 * x) + 1];
                    const uint8_t b = bytes[(3 * x) + 2];

                    if (is_png)
                    {
                        *(temp_buffer++) = r;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = b;
                    }
                    else
                    {
                        *(temp_buffer++) = b;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = r;
                    }

                    if (write_alpha)
                    {
                        *(temp_buffer++) = 0xff;
                    }
                }

                bytes += data_pitch;
                temp_buffer = reinterpret_cast<uint8_t*>(temporary_buffer.data()) + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_RGBA:
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint8_t r = bytes[(4 * x) + 0];
                    const uint8_t g = bytes[(4 * x) + 1];
                    const uint8_t b = bytes[(4 * x) + 2];
                    const uint8_t a = bytes[(4 * x) + 3];

                    if (is_png)
                    {
                        *(temp_buffer++) = r;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = b;
                    }
                    else
                    {
                        *(temp_buffer++) = b;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = r;
                    }

                    if (write_alpha)
                    {
                        *(temp_buffer++) = a;
                    }
                }

                bytes += data_pitch;
                temp_buffer = temporary_buffer.data() + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_BGR:
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint8_t b = bytes[(3 * x) + 0];
                    const uint8_t g = bytes[(3 * x) + 1];
                    const uint8_t r = bytes[(3 * x) + 2];

                    if (is_png)
                    {
                        *(temp_buffer++) = r;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = b;
                    }
                    else
                    {
                        *(temp_buffer++) = b;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = r;
                    }

                    if (write_alpha)
                    {
                        *(temp_buffer++) = 0xff;
                    }
                }

                bytes += data_pitch;
                temp_buffer = temporary_buffer.data() + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_BGRA:
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint8_t b = bytes[(4 * x) + 0];
                    const uint8_t g = bytes[(4 * x) + 1];
                    const uint8_t r = bytes[(4 * x) + 2];
                    const uint8_t a = bytes[(4 * x) + 3];

                    if (is_png)
                    {
                        *(temp_buffer++) = r;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = b;
                    }
                    else
                    {
                        *(temp_buffer++) = b;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = r;
                    }

                    if (write_alpha)
                    {
                        *(temp_buffer++) = a;
                    }
                }

                bytes += data_pitch;
                temp_buffer = temporary_buffer.data() + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_B10G11R11_UFLOAT:
        {
            const uint32_t* u32_vals = reinterpret_cast<const uint32_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    // clang-format off
                    const float b = Ufloat10ToFloat(static_cast<uint16_t>((u32_vals[x] & (0xFFC00000)) >> 22));
                    const float g = Ufloat11ToFloat(static_cast<uint16_t>((u32_vals[x] & (0x003FF800)) >> 11));
                    const float r = Ufloat11ToFloat(static_cast<uint16_t>((u32_vals[x] & (0x000007FF)) >> 0));
                    // clang-format on

                    const uint8_t b_u8 = static_cast<uint8_t>(std::min(b, 1.0f) * 255.0f);
                    const uint8_t g_u8 = static_cast<uint8_t>(std::min(g, 1.0f) * 255.0f);
                    const uint8_t r_u8 = static_cast<uint8_t>(std::min(r, 1.0f) * 255.0f);

                    if (is_png)
                    {
                        *(temp_buffer++) = r_u8;
                        *(temp_buffer++) = g_u8;
                        *(temp_buffer++) = b_u8;
                    }
                    else
                    {
                        *(temp_buffer++) = b_u8;
                        *(temp_buffer++) = g_u8;
                        *(temp_buffer++) = r_u8;
                    }

                    if (write_alpha)
                    {
                        *(temp_buffer++) = 0xff;
                    }
                }

                u32_vals = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(u32_vals) + data_pitch);
                temp_buffer = reinterpret_cast<uint8_t*>(temporary_buffer.data()) + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_A2B10G10R10:
        {
            const uint32_t* u32_vals = reinterpret_cast<const uint32_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    uint8_t a = static_cast<uint8_t>((u32_vals[x] & 0xC0000000) >> 30);
                    uint8_t b = static_cast<uint8_t>((u32_vals[x] & 0x3FF00000) >> 20);
                    uint8_t g = static_cast<uint8_t>((u32_vals[x] & 0x000FFC00) >> 10);
                    uint8_t r = static_cast<uint8_t>((u32_vals[x] & 0x000003FF) >> 0);

                    r = static_cast<uint8_t>(static_cast<float>(r) / 1023.0f * 255.0f);
                    g = static_cast<uint8_t>(static_cast<float>(g) / 1023.0f * 255.0f);
                    b = static_cast<uint8_t>(static_cast<float>(b) / 1023.0f * 255.0f);
                    a = static_cast<uint8_t>(static_cast<float>(a) / 3.0f * 255.0f);

                    if (is_png)
                    {
                        *(temp_buffer++) = r;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = b;
                    }
                    else
                    {
                        *(temp_buffer++) = b;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = r;
                    }

                    if (write_alpha)
                    {
                        *(temp_buffer++) = a;
                    }
                }

                u32_vals = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(u32_vals) + data_pitch);
                temp_buffer = reinterpret_cast<uint8_t*>(temporary_buffer.data()) + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_R16G16B16A16_SFLOAT:
        {
            const uint64_t* u64_vals = reinterpret_cast<const uint64_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    uint16_t a_u16 = static_cast<uint16_t>((u64_vals[x] & 0xFFFF000000000000ULL) >> 48);
                    uint16_t b_u16 = static_cast<uint16_t>((u64_vals[x] & 0x0000FFFF00000000ULL) >> 32);
                    uint16_t g_u16 = static_cast<uint16_t>((u64_vals[x] & 0x00000000FFFF0000ULL) >> 16);
                    uint16_t r_u16 = static_cast<uint16_t>((u64_vals[x] & 0x000000000000FFFFULL) >> 0);

                    float r_f = std::clamp(ConvertFromHalf(r_u16), 0.0f, 1.0f);
                    float g_f = std::clamp(ConvertFromHalf(g_u16), 0.0f, 1.0f);
                    float b_f = std::clamp(ConvertFromHalf(b_u16), 0.0f, 1.0f);
                    float a_f = std::clamp(ConvertFromHalf(a_u16), 0.0f, 1.0f);

                    r_f = ConvertLinearToSRGB(r_f);
                    g_f = ConvertLinearToSRGB(g_f);
                    b_f = ConvertLinearToSRGB(b_f);

                    uint8_t r = static_cast<uint8_t>(r_f * 255.0f + 0.5f);
                    uint8_t g = static_cast<uint8_t>(g_f * 255.0f + 0.5f);
                    uint8_t b = static_cast<uint8_t>(b_f * 255.0f + 0.5f);
                    uint8_t a = static_cast<uint8_t>(a_f * 255.0f + 0.5f);

                    if (is_png)
                    {
                        *(temp_buffer++) = r;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = b;
                    }
                    else
                    {
                        *(temp_buffer++) = b;
                        *(temp_buffer++) = g;
                        *(temp_buffer++) = r;
                    }

                    if (write_alpha)
                    {
                        *(temp_buffer++) = a;
                    }
                }

                u64_vals = reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(u64_vals) + data_pitch);
                temp_buffer = reinterpret_cast<uint8_t*>(temporary_buffer.data()) + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_D32_FLOAT:
        {
            const float* floats = reinterpret_cast<const float*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const float   float_depth = floats[x];
                    const uint8_t depth       = static_cast<uint8_t>(float_depth * 255.0f);

                    *(temp_buffer++) = depth;
                    *(temp_buffer++) = depth;
                    *(temp_buffer++) = depth;

                    if (write_alpha)
                    {
                        *(temp_buffer++) = 0xff;
                    }
                }

                floats      = reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(floats) + data_pitch);
                temp_buffer = temporary_buffer.data() + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_D24_UNORM:
        {
            const uint32_t* bytes_u32 = reinterpret_cast<const uint32_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint32_t normalized_depth = bytes_u32[x] & 0x00FFFFFF;
                    const float    float_depth = static_cast<float>(normalized_depth) / static_cast<float>(0xffffff);
                    const uint8_t  depth       = static_cast<uint8_t>(float_depth * 255.0f);

                    *(temp_buffer++) = depth;
                    *(temp_buffer++) = depth;
                    *(temp_buffer++) = depth;

                    if (write_alpha)
                    {
                        *(temp_buffer++) = 0xff;
                    }
                }

                bytes_u32 = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(bytes_u32) + data_pitch);
                temp_buffer = temporary_buffer.data() + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_D16_UNORM:
        {
            const uint16_t* bytes_u16 = reinterpret_cast<const uint16_t*>(data);

            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint16_t normalized_depth = bytes_u16[x];
                    const float    float_depth      = static_cast<float>(normalized_depth) / static_cast<float>(0xffff);
                    const uint8_t  depth            = static_cast<uint8_t>(float_depth * 255.0f);

                    *(temp_buffer++) = depth;
                    *(temp_buffer++) = depth;
                    *(temp_buffer++) = depth;

                    if (write_alpha)
                    {
                        *(temp_buffer++) = 0xff;
                    }
                }

                bytes_u16 = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(bytes_u16) + data_pitch);
                temp_buffer = temporary_buffer.data() + (y + 1) * output_pitch;
            }
        }
        break;

        case kFormat_S8_UINT:
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
            for (uint32_t y = 0; y < height; ++y)
            {
                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint8_t s = bytes[x];

                    *(temp_buffer++) = s;
                    *(temp_buffer++) = s;
                    *(temp_buffer++) = s;

                    if (write_alpha)
                    {
                        *(temp_buffer++) = 0xff;
                    }
                }

                bytes       = bytes + data_pitch;
                temp_buffer = temporary_buffer.data() + (y + 1) * output_pitch;
            }
        }
        break;

        default:
            GFXRECON_LOG_ERROR("Format %u not handled", format);
            assert(0);
            return nullptr;
    }

    return reinterpret_cast<const uint8_t*>(temporary_buffer.data());
}

static uint8_t*
ExtractAlphaChannel(uint32_t width, uint32_t height, const void* data, uint32_t data_pitch, bool expand_to_rgb)
{
    const size_t output_size = width * height * (expand_to_rgb ? kImageBppNoAlpha : 1);
    if (temporary_buffer_size < output_size)
    {
        temporary_buffer_size = output_size;
        temporary_buffer.resize(output_size);
    }

    const uint32_t* pixels      = reinterpret_cast<const uint32_t*>(data);
    uint8_t*        temp_buffer = temporary_buffer.data();

    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const auto alpha = static_cast<uint8_t>((pixels[x] & 0xff000000) >> 24);

            *(temp_buffer++) = alpha;

            if (expand_to_rgb)
            {
                *(temp_buffer++) = alpha;
                *(temp_buffer++) = alpha;
            }
        }

        pixels = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(pixels) + data_pitch);
    }

    return temporary_buffer.data();
}

static bool WriteBmpHeader(FILE* file, uint32_t width, uint32_t height, bool write_alpha)
{
    assert(file);

    // BMP image data requires row to be a multiple of 4 bytes
    // Round-up row size to next multiple of 4, if it isn't already
    const uint32_t bmp_pitch =
        static_cast<uint32_t>(util::platform::GetAlignedSize(width * (write_alpha ? kImageBpp : kImageBppNoAlpha), 4));

    BmpFileHeader file_header;
    BmpInfoHeader info_header;

    file_header.type      = ('M' << 8) | 'B';
    file_header.reserved1 = 0;
    file_header.reserved2 = 0;
    file_header.off_bits  = sizeof(file_header) + sizeof(info_header);
    file_header.size      = (height * bmp_pitch) + file_header.off_bits;

    info_header.size             = sizeof(info_header);
    info_header.width            = static_cast<int32_t>(width);
    info_header.height           = static_cast<int32_t>(height);
    info_header.planes           = 1;
    info_header.bit_count        = write_alpha ? kBmpBitCount : kBmpBitCountNoAlpha;
    info_header.compression      = 0;
    info_header.size_image       = 0;
    info_header.x_pels_per_meter = 0;
    info_header.y_pels_per_meter = 0;
    info_header.clr_used         = 0;
    info_header.clr_important    = 0;

    bool ret = util::platform::FileWrite(&file_header, sizeof(file_header), file);
    CheckFwriteRetVal(ret, file);

    ret = util::platform::FileWrite(&info_header, sizeof(info_header), file);
    CheckFwriteRetVal(ret, file);

    return true;
}

bool WriteBmpImage(const std::string& filename,
                   uint32_t           width,
                   uint32_t           height,
                   const void*        data,
                   uint32_t           data_pitch,
                   DataFormats        format,
                   bool               write_alpha)
{
    GFXRECON_LOG_INFO("%s(): Writing file \"%s\"", __func__, filename.c_str())

    if (data_pitch == 0)
    {
        data_pitch = static_cast<uint32_t>(width * DataFormatsSizes(format));
        if (data_pitch == 0)
        {
            GFXRECON_LOG_ERROR("%s() Failed writing file (data_pitch == 0)", __func__);
            return false;
        }
    }

    bool    success = false;
    FILE*   file    = nullptr;
    int32_t result  = util::platform::FileOpen(&file, filename.c_str(), "wb");

    if ((result == 0) && (file != nullptr))
    {
        success = WriteBmpHeader(file, width, height, write_alpha);
        if (!success)
        {
            GFXRECON_LOG_ERROR("%s() Failed writing file", __func__);
            return false;
        }

        // Y needs to be inverted when writing the bitmap data.
        auto height_1 = height - 1;

        if ((format == kFormat_BGR && !write_alpha) || (format == kFormat_BGRA && write_alpha))
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
            for (uint32_t y = 0; y < height; ++y)
            {
                success = util::platform::FileWrite(&bytes[(height_1 - y) * data_pitch], data_pitch, file);
                CheckFwriteRetVal(success, file);
            }
        }
        else
        {
            const uint32_t bmp_pitch = static_cast<uint32_t>(
                util::platform::GetAlignedSize(width * (write_alpha ? kImageBpp : kImageBppNoAlpha), 4));

            const uint8_t* bytes =
                ConvertIntoTemporaryBuffer(width, height, data, data_pitch, format, false, write_alpha);
            for (uint32_t y = 0; y < height; ++y)
            {
                success = util::platform::FileWrite(&bytes[(height_1 - y) * bmp_pitch], bmp_pitch, file);
                CheckFwriteRetVal(success, file);
            }
        }

        if (!ferror(file))
        {
            success = true;
        }

        util::platform::FileClose(file);
    }
    else
    {
        GFXRECON_LOG_ERROR("%s() Failed to open file (%s)", __func__, strerror(errno));
    }

    return success;
}

bool WriteBmpImageSeparateAlpha(const std::string& filename,
                                uint32_t           width,
                                uint32_t           height,
                                const void*        data,
                                uint32_t           data_pitch,
                                DataFormats        data_format)
{
    bool success = WriteBmpImage(filename, width, height, data, data_pitch, data_format, false);

    if (success && DataFormatHasAlpha(data_format))
    {
        const uint8_t*    alpha_channel  = ExtractAlphaChannel(width, height, data, data_pitch, true);
        const std::string alpha_filename = util::filepath::InsertFilenamePostfix(filename, "_alpha");
        const size_t      alpha_pitch    = width * kImageBppNoAlpha;
        const DataFormats alpha_format   = data_format == kFormat_BGRA ? kFormat_BGR : kFormat_RGB;
        success                          = WriteBmpImage(
            alpha_filename, width, height, alpha_channel, static_cast<uint32_t>(alpha_pitch), alpha_format, false);
    }

    return success;
}

bool WritePngImage(const std::string& filename,
                   uint32_t           width,
                   uint32_t           height,
                   const void*        data,
                   uint32_t           data_pitch,
                   DataFormats        format,
                   bool               write_alpha)
{
    bool success = false;

#ifdef GFXRECON_ENABLE_PNG_SCREENSHOT
    GFXRECON_LOG_INFO("%s(): Writing file \"%s\"", __func__, filename.c_str())

    if (data_pitch == 0)
    {
        data_pitch = static_cast<uint32_t>(width * DataFormatsSizes(format));
        if (data_pitch == 0)
        {
            GFXRECON_LOG_ERROR("%s() Failed writing file (data_pitch == 0)", __func__);
            return false;
        }
    }

    const uint8_t* bytes = ConvertIntoTemporaryBuffer(width, height, data, data_pitch, format, true, write_alpha);

    stbi_write_png_compression_level = 4;
    const uint32_t png_row_pitch     = width * (write_alpha ? kImageBpp : kImageBppNoAlpha);

    if (1 == stbi_write_png(filename.c_str(),
                            static_cast<int>(width),
                            static_cast<int>(height),
                            static_cast<int>(write_alpha ? kImageBpp : kImageBppNoAlpha),
                            bytes,
                            (int)png_row_pitch))
    {
        success = true;
    }
    else
    {
        GFXRECON_LOG_ERROR("%s() Failed writing file", __func__);
    }
#endif

    return success;
}

bool WritePngImageSeparateAlpha(const std::string& filename,
                                uint32_t           width,
                                uint32_t           height,
                                const void*        data,
                                uint32_t           data_pitch,
                                DataFormats        format)
{
    bool success = true;

#ifdef GFXRECON_ENABLE_PNG_SCREENSHOT
    success = WritePngImage(filename, width, height, data, data_pitch, format, false);
    if (success && DataFormatHasAlpha(format))
    {
        const std::string alpha_filename = util::filepath::InsertFilenamePostfix(filename, "_alpha");
        const uint8_t*    alpha_channel  = ExtractAlphaChannel(width, height, data, data_pitch, false);
        const size_t      alpha_pitch    = width;
        success                          = stbi_write_png(alpha_filename.c_str(),
                                 static_cast<int>(width),
                                 static_cast<int>(height),
                                 1,
                                 alpha_channel,
                                 static_cast<int>(alpha_pitch));

        if (!success)
        {
            GFXRECON_LOG_ERROR("%s() Failed writing file %s", __func__, alpha_filename.c_str());
        }
    }
#endif

    return success;
}

template <ImageRotation ROTATION, bool MIRRORED = false>
static void RotateAndMirrorPixelsInternal(const uint32_t* src_pixels,
                                          uint32_t*       dst_pixels,
                                          uint32_t        src_width,
                                          uint32_t        src_height,
                                          uint32_t        dst_width,
                                          uint32_t        dst_height)
{
    // Loop Tiling (32x32 blocks) to improve CPU cache utilization when reading
    // from the src_pixels array with a large stride during rotation.
    constexpr uint32_t TILE_SIZE = 32;
    for (uint32_t dst_y_block = 0; dst_y_block < dst_height; dst_y_block += TILE_SIZE)
    {
        for (uint32_t dst_x_block = 0; dst_x_block < dst_width; dst_x_block += TILE_SIZE)
        {
            uint32_t max_y = std::min(dst_y_block + TILE_SIZE, dst_height);
            uint32_t max_x = std::min(dst_x_block + TILE_SIZE, dst_width);

            for (uint32_t dst_y = dst_y_block; dst_y < max_y; ++dst_y)
            {
                for (uint32_t dst_x = dst_x_block; dst_x < max_x; ++dst_x)
                {
                    // Conceptually, when an image is both mirrored and rotated, we define
                    // the forward transformation as Mirror-then-Rotate.
                    // To perform the inverse operation, we un-rotate then un-mirror
                    // (Rotate-then-Mirror). When pulling from the source backwards, this
                    // means we mirror our destination X coordinate first, then apply
                    // the inverse rotation to find the corresponding source pixel.
                    uint32_t mirrored_dst_x = dst_x;
                    if constexpr (MIRRORED)
                    {
                        mirrored_dst_x = dst_width - 1 - dst_x;
                    }

                    uint32_t src_x = mirrored_dst_x;
                    uint32_t src_y = dst_y;
                    if constexpr (ROTATION == ImageRotation::DEG_90)
                    {
                        src_x = src_width - 1 - dst_y;
                        src_y = mirrored_dst_x;
                    }
                    else if constexpr (ROTATION == ImageRotation::DEG_270)
                    {
                        src_x = dst_y;
                        src_y = src_height - 1 - mirrored_dst_x;
                    }
                    else if constexpr (ROTATION == ImageRotation::DEG_180)
                    {
                        src_x = src_width - 1 - mirrored_dst_x;
                        src_y = src_height - 1 - dst_y;
                    }

                    dst_pixels[dst_y * dst_width + dst_x] = src_pixels[src_y * src_width + src_x];
                }
            }
        }
    }
}

void RotateAndMirrorPixels(ImageRotation   rotation,
                           bool            is_mirrored,
                           const uint32_t* src_pixels,
                           uint32_t*       dst_pixels,
                           uint32_t        src_width,
                           uint32_t        src_height,
                           uint32_t        dst_width,
                           uint32_t        dst_height)
{
    if (rotation == ImageRotation::DEG_90)
    {
        if (is_mirrored)
        {
            RotateAndMirrorPixelsInternal<ImageRotation::DEG_90, /*mirrored=*/true>(
                src_pixels, dst_pixels, src_width, src_height, dst_width, dst_height);
        }
        else
        {
            RotateAndMirrorPixelsInternal<ImageRotation::DEG_90>(
                src_pixels, dst_pixels, src_width, src_height, dst_width, dst_height);
        }
    }
    else if (rotation == ImageRotation::DEG_270)
    {
        if (is_mirrored)
        {
            RotateAndMirrorPixelsInternal<ImageRotation::DEG_270, /*mirrored=*/true>(
                src_pixels, dst_pixels, src_width, src_height, dst_width, dst_height);
        }
        else
        {
            RotateAndMirrorPixelsInternal<ImageRotation::DEG_270>(
                src_pixels, dst_pixels, src_width, src_height, dst_width, dst_height);
        }
    }
    else if (rotation == ImageRotation::DEG_180)
    {
        if (is_mirrored)
        {
            RotateAndMirrorPixelsInternal<ImageRotation::DEG_180, /*mirrored=*/true>(
                src_pixels, dst_pixels, src_width, src_height, dst_width, dst_height);
        }
        else
        {
            RotateAndMirrorPixelsInternal<ImageRotation::DEG_180>(
                src_pixels, dst_pixels, src_width, src_height, dst_width, dst_height);
        }
    }
    else
    {
        if (is_mirrored)
        {
            RotateAndMirrorPixelsInternal<ImageRotation::DEG_0, /*mirrored=*/true>(
                src_pixels, dst_pixels, src_width, src_height, dst_width, dst_height);
        }
        else
        {
            RotateAndMirrorPixelsInternal<ImageRotation::DEG_0, /*mirrored=*/false>(
                src_pixels, dst_pixels, src_width, src_height, dst_width, dst_height);
        }
    }
}

GFXRECON_END_NAMESPACE(imagewriter)
GFXRECON_END_NAMESPACE(util)
GFXRECON_END_NAMESPACE(gfxrecon)
