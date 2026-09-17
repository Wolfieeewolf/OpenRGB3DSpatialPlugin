// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"
#include <algorithm>
#include <cmath>

namespace EffectPack
{
namespace detail
{

inline RGBColor ScaleIntensity(RGBColor c, float intensity)
{
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    return ToRGBColor(
        (int)std::lround(RGBGetRValue(c) * intensity),
        (int)std::lround(RGBGetGValue(c) * intensity),
        (int)std::lround(RGBGetBValue(c) * intensity));
}

inline RGBColor LerpColor(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return ToRGBColor(
        (int)std::lround(RGBGetRValue(a) * u + RGBGetRValue(b) * t),
        (int)std::lround(RGBGetGValue(a) * u + RGBGetGValue(b) * t),
        (int)std::lround(RGBGetBValue(a) * u + RGBGetBValue(b) * t));
}

inline float AxisPos(Direction dir, int led_index, int led_count)
{
    if(led_count <= 1)
    {
        return 0.0f;
    }
    const float t = (float)led_index / (float)(led_count - 1);
    return DirectionInvertsAxis(dir) ? (1.0f - t) : t;
}

inline float NormOnAxis(float v, float vmin, float vmax, bool invert)
{
    const float span = vmax - vmin;
    if(span <= 1e-5f)
    {
        return invert ? 1.0f : 0.0f;
    }
    float t = std::clamp((v - vmin) / span, 0.0f, 1.0f);
    return invert ? (1.0f - t) : t;
}

inline unsigned int HashLed(int led_index, int local_ms, int period)
{
    unsigned int x = (unsigned int)(led_index * 374761393u + (local_ms / std::max(1, period)) * 668265263u);
    x = (x ^ (x >> 13)) * 1274126177u;
    return x ^ (x >> 16);
}

} // namespace detail
} // namespace EffectPack
