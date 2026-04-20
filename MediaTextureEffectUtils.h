// SPDX-License-Identifier: GPL-2.0-only
//
// Shared helpers for texture / GIF based spatial effects (bilinear sample, ambience, blending).

#ifndef MEDIATEXTUREEFFECTUTILS_H
#define MEDIATEXTUREEFFECTUTILS_H

#include "RGBController.h"

#include <QImage>
#include <QRgb>

#include <algorithm>
#include <cmath>

namespace MediaTextureEffect
{
inline float Frac01(float x)
{
    return x - std::floor(x);
}

inline float Smoothstep(float edge0, float edge1, float x)
{
    const float t = std::clamp((x - edge0) / std::max(1e-5f, edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float AmbienceGain(float dist_origin, float max_r, float d_face,
                          unsigned int dist_pct, unsigned int curve_pct, unsigned int edge_pct)
{
    float g = 1.0f;
    const float fd = dist_pct / 100.0f;
    const float c = curve_pct / 100.0f;
    const float es = edge_pct / 100.0f;

    if(max_r > 1e-4f)
    {
        const float t = std::min(1.0f, dist_origin / max_r);
        const float dim_eff = std::clamp(fd + (1.0f - fd) * c * 0.70f, 0.0f, 1.0f);
        if(dim_eff > 1e-4f)
        {
            const float linear = 1.0f - t * (0.03f + 0.97f * dim_eff);
            const float exponent = 1.0f + 4.0f * c;
            g *= std::pow(std::clamp(linear, 0.0f, 1.0f), exponent);
        }
    }
    if(es > 1e-4f)
    {
        const float feather = 0.10f + 0.78f * es;
        g *= Smoothstep(0.0f, feather, d_face);
    }
    return std::clamp(g, 0.0f, 1.0f);
}

inline RGBColor SampleImageBilinear(const QImage& img, float u, float v)
{
    if(img.isNull() || img.width() < 1 || img.height() < 1)
    {
        return 0x00000000;
    }
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    const int w = img.width();
    const int h = img.height();
    if(w == 1 && h == 1)
    {
        const QRgb p = img.pixel(0, 0);
        return ToRGBColor(qRed(p), qGreen(p), qBlue(p));
    }

    const float fx = u * (float)(w - 1);
    const float fy = (1.0f - v) * (float)(h - 1);
    const int x0 = (int)std::floor(fx);
    const int y0 = (int)std::floor(fy);
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;

    const QRgb p00 = img.pixel(x0, y0);
    const QRgb p10 = img.pixel(x1, y0);
    const QRgb p01 = img.pixel(x0, y1);
    const QRgb p11 = img.pixel(x1, y1);

    auto ch = [](int a00, int a10, int a01, int a11, float ttx, float tty) -> int {
        const float top = (float)a00 * (1.0f - ttx) + (float)a10 * ttx;
        const float bot = (float)a01 * (1.0f - ttx) + (float)a11 * ttx;
        return (int)(top * (1.0f - tty) + bot * tty + 0.5f);
    };

    const int r = ch(qRed(p00), qRed(p10), qRed(p01), qRed(p11), tx, ty);
    const int g = ch(qGreen(p00), qGreen(p10), qGreen(p01), qGreen(p11), tx, ty);
    const int b = ch(qBlue(p00), qBlue(p10), qBlue(p01), qBlue(p11), tx, ty);
    return ToRGBColor(r, g, b);
}

inline RGBColor LerpRGB(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const int r = (int)(RGBGetRValue(a) * (1.0f - t) + RGBGetRValue(b) * t + 0.5f);
    const int g = (int)(RGBGetGValue(a) * (1.0f - t) + RGBGetGValue(b) * t + 0.5f);
    const int bl = (int)(RGBGetBValue(a) * (1.0f - t) + RGBGetBValue(b) * t + 0.5f);
    return ToRGBColor(std::min(255, r), std::min(255, g), std::min(255, bl));
}
}

#endif
