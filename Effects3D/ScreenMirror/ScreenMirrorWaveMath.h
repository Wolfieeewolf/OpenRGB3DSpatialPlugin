// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENMIRROR_WAVE_MATH_H
#define SCREENMIRROR_WAVE_MATH_H

#include <algorithm>
#include <cmath>

constexpr int kScreenMirrorGpuMaxHistory = 8;
constexpr float kWaveTimeToEdgeEnableSec = 0.05f;
constexpr float kWaveIntensityEnablePct = 5.0f;
constexpr int kRadialMapUiNeutral = 50;

inline float RadialMapUiToInternal(int ui_0_100)
{
    return (float)std::clamp(ui_0_100, 0, 100) - (float)kRadialMapUiNeutral;
}

inline float WaveIntensityToSpeedMmPerMs(float intensity_0_to_100)
{
    if(intensity_0_to_100 < 0.5f)
    {
        return 0.0f;
    }
    float p = std::clamp(intensity_0_to_100, 1.0f, 100.0f) / 100.0f;
    float speed = 200.0f * powf(0.01f, p);
    return std::clamp(speed, 0.5f, 500.0f);
}

/** Time-to-edge (>0) replaces wave intensity. Intensity below 5% is instant (no wave). */
inline float ResolveWaveSpeedMmPerMs(float time_to_edge_sec, float intensity_pct, float max_distance_mm)
{
    if(time_to_edge_sec >= kWaveTimeToEdgeEnableSec)
    {
        float t_sec = std::clamp(time_to_edge_sec, kWaveTimeToEdgeEnableSec, 10.0f);
        float dist = std::max(max_distance_mm, 1.0f);
        return std::max(dist / (t_sec * 1000.0f), 0.1f);
    }
    if(intensity_pct >= kWaveIntensityEnablePct)
    {
        return WaveIntensityToSpeedMmPerMs(intensity_pct);
    }
    return 0.0f;
}

inline float WaveHistorySpanMs(float speed_mm_per_ms, float max_distance_mm, float decay_ms)
{
    if(speed_mm_per_ms < 0.1f)
    {
        return 0.0f;
    }
    float max_delay = std::max(max_distance_mm, 1.0f) / std::max(speed_mm_per_ms, 0.1f);
    return max_delay + std::max(decay_ms, 0.0f);
}

inline float WaveHistoryRow(float delay_ms, float span_ms, int nhist)
{
    if(nhist <= 1 || span_ms <= 0.1f)
    {
        return 0.0f;
    }
    float h = delay_ms / span_ms * (float)(nhist - 1);
    return std::clamp(h, 0.0f, (float)(nhist - 1));
}

/** Trail weight for a history tile. Age 0 = newest frame. Decay 0 = nearest tile only. */
inline float WaveTrailWeight(float tile_age_ms, float delay_ms, float decay_ms, float span_ms, int nhist)
{
    float behind = tile_age_ms - delay_ms;
    if(decay_ms <= 0.1f)
    {
        float bin = span_ms / std::max((float)(nhist - 1), 1.0f);
        return (std::fabs(behind) <= 0.51f * std::max(bin, 1.0f)) ? 1.0f : 0.0f;
    }
    float bin = span_ms / std::max((float)(nhist - 1), 1.0f);
    if(behind < -0.51f * std::max(bin, 1.0f))
    {
        return 0.0f;
    }
    return expf(-std::max(behind, 0.0f) / std::max(decay_ms, 0.1f));
}

#endif
