// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialPatternPalettes.h"
#include "SpatialPatternKernels.h"
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

RGBColor TonePairByKernel(int kernel_id, float p01)
{
    static const RGBColor kPairs[][2] = {
        {PackBGR(24, 120, 220), PackBGR(180, 230, 255)}, // blue sky
        {PackBGR(80, 30, 160), PackBGR(220, 140, 255)},  // violet neon
        {PackBGR(0, 130, 110), PackBGR(140, 230, 180)},  // teal mint
        {PackBGR(180, 70, 20), PackBGR(255, 210, 120)},  // amber
        {PackBGR(140, 20, 70), PackBGR(255, 150, 200)},  // magenta rose
        {PackBGR(40, 90, 170), PackBGR(180, 200, 255)},  // steel blue
    };
    constexpr int kCount = (int)(sizeof(kPairs) / sizeof(kPairs[0]));
    int idx = SpatialPatternKernelClamp(kernel_id) % kCount;
    return LerpBGR(kPairs[idx][0], kPairs[idx][1], p01);
}
} // namespace

RGBColor SampleKernelPatternPalette(int kernel_id, float p01, float time_sec)
{
    p01 = std::fmod(p01, 1.0f);
    if(p01 < 0.0f)
        p01 += 1.0f;
    const SpatialPatternKernel k = static_cast<SpatialPatternKernel>(SpatialPatternKernelClamp(kernel_id));

    switch(k)
    {
    case SpatialPatternKernel::FireSimple:
    case SpatialPatternKernel::FireLayered:
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
    case SpatialPatternKernel::OceanDriftLite:
    {
        RGBColor deep = PackBGR(8, 40, 120);
        RGBColor mid = PackBGR(30, 140, 200);
        RGBColor foam = PackBGR(180, 230, 255);
        if(p01 < 0.55f)
            return LerpBGR(deep, mid, p01 / 0.55f);
        return LerpBGR(mid, foam, (p01 - 0.55f) / 0.45f);
    }
    case SpatialPatternKernel::MatrixRain1D:
    {
        float g = 40.0f + 215.0f * p01;
        return PackBGR((unsigned char)(g * 0.15f), (unsigned char)g, (unsigned char)(g * 0.25f));
    }
    case SpatialPatternKernel::HalloweenFlicker:
        return LerpBGR(PackBGR(255, 80, 0), PackBGR(140, 0, 200), 0.35f + 0.65f * p01);
    case SpatialPatternKernel::Cylon:
        return LerpBGR(PackBGR(40, 0, 0), PackBGR(255, 20, 10), std::pow(p01, 2.2f));
    case SpatialPatternKernel::CandleSoft:
        return LerpBGR(PackBGR(255, 120, 40), PackBGR(255, 230, 160), p01);
    case SpatialPatternKernel::Meteor:
        return LerpBGR(PackBGR(255, 200, 80), PackBGR(40, 20, 80), 1.0f - p01);
    case SpatialPatternKernel::SparkleDark:
    case SpatialPatternKernel::TwinkleSparse:
    case SpatialPatternKernel::GlitterBurst:
    case SpatialPatternKernel::Confetti:
    {
        float hue = std::fmod(p01 * 220.0f + Hash01(p01 * 17.3f + time_sec) * 45.0f, 360.0f);
        return HsvToBgr(hue, 0.65f, 0.25f + 0.7f * p01);
    }
    case SpatialPatternKernel::TricolorChase:
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
    case SpatialPatternKernel::TriFade:
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
    case SpatialPatternKernel::TheaterChase:
    case SpatialPatternKernel::RunningLight:
        return LerpBGR(PackBGR(0, 0, 0), PackBGR(255, 255, 255), (std::sin(p01 * 6.2831853f) * 0.5f + 0.5f));
    case SpatialPatternKernel::SpectrumWaves:
    case SpatialPatternKernel::ColorWave:
    case SpatialPatternKernel::PlasmaSinProduct:
        return HsvToBgr(p01 * 240.0f + time_sec * 14.0f, 0.72f, 0.92f);
    case SpatialPatternKernel::HueBounceDot:
    case SpatialPatternKernel::DotBounce:
    {
        RGBColor deep = PackBGR(160, 30, 120);
        RGBColor hot  = PackBGR(255, 140, 230);
        return LerpBGR(deep, hot, std::pow(p01, 0.85f));
    }
    case SpatialPatternKernel::BpmPulse:
        return LerpBGR(PackBGR(80, 0, 120), PackBGR(255, 220, 60), p01);
    default:
        return TonePairByKernel(kernel_id, p01);
    }
}
