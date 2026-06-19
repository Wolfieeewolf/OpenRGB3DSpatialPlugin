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
    SpatialLighting::BuildSpatialOccluders(frame_occluder_quads_, frame_occluder_aabbs_, grid, options);
    if(options.light_blockers)
    {
        SpatialLighting::BuildBlockerGridOccluders(frame_blocker_grids_, grid.grid_scale_mm);
    }
    SpatialLighting::BuildOccluderAabbSpatialIndex(frame_occluder_aabbs_, grid, frame_occluder_index_);
    frame_occluder_options_ = options;
    frame_occluders_valid_ = true;
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

} // namespace

void SpatialLightingSceneProvider::BeginAmbientShadeCacheFrame(std::uint64_t render_sequence, float quant_size)
{
    if(shade_cache_epoch_ != render_sequence)
    {
        shade_cache_.clear();
        shade_slot_cache_.clear();
        shade_cache_epoch_ = render_sequence;
        shade_cache_quant_ = std::max(quant_size, 0.05f);
    }
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
        const auto slot_found = shade_slot_cache_.find(shade_slot);
        if(slot_found != shade_slot_cache_.end())
        {
            return slot_found->second;
        }
    }
    else
    {
        const std::uint64_t key = PackAmbientShadeKey(room_x, room_y, room_z, shade_cache_quant_);
        const auto found = shade_cache_.find(key);
        if(found != shade_cache_.end())
        {
            return found->second;
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
                                                                            &frame_occluder_index_);
    if(shade_slot >= 0)
    {
        shade_slot_cache_[shade_slot] = shade_factor;
    }
    else
    {
        const std::uint64_t key = PackAmbientShadeKey(room_x, room_y, room_z, shade_cache_quant_);
        shade_cache_[key] = shade_factor;
    }
    return shade_factor;
}
