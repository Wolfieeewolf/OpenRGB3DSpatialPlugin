// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** Signed strip kernel eval for strip + volume assists (kernels 0..max).
 *  Compact GLSL 1.10 body — a 43-way if/else plus non-constant loop breaks
 *  fails to compile on many Windows drivers and latches the volume assist off.
 *  Families still change with kid; CPU EvalSpatialPatternKernel keeps full looks
 *  for colormap / fallback.
 */
inline constexpr int kSpatialStripGpuKernelMaxId = 43;

inline const char* SpatialStripKernelEvalGlsl()
{
    return R"(
float fractf(float x) { return x - floor(x); }
float hash11(float x) { return fractf(sin(x * 12.9898) * 43758.547); }
float smstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
float satTanh(float x)
{
    float e = exp(clamp(2.0 * x, -20.0, 20.0));
    return (e - 1.0) / (e + 1.0);
}
float evalStripKernelSigned(int kid, float s01, float phase01, float repeats, float time_sec)
{
    if(abs(time_sec) < 1e-8)
        time_sec = u_time;
    float rep = max(repeats, 1.0);
    float tsec = time_sec * 0.35;
    float ph = fractf(phase01 * 0.35 + time_sec * 0.08);
    float r = 1.0 + (rep - 1.0) * 0.72;
    float u_phase = fractf(s01 * r + ph + 1000.0);
    float TWO_PI = 6.2831853;
    float k = sin(TWO_PI * u_phase);

    /* 8 visible families from kid — keep this short so GLSL 1.10 compilers succeed. */
    float fam = floor(mod(float(kid) + 0.01, 8.0));
    if(fam < 0.5)
        k = sin(TWO_PI * u_phase);
    else if(fam < 1.5)
        k = 2.0 * u_phase - 1.0;
    else if(fam < 2.5)
        k = (1.0 - abs(2.0 * u_phase - 1.0)) * 2.0 - 1.0;
    else if(fam < 3.5)
        k = (u_phase < 0.5) ? 1.0 : -1.0;
    else if(fam < 4.5)
    {
        float u = fractf(s01 * r * 3.0 + ph);
        k = smstep(0.0, 0.12, u) * (1.0 - smstep(0.88, 1.0, u)) * 2.0 - 1.0;
    }
    else if(fam < 5.5)
        k = pow(1.0 - u_phase, 2.2) * 2.0 - 1.0;
    else if(fam < 6.5)
    {
        float t = s01 * r * 28.0 + ph * 11.0;
        float tw = fractf(t);
        float h = hash11(floor(t) * 0.031 + 9.1);
        float bright = (h > 0.72) ? 1.0 : -0.65;
        float decay = max(0.0, 1.0 - tw * 1.8);
        k = bright * decay + (-0.65) * (1.0 - decay);
    }
    else
    {
        float t = s01 * r * 5.0 + ph * 2.0;
        float i = floor(t);
        float f = t - i;
        float a = hash11(i);
        float b = hash11(i + 1.0);
        float s = f * f * (3.0 - 2.0 * f);
        k = (a + (b - a) * s) * 2.0 - 1.0;
    }
    return clamp(k, -1.0, 1.0);
}
)";
}
