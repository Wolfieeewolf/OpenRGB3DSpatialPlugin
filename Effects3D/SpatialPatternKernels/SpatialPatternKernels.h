// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIAL_PATTERN_KERNELS_H
#define SPATIAL_PATTERN_KERNELS_H

enum class SpatialPatternKernel : int
{
    Sine = 0,
    Saw,
    Triangle,
    Square,
    Chase,
    Comet,
    Pulse,
    Interference,
    Sparkle,
    SmoothNoise,
    Steps,
    Bounce,
    GradientS,
    TheaterChase,
    RunningLight,
    LarsonScanner,
    ColorWave,
    TwinkleSparse,
    GlitterBurst,
    FireSimple,
    FireLayered,
    OceanDriftLite,
    NoiseOctaves,
    BpmPulse,
    JuggleDots,
    MatrixRain1D,
    PlasmaSinProduct,
    HueBounceDot,
    Cylon,
    RandomColorsDrift,
    WipeSim,
    TwinkleUp,
    HalloweenFlicker,
    SparkleDark,
    ColorBlend,
    TriFade,
    DotBounce,
    TricolorChase,
    Ripple1D,
    Heartbeat,
    Confetti,
    SpectrumWaves,
    CandleSoft,
    Meteor,
    /* MoonLight / WLED-MM inspired 1D looks (CPU full; GPU still 8 families). */
    FrequencySaws,
    DistortionWaves,
    Lissajous1D,
    Blackhole1D,
    Sin3,
};

inline int SpatialPatternKernelCount()
{
    return (int)SpatialPatternKernel::Sin3 + 1;
}

int SpatialPatternKernelClamp(int id);

const char* SpatialPatternKernelDisplayName(int kernel_id);

float EvalSpatialPatternKernel(int kernel_id, float s01, float phase01, float rep, float time_sec);

#endif
