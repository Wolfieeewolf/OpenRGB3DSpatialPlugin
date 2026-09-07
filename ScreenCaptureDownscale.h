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

    if(src_w == dst_w && src_h == dst_h && src_stride == dst_w * 4 &&
       src_r == 0 && src_g == 1 && src_b == 2 && src_a == 3)
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

/** Never keep a CPU working frame larger than 1080p. 1440/4K displays are
 *  box-averaged into this size so ambilight stays area-sampled without lag. */
inline constexpr int kScreenMirrorMaxWorkingWidth = 1920;
inline constexpr int kScreenMirrorMaxWorkingHeight = 1080;

inline void ScreenMirrorQualityToSize(int quality, int& width, int& height)
{
    quality = std::clamp(quality, 0, 7);
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
        case 6:
        case 7:
            width = kScreenMirrorMaxWorkingWidth;
            height = kScreenMirrorMaxWorkingHeight;
            break;
        default:
            width = 640;
            height = 360;
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
