// SPDX-License-Identifier: GPL-2.0-only
// Per-LED spatial lighting: emissive sources, direct light with occlusion, ambient AO.

#ifndef SPATIALLIGHTINGENGINE_H
#define SPATIALLIGHTINGENGINE_H

#include "RGBController.h"
#include "LEDPosition3D.h"

#include <cstdint>
#include <vector>

namespace SpatialLighting
{
class OccluderSpatialIndex;
}

struct GridContext3D;

#include "SpatialLighting/BlockerGridOccluder.h"

namespace SpatialLighting
{

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/** Thin quad occluder (display plane, controller face, etc.) in room grid units. */
struct OccluderQuad
{
    Vec3 corners[4]{};
    Vec3 normal{};
    bool double_sided = true;
    /** Controller index in layout list, or -1 for planes / room walls. */
    int controller_index = -1;
};

/** Axis-aligned blocker volume in room/world grid units (fast ray tests). */
struct OccluderAabb
{
    Vec3 min{};
    Vec3 max{};
    int controller_index = -1;
};

struct OccluderBuildOptions
{
    bool display_planes = true;
    bool room_walls = false;
    bool controllers = true;
    /** Custom-controller light blocker cells (independent of full controller body occlusion). */
    bool light_blockers = false;
};

struct EmissiveSource
{
    Vec3 position{};
    /** Tight glow core (campfire/blob). */
    float radius = 0.08f;
    /** Direct light falloff distance (can be larger than glow). */
    float light_radius = 0.35f;
    float r = 1.0f;
    float g = 0.45f;
    float b = 0.12f;
    float emissive_strength = 1.0f;
    float light_strength = 1.2f;
};

struct ShadeSettings
{
    float ambient_level = 0.12f;
    float ao_strength = 0.65f;
    /** Omnidirectional fill (campfire lighting the room), not a spotlight at the viewer. */
    float room_fill_strength = 0.45f;
    float room_fill_atten = 1.0f;
    float direct_falloff = 1.0f;
    /** Axis probe length for AO (room grid units). */
    float ao_probe_span = 1.0f;
    bool use_occlusion = true;
    /** Six-axis AO probes per LED (expensive when many blockers/controllers). */
    bool use_ambient_occlusion = true;
};

struct RoomScene
{
    EmissiveSource source{};
    /** When non-empty, ShadeLed sums all entries (emitter relay). Else uses `source`. */
    std::vector<EmissiveSource> sources;
    std::vector<OccluderQuad> occluders;
    std::vector<OccluderAabb> occluder_aabbs;
    std::vector<BlockerGridOccluder> blocker_grids;
    /** When set, AABB ray tests use this index (built from occluder_aabbs). */
    const OccluderSpatialIndex* occluder_aabb_index = nullptr;
    ShadeSettings shade{};
};

/** Build occluders from visible display planes (room grid units). */
void AppendDisplayPlaneOccluders(std::vector<OccluderQuad>& out, float grid_scale_mm);

void AppendRoomWallOccluders(std::vector<OccluderQuad>& out,
                             float min_x,
                             float min_y,
                             float min_z,
                             float max_x,
                             float max_y,
                             float max_z);

void AppendControllerOccluders(std::vector<OccluderAabb>& out, float grid_scale_mm);

void BuildSpatialOccluders(std::vector<OccluderQuad>& out,
                           std::vector<OccluderAabb>& aabbs,
                           const GridContext3D& grid,
                           const OccluderBuildOptions& options);

Vec3 GridToVec3(float x, float y, float z);

RGBColor ShadeLed(const RoomScene& scene, float led_x, float led_y, float led_z);

/** Line-of-sight test for mirror relay / receiver shading (skips receiver + optional emitter controller). */
bool LedSegmentOccluded(float ax,
                        float ay,
                        float az,
                        float bx,
                        float by,
                        float bz,
                        const std::vector<OccluderAabb>& aabbs,
                        const std::vector<OccluderQuad>& quads,
                        int also_skip_controller = -1);

/** Six-axis openness at a receiver LED (1 = fully open, 0 = fully occluded). */
float ComputeLedAmbientOcclusion(float led_x,
                                 float led_y,
                                 float led_z,
                                 const std::vector<OccluderAabb>& aabbs,
                                 const std::vector<OccluderQuad>& quads,
                                 float probe_span,
                                 const OccluderSpatialIndex* aabb_index = nullptr);

/** Receiver shading: center shadow + AO, combined into one multiplier in [0,1]. */
float ComputeRoomAmbientShadeFactor(float room_x,
                                    float room_y,
                                    float room_z,
                                    float room_center_x,
                                    float room_center_y,
                                    float room_center_z,
                                    const std::vector<OccluderAabb>& aabbs,
                                    const std::vector<OccluderQuad>& quads,
                                    const std::vector<BlockerGridOccluder>& blocker_grids,
                                    float ao_strength_norm,
                                    float probe_span,
                                    const OccluderSpatialIndex* aabb_index = nullptr);

void BuildOccluderAabbSpatialIndex(const std::vector<OccluderAabb>& aabbs,
                                   const GridContext3D& grid,
                                   OccluderSpatialIndex& out_index);

} // namespace SpatialLighting

#endif
