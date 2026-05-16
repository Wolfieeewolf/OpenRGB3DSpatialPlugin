// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALLAYERCORE_H
#define SPATIALLAYERCORE_H

#include "RGBController.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace SpatialLayerCore
{

constexpr int kMaxLayerCount = 4;
constexpr int kSectorCount = 9;

enum class LayerProfileMode : int
{
    Auto = 0,
    ThreeLayer = 3,
    FourLayer = 4
};

struct LayeredProbeSet
{
    bool has_layered = false;
    int layer_count = 0;
    std::array<RGBColor, kMaxLayerCount * kSectorCount> colors{};
};

struct ProbeInput
{
    LayeredProbeSet layered;
};

struct Basis
{
    float forward_x = 0.0f;
    float forward_y = 0.0f;
    float forward_z = 1.0f;
    float up_x = 0.0f;
    float up_y = 1.0f;
    float up_z = 0.0f;
    bool valid = false;
};

struct SamplePoint
{
    float grid_x = 0.0f;
    float grid_y = 0.0f;
    float grid_z = 0.0f;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    float origin_z = 0.0f;
    float y_norm = 0.5f;
};

struct MapperSettings
{
    LayerProfileMode profile_mode = LayerProfileMode::Auto;
    float directional_response = 0.5f;
    float directional_sharpness = 1.6f;
    float center_size = 0.14f;
    float blend_softness = 0.10f;
    float floor_end = 0.30f;
    float desk_end = 0.55f;
    float upper_end = 0.78f;
    float compass_azimuth_offset_rad = 0.0f;
};

struct CompassStratumSample
{
    int sector_i0 = 0;
    int sector_i1 = 0;
    float sector_t = 0.0f;
    float directional_blend = 1.0f;
    float layer_w[kMaxLayerCount]{};
    int layer_count = 4;
    bool valid = false;
};

void ComputeVerticalStratumWeights(float y_norm, const MapperSettings& settings, int layer_count, float* out_w);

void MakeBasisFromEffectEulerDegrees(float yaw_deg, float pitch_deg, float roll_deg, Basis& out);

bool MapPositionToCompassStrata(const Basis& basis,
                                const SamplePoint& sample,
                                const MapperSettings& settings,
                                int layer_count,
                                CompassStratumSample& out);

inline void InitAudioEffectMapperSettings(MapperSettings& m, float normalized_scale_01, float scaled_detail)
{
    const float d = std::clamp(scaled_detail, 0.05f, 1.0f);
    const float s = std::clamp(normalized_scale_01, 0.05f, 1.0f);
    m.floor_end = 0.30f;
    m.desk_end = 0.55f;
    m.upper_end = 0.78f;
    m.blend_softness = std::clamp(0.09f + 0.06f * (1.0f - d), 0.05f, 0.22f);
    m.center_size = std::clamp(0.10f + 0.22f * s, 0.06f, 0.52f);
    m.directional_sharpness = std::clamp(0.95f + d * 0.12f, 0.85f, 2.3f);
}

float CompassStratumHueOffsetDegrees(const Basis& basis,
                                     const SamplePoint& sample,
                                     const MapperSettings& settings,
                                     int layer_count = 3);

float CompassStratumPalettePosition01(const CompassStratumSample& css,
                                      const int layer_spin_sign[3],
                                      float time_scroll,
                                      float plane_mix01,
                                      float plane_angle01);

inline float ShiftGradient01WithCompassHue(float pos01, float compass_hue_deg)
{
    float p = std::fmod(pos01 + compass_hue_deg / 360.0f, 1.0f);
    if(p < 0.0f) p += 1.0f;
    return p;
}

RGBColor ComputeProjectedProbeColor(const ProbeInput& probes,
                                    const Basis& basis,
                                    const SamplePoint& sample,
                                    const MapperSettings& settings,
                                    bool* out_used_layered);

}

#endif
