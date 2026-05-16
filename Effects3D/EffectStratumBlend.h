// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTSTRATUMBLEND_H
#define EFFECTSTRATUMBLEND_H

#include "SpatialLayerCore.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace EffectStratumBlend
{

inline const char* BandNameUi(int i)
{
    static const char* n[] = {"Floor", "Mid", "Ceiling"};
    return (i >= 0 && i < 3) ? n[i] : "?";
}

inline void InitStratumBreaks(SpatialLayerCore::MapperSettings& m)
{
    m.floor_end = 0.30f;
    m.desk_end = 0.55f;
    m.upper_end = 0.78f;
    m.blend_softness = 0.10f;
}

inline void WeightsForYNorm(float y_norm,
                            const SpatialLayerCore::MapperSettings& m,
                            float wOut3[3])
{
    SpatialLayerCore::ComputeVerticalStratumWeights(y_norm, m, 3, wOut3);
}

struct BandTuningPct
{
    std::array<int, 3> speed{{100, 100, 100}};
    std::array<int, 3> tight{{100, 100, 100}};
    std::array<int, 3> phase_deg{{0, 0, 0}};
    std::array<int, 3> motion_kind{{0, 0, 0}};
    std::array<int, 3> motion_rate{{100, 100, 100}};
};

struct BandBlendScalars
{
    float speed_mul = 1.0f;
    float tight_mul = 1.0f;
    float phase_deg = 0.0f;
};

inline BandBlendScalars BlendBands(int layout_mode,
                                   const float layer_w[3],
                                   const BandTuningPct& b)
{
    BandBlendScalars o;
    if(layout_mode != 1)
    {
        return o;
    }
    o.speed_mul = (layer_w[0] * (float)b.speed[0] + layer_w[1] * (float)b.speed[1] +
                   layer_w[2] * (float)b.speed[2]) *
                  0.01f;
    o.tight_mul = (layer_w[0] * (float)b.tight[0] + layer_w[1] * (float)b.tight[1] +
                   layer_w[2] * (float)b.tight[2]) *
                  0.01f;
    o.phase_deg =
        layer_w[0] * (float)b.phase_deg[0] + layer_w[1] * (float)b.phase_deg[1] + layer_w[2] * (float)b.phase_deg[2];
    o.speed_mul = std::max(0.0f, o.speed_mul);
    o.tight_mul = std::clamp(o.tight_mul, 0.25f, 4.0f);
    return o;
}

inline float StratumMotionPhase01(int layout_mode,
                                  const float layer_w[3],
                                  const BandTuningPct& b,
                                  float nx01,
                                  float ny01,
                                  float nz01,
                                  float origin_x,
                                  float origin_z,
                                  float px,
                                  float pz,
                                  float time_sec)
{
    if(layout_mode != 1)
    {
        return 0.0f;
    }
    float acc = 0.0f;
    for(int i = 0; i < 3; ++i)
    {
        const float w = layer_w[(size_t)i];
        if(w < 1e-5f)
        {
            continue;
        }
        const int k = std::clamp(b.motion_kind[(size_t)i], 0, 6);
        const float rate = std::clamp(b.motion_rate[(size_t)i], 0, 200) * 0.01f;
        switch(k)
        {
        case 0:
            break;
        case 1:
            acc += w * nx01 * 0.35f * rate;
            break;
        case 2:
            acc += w * ny01 * 0.35f * rate;
            break;
        case 3:
            acc += w * nz01 * 0.35f * rate;
            break;
        case 4:
            acc += w * std::fmod(time_sec * 0.18f * rate, 1.0f);
            break;
        case 5:
            acc -= w * std::fmod(time_sec * 0.18f * rate, 1.0f);
            break;
        case 6:
        {
            const float ax = px - origin_x;
            const float az = pz - origin_z;
            const float ang = std::atan2(az, ax) * (0.5f / 3.14159265f) + 0.5f;
            acc += w * std::fmod(ang + time_sec * 0.12f * rate, 1.0f);
            break;
        }
        default:
            break;
        }
    }
    acc = std::fmod(acc, 1.0f);
    if(acc < 0.0f)
    {
        acc += 1.0f;
    }
    return acc;
}

inline float PhaseShift01(const BandBlendScalars& bb)
{
    return bb.phase_deg * (1.0f / 360.0f);
}

inline float PhaseShiftRad(const BandBlendScalars& bb)
{
    return bb.phase_deg * (3.14159265f / 180.0f);
}

inline float CombinedPhase01(const BandBlendScalars& bb, float motion01, float motion_strength = 0.55f)
{
    return PhaseShift01(bb) + motion01 * motion_strength;
}

inline float ApplyMotionToPhase01(float phase01, float motion01, float motion_strength = 0.55f)
{
    if(motion01 <= 1e-6f)
    {
        return phase01;
    }
    return std::fmod(phase01 + motion01 * motion_strength + 1.0f, 1.0f);
}

inline float ApplyMotionToAngleRad(float angle_rad, float motion01, float motion_strength = 0.55f)
{
    if(motion01 <= 1e-6f)
    {
        return angle_rad;
    }
    return angle_rad + motion01 * 6.2831853f * motion_strength;
}

inline float ApplyMotionToUnit01(float value01, float motion01, float motion_strength = 0.28f)
{
    if(motion01 <= 1e-6f)
    {
        return value01;
    }
    return std::fmod(value01 + motion01 * motion_strength + 1.0f, 1.0f);
}

namespace
{
inline void LoadJsonIntArray3Clamp(const nlohmann::json& settings,
                                   const char* key,
                                   std::array<int, 3>& out,
                                   int lo,
                                   int hi)
{
    if(!settings.contains(key) || !settings[key].is_array())
    {
        return;
    }
    const nlohmann::json& a = settings[key];
    for(size_t i = 0; i < 3 && i < a.size(); i++)
    {
        if(a[i].is_number_integer())
        {
            out[i] = std::clamp(a[i].get<int>(), lo, hi);
        }
    }
}
}

inline void LoadBandTuningJson(const nlohmann::json& settings, int& layout_mode, BandTuningPct& b)
{
    if(settings.contains("stratum_layout_mode") && settings["stratum_layout_mode"].is_number_integer())
    {
        layout_mode = std::clamp(settings["stratum_layout_mode"].get<int>(), 0, 1);
    }
    LoadJsonIntArray3Clamp(settings, "stratum_band_speed_pct", b.speed, 0, 200);
    LoadJsonIntArray3Clamp(settings, "stratum_band_tight_pct", b.tight, 25, 300);
    LoadJsonIntArray3Clamp(settings, "stratum_band_phase_deg", b.phase_deg, -180, 180);
    LoadJsonIntArray3Clamp(settings, "stratum_band_motion_kind", b.motion_kind, 0, 6);
    LoadJsonIntArray3Clamp(settings, "stratum_band_motion_rate", b.motion_rate, 0, 200);
}

inline void SaveBandTuningJson(nlohmann::json& j, int layout_mode, const BandTuningPct& b)
{
    j["stratum_layout_mode"] = layout_mode;
    j["stratum_band_speed_pct"] = nlohmann::json::array({b.speed[0], b.speed[1], b.speed[2]});
    j["stratum_band_tight_pct"] = nlohmann::json::array({b.tight[0], b.tight[1], b.tight[2]});
    j["stratum_band_phase_deg"] = nlohmann::json::array({b.phase_deg[0], b.phase_deg[1], b.phase_deg[2]});
    j["stratum_band_motion_kind"] = nlohmann::json::array({b.motion_kind[0], b.motion_kind[1], b.motion_kind[2]});
    j["stratum_band_motion_rate"] = nlohmann::json::array({b.motion_rate[0], b.motion_rate[1], b.motion_rate[2]});
}

}

#endif
