// SPDX-License-Identifier: GPL-2.0-only
// Scene pointers for spatial lighting occlusion (synced during effect render).

#ifndef SPATIALLIGHTINGSCENEPROVIDER_H
#define SPATIALLIGHTINGSCENEPROVIDER_H

#include "SpatialLightingEngine.h"
#include "SpatialLighting/EmitterRelayMirror.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

class SpatialLightingSceneProvider
{
public:
    static SpatialLightingSceneProvider* instance();

    void SetControllers(const std::vector<std::unique_ptr<ControllerTransform>>* transforms);
    const std::vector<std::unique_ptr<ControllerTransform>>* controllers() const { return controllers_; }

    void SetShadingControllerIndex(int index) { shading_controller_index_ = index; }
    int shadingControllerIndex() const { return shading_controller_index_; }

    /** Room grid overlay: sample relay/lighting at each voxel (same shading as LEDs). */
    void SetRoomGridOverlayPreview(bool preview) { (void)preview; }
    bool roomGridOverlayPreview() const { return false; }

    void ClearEmitterRelayFrame();
    void SetEmitterRelayFrame(std::vector<SpatialLighting::EmissiveSource> sources,
                              std::unordered_set<int> emitter_controller_indices);
    void SetEmitterRelayMirrorFrame(EmitterRelayMirror::MirrorFrame frame,
                                    std::unordered_set<int> emitter_controller_indices);
    bool emitterRelayActive() const { return emitter_relay_active_; }
    bool emitterRelayMirrorActive() const { return emitter_relay_mirror_active_; }
    const std::vector<SpatialLighting::EmissiveSource>& emitterRelaySources() const { return emitter_relay_sources_; }
    const EmitterRelayMirror::MirrorFrame& emitterRelayMirrorFrame() const { return emitter_relay_mirror_; }
    bool isEmitterController(int controller_index) const;

private:
    SpatialLightingSceneProvider() = default;

    const std::vector<std::unique_ptr<ControllerTransform>>* controllers_ = nullptr;
    int shading_controller_index_ = -1;

    bool emitter_relay_active_ = false;
    bool emitter_relay_mirror_active_ = false;
    std::vector<SpatialLighting::EmissiveSource> emitter_relay_sources_;
    EmitterRelayMirror::MirrorFrame emitter_relay_mirror_;
    std::unordered_set<int> emitter_controller_indices_;
};

#endif
