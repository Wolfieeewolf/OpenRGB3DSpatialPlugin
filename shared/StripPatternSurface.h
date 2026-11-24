// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef STRIPPATTERNSURFACE_H
#define STRIPPATTERNSURFACE_H

#include <algorithm>
#include <cmath>

namespace StripPatternSurface
{

enum class UnfoldMode : int
{
    AlongX = 0,
    AlongY,
    AlongZ,
    PlaneXZ,
    RadialXZ,
    DiagonalXYZ,
    Manhattan01,
    EffectPhaseOnly,
    StaticRoomPlane,
    COUNT
};

inline constexpr float Pi()
{
    return 3.14159265358979323846f;
}

/** Perpendicular-ish 0..1 for 2D kernel pilots (paired with StripCoord01). */
inline float CrossCoord01(float lx, float ly, float lz, UnfoldMode mode, float dir_deg)
{
    (void)ly; /* Cross uses XZ / phase axes for current unfold modes. */
    float t = 0.5f;
    switch(mode)
    {
    case UnfoldMode::AlongX:
        t = 0.5f + 0.5f * std::tanh(lz);
        break;
    case UnfoldMode::AlongY:
        t = 0.5f + 0.5f * std::tanh(lx);
        break;
    case UnfoldMode::AlongZ:
        t = 0.5f + 0.5f * std::tanh(lx);
        break;
    case UnfoldMode::PlaneXZ:
    {
        float r = dir_deg * (Pi() / 180.0f);
        float w = -std::sin(r) * lx + std::cos(r) * lz;
        t = std::clamp(0.5f + 0.35f * w, 0.0f, 1.0f);
        break;
    }
    case UnfoldMode::RadialXZ:
        t = std::clamp(0.5f + 0.5f * std::tanh(std::sqrt(lx * lx + lz * lz) - 0.5f), 0.0f, 1.0f);
        break;
    case UnfoldMode::DiagonalXYZ:
        t = 0.5f + 0.5f * std::tanh((lx - lz) * 0.5f);
        break;
    case UnfoldMode::Manhattan01:
        t = std::clamp((std::fabs(lx) + std::fabs(lz)) * 0.5f, 0.0f, 1.0f);
        break;
    case UnfoldMode::EffectPhaseOnly:
    case UnfoldMode::StaticRoomPlane:
    case UnfoldMode::COUNT:
        t = 0.5f;
        break;
    }
    return std::clamp(t, 0.0f, 1.0f);
}

/** Third axis / height for 3D-feel pilots. */
inline float UpCoord01(float lx, float ly, float lz, UnfoldMode mode)
{
    (void)lx;
    (void)lz;
    if(mode == UnfoldMode::AlongY)
    {
        return std::clamp(0.5f + 0.5f * std::tanh(lz), 0.0f, 1.0f);
    }
    return std::clamp(0.5f + 0.5f * std::tanh(ly), 0.0f, 1.0f);
}

inline float StripCoord01(float lx, float ly, float lz, UnfoldMode mode, float dir_deg)
{
    float s = 0.5f;
    switch(mode)
    {
    case UnfoldMode::AlongX:
        s = 0.5f + 0.5f * std::tanh(lx);
        break;
    case UnfoldMode::AlongY:
        s = 0.5f + 0.5f * std::tanh(ly);
        break;
    case UnfoldMode::AlongZ:
        s = 0.5f + 0.5f * std::tanh(lz);
        break;
    case UnfoldMode::PlaneXZ:
    {
        float r = dir_deg * (Pi() / 180.0f);
        float w = std::cos(r) * lx + std::sin(r) * lz;
        s = std::clamp(0.5f + 0.35f * w, 0.0f, 1.0f);
        break;
    }
    case UnfoldMode::RadialXZ:
    {
        float ang = std::atan2(lz, lx);
        if(ang < 0.0f)
            ang += 2.0f * Pi();
        s = ang / (2.0f * Pi());
        if(s >= 1.0f)
            s -= 1.0f;
        if(s < 0.0f)
            s += 1.0f;
        return s;
    }
    case UnfoldMode::DiagonalXYZ:
        s = 0.5f + 0.5f * std::tanh((lx + ly + lz) / 3.0f);
        break;
    case UnfoldMode::Manhattan01:
        s = std::clamp((std::fabs(lx) + std::fabs(ly) + std::fabs(lz)) / 3.0f, 0.0f, 1.0f);
        break;
    case UnfoldMode::EffectPhaseOnly:
    case UnfoldMode::StaticRoomPlane:
    case UnfoldMode::COUNT:
        break;
    }
    return std::clamp(s, 0.0f, 1.0f);
}

}

#endif
