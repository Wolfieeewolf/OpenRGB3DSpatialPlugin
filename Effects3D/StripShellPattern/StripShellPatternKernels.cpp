// SPDX-License-Identifier: GPL-2.0-only

#include "StripShellPatternKernels.h"
#include "EffectHelpers.h"
#include <algorithm>
#include <cmath>

namespace
{
float fractf(float x)
{
    return x - std::floor(x);
}
float hash11(float x)
{
    return fractf(std::sin(x * 12.9898f) * 43758.547f);
}
}

int StripShellKernelClamp(int id)
{
    const int n = StripShellKernelCount();
    if(id < 0)
        return 0;
    if(id >= n)
        return n - 1;
    return id;
}

const char* StripShellKernelDisplayName(int kernel_id)
{
    switch(static_cast<StripShellKernel>(StripShellKernelClamp(kernel_id)))
    {
    case StripShellKernel::Sine: return "Sine";
    case StripShellKernel::Saw: return "Saw / ramp";
    case StripShellKernel::Triangle: return "Triangle";
    case StripShellKernel::Square: return "Square (half)";
    case StripShellKernel::Chase: return "Chase bands";
    case StripShellKernel::Comet: return "Comet / taper";
    case StripShellKernel::Pulse: return "Pulse train";
    case StripShellKernel::Interference: return "Two-tone beat";
    case StripShellKernel::Sparkle: return "Sparkle";
    case StripShellKernel::SmoothNoise: return "Smooth noise";
    case StripShellKernel::Steps: return "Stepped";
    case StripShellKernel::Bounce: return "V / bounce";
    case StripShellKernel::GradientS: return "Gradient (position)";
    case StripShellKernel::TheaterChase: return "Theater chase";
    case StripShellKernel::RunningLight: return "Running dot";
    case StripShellKernel::LarsonScanner: return "Larson / scanner";
    case StripShellKernel::ColorWave: return "Color wave (multi-sine)";
    case StripShellKernel::TwinkleSparse: return "Twinkle (sparse)";
    case StripShellKernel::GlitterBurst: return "Glitter burst";
    case StripShellKernel::FireSimple: return "Fire (simple heat)";
    case StripShellKernel::FireLayered: return "Fire (layered heat)";
    case StripShellKernel::OceanDriftLite: return "Ocean drift blend";
    case StripShellKernel::NoiseOctaves: return "Noise octaves (1D)";
    case StripShellKernel::BpmPulse: return "BPM pulse";
    case StripShellKernel::JuggleDots: return "Juggle (multi dot)";
    case StripShellKernel::MatrixRain1D: return "Matrix rain (1D)";
    case StripShellKernel::PlasmaSinProduct: return "Plasma (sin product)";
    case StripShellKernel::HueBounceDot: return "Hue bounce dot";
    case StripShellKernel::Cylon: return "Cylon / eye sweep";
    case StripShellKernel::RandomColorsDrift: return "Random drift blocks";
    case StripShellKernel::WipeSim: return "Wipe progress";
    case StripShellKernel::TwinkleUp: return "Twinkle rise";
    case StripShellKernel::HalloweenFlicker: return "Halloween flicker";
    case StripShellKernel::SparkleDark: return "Sparkle on dark";
    case StripShellKernel::ColorBlend: return "Color blend beat";
    case StripShellKernel::TriFade: return "Tri-level fade";
    case StripShellKernel::DotBounce: return "Dot bounce";
    case StripShellKernel::TricolorChase: return "Tri-color chase";
    case StripShellKernel::Ripple1D: return "Ripple (from center)";
    case StripShellKernel::Heartbeat: return "Heartbeat";
    case StripShellKernel::Confetti: return "Confetti (sparse pops)";
    case StripShellKernel::SpectrumWaves: return "Spectrum waves (smooth)";
    case StripShellKernel::CandleSoft: return "Candle (soft warm flicker)";
    case StripShellKernel::Meteor: return "Meteor (head + tail)";
    default: return "Sine";
    }
}

float EvalStripShellKernel(int kernel_id, float s01, float phase01, float rep, float time_sec)
{
    const StripShellKernel k = static_cast<StripShellKernel>(StripShellKernelClamp(kernel_id));
    const float ph = phase01;
    const float r = std::max(1.0f, rep);
    const float u_phase = fractf(s01 * r + ph + 1000.0f);

    switch(k)
    {
    case StripShellKernel::Saw:
        return 2.0f * u_phase - 1.0f;
    case StripShellKernel::Triangle:
        return (1.0f - std::fabs(2.0f * u_phase - 1.0f)) * 2.0f - 1.0f;
    case StripShellKernel::Square:
        return (u_phase < 0.5f) ? 1.0f : -1.0f;
    case StripShellKernel::Chase:
    {
        float u = std::fmod(s01 * r * 3.0f + ph, 1.0f);
        float a = smoothstep(0.0f, 0.12f, u);
        float b = 1.0f - smoothstep(0.88f, 1.0f, u);
        return a * b * 2.0f - 1.0f;
    }
    case StripShellKernel::Comet:
        return std::pow(1.0f - u_phase, 2.2f) * 2.0f - 1.0f;
    case StripShellKernel::Pulse:
    {
        float w = 0.5f + 0.5f * std::sin(TWO_PI * u_phase);
        return w * 2.0f - 1.0f;
    }
    case StripShellKernel::Interference:
        return std::sin(TWO_PI * (s01 * r + ph)) * std::sin(TWO_PI * (s01 * r * 0.37f - ph * 1.7f));
    case StripShellKernel::Sparkle:
    {
        float t = s01 * r * 28.0f + ph * 11.0f;
        float cell = std::floor(t);
        float tw = fractf(t);
        float h = hash11(cell * 0.031f + 9.1f);
        float bright = (h > 0.72f) ? 1.0f : -0.65f;
        float decay = std::max(0.0f, 1.0f - tw * 1.8f);
        return bright * decay + (-0.65f) * (1.0f - decay);
    }
    case StripShellKernel::SmoothNoise:
    {
        float t = s01 * r * 5.0f + ph * 2.0f;
        float i = std::floor(t);
        float f = t - i;
        float a = hash11(i);
        float b = hash11(i + 1.0f);
        float s = f * f * (3.0f - 2.0f * f);
        float n = a + (b - a) * s;
        return n * 2.0f - 1.0f;
    }
    case StripShellKernel::Steps:
    {
        constexpr int n = 8;
        float u = fractf(s01 * r + ph + 400.0f);
        int step = (int)std::floor(u * (float)n);
        step = std::clamp(step, 0, n - 1);
        float g = (n <= 1) ? 0.5f : (float)step / (float)(n - 1);
        return g * 2.0f - 1.0f;
    }
    case StripShellKernel::Bounce:
        return std::fabs(2.0f * u_phase - 1.0f) * 2.0f - 1.0f;
    case StripShellKernel::GradientS:
        return 2.0f * s01 - 1.0f;

    case StripShellKernel::TheaterChase:
    {
        int idx = (int)std::floor(s01 * r * 12.0f + ph * 12.0f + time_sec * 1.5f);
        int bucket = ((idx % 3) + 3) % 3;
        return (bucket == 0) ? 1.0f : -0.75f;
    }
    case StripShellKernel::RunningLight:
    {
        float u = fractf(s01 * r - ph - time_sec * 0.22f);
        float w = smoothstep(0.0f, 0.08f, u) * (1.0f - smoothstep(0.12f, 0.2f, u));
        return w * 2.0f - 1.0f;
    }
    case StripShellKernel::LarsonScanner:
    {
        float u = fractf(s01 * r * 0.5f + ph * 0.5f);
        float tri = std::fabs(2.0f * fractf(u * 2.0f) - 1.0f);
        return tri * 2.0f - 1.0f;
    }
    case StripShellKernel::ColorWave:
    {
        float v = std::sin(TWO_PI * (s01 * r + ph)) +
                  0.5f * std::sin(TWO_PI * (s01 * r * 1.13f + ph * 0.7f + 0.3f)) +
                  0.25f * std::sin(TWO_PI * (s01 * r * 0.77f + time_sec * 0.11f));
        return std::clamp(v * 0.45f, -1.0f, 1.0f);
    }
    case StripShellKernel::TwinkleSparse:
    {
        float id = std::floor(s01 * r * 16.0f);
        float h = hash11(id + std::floor(time_sec * 3.0f) * 0.01f);
        return (h > 0.92f) ? 1.0f : -0.9f;
    }
    case StripShellKernel::GlitterBurst:
    {
        float t = s01 * r * 32.0f + time_sec * 6.0f;
        float h = hash11(std::floor(t) * 0.07f);
        float tw = fractf(t);
        return (h > 0.88f) ? std::max(-1.0f, 1.0f - tw * 4.0f) : -0.85f;
    }
    case StripShellKernel::FireSimple:
    {
        float u = 1.0f - fractf(s01 * r * 0.7f + ph + time_sec * 0.07f);
        float heat = std::pow(std::max(0.0f, u), 2.5f);
        heat *= 0.6f + 0.4f * std::sin(TWO_PI * s01 * r * 2.0f);
        return heat * 2.0f - 1.0f;
    }
    case StripShellKernel::FireLayered:
    {
        float heat = 0.0f;
        for(int o = 0; o < 4; o++)
        {
            float f = std::pow(2.0f, (float)o);
            heat += std::sin(TWO_PI * (s01 * r * f * 0.31f + ph + time_sec * (0.05f + 0.02f * (float)o))) / f;
        }
        float t = std::tanh(heat * 1.15f);
        return std::clamp(t, -1.0f, 1.0f);
    }
    case StripShellKernel::OceanDriftLite:
    {
        float v = std::sin(TWO_PI * (s01 * r + time_sec * 0.04f)) * 0.5f +
                  std::sin(TWO_PI * (s01 * r * 0.6f + ph + time_sec * 0.07f)) * 0.35f +
                  std::sin(TWO_PI * (s01 * r * 2.1f + time_sec * 0.03f)) * 0.15f;
        return std::clamp(v, -1.0f, 1.0f);
    }
    case StripShellKernel::NoiseOctaves:
    {
        float sum = 0.0f;
        float amp = 1.0f;
        float x = s01 * r * 4.0f + ph;
        for(int o = 0; o < 4; o++)
        {
            float i = std::floor(x);
            float f = x - i;
            float a = hash11(i);
            float b = hash11(i + 1.0f);
            float s = f * f * (3.0f - 2.0f * f);
            sum += (a + (b - a) * s) * amp;
            amp *= 0.5f;
            x *= 2.0f;
        }
        return sum * 2.0f - 1.0f;
    }
    case StripShellKernel::BpmPulse:
    {
        float beat = std::sin(TWO_PI * time_sec * 1.5f);
        if(beat <= 0.0f)
            return -0.85f;
        return beat * std::sin(TWO_PI * (s01 * r + ph));
    }
    case StripShellKernel::JuggleDots:
    {
        float v = -1.0f;
        for(int b = 0; b < 3; b++)
        {
            float c = fractf(ph + time_sec * 0.11f * (float)(b + 1) + (float)b * 0.33f);
            float d = std::fabs(fractf(s01 * r + 1.0f - c) - 0.5f);
            v = std::max(v, 1.0f - d * 8.0f);
        }
        return std::clamp(v, -1.0f, 1.0f);
    }
    case StripShellKernel::MatrixRain1D:
    {
        float col = fractf(s01 * r);
        float drop = fractf(col * 7.0f + time_sec * 0.45f + ph);
        if(drop > 0.88f)
            return 1.0f;
        if(drop > 0.55f)
            return drop * 2.0f - 1.1f;
        return -1.0f + drop * 0.4f;
    }
    case StripShellKernel::PlasmaSinProduct:
    {
        float v = std::sin(TWO_PI * (s01 + ph)) * std::cos(TWO_PI * (s01 * r * 0.5f + time_sec * 0.07f)) *
                  std::sin(TWO_PI * (s01 * r + ph * 0.3f));
        return std::clamp(v * 1.25f, -1.0f, 1.0f);
    }
    case StripShellKernel::HueBounceDot:
    {
        float pos = std::fabs(2.0f * fractf(time_sec * 0.15f + ph) - 1.0f);
        float d = std::fabs(fractf(s01 * r + 1.0f - pos) - 0.5f) * 2.0f;
        return std::max(-1.0f, 1.0f - d * 4.0f);
    }
    case StripShellKernel::Cylon:
    {
        float u = std::fabs(fractf(s01 * r + ph) - 0.5f) * 2.0f;
        float eye = fractf(time_sec * 0.18f);
        float d1 = std::fabs(u - eye * 2.0f);
        float d2 = std::fabs(u - fractf(eye + 0.5f) * 2.0f);
        float v = std::max(1.0f - d1 * 5.0f, 1.0f - d2 * 5.0f);
        return std::clamp(v * 2.0f - 1.0f, -1.0f, 1.0f);
    }
    case StripShellKernel::RandomColorsDrift:
    {
        float id = std::floor(s01 * r * 8.0f + ph * 3.0f);
        float h = hash11(id * 3.17f + std::floor(time_sec * 2.0f) * 0.1f);
        return h * 2.0f - 1.0f;
    }
    case StripShellKernel::WipeSim:
    {
        float cut = fractf(ph + time_sec * 0.12f);
        return (fractf(s01) < cut) ? 1.0f : -1.0f;
    }
    case StripShellKernel::TwinkleUp:
    {
        float h = hash11(std::floor(s01 * r * 20.0f) + std::floor(time_sec * 4.0f) * 0.1f);
        float rise = fractf(time_sec * 2.0f + s01);
        return (h > 0.85f) ? (2.0f * rise - 1.0f) : -0.9f;
    }
    case StripShellKernel::HalloweenFlicker:
    {
        float n = hash11(std::floor(time_sec * 30.0f) + s01 * 13.0f);
        return (n > 0.4f) ? 0.9f : -1.0f + n * 0.5f;
    }
    case StripShellKernel::SparkleDark:
    {
        float h = hash11(std::floor(s01 * r * 25.0f) + ph);
        return (h > 0.94f) ? 1.0f : -0.95f;
    }
    case StripShellKernel::ColorBlend:
        return std::sin(TWO_PI * (s01 * r * 0.5f + time_sec * 0.08f)) * std::cos(TWO_PI * time_sec * 0.5f);
    case StripShellKernel::TriFade:
    {
        float u = fractf(s01 * r + ph);
        if(u < 0.33f)
            return -1.0f;
        if(u < 0.66f)
            return 0.0f;
        return 1.0f;
    }
    case StripShellKernel::DotBounce:
    {
        float u = std::fabs(2.0f * fractf(time_sec * 0.4f + 0.25f) - 1.0f);
        float d = std::fabs(fractf(s01 * r) - u);
        return std::max(-1.0f, 1.0f - d * 10.0f);
    }
    case StripShellKernel::TricolorChase:
    {
        int seg = ((int)std::floor(s01 * r * 9.0f + ph * 9.0f + time_sec * 2.0f) % 3 + 3) % 3;
        return (seg == 0) ? 1.0f : (seg == 1) ? 0.0f : -1.0f;
    }
    case StripShellKernel::Ripple1D:
    {
        float d = std::fabs(s01 - 0.5f) * 2.0f;
        return std::sin(TWO_PI * (2.0f * d - fractf(ph + time_sec * 0.25f) * 2.0f));
    }
    case StripShellKernel::Heartbeat:
    {
        float u = fractf(time_sec * 0.8f);
        if(u < 0.12f)
            return std::sin(TWO_PI * u / 0.12f);
        if(u < 0.2f)
            return 0.35f * std::sin(TWO_PI * (u - 0.12f) / 0.08f);
        return -0.55f + 0.2f * std::sin(TWO_PI * s01 * r);
    }
    case StripShellKernel::Confetti:
    {
        float cell = std::floor(s01 * r * 22.0f + ph * 6.0f + 50.0f);
        float h = hash11(cell * 0.19f + 2.7f);
        if(h < 0.965f)
            return -0.92f;
        float spd = 0.65f + hash11(cell * 0.41f) * 2.2f;
        float t = fractf(time_sec * spd + cell * 0.037f);
        float flash = (t < 0.14f) ? (1.0f - smoothstep(0.0f, 0.14f, t)) : 0.0f;
        flash = flash * flash;
        return flash * 2.0f - 1.0f;
    }
    case StripShellKernel::SpectrumWaves:
    {
        float a = TWO_PI * (s01 * r * 1.0f + time_sec * 0.085f + ph);
        float b = TWO_PI * (s01 * r * 1.62f - time_sec * 0.052f + ph * 0.73f);
        float c = TWO_PI * (s01 * r * 0.48f + time_sec * 0.11f + std::sin(TWO_PI * ph) * 0.08f);
        float wave = std::sin(a) + 0.55f * std::sin(b) + 0.28f * std::sin(c);
        return std::clamp(wave * 0.38f, -1.0f, 1.0f);
    }
    case StripShellKernel::CandleSoft:
    {
        float t = s01 * r * 1.8f + time_sec * 0.12f;
        float i = std::floor(t);
        float f = t - i;
        float n0 = hash11(i + 1.1f);
        float n1 = hash11(i + 2.1f);
        float s = f * f * (3.0f - 2.0f * f);
        float smooth = n0 + (n1 - n0) * s;
        float breathe = 0.5f + 0.5f * std::sin(TWO_PI * (time_sec * 0.22f + s01 * r * 0.08f));
        float micro = (hash11(std::floor(time_sec * 7.0f) + s01 * r * 9.0f) - 0.5f) * 0.12f;
        float v = 0.58f + 0.32f * smooth * breathe + micro;
        return std::clamp(v * 2.0f - 1.0f, -1.0f, 1.0f);
    }
    case StripShellKernel::Meteor:
    {
        float pos = fractf(time_sec * (0.14f + 0.06f * hash11(ph * 3.1f + 0.2f)) + ph * 0.35f);
        float u = fractf(s01 * r - pos + 1.0f);
        float head = smoothstep(0.0f, 0.035f, u) * (1.0f - smoothstep(0.035f, 0.055f, u));
        float tail = std::max(0.0f, 1.0f - u * 5.5f);
        tail = tail * tail * 0.92f;
        float v = std::max(head, tail);
        return std::clamp(v * 2.0f - 1.0f, -1.0f, 1.0f);
    }
    case StripShellKernel::Sine:
    default:
        return std::sin(TWO_PI * (s01 * r + ph));
    }
}
