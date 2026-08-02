// SPDX-License-Identifier: GPL-2.0-only
#pragma once

/** 1D strip pattern kernels (signed encoded as (k+1)/2 in R).
 *  u_params: [0]=kernel_id [1]=phase01 [2]=repeats [3]=time_sec (also u_time)
 *  Covers kernels 0..kSpatialStripGpuKernelMaxId (all SpatialPatternKernel ids);
 *  unknown ids fall back to sine.
 */
inline constexpr int kSpatialStripGpuKernelMaxId = 43;

inline const char* SpatialStripKernelFieldGlsl()
{
    return R"(
float fractf(float x) { return x - floor(x); }
float hash11(float x) { return fractf(sin(x * 12.9898) * 43758.547); }
float smstep(float e0, float e1, float x)
{
    float t = clamp((x - e0) / max(e1 - e0, 1e-5), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
void stripMain(out vec4 out_color, in float s01)
{
    int kid = int(u_params[0] + 0.5);
    float phase01 = u_params[1];
    float rep = max(u_params[2], 1.0);
    float time_sec = u_params[3];
    if(abs(time_sec) < 1e-8)
        time_sec = u_time;
    float tsec = time_sec * 0.35;

    float ph = fractf(phase01 * 0.35 + time_sec * 0.08);
    float r = 1.0 + (rep - 1.0) * 0.72;
    float u_phase = fractf(s01 * r + ph + 1000.0);
    float k = 0.0;
    const float TWO_PI = 6.2831853;

    if(kid == 1)
        k = 2.0 * u_phase - 1.0;
    else if(kid == 2)
        k = (1.0 - abs(2.0 * u_phase - 1.0)) * 2.0 - 1.0;
    else if(kid == 3)
        k = (u_phase < 0.5) ? 1.0 : -1.0;
    else if(kid == 4)
    {
        float u = mod(s01 * r * 3.0 + ph, 1.0);
        float a = smstep(0.0, 0.12, u);
        float b = 1.0 - smstep(0.88, 1.0, u);
        k = a * b * 2.0 - 1.0;
    }
    else if(kid == 5)
        k = pow(1.0 - u_phase, 2.2) * 2.0 - 1.0;
    else if(kid == 6)
        k = (0.5 + 0.5 * sin(TWO_PI * u_phase)) * 2.0 - 1.0;
    else if(kid == 7)
        k = sin(TWO_PI * (s01 * r + ph)) * sin(TWO_PI * (s01 * r * 0.37 - ph * 1.7));
    else if(kid == 8)
    {
        float t = s01 * r * 28.0 + ph * 11.0;
        float cell = floor(t);
        float tw = fractf(t);
        float h = hash11(cell * 0.031 + 9.1);
        float bright = (h > 0.72) ? 1.0 : -0.65;
        float decay = max(0.0, 1.0 - tw * 1.8);
        k = bright * decay + (-0.65) * (1.0 - decay);
    }
    else if(kid == 9)
    {
        float t = s01 * r * 5.0 + ph * 2.0;
        float i = floor(t);
        float f = t - i;
        float a = hash11(i);
        float b = hash11(i + 1.0);
        float s = f * f * (3.0 - 2.0 * f);
        k = (a + (b - a) * s) * 2.0 - 1.0;
    }
    else if(kid == 10)
    {
        float u = fractf(s01 * r + ph + 400.0);
        float stepv = floor(u * 8.0);
        stepv = clamp(stepv, 0.0, 7.0);
        k = (stepv / 7.0) * 2.0 - 1.0;
    }
    else if(kid == 11)
        k = abs(2.0 * u_phase - 1.0) * 2.0 - 1.0;
    else if(kid == 12)
        k = 2.0 * s01 - 1.0;
    else if(kid == 13)
    {
        float idx = floor(s01 * r * 12.0 + ph * 12.0 + tsec * 1.5);
        float bucket = mod(idx, 3.0);
        k = (abs(bucket) < 0.5) ? 1.0 : -0.75;
    }
    else if(kid == 14)
    {
        float u = fractf(s01 * r - ph - tsec * 0.22);
        float w = smstep(0.0, 0.08, u) * (1.0 - smstep(0.12, 0.2, u));
        k = w * 2.0 - 1.0;
    }
    else if(kid == 15)
    {
        float u = fractf(s01 * r * 0.5 + ph * 0.5);
        float tri = abs(2.0 * fractf(u * 2.0) - 1.0);
        k = tri * 2.0 - 1.0;
    }
    else if(kid == 16)
    {
        float v = sin(TWO_PI * (s01 * r + ph)) +
                  0.5 * sin(TWO_PI * (s01 * r * 1.13 + ph * 0.7 + 0.3)) +
                  0.25 * sin(TWO_PI * (s01 * r * 0.77 + tsec * 0.11));
        k = clamp(v * 0.45, -1.0, 1.0);
    }
    else if(kid == 17)
    {
        float id = floor(s01 * r * 16.0);
        float h = hash11(id + floor(tsec * 3.0) * 0.01);
        k = (h > 0.92) ? 1.0 : -0.9;
    }
    else if(kid == 18)
    {
        float t = s01 * r * 32.0 + tsec * 6.0;
        float h = hash11(floor(t) * 0.07);
        float tw = fractf(t);
        k = (h > 0.88) ? max(-1.0, 1.0 - tw * 4.0) : -0.85;
    }
    else if(kid == 19)
    {
        float u = 1.0 - fractf(s01 * r * 0.7 + ph + tsec * 0.07);
        float heat = pow(max(0.0, u), 2.5);
        heat *= 0.6 + 0.4 * sin(TWO_PI * s01 * r * 2.0);
        k = heat * 2.0 - 1.0;
    }
    else if(kid == 20)
    {
        float heat = 0.0;
        for(int o = 0; o < 4; o++)
        {
            float f = pow(2.0, float(o));
            heat += sin(TWO_PI * (s01 * r * f * 0.31 + ph + tsec * (0.05 + 0.02 * float(o)))) / f;
        }
        k = clamp(tanh(heat * 1.15), -1.0, 1.0);
    }
    else if(kid == 21)
    {
        float v = sin(TWO_PI * (s01 * r + tsec * 0.04)) * 0.5 +
                  sin(TWO_PI * (s01 * r * 0.6 + ph + tsec * 0.07)) * 0.35 +
                  sin(TWO_PI * (s01 * r * 2.1 + tsec * 0.03)) * 0.15;
        k = clamp(v, -1.0, 1.0);
    }
    else if(kid == 22)
    {
        float sum = 0.0;
        float amp = 1.0;
        float x = s01 * r * 4.0 + ph;
        for(int o = 0; o < 4; o++)
        {
            float i = floor(x);
            float f = x - i;
            float a = hash11(i);
            float b = hash11(i + 1.0);
            float s = f * f * (3.0 - 2.0 * f);
            sum += (a + (b - a) * s) * amp;
            amp *= 0.5;
            x *= 2.0;
        }
        k = sum * 2.0 - 1.0;
    }
    else if(kid == 23)
    {
        float beat = sin(TWO_PI * tsec * 1.5);
        k = (beat <= 0.0) ? -0.85 : beat * sin(TWO_PI * (s01 * r + ph));
    }
    else if(kid == 24)
    {
        float v = -1.0;
        for(int b = 0; b < 3; b++)
        {
            float c = fractf(ph + tsec * 0.11 * float(b + 1) + float(b) * 0.33);
            float d = abs(fractf(s01 * r + 1.0 - c) - 0.5);
            v = max(v, 1.0 - d * 8.0);
        }
        k = clamp(v, -1.0, 1.0);
    }
    else if(kid == 25)
    {
        float col = fractf(s01 * r);
        float drop = fractf(col * 7.0 + tsec * 0.45 + ph);
        if(drop > 0.88)
            k = 1.0;
        else if(drop > 0.55)
            k = drop * 2.0 - 1.1;
        else
            k = -1.0 + drop * 0.4;
    }
    else if(kid == 26)
    {
        float v = sin(TWO_PI * (s01 + ph)) * cos(TWO_PI * (s01 * r * 0.5 + tsec * 0.07)) *
                  sin(TWO_PI * (s01 * r + ph * 0.3));
        k = clamp(v * 1.25, -1.0, 1.0);
    }
    else if(kid == 27)
    {
        float pos = abs(2.0 * fractf(tsec * 0.15 + ph) - 1.0);
        float d = abs(fractf(s01 * r + 1.0 - pos) - 0.5) * 2.0;
        k = max(-1.0, 1.0 - d * 4.0);
    }
    else if(kid == 28)
    {
        float u = abs(fractf(s01 * r + ph) - 0.5) * 2.0;
        float eye = fractf(tsec * 0.18);
        float d1 = abs(u - eye * 2.0);
        float d2 = abs(u - fractf(eye + 0.5) * 2.0);
        float v = max(1.0 - d1 * 5.0, 1.0 - d2 * 5.0);
        k = clamp(v * 2.0 - 1.0, -1.0, 1.0);
    }
    else if(kid == 29)
    {
        float id = floor(s01 * r * 8.0 + ph * 3.0);
        float h = hash11(id * 3.17 + floor(tsec * 2.0) * 0.1);
        k = h * 2.0 - 1.0;
    }
    else if(kid == 30)
    {
        float cut = fractf(ph + tsec * 0.12);
        k = (fractf(s01) < cut) ? 1.0 : -1.0;
    }
    else if(kid == 31)
    {
        float h = hash11(floor(s01 * r * 20.0) + floor(tsec * 4.0) * 0.1);
        float rise = fractf(tsec * 2.0 + s01);
        k = (h > 0.85) ? (2.0 * rise - 1.0) : -0.9;
    }
    else if(kid == 32)
    {
        float n = hash11(floor(tsec * 30.0) + s01 * 13.0);
        k = (n > 0.4) ? 0.9 : -1.0 + n * 0.5;
    }
    else if(kid == 33)
    {
        float h = hash11(floor(s01 * r * 25.0) + ph);
        k = (h > 0.94) ? 1.0 : -0.95;
    }
    else if(kid == 34)
        k = sin(TWO_PI * (s01 * r * 0.5 + tsec * 0.08)) * cos(TWO_PI * tsec * 0.5);
    else if(kid == 35)
    {
        float u = fractf(s01 * r + ph);
        if(u < 0.33)
            k = -1.0;
        else if(u < 0.66)
            k = 0.0;
        else
            k = 1.0;
    }
    else if(kid == 36)
    {
        float u = abs(2.0 * fractf(tsec * 0.4 + 0.25) - 1.0);
        float d = abs(fractf(s01 * r) - u);
        k = max(-1.0, 1.0 - d * 10.0);
    }
    else if(kid == 37)
    {
        float idx = floor(s01 * r * 9.0 + ph * 9.0 + tsec * 2.0);
        float seg = mod(mod(idx, 3.0) + 3.0, 3.0);
        k = (seg < 0.5) ? 1.0 : (seg < 1.5) ? 0.0 : -1.0;
    }
    else if(kid == 38)
    {
        float d = abs(s01 - 0.5) * 2.0;
        k = sin(TWO_PI * (2.0 * d - fractf(ph + tsec * 0.25) * 2.0));
    }
    else if(kid == 39)
    {
        float u = fractf(tsec * 0.8);
        if(u < 0.12)
            k = sin(TWO_PI * u / 0.12);
        else if(u < 0.2)
            k = 0.35 * sin(TWO_PI * (u - 0.12) / 0.08);
        else
            k = -0.55 + 0.2 * sin(TWO_PI * s01 * r);
    }
    else if(kid == 40)
    {
        float cell = floor(s01 * r * 22.0 + ph * 6.0 + 50.0);
        float h = hash11(cell * 0.19 + 2.7);
        if(h < 0.965)
            k = -0.92;
        else
        {
            float spd = 0.65 + hash11(cell * 0.41) * 2.2;
            float t = fractf(tsec * spd + cell * 0.037);
            float flash = (t < 0.14) ? (1.0 - smstep(0.0, 0.14, t)) : 0.0;
            flash = flash * flash;
            k = flash * 2.0 - 1.0;
        }
    }
    else if(kid == 41)
    {
        float a = TWO_PI * (s01 * r * 1.0 + tsec * 0.085 + ph);
        float b = TWO_PI * (s01 * r * 1.62 - tsec * 0.052 + ph * 0.73);
        float c = TWO_PI * (s01 * r * 0.48 + tsec * 0.11 + sin(TWO_PI * ph) * 0.08);
        float wave = sin(a) + 0.55 * sin(b) + 0.28 * sin(c);
        k = clamp(wave * 0.38, -1.0, 1.0);
    }
    else if(kid == 42)
    {
        float t = s01 * r * 1.8 + tsec * 0.12;
        float i = floor(t);
        float f = t - i;
        float n0 = hash11(i + 1.1);
        float n1 = hash11(i + 2.1);
        float s = f * f * (3.0 - 2.0 * f);
        float smoothn = n0 + (n1 - n0) * s;
        float breathe = 0.5 + 0.5 * sin(TWO_PI * (tsec * 0.22 + s01 * r * 0.08));
        float micro = (hash11(floor(tsec * 7.0) + s01 * r * 9.0) - 0.5) * 0.12;
        float v = 0.58 + 0.32 * smoothn * breathe + micro;
        k = clamp(v * 2.0 - 1.0, -1.0, 1.0);
    }
    else if(kid == 43)
    {
        float pos = fractf(tsec * (0.14 + 0.06 * hash11(ph * 3.1 + 0.2)) + ph * 0.35);
        float u = fractf(s01 * r - pos + 1.0);
        float head = smstep(0.0, 0.035, u) * (1.0 - smstep(0.035, 0.055, u));
        float tail = max(0.0, 1.0 - u * 5.5);
        tail = tail * tail * 0.92;
        float v = max(head, tail);
        k = clamp(v * 2.0 - 1.0, -1.0, 1.0);
    }
    else
        k = sin(TWO_PI * u_phase);

    float enc = clamp((k + 1.0) * 0.5, 0.0, 1.0);
    out_color = vec4(enc, enc, enc, 1.0);
}
)";
}
