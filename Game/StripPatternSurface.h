// SPDX-License-Identifier: GPL-2.0-only

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
        s = std::fmod(0.35f * w + 0.5f + 1000.0f, 1.0f);
        break;
    }
    case UnfoldMode::RadialXZ:
    {
        float ang = std::atan2(lz, lx);
        if(ang < 0.0f)
            ang += 2.0f * Pi();
        s = ang / (2.0f * Pi());
        break;
    }
    case UnfoldMode::DiagonalXYZ:
        s = 0.5f + 0.5f * std::tanh((lx + ly + lz) / 3.0f);
        break;
    case UnfoldMode::Manhattan01:
    {
        float m = (std::fabs(lx) + std::fabs(ly) + std::fabs(lz)) * 0.5f;
        s = std::fmod(m + 0.5f, 1.0f);
        break;
    }
    default:
        break;
    }
    if(s < 0.0f)
        s += 1.0f;
    if(s >= 1.0f)
        s = std::fmod(s, 1.0f);
    return s;
}

inline float ShellIntensityGaussianY(float ly, float surface_y, float sigma, float amp)
{
    float d = std::fabs(ly - surface_y);
    float sig = std::max(sigma, 0.02f);
    float d_cut = 3.0f * sig * std::max(1.0f, amp);
    if(d > d_cut)
        return 0.0f;
    float g = std::exp(-(d * d) / (sig * sig));
    return std::min(1.0f, g);
}

}

#endif
