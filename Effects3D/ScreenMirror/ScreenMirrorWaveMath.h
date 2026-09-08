// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENMIRROR_WAVE_MATH_H
#define SCREENMIRROR_WAVE_MATH_H

#include <algorithm>
#include <cmath>

constexpr float kWaveTimeToEdgeEnableSec = 0.05f;
constexpr float kWaveIntensityEnablePct = 5.0f;
constexpr int kRadialMapUiNeutral = 50;

inline float RadialMapUiToInternal(int ui_0_100)
{
    return (float)std::clamp(ui_0_100, 0, 100) - (float)kRadialMapUiNeutral;
}

inline float RoomSpanLengthMm(float span_mm_x, float span_mm_y, float span_mm_z)
{
    float lx = std::max(span_mm_x, 0.0f);
    float ly = std::max(span_mm_y, 0.0f);
    float lz = std::max(span_mm_z, 0.0f);
    float len = std::sqrt(lx * lx + ly * ly + lz * lz);
    return std::max(len, 1.0f);
}

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

#endif
