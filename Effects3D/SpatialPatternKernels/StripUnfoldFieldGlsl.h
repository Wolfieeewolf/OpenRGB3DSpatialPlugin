// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Room unfold + Y-shell helpers matching Game/StripPatternSurface.h (modes 0-6). */
inline const char* StripUnfoldFieldGlsl()
{
    return R"(
const float STRIP_PI = 3.14159265;
const float STRIP_TWO_PI = 6.2831853;

/* GLSL 1.10 has no tanh. */
float stripSatTanh(float x)
{
    float e = exp(clamp(2.0 * x, -20.0, 20.0));
    return (e - 1.0) / (e + 1.0);
}

float stripUnfoldCoord01(float lx, float ly, float lz, int unfold_mode, float dir_deg)
{
    float s = 0.5;
    if(unfold_mode == 0)
        s = 0.5 + 0.5 * stripSatTanh(lx);
    else if(unfold_mode == 1)
        s = 0.5 + 0.5 * stripSatTanh(ly);
    else if(unfold_mode == 2)
        s = 0.5 + 0.5 * stripSatTanh(lz);
    else if(unfold_mode == 3)
    {
        float r = dir_deg * (STRIP_PI / 180.0);
        float w = cos(r) * lx + sin(r) * lz;
        s = fractf(0.35 * w + 0.5 + 1000.0);
    }
    else if(unfold_mode == 4)
    {
        float ang = atan(lz, lx);
        if(ang < 0.0)
            ang += STRIP_TWO_PI;
        s = ang / STRIP_TWO_PI;
    }
    else if(unfold_mode == 5)
        s = 0.5 + 0.5 * stripSatTanh((lx + ly + lz) / 3.0);
    else if(unfold_mode == 6)
        s = fractf((abs(lx) + abs(ly) + abs(lz)) * 0.5 + 0.5);
    else
        s = 0.5;

    if(s < 0.0)
        s += 1.0;
    if(s >= 1.0)
        s = fractf(s);
    return s;
}

float shellIntensityGaussian(float ly, float surface_y, float sigma, float amp)
{
    float d = abs(ly - surface_y);
    float sig = max(sigma, 0.02);
    float d_cut = 3.0 * sig * max(1.0, amp);
    if(d > d_cut)
        return 0.0;
    float g = exp(-(d * d) / (sig * sig));
    return min(1.0, g);
}
)";
}
