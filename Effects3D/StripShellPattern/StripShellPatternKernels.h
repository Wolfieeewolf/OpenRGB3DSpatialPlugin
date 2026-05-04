// SPDX-License-Identifier: GPL-2.0-only

#ifndef STRIPSHELLPATTERNKERNELS_H
#define STRIPSHELLPATTERNKERNELS_H

enum class StripShellKernel : int
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
};

inline int StripShellKernelCount()
{
    return (int)StripShellKernel::Meteor + 1;
}

int StripShellKernelClamp(int id);

const char* StripShellKernelDisplayName(int kernel_id);

float EvalStripShellKernel(int kernel_id, float s01, float phase01, float rep, float time_sec);

#endif
