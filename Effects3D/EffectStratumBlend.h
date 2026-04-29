// SPDX-License-Identifier: GPL-2.0-only
/** Shared floor / mid / ceiling stratum blending (same breaks as Room sampler compass). */

#ifndef EFFECTSTRATUMBLEND_H
#define EFFECTSTRATUMBLEND_H

#include "SpatialLayerCore.h"

#include <nlohmann/json.hpp>
#include <array>
#include <cmath>

namespace EffectStratumBlend
{

/** Standard vertical band breaks (matches typical effect mapper UI). */
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

/** Per-band integer tuning (percent / degrees), edited in UI. */
struct BandTuningPct
{
    std::array<int, 3> speed{{100, 100, 100}};   // 0–200
    std::array<int, 3> tight{{100, 100, 100}};   // 25–300
    std::array<int, 3> phase_deg{{0, 0, 0}};    // -180..180
};

struct BandBlendScalars
{
    float speed_mul = 1.0f;
    float tight_mul = 1.0f;
    float phase_deg = 0.0f;
};

/** layout_mode 0 = off; 1 = blend bands using layer_w[3]. */
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

inline void LoadBandTuningJson(const nlohmann::json& settings,
                               const char* key_mode,
                               int& layout_mode,
                               BandTuningPct& b,
                               const char* key_speed = "stratum_band_speed_pct",
                               const char* key_tight = "stratum_band_tight_pct",
                               const char* key_phase = "stratum_band_phase_deg")
{
    if(settings.contains(key_mode) && settings[key_mode].is_number_integer())
    {
        layout_mode = std::clamp(settings[key_mode].get<int>(), 0, 1);
    }
    auto load3 = [&](const char* key, std::array<int, 3>& out, int lo, int hi) {
        if(!settings.contains(key) || !settings[key].is_array())
        {
            return;
        }
        const auto& a = settings[key];
        for(size_t i = 0; i < 3 && i < a.size(); i++)
        {
            if(a[i].is_number_integer())
            {
                out[i] = std::clamp(a[i].get<int>(), lo, hi);
            }
        }
    };
    load3(key_speed, b.speed, 0, 200);
    load3(key_tight, b.tight, 25, 300);
    load3(key_phase, b.phase_deg, -180, 180);
}

inline void SaveBandTuningJson(nlohmann::json& j,
                               const char* key_mode,
                               int layout_mode,
                               const BandTuningPct& b,
                               const char* key_speed = "stratum_band_speed_pct",
                               const char* key_tight = "stratum_band_tight_pct",
                               const char* key_phase = "stratum_band_phase_deg")
{
    j[key_mode] = layout_mode;
    j[key_speed] = nlohmann::json::array({b.speed[0], b.speed[1], b.speed[2]});
    j[key_tight] = nlohmann::json::array({b.tight[0], b.tight[1], b.tight[2]});
    j[key_phase] = nlohmann::json::array({b.phase_deg[0], b.phase_deg[1], b.phase_deg[2]});
}

} // namespace EffectStratumBlend

#endif
