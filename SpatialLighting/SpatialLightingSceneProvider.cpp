// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialLightingSceneProvider.h"

#include "ControllerLayout3D.h"
#include "SpatialEffect3D.h"

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
}

void SpatialLightingSceneProvider::EnsureFrameOccluders(const GridContext3D& grid,
                                                        const SpatialLighting::OccluderBuildOptions& options)
{
    (void)grid;
    if(frame_occluders_valid_ && frame_occluder_options_.display_planes == options.display_planes &&
       frame_occluder_options_.room_walls == options.room_walls &&
       frame_occluder_options_.controllers == options.controllers)
    {
        return;
    }

    frame_occluder_quads_.clear();
    frame_occluder_aabbs_.clear();
    SpatialLighting::BuildSpatialOccluders(frame_occluder_quads_, frame_occluder_aabbs_, grid, options);
    frame_occluder_options_ = options;
    frame_occluders_valid_ = true;
}
