// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Action.h"
#include "LEDPosition3D.h"
#include "RGBControllerInterface.h"
#include <memory>
#include <string>
#include <vector>

class ZoneManager3D;
struct GpuRoomGridOverlayLayer; /* shared/GpuRoomGridOverlay.h (global namespace) */

namespace Action
{

struct ApplyStats
{
    int tracks_applied = 0;
    int controllers_touched = 0;
    int viewport_leds_painted = 0;
};

bool NameMatches(const std::string& haystack, const std::string& needle);
bool ControllerMatchesDevice(RGBControllerInterface* c, const std::string& device_name);
int FindZoneIndex(RGBControllerInterface* c, const std::string& zone_name);
bool PackIncludesTransform(const Pack& pack, ControllerTransform* transform);
bool TransformMatchesDevice(ControllerTransform* transform, const std::string& device_name);
bool TransformInSceneZone(ControllerTransform* transform,
                          int transform_index,
                          const Target& target,
                          ZoneManager3D* zone_manager);
bool TrackAppliesToTransform(const Pack& pack,
                             const Track& track,
                             ControllerTransform* transform,
                             int transform_index,
                             ZoneManager3D* zone_manager);

/** Room / Movie Sync spatial settings for RoomBox play_bounds and origin_ref. */
struct ApplySpatialContext
{
    /** Effect global zone (-1 = all). Filters which controllers/transforms are painted. */
    int layer_zone_index = -1;
    /** Full room AABB (PlayBounds::RoomBox). */
    bool has_world_bounds = false;
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    /** Scene reference points for clip origin_ref resolution. */
    std::vector<NamedWorldPoint> reference_points;

    /** Active room grid (grid units) — the same GridContext3D bounds stack effects sample.
        When set, RoomBox field clips use the effect origin-local mapping (see
        ClipFieldUsesRoomGrid). */
    bool has_room_grid = false;
    float grid_min_x = 0.0f;
    float grid_max_x = 0.0f;
    float grid_min_y = 0.0f;
    float grid_max_y = 0.0f;
    float grid_min_z = 0.0f;
    float grid_max_z = 0.0f;
    /** Spatial Anchor in grid units (GetEffectOriginGrid; room centre with no effect layer).
        Clip origins start here, exactly like effect origins. */
    float anchor_x = 0.0f;
    float anchor_y = 0.0f;
    float anchor_z = 0.0f;
};

/** True when a field clip (volume / particle / ribbon / built-in GPU) samples the room
    with the stack-effect origin-local mapping: RoomBox play bounds + a room grid. */
bool ClipFieldUsesRoomGrid(const Clip& clip, const ApplySpatialContext* ctx);

/** Clip origin in grid units: anchor (or named reference point) plus origin_offset as a
    percent of the half room — the same rule as SpatialEffect3D::GetEffectOriginGrid. */
Vector3D ResolveClipOriginGrid(const Clip& clip, const ApplySpatialContext& ctx);

/** Origin-local field UV for one LED (anchor = 0.5, clip size = Scale), via the shared
    EffectGpuAtlasUvFromGrid. Returns false outside the clip box — leave that LED unlit. */
bool RoomGridFieldUv(const Clip& clip,
                     const ApplySpatialContext& ctx,
                     const Vector3D& clip_origin,
                     const Vector3D& room_position,
                     float* u, float* v, float* w);

/** Resolve the clip origin and bind it for scripts (SetActiveClipOrigin01) and the GPU
    field samplers. Room-grid field clips get an identity remap because their sample UVs
    are already origin-local. */
void BindClipOrigin(const Clip& clip, const ApplySpatialContext* ctx);

void BuildSpatialAxesForTarget(const Pack& pack,
                               const Target& target,
                               const Clip& sample,
                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                               std::vector<float>* out_axes,
                               std::vector<int>* out_seeds,
                               std::vector<float>* out_nx = nullptr,
                               std::vector<float>* out_ny = nullptr,
                               std::vector<float>* out_nz = nullptr,
                               ZoneManager3D* zone_manager = nullptr,
                               /** Required when sample.play_bounds is RoomBox. */
                               const ApplySpatialContext* room_bounds = nullptr);

ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms = nullptr,
                          bool force_hw_update = false,
                          ZoneManager3D* zone_manager = nullptr);

ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                          bool force_hw_update,
                          ZoneManager3D* zone_manager,
                          const ApplySpatialContext* spatial,
                          /** Optional: room-grid overlay layers for RoomBox field clips. */
                          std::vector<::GpuRoomGridOverlayLayer>* overlay_out = nullptr);

void PrepareControllersForPreview(const std::vector<RGBControllerInterface*>& controllers);

} // namespace Action
