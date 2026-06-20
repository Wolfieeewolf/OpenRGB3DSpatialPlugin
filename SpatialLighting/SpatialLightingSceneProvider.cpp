// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialLightingSceneProvider.h"

#include "ControllerLayout3D.h"
#include "SpatialEffect3D.h"
#include "SpatialLighting/BlockerGridOccluder.h"

#include <cmath>

SpatialLightingSceneProvider* SpatialLightingSceneProvider::instance()
{
    static SpatialLightingSceneProvider inst;
    return &inst;
}

void SpatialLightingSceneProvider::SetControllers(
    const std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    if(controllers_ != transforms)
    {
        controllers_ = transforms;
        InvalidateFrameOccluders();
    }
}

void SpatialLightingSceneProvider::ClearEmitterRelayFrame()
{
    emitter_relay_mirror_active_ = false;
    emitter_relay_mirror_ = EmitterRelayMirror::MirrorFrame{};
    emitter_controller_indices_.clear();
}

void SpatialLightingSceneProvider::SetEmitterRelayMirrorFrame(
    EmitterRelayMirror::MirrorFrame frame,
    std::unordered_set<int> emitter_controller_indices)
{
    emitter_relay_mirror_active_ = !frame.surfaces.empty();
    emitter_relay_mirror_ = std::move(frame);
    emitter_controller_indices_ = std::move(emitter_controller_indices);
}

bool SpatialLightingSceneProvider::isEmitterController(int controller_index) const
{
    return emitter_controller_indices_.find(controller_index) != emitter_controller_indices_.end();
}

void SpatialLightingSceneProvider::InvalidateFrameOccluders()
{
    frame_occluders_valid_ = false;
    frame_occluder_index_.Clear();
    frame_blocker_grids_.clear();
    frame_room_blocker_field_ = SpatialLighting::RoomBlockerField{};
    ++scene_geometry_epoch_;
}

void SpatialLightingSceneProvider::EnsureFrameOccluders(const GridContext3D& grid,
                                                        const SpatialLighting::OccluderBuildOptions& options)
{
    if(frame_occluders_valid_ && frame_occluder_options_.display_planes == options.display_planes &&
       frame_occluder_options_.room_walls == options.room_walls &&
       frame_occluder_options_.controllers == options.controllers &&
       frame_occluder_options_.light_blockers == options.light_blockers)
    {
        return;
    }

    frame_occluder_quads_.clear();
    frame_occluder_aabbs_.clear();
    frame_occluder_index_.Clear();
    frame_blocker_grids_.clear();
    frame_room_blocker_field_ = SpatialLighting::RoomBlockerField{};
    SpatialLighting::BuildSpatialOccluders(frame_occluder_quads_, frame_occluder_aabbs_, grid, options);
    if(options.light_blockers)
    {
        SpatialLighting::BuildBlockerGridOccluders(frame_blocker_grids_, grid.grid_scale_mm);
        SpatialLighting::BuildRoomBlockerField(frame_room_blocker_field_, grid.grid_scale_mm, &grid);
    }
    SpatialLighting::BuildOccluderAabbSpatialIndex(frame_occluder_aabbs_, grid, frame_occluder_index_);
    frame_occluder_options_ = options;
    frame_occluders_valid_ = true;
    ++scene_geometry_epoch_;
}

namespace
{

std::uint64_t PackAmbientShadeKey(float x, float y, float z, float quant)
{
    const auto q = [quant](float v) -> std::uint64_t {
        const int qi = static_cast<int>(std::floor(v / quant));
        return static_cast<std::uint64_t>(qi + 0x100000) & 0x1FFFFFu;
    };
    return q(x) | (q(y) << 21) | (q(z) << 42);
}

struct AmbientShadeCacheEntry
{
    float shade_factor = 1.0f;
    float room_center_x = 0.0f;
    float room_center_y = 0.0f;
    float room_center_z = 0.0f;
    float ao_strength_norm = 0.0f;
    float probe_span = 0.0f;
};

bool ShadeCacheParamsMatch(const AmbientShadeCacheEntry& entry,
                           float room_center_x,
                           float room_center_y,
                           float room_center_z,
                           float ao_strength_norm,
                           float probe_span)
{
    constexpr float kEps = 1e-4f;
    return std::fabs(entry.room_center_x - room_center_x) < kEps && std::fabs(entry.room_center_y - room_center_y) < kEps &&
           std::fabs(entry.room_center_z - room_center_z) < kEps &&
           std::fabs(entry.ao_strength_norm - ao_strength_norm) < kEps && std::fabs(entry.probe_span - probe_span) < kEps;
}

thread_local std::unordered_map<std::uint64_t, AmbientShadeCacheEntry> g_shade_position_cache;
thread_local std::unordered_map<int, AmbientShadeCacheEntry> g_shade_slot_cache;
thread_local std::uint64_t g_shade_cache_geometry_epoch = 0;

} // namespace

void SpatialLightingSceneProvider::BeginAmbientShadeCacheFrame(std::uint64_t render_sequence, float quant_size)
{
    (void)render_sequence;
    if(g_shade_cache_geometry_epoch != scene_geometry_epoch_)
    {
        g_shade_position_cache.clear();
        g_shade_slot_cache.clear();
        g_shade_cache_geometry_epoch = scene_geometry_epoch_;
    }
    shade_cache_quant_ = std::max(quant_size, 0.05f);
}

float SpatialLightingSceneProvider::ComputeAmbientShadeFactorCached(int shade_slot,
                                                                  float room_x,
                                                                  float room_y,
                                                                  float room_z,
                                                                  float room_center_x,
                                                                  float room_center_y,
                                                                  float room_center_z,
                                                                  float ao_strength_norm,
                                                                  float probe_span)
{
    if(shade_slot >= 0)
    {
        const auto slot_found = g_shade_slot_cache.find(shade_slot);
        if(slot_found != g_shade_slot_cache.end() &&
           ShadeCacheParamsMatch(slot_found->second,
                                 room_center_x,
                                 room_center_y,
                                 room_center_z,
                                 ao_strength_norm,
                                 probe_span))
        {
            return slot_found->second.shade_factor;
        }
    }
    else
    {
        const std::uint64_t key = PackAmbientShadeKey(room_x, room_y, room_z, shade_cache_quant_);
        const auto found = g_shade_position_cache.find(key);
        if(found != g_shade_position_cache.end() &&
           ShadeCacheParamsMatch(found->second,
                                 room_center_x,
                                 room_center_y,
                                 room_center_z,
                                 ao_strength_norm,
                                 probe_span))
        {
            return found->second.shade_factor;
        }
    }

    const float shade_factor = SpatialLighting::ComputeRoomAmbientShadeFactor(room_x,
                                                                            room_y,
                                                                            room_z,
                                                                            room_center_x,
                                                                            room_center_y,
                                                                            room_center_z,
                                                                            frame_occluder_aabbs_,
                                                                            frame_occluder_quads_,
                                                                            frame_blocker_grids_,
                                                                            ao_strength_norm,
                                                                            probe_span,
                                                                            &frame_occluder_index_,
                                                                            frame_room_blocker_field_.IsValid()
                                                                                ? &frame_room_blocker_field_
                                                                                : nullptr);
    AmbientShadeCacheEntry entry{};
    entry.shade_factor = shade_factor;
    entry.room_center_x = room_center_x;
    entry.room_center_y = room_center_y;
    entry.room_center_z = room_center_z;
    entry.ao_strength_norm = ao_strength_norm;
    entry.probe_span = probe_span;

    if(shade_slot >= 0)
    {
        g_shade_slot_cache[shade_slot] = entry;
    }
    else
    {
        const std::uint64_t key = PackAmbientShadeKey(room_x, room_y, room_z, shade_cache_quant_);
        g_shade_position_cache[key] = entry;
    }
    return shade_factor;
}
