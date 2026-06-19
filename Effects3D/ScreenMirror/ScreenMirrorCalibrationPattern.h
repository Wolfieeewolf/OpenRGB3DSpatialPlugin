// SPDX-License-Identifier: GPL-2.0-only
#ifndef SCREENMIRROR_CALIBRATION_PATTERN_H
#define SCREENMIRROR_CALIBRATION_PATTERN_H

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline void ScreenMirrorFillCalibrationPatternBuffer(std::vector<uint8_t>& rgba)
{
    constexpr int w = 512;
    constexpr int h = 512;
    const float cx = (w - 1) * 0.5f;
    const float cy = (h - 1) * 0.5f;
    const int minor_step = std::max(8, w / 32);
    const int major_step = std::max(minor_step * 2, w / 8);

    rgba.resize((size_t)w * h * 4);
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const float u = (x + 0.5f) / (float)w;
            const float v = (y + 0.5f) / (float)h;
            const bool top = v < 0.5f;
            const bool left = u < 0.5f;
            float br = 96.0f, bg = 96.0f, bb = 96.0f;
            if(top && left)
            {
                br = 230.0f;
                bg = 55.0f;
                bb = 55.0f;
            }
            else if(top && !left)
            {
                br = 55.0f;
                bg = 230.0f;
                bb = 70.0f;
            }
            else if(!top && !left)
            {
                br = 60.0f;
                bg = 90.0f;
                bb = 240.0f;
            }
            else
            {
                br = 240.0f;
                bg = 220.0f;
                bb = 45.0f;
            }

            const int cx64 = (x * 48) / w;
            const int cy64 = (y * 48) / h;
            const float chk = (((cx64 ^ cy64) & 1) != 0) ? 1.0f : 0.78f;
            br *= chk;
            bg *= chk;
            bb *= chk;

            const float dx = (float)x - cx;
            const float dy = (float)y - cy;
            const float dist = sqrtf(dx * dx + dy * dy);
            const float max_ring_r = (float)std::min(w, h) * 0.48f;
            for(int ring_i = 1; ring_i <= 7; ring_i++)
            {
                const float ring_r = (ring_i / 8.0f) * max_ring_r;
                if(fabsf(dist - ring_r) < 1.35f)
                {
                    br = bg = bb = 245.0f;
                    break;
                }
            }

            const int spoke_count = 16;
            const float ang = atan2f(dy, dx);
            const float spoke_phase = ang * ((float)spoke_count / (2.0f * (float)M_PI));
            const float sp = spoke_phase - floorf(spoke_phase);
            if(dist > (float)std::min(w, h) * 0.04f && sp < 0.018f)
            {
                br = std::min(255.0f, br + 90.0f);
                bg = std::min(255.0f, bg + 90.0f);
                bb = std::min(255.0f, bb + 90.0f);
            }

            const bool minor_line = (x % minor_step) < 1 || (y % minor_step) < 1;
            const bool major_line = (x % major_step) < 3 || (y % major_step) < 3;
            if(minor_line && !major_line)
            {
                br *= 0.35f;
                bg *= 0.35f;
                bb *= 0.35f;
            }
            if(major_line)
            {
                br = bg = bb = 252.0f;
            }

            if(dist < 4.0f)
            {
                br = bg = bb = 255.0f;
            }

            const size_t idx = ((size_t)y * (size_t)w + (size_t)x) * 4u;
            rgba[idx + 0] = (uint8_t)std::clamp((int)std::lround(br), 0, 255);
            rgba[idx + 1] = (uint8_t)std::clamp((int)std::lround(bg), 0, 255);
            rgba[idx + 2] = (uint8_t)std::clamp((int)std::lround(bb), 0, 255);
            rgba[idx + 3] = 255;
        }
    }
}

constexpr int kScreenMirrorCalibrationPatternW = 512;
constexpr int kScreenMirrorCalibrationPatternH = 512;

#endif
