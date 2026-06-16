// SPDX-License-Identifier: GPL-2.0-only
// Screen-mirror-style relay: receiver LEDs sample emitter device planes by UV projection.

#ifndef EMITTERRELAYMIRROR_H
#define EMITTERRELAYMIRROR_H

#include "RGBController.h"
#include "LEDPosition3D.h"
#include "SpatialLighting/SpatialLightingEngine.h"

#include <cstdint>
#include <vector>

struct ControllerTransform;
struct GridContext3D;

namespace EmitterRelayMirror
{

struct LedColorSample
{
    float u = 0.0f;
    float v = 0.0f;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

struct EmitterSurface
{
    int controller_index = -1;
    Vector3D plane_center_room{};
    Rotation3D plane_rotation{};
    float width_grid = 1.0f;
    float height_grid = 1.0f;
    int tex_w = 48;
    int tex_h = 48;
    std::vector<float> tex_rgb;
    std::vector<uint8_t> tex_weight;
};

struct MirrorFrame
{
    std::vector<EmitterSurface> surfaces;
    Vector3D reference_room{};
    float reference_max_distance_mm = 3000.0f;
    float grid_scale_mm = 10.0f;
    /** Direct falloff range from emitters (mm). */
    float light_reach_mm = 280.0f;
    /** Edge feather for reach falloff (percent of range). */
    float glow_feather_percent = 30.0f;
    /** 0..1 room wash added on receivers. */
    float room_fill_strength = 0.35f;
    float coverage = 1.0f;
    float edge_softness = 18.0f;
    float brightness = 1.0f;
};

struct MirrorShadeContext
{
    const SpatialLighting::ShadeSettings* shade = nullptr;
    const std::vector<SpatialLighting::OccluderAabb>* occluder_aabbs = nullptr;
    const std::vector<SpatialLighting::OccluderQuad>* occluders = nullptr;
};

void BuildSurfaceFromSamples(int controller_index,
                             const ControllerTransform* ctrl,
                             const std::vector<LedColorSample>& samples,
                             EmitterSurface& out);

RGBColor SampleReceiver(const MirrorFrame& frame,
                        float room_x,
                        float room_y,
                        float room_z,
                        const MirrorShadeContext* shade_ctx = nullptr);

} // namespace EmitterRelayMirror

#endif
