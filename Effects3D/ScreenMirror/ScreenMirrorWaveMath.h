// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENMIRROR_WAVE_MATH_H
#define SCREENMIRROR_WAVE_MATH_H

#include <algorithm>
#include <cmath>

constexpr int kScreenMirrorGpuMaxHistory = 8;
constexpr float kWaveTimeToEdgeEnableSec = 0.05f;
constexpr float kWaveIntensityEnablePct = 5.0f;
constexpr int kRadialMapUiNeutral = 50;
/** GPU pack ceiling for wave speed (mm/ms). Matches WaveIntensityToSpeedMmPerMs. */
constexpr float kWaveSpeedPackMmPerMs = 500.0f;

inline float RadialMapUiToInternal(int ui_0_100)
{
    return (float)std::clamp(ui_0_100, 0, 100) - (float)kRadialMapUiNeutral;
}

/** Live AABB diagonal in mm — fallback when a degenerate grid has no corners. */
inline float RoomSpanLengthMm(float span_mm_x, float span_mm_y, float span_mm_z)
{
    float lx = std::max(span_mm_x, 0.0f);
    float ly = std::max(span_mm_y, 0.0f);
    float lz = std::max(span_mm_z, 0.0f);
    float len = std::sqrt(lx * lx + ly * ly + lz * lz);
    return std::max(len, 1.0f);
}

/** Farthest AABB corner from a room-UV origin, in mm. Scales with layout size. */
inline float RoomCornerMaxDistanceMm(float span_mm_x, float span_mm_y, float span_mm_z,
                                     float falloff_uv_x, float falloff_uv_y, float falloff_uv_z)
{
    float max_mm = 0.0f;
    const float c[2] = {0.0f, 1.0f};
    for(int ix = 0; ix < 2; ++ix)
    {
        float dx = (c[ix] - falloff_uv_x) * span_mm_x;
        for(int iy = 0; iy < 2; ++iy)
        {
            float dy = (c[iy] - falloff_uv_y) * span_mm_y;
            for(int iz = 0; iz < 2; ++iz)
            {
                float dz = (c[iz] - falloff_uv_z) * span_mm_z;
                max_mm = std::max(max_mm, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
    }
    if(max_mm <= 1e-3f)
    {
        max_mm = RoomSpanLengthMm(span_mm_x, span_mm_y, span_mm_z);
    }
    return max_mm;
}

inline float PackWaveSpeed01(float speed_mm_per_ms)
{
    return std::clamp(speed_mm_per_ms / kWaveSpeedPackMmPerMs, 0.0f, 1.0f);
}

inline float UnpackWaveSpeedMmPerMs(float packed01)
{
    return std::clamp(packed01, 0.0f, 1.0f) * kWaveSpeedPackMmPerMs;
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
