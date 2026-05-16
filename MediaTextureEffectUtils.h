// SPDX-License-Identifier: GPL-2.0-only

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

inline int BilinearChannelSample(int a00, int a10, int a01, int a11, float ttx, float tty)
{
    const float top = (float)a00 * (1.0f - ttx) + (float)a10 * ttx;
    const float bot = (float)a01 * (1.0f - ttx) + (float)a11 * ttx;
    return (int)(top * (1.0f - tty) + bot * tty + 0.5f);
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

    const int r = BilinearChannelSample(qRed(p00), qRed(p10), qRed(p01), qRed(p11), tx, ty);
    const int g = BilinearChannelSample(qGreen(p00), qGreen(p10), qGreen(p01), qGreen(p11), tx, ty);
    const int b = BilinearChannelSample(qBlue(p00), qBlue(p10), qBlue(p01), qBlue(p11), tx, ty);
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

inline void RgbBytesToHsv(float r, float g, float b, float& h_deg, float& s, float& v)
{
    r *= (1.0f / 255.0f);
    g *= (1.0f / 255.0f);
    b *= (1.0f / 255.0f);
    const float mx = std::max({r, g, b});
    const float mn = std::min({r, g, b});
    const float d = mx - mn;
    v = mx;
    s = mx < 1e-5f ? 0.0f : d / mx;
    if(d < 1e-5f)
    {
        h_deg = 0.0f;
        return;
    }
    float h = 0.0f;
    if(mx == r)
    {
        h = 60.0f * std::fmod((g - b) / d, 6.0f);
    }
    else if(mx == g)
    {
        h = 60.0f * (((b - r) / d) + 2.0f);
    }
    else
    {
        h = 60.0f * (((r - g) / d) + 4.0f);
    }
    if(h < 0.0f)
    {
        h += 360.0f;
    }
    h_deg = h;
}

inline RGBColor HsvToRgbBytes(float h_deg, float s, float v)
{
    h_deg = std::fmod(h_deg, 360.0f);
    if(h_deg < 0.0f)
    {
        h_deg += 360.0f;
    }
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h_deg / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float rp = 0.0f;
    float gp = 0.0f;
    float bp = 0.0f;
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
    const int r = (int)std::clamp((rp + m) * 255.0f + 0.5f, 0.0f, 255.0f);
    const int g = (int)std::clamp((gp + m) * 255.0f + 0.5f, 0.0f, 255.0f);
    const int bl = (int)std::clamp((bp + m) * 255.0f + 0.5f, 0.0f, 255.0f);
    return ToRGBColor(r, g, bl);
}
}

#endif
