// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALLAYERCORE_H
#define SPATIALLAYERCORE_H

#include "RGBController.h"

#include <array>

namespace SpatialLayerCore
{

constexpr int kMaxLayerCount = 4;
constexpr int kSectorCount = 9; // N,NE,E,SE,S,SW,W,NW,Center

enum class LayerProfileMode : int
{
    Auto = 0,
    ThreeLayer = 3,
    FourLayer = 4
};

struct LayeredProbeSet
{
    bool has_layered = false;
    int layer_count = 0; // 3 or 4
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
    /** Added to atan2(lx,lz) before 8-sector indexing (N,NE,…). Rotates probe compass vs. room axes. */
    float compass_azimuth_offset_rad = 0.0f;
};

RGBColor ComputeProjectedProbeColor(const ProbeInput& probes,
                                    const Basis& basis,
                                    const SamplePoint& sample,
                                    const MapperSettings& settings,
                                    bool* out_used_layered);

}

#endif
