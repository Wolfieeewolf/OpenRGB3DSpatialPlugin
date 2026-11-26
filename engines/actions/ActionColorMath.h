// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* Action Editor colour compositing in linear light.

   Effects hand over display (sRGB-ish) colours; stacking them is done on linear light
   in float, the way light actually adds, and only the finished mix is turned back into
   an 8-bit display value. That keeps fades smooth through many stacked layers and stops
   Add / Screen from clipping early. Dependency-free so it can be tested on its own. */

#include <algorithm>
#include <cmath>

namespace Action
{
namespace colormath
{

/** Blend modes, matching ClipBlend's order. */
enum Mode : int
{
    kNormal = 0,
    kAdd = 1,
    kScreen = 2,
    kLighten = 3,
    kMultiply = 4,
    kDarken = 5,
};

inline float SrgbToLinear(float c)
{
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline float LinearToSrgb(float l)
{
    l = std::clamp(l, 0.0f, 1.0f);
    return l <= 0.0031308f ? l * 12.92f : 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f;
}

/** 8-bit display value -> linear light (table). */
inline float Srgb8ToLinear(int v)
{
    static float table[256];
    static bool ready = false;
    if(!ready)
    {
        for(int i = 0; i < 256; ++i)
        {
            table[i] = SrgbToLinear((float)i / 255.0f);
        }
        ready = true;
    }
    return table[std::clamp(v, 0, 255)];
}

/** Linear light -> nearest 8-bit display value (4096-step table, exact to the byte). */
inline int LinearToSrgb8(float l)
{
    constexpr int kSteps = 4096;
    static unsigned char table[kSteps + 1];
    static bool ready = false;
    if(!ready)
    {
        for(int i = 0; i <= kSteps; ++i)
        {
            table[i] = (unsigned char)std::lround(LinearToSrgb((float)i / (float)kSteps) * 255.0f);
        }
        ready = true;
    }
    if(!(l > 0.0f))
    {
        return 0;
    }
    if(l >= 1.0f)
    {
        return 255;
    }
    /* The table is coarse near black where the curve is steep: compute those directly. */
    if(l < 0.02f)
    {
        return (int)std::lround(LinearToSrgb(l) * 255.0f);
    }
    return table[(int)std::lround(l * (float)kSteps)];
}

/** Add with a highlight roll-off: below the knee light adds exactly; above it the sum
    eases toward full instead of clipping flat, so stacked hits keep shape and colour. */
inline float AddSoft(float a, float b)
{
    constexpr float kKnee = 0.75f;
    const float s = a + b;
    if(s <= kKnee)
    {
        return s;
    }
    const float room = 1.0f - kKnee;
    return kKnee + room * (1.0f - std::exp(-(s - kKnee) / room));
}

/** One channel of `top` over `below` (both linear light, 0..1). */
inline float Blend(int mode, float below, float top)
{
    switch(mode)
    {
        case kAdd:      return AddSoft(below, top);
        case kScreen:   return below + top - below * top;
        case kLighten:  return std::max(below, top);
        case kMultiply: return below * top;
        case kDarken:   return std::min(below, top);
        case kNormal:
        default:        return top;
    }
}

} // namespace colormath
} // namespace Action
