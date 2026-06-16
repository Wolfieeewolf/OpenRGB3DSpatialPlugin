// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialLightingSceneProvider.h"

#include "ControllerLayout3D.h"

SpatialLightingSceneProvider* SpatialLightingSceneProvider::instance()
{
    static SpatialLightingSceneProvider inst;
    return &inst;
}

void SpatialLightingSceneProvider::SetControllers(
    const std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    controllers_ = transforms;
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
