// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENCAPTURE_DOWNSCALE_H
#define SCREENCAPTURE_DOWNSCALE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>

/** Box-average a packed 8-bit 4-channel image into RGBA8888.
 *  src_r/g/b/a are byte offsets in each source pixel (BGRA: 2,1,0,3). */
inline void BoxDownscaleToRgba(const uint8_t* src,
                               int src_w,
                               int src_h,
                               int src_stride,
                               uint8_t* dst,
                               int dst_w,
                               int dst_h,
                               int src_r,
                               int src_g,
                               int src_b,
                               int src_a)
{
    if(!src || !dst || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0 || src_stride <= 0)
    {
        return;
    }

    if(src_w == dst_w && src_h == dst_h)
    {
        if(src_stride == dst_w * 4 && src_r == 0 && src_g == 1 && src_b == 2 && src_a == 3)
        {
            const size_t bytes = (size_t)dst_w * (size_t)dst_h * 4u;
            if(src != dst)
            {
                std::copy(src, src + bytes, dst);
            }
            return;
        }
        for(int y = 0; y < dst_h; ++y)
        {
            const uint8_t* row = src + (size_t)y * (size_t)src_stride;
            uint8_t* row_dst = dst + (size_t)y * (size_t)dst_w * 4u;
            for(int x = 0; x < dst_w; ++x)
            {
                const uint8_t* px = row + (size_t)x * 4u;
                row_dst[0] = px[src_r];
                row_dst[1] = px[src_g];
                row_dst[2] = px[src_b];
                row_dst[3] = px[src_a];
                row_dst += 4;
            }
        }
        return;
    }

    for(int y = 0; y < dst_h; ++y)
    {
        int y0 = (int)(((int64_t)y * src_h) / dst_h);
        int y1 = (int)(((int64_t)(y + 1) * src_h) / dst_h);
        if(y1 <= y0)
        {
            y1 = y0 + 1;
        }
        if(y1 > src_h)
        {
            y1 = src_h;
        }
        uint8_t* row_dst = dst + (size_t)y * (size_t)dst_w * 4u;
        for(int x = 0; x < dst_w; ++x)
        {
            int x0 = (int)(((int64_t)x * src_w) / dst_w);
            int x1 = (int)(((int64_t)(x + 1) * src_w) / dst_w);
            if(x1 <= x0)
            {
                x1 = x0 + 1;
            }
            if(x1 > src_w)
            {
                x1 = src_w;
            }
            uint32_t sr = 0, sg = 0, sb = 0, sa = 0;
            uint32_t count = 0;
            for(int sy = y0; sy < y1; ++sy)
            {
                const uint8_t* row = src + (size_t)sy * (size_t)src_stride;
                for(int sx = x0; sx < x1; ++sx)
                {
                    const uint8_t* px = row + (size_t)sx * 4u;
                    sr += px[src_r];
                    sg += px[src_g];
                    sb += px[src_b];
                    sa += px[src_a];
                    ++count;
                }
            }
            if(count == 0)
            {
                count = 1;
            }
            row_dst[0] = (uint8_t)((sr + count / 2) / count);
            row_dst[1] = (uint8_t)((sg + count / 2) / count);
            row_dst[2] = (uint8_t)((sb + count / 2) / count);
            row_dst[3] = (uint8_t)((sa + count / 2) / count);
            row_dst += 4;
        }
    }
}

/** Pack BGRA8 into RGBA8888. flip_y matches DXGI's CPU mirror (GDI is already top-down). */
inline void CopyBgraToRgba(const uint8_t* src,
                            int w,
                            int h,
                            int src_stride,
                            uint8_t* dst,
                            bool flip_y)
{
    if(!src || !dst || w <= 0 || h <= 0 || src_stride <= 0)
    {
        return;
    }
    const int dst_stride = w * 4;
    for(int y = 0; y < h; ++y)
    {
        const int sy = flip_y ? (h - 1 - y) : y;
        const uint8_t* row = src + (size_t)sy * (size_t)src_stride;
        uint8_t* row_dst = dst + (size_t)y * (size_t)dst_stride;
        for(int x = 0; x < w; ++x)
        {
            const uint8_t* px = row + (size_t)x * 4u;
            row_dst[0] = px[2];
            row_dst[1] = px[1];
            row_dst[2] = px[0];
            row_dst[3] = px[3];
            row_dst += 4;
        }
    }
}

/** Safety ceiling for Native (match display). Never used to upscale.
 *  Covers 8K and super-ultrawide; DXGI still Maps only the real desktop size. */
inline constexpr int kScreenMirrorMaxWorkingWidth = 7680;
inline constexpr int kScreenMirrorMaxWorkingHeight = 4320;

/** Combo value: copy each monitor at its current resolution (no cap). */
inline constexpr int kScreenMirrorQualityNative = 8;

inline int ScreenMirrorClampQuality(int quality)
{
    return std::clamp(quality, 0, kScreenMirrorQualityNative);
}

inline bool ScreenMirrorQualityIsNative(int quality)
{
    return quality == kScreenMirrorQualityNative;
}

inline void ScreenMirrorQualityToSize(int quality, int& width, int& height)
{
    quality = ScreenMirrorClampQuality(quality);
    switch(quality)
    {
        case 0:
            width = 320;
            height = 180;
            break;
        case 1:
            width = 480;
            height = 270;
            break;
        case 2:
            width = 640;
            height = 360;
            break;
        case 3:
            width = 960;
            height = 540;
            break;
        case 4:
            width = 1280;
            height = 720;
            break;
        case 5:
            width = 1920;
            height = 1080;
            break;
        case 6:
            width = 2560;
            height = 1440;
            break;
        case 7:
            width = 3840;
            height = 2160;
            break;
        case 8:
            width = kScreenMirrorMaxWorkingWidth;
            height = kScreenMirrorMaxWorkingHeight;
            break;
        default:
            width = kScreenMirrorMaxWorkingWidth;
            height = kScreenMirrorMaxWorkingHeight;
            break;
    }
}

inline void ClampWorkingCaptureSize(int native_w, int native_h, int& width, int& height)
{
    if(native_w > 0)
    {
        width = std::min(width, native_w);
    }
    if(native_h > 0)
    {
        height = std::min(height, native_h);
    }
    width = std::max(width, 1);
    height = std::max(height, 1);
}

#endif
