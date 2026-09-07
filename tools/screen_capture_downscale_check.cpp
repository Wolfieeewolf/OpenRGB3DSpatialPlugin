// SPDX-License-Identifier: GPL-2.0-only
// g++ -std=c++17 -O2 -o /tmp/screen_capture_downscale_check tools/screen_capture_downscale_check.cpp && /tmp/screen_capture_downscale_check

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../ScreenCaptureDownscale.h"

int main()
{
    int fails = 0;

    int w = 0;
    int h = 0;
    ScreenMirrorQualityToSize(0, w, h);
    if(w != 320 || h != 180)
    {
        std::fprintf(stderr, "quality 0 size %dx%d\n", w, h);
        fails++;
    }
    ScreenMirrorQualityToSize(4, w, h);
    if(w != 1280 || h != 720)
    {
        std::fprintf(stderr, "quality 4 size %dx%d\n", w, h);
        fails++;
    }
    ScreenMirrorQualityToSize(5, w, h);
    if(w != 1920 || h != 1080)
    {
        std::fprintf(stderr, "quality 5 size %dx%d\n", w, h);
        fails++;
    }
    ScreenMirrorQualityToSize(6, w, h);
    if(w != 2560 || h != 1440)
    {
        std::fprintf(stderr, "quality 6 size %dx%d\n", w, h);
        fails++;
    }
    ScreenMirrorQualityToSize(7, w, h);
    if(w != 3840 || h != 2160)
    {
        std::fprintf(stderr, "quality 7 size %dx%d\n", w, h);
        fails++;
    }
    if(kScreenMirrorMaxWorkingWidth != 3840 || kScreenMirrorMaxWorkingHeight != 2160)
    {
        std::fprintf(stderr, "max working size %dx%d\n",
                     kScreenMirrorMaxWorkingWidth, kScreenMirrorMaxWorkingHeight);
        fails++;
    }

    w = kScreenMirrorMaxWorkingWidth;
    h = kScreenMirrorMaxWorkingHeight;
    ClampWorkingCaptureSize(1280, 720, w, h);
    if(w != 1280 || h != 720)
    {
        std::fprintf(stderr, "should not upscale past native, got %dx%d\n", w, h);
        fails++;
    }

    /* 2x2 BGRA -> 1x1 RGBA box average. */
    const uint8_t src[] = {
        10, 20, 30, 255,
        20, 40, 60, 255,
        30, 60, 90, 255,
        40, 80, 120, 255
    };
    uint8_t dst[4] = {0, 0, 0, 0};
    BoxDownscaleToRgba(src, 2, 2, 8, dst, 1, 1, 2, 1, 0, 3);
    if(dst[0] != 75 || dst[1] != 50 || dst[2] != 25 || dst[3] != 255)
    {
        std::fprintf(stderr, "2x2 box average got %u %u %u %u want 75 50 25 255\n",
                     dst[0], dst[1], dst[2], dst[3]);
        fails++;
    }

    /* 1:1 RGBA copy path. */
    uint8_t rgba[] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t rgba_out[8] = {};
    BoxDownscaleToRgba(rgba, 2, 1, 8, rgba_out, 2, 1, 0, 1, 2, 3);
    if(rgba_out[0] != 1 || rgba_out[7] != 8)
    {
        std::fprintf(stderr, "1:1 RGBA copy failed\n");
        fails++;
    }

    /* Checkerboard 4x4 -> 2x2 should average each 2x2 tile. */
    std::vector<uint8_t> checker((size_t)4 * 4 * 4, 0);
    for(int y = 0; y < 4; ++y)
    {
        for(int x = 0; x < 4; ++x)
        {
            uint8_t* px = checker.data() + ((size_t)y * 4 + (size_t)x) * 4;
            const uint8_t v = ((x / 2) == (y / 2)) ? 200 : 0;
            px[0] = v;
            px[1] = v;
            px[2] = v;
            px[3] = 255;
        }
    }
    uint8_t tiles[16] = {};
    BoxDownscaleToRgba(checker.data(), 4, 4, 16, tiles, 2, 2, 0, 1, 2, 3);
    if(tiles[0] != 200 || tiles[4] != 0 || tiles[8] != 0 || tiles[12] != 200)
    {
        std::fprintf(stderr, "4x4 checker box got %u %u %u %u\n",
                     tiles[0], tiles[4], tiles[8], tiles[12]);
        fails++;
    }

    /* 1:1 BGRA swizzle with padded stride. */
    const uint8_t bgra_pad[] = {
        10, 20, 30, 255, 40, 50, 60, 255, 0, 0, 0, 0, 0, 0, 0, 0
    };
    uint8_t rgba_pad[8] = {};
    BoxDownscaleToRgba(bgra_pad, 2, 1, 16, rgba_pad, 2, 1, 2, 1, 0, 3);
    if(rgba_pad[0] != 30 || rgba_pad[1] != 20 || rgba_pad[2] != 10 || rgba_pad[3] != 255 ||
       rgba_pad[4] != 60 || rgba_pad[5] != 50 || rgba_pad[6] != 40 || rgba_pad[7] != 255)
    {
        std::fprintf(stderr, "1:1 BGRA padded stride swizzle failed\n");
        fails++;
    }

    if(fails != 0)
    {
        std::fprintf(stderr, "screen_capture_downscale_check: %d fail(s)\n", fails);
        return 1;
    }
    std::printf("screen_capture_downscale_check: box-average, 1080/1440/4K sizes, native clamp OK\n");
    return 0;
}
