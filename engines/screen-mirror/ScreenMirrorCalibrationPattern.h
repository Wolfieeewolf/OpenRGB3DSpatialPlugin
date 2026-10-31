// SPDX-License-Identifier: GPL-2.0-only
#ifndef SCREENMIRROR_CALIBRATION_PATTERN_H
#define SCREENMIRROR_CALIBRATION_PATTERN_H

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

/* 3D-friendly Screen Mirror calibration frame.
   Four saturated quadrant colors (R/G/B/Y) with a soft center→corner
   luminance falloff so brightness / threshold / Reach read clearly on
   room LEDs. No white rings, spokes, or grid — those wash out ambilight. */
inline void ScreenMirrorFillCalibrationPatternBuffer(std::vector<uint8_t>& rgba)
{
    constexpr int w = 512;
    constexpr int h = 512;

    rgba.resize((size_t)w * h * 4);
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const float u = (x + 0.5f) / (float)w;
            const float v = (y + 0.5f) / (float)h;
            const bool top = v < 0.5f;
            const bool left = u < 0.5f;

            float br = 96.0f;
            float bg = 96.0f;
            float bb = 96.0f;
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

            /* Soft radial luminance: bright near screen centre, darker toward
               corners — gives threshold / brightness something to bite without
               painting LEDs white. */
            const float ru = (u - 0.5f) * 2.0f;
            const float rv = (v - 0.5f) * 2.0f;
            const float radial = sqrtf(ru * ru + rv * rv);
            float lum = 1.0f;
            if(radial > 0.18f)
            {
                const float t = std::clamp((radial - 0.18f) / 1.05f, 0.0f, 1.0f);
                lum = 1.0f - 0.62f * (t * t * (3.0f - 2.0f * t));
            }
            br *= lum;
            bg *= lum;
            bb *= lum;

            /* Thin dark cross at midlines — orientation without white wash. */
            const float du = fabsf(u - 0.5f);
            const float dv = fabsf(v - 0.5f);
            const float cross_px_u = du * (float)w;
            const float cross_px_v = dv * (float)h;
            if(cross_px_u < 1.6f || cross_px_v < 1.6f)
            {
                br *= 0.18f;
                bg *= 0.18f;
                bb *= 0.18f;
            }

            /* Soft dark bezel so letterbox / edge mapping stays readable. */
            const float edge = std::min(std::min(u, 1.0f - u), std::min(v, 1.0f - v));
            if(edge < 0.02f)
            {
                const float e = std::clamp(edge / 0.02f, 0.0f, 1.0f);
                const float edge_mul = 0.22f + 0.78f * e;
                br *= edge_mul;
                bg *= edge_mul;
                bb *= edge_mul;
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
