// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Room unfold + Y-shell helpers matching Game/StripPatternSurface.h +
 *  SpatialKernelColormap SampleStripKernelPalette01 (modes 0-8).
 */
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
    else if(unfold_mode == 3 || unfold_mode == 8)
    {
        /* 3 = PlaneXZ; 8 = StaticRoomPlane (same spatial map; phase freeze is elsewhere). */
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
        /* 7 EffectPhaseOnly needs phase/time — use stripUnfoldKernelInputs. */
        s = 0.5;

    if(s < 0.0)
        s += 1.0;
    if(s >= 1.0)
        s = fractf(s);
    return s;
}

/* Matches SampleStripKernelPalette01 unfold/phase/time handling.
 * Returns vec3(s01, phase_eff, time_eff). */
vec3 stripUnfoldKernelInputs(float lx, float ly, float lz, int unfold_mode, float dir_deg,
                             float phase01, float time_sec)
{
    float s = 0.5;
    float phase_eff = phase01;
    float time_eff = time_sec;
    if(unfold_mode == 7)
    {
        s = fractf(phase01 + time_sec * 0.12 + 1000.0);
    }
    else if(unfold_mode == 8)
    {
        s = stripUnfoldCoord01(lx, ly, lz, 3, dir_deg);
        phase_eff = 0.0;
        time_eff = 0.0;
    }
    else
    {
        s = stripUnfoldCoord01(lx, ly, lz, unfold_mode, dir_deg);
    }
    return vec3(s, phase_eff, time_eff);
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
