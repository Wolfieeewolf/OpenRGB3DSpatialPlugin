// SPDX-License-Identifier: GPL-2.0-only

#include "StripKernelPatternPalettes.h"
#include "StripShellPatternKernels.h"
#include <algorithm>
#include <cmath>

namespace
{
RGBColor PackBGR(unsigned char r, unsigned char g, unsigned char b)
{
    return (RGBColor)((b << 16) | (g << 8) | r);
}

RGBColor HsvToBgr(float h_deg, float s, float v)
{
    h_deg = std::fmod(h_deg, 360.0f);
    if(h_deg < 0.0f)
        h_deg += 360.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h_deg / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rp = 0, gp = 0, bp = 0;
    if(h_deg < 60.0f)
    {
        rp = c;
        gp = x;
    }
    else if(h_deg < 120.0f)
    {
        rp = x;
        gp = c;
    }
    else if(h_deg < 180.0f)
    {
        gp = c;
        bp = x;
    }
    else if(h_deg < 240.0f)
    {
        gp = x;
        bp = c;
    }
    else if(h_deg < 300.0f)
    {
        rp = x;
        bp = c;
    }
    else
    {
        rp = c;
        bp = x;
    }
    return PackBGR((unsigned char)(255.0f * (rp + m)),
                   (unsigned char)(255.0f * (gp + m)),
                   (unsigned char)(255.0f * (bp + m)));
}

RGBColor LerpBGR(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    unsigned char ra = (unsigned char)(a & 0xFF), ga = (unsigned char)((a >> 8) & 0xFF), ba = (unsigned char)((a >> 16) & 0xFF);
    unsigned char rb = (unsigned char)(b & 0xFF), gb = (unsigned char)((b >> 8) & 0xFF), bb = (unsigned char)((b >> 16) & 0xFF);
    auto l = [t](unsigned char u, unsigned char v) -> unsigned char {
        return (unsigned char)(u + (float)(v - u) * t);
    };
    return PackBGR(l(ra, rb), l(ga, gb), l(ba, bb));
}

float Hash01(float x)
{
    float f = std::sin(x * 12.9898f) * 43758.547f;
    return f - std::floor(f);
}
} // namespace

RGBColor SampleKernelPatternPalette(int kernel_id, float p01, float time_sec)
{
    p01 = std::fmod(p01, 1.0f);
    if(p01 < 0.0f)
        p01 += 1.0f;
    const StripShellKernel k = static_cast<StripShellKernel>(StripShellKernelClamp(kernel_id));

    switch(k)
    {
    case StripShellKernel::FireSimple:
    case StripShellKernel::FireLayered:
    {
        RGBColor black = PackBGR(0, 0, 0);
        RGBColor red = PackBGR(255, 24, 0);
        RGBColor orange = PackBGR(255, 120, 0);
        RGBColor yellow = PackBGR(255, 220, 40);
        RGBColor white = PackBGR(255, 245, 220);
        if(p01 < 0.25f)
            return LerpBGR(black, red, p01 / 0.25f);
        if(p01 < 0.5f)
            return LerpBGR(red, orange, (p01 - 0.25f) / 0.25f);
        if(p01 < 0.78f)
            return LerpBGR(orange, yellow, (p01 - 0.5f) / 0.28f);
        return LerpBGR(yellow, white, (p01 - 0.78f) / 0.22f);
    }
    case StripShellKernel::OceanDriftLite:
    {
        RGBColor deep = PackBGR(8, 40, 120);
        RGBColor mid = PackBGR(30, 140, 200);
        RGBColor foam = PackBGR(180, 230, 255);
        if(p01 < 0.55f)
            return LerpBGR(deep, mid, p01 / 0.55f);
        return LerpBGR(mid, foam, (p01 - 0.55f) / 0.45f);
    }
    case StripShellKernel::MatrixRain1D:
    {
        float g = 40.0f + 215.0f * p01;
        return PackBGR((unsigned char)(g * 0.15f), (unsigned char)g, (unsigned char)(g * 0.25f));
    }
    case StripShellKernel::HalloweenFlicker:
        return LerpBGR(PackBGR(255, 80, 0), PackBGR(140, 0, 200), 0.35f + 0.65f * p01);
    case StripShellKernel::Cylon:
        return LerpBGR(PackBGR(40, 0, 0), PackBGR(255, 20, 10), std::pow(p01, 2.2f));
    case StripShellKernel::CandleSoft:
        return LerpBGR(PackBGR(255, 120, 40), PackBGR(255, 230, 160), p01);
    case StripShellKernel::Meteor:
        return LerpBGR(PackBGR(255, 200, 80), PackBGR(40, 20, 80), 1.0f - p01);
    case StripShellKernel::SparkleDark:
    case StripShellKernel::TwinkleSparse:
    case StripShellKernel::GlitterBurst:
    case StripShellKernel::Confetti:
    {
        float hue = std::fmod(p01 * 360.0f + Hash01(p01 * 17.3f + time_sec) * 80.0f, 360.0f);
        return HsvToBgr(hue, 0.85f, 0.2f + 0.8f * p01);
    }
    case StripShellKernel::TricolorChase:
    {
        float u = p01 * 3.0f;
        int band = (int)std::floor(u) % 3;
        float f = u - std::floor(u);
        RGBColor c0 = PackBGR(255, 0, 0);
        RGBColor c1 = PackBGR(0, 255, 0);
        RGBColor c2 = PackBGR(0, 0, 255);
        if(band == 0)
            return LerpBGR(c0, c1, f);
        if(band == 1)
            return LerpBGR(c1, c2, f);
        return LerpBGR(c2, c0, f);
    }
    case StripShellKernel::TriFade:
    {
        float u = p01 * 3.0f;
        int band = (int)std::floor(u) % 3;
        float f = u - std::floor(u);
        RGBColor a = PackBGR(255, 0, 120);
        RGBColor b = PackBGR(0, 200, 255);
        RGBColor c = PackBGR(255, 220, 0);
        if(band == 0)
            return LerpBGR(a, b, f);
        if(band == 1)
            return LerpBGR(b, c, f);
        return LerpBGR(c, a, f);
    }
    case StripShellKernel::TheaterChase:
    case StripShellKernel::RunningLight:
        return LerpBGR(PackBGR(0, 0, 0), PackBGR(255, 255, 255), (std::sin(p01 * 6.2831853f) * 0.5f + 0.5f));
    case StripShellKernel::SpectrumWaves:
    case StripShellKernel::HueBounceDot:
    case StripShellKernel::ColorWave:
    case StripShellKernel::PlasmaSinProduct:
        return HsvToBgr(p01 * 360.0f + time_sec * 25.0f, 0.95f, 1.0f);
    case StripShellKernel::BpmPulse:
        return LerpBGR(PackBGR(80, 0, 120), PackBGR(255, 220, 60), p01);
    default:
        return HsvToBgr(p01 * 360.0f + time_sec * 8.0f, 0.92f, 1.0f);
    }
}
