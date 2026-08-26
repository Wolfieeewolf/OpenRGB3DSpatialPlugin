// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "RGBController.h"
#include <algorithm>
#include <cmath>

inline RGBColor EffectHsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
        h += 1.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    const float hf = h * 6.0f;
    const int i = (int)std::floor(hf) % 6;
    const float f = hf - std::floor(hf);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);
    switch(i)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    const int ri = std::min(255, std::max(0, (int)std::lround(r * 255.0f)));
    const int gi = std::min(255, std::max(0, (int)std::lround(g * 255.0f)));
    const int bi = std::min(255, std::max(0, (int)std::lround(b * 255.0f)));
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}

inline RGBColor EffectLerpColor(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const int ar = a & 0xFF;
    const int ag = (a >> 8) & 0xFF;
    const int ab = (a >> 16) & 0xFF;
    const int br = b & 0xFF;
    const int bg = (b >> 8) & 0xFF;
    const int bb = (b >> 16) & 0xFF;
    const int r = (int)std::lround(ar + (br - ar) * t);
    const int g = (int)std::lround(ag + (bg - ag) * t);
    const int bl = (int)std::lround(ab + (bb - ab) * t);
    return (RGBColor)((bl << 16) | (g << 8) | r);
}
