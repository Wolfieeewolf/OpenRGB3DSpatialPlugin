// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALLIGHTINGSCENEPROVIDER_H
#define SPATIALLIGHTINGSCENEPROVIDER_H

#include "SpatialLighting/EmitterRelayMirror.h"
#include "SpatialLighting/BlockerGridOccluder.h"
#include "SpatialLighting/OccluderSpatialIndex.h"
#include "SpatialLighting/SpatialLightingEngine.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ControllerTransform;
struct GridContext3D;

class SpatialLightingSceneProvider
{
public:
    static SpatialLightingSceneProvider* instance();

    void SetControllers(const std::vector<std::unique_ptr<ControllerTransform>>* transforms);
    const std::vector<std::unique_ptr<ControllerTransform>>* controllers() const { return controllers_; }

    void SetShadingControllerIndex(int index) { shading_controller_index_ = index; }
    int shadingControllerIndex() const { return shading_controller_index_; }

    void ClearEmitterRelayFrame();
    void SetEmitterRelayMirrorFrame(EmitterRelayMirror::MirrorFrame frame,
                                    std::unordered_set<int> emitter_controller_indices);
    bool emitterRelayMirrorActive() const { return emitter_relay_mirror_active_; }
    const EmitterRelayMirror::MirrorFrame& emitterRelayMirrorFrame() const { return emitter_relay_mirror_; }
    bool isEmitterController(int controller_index) const;

    void InvalidateFrameOccluders();
    void EnsureFrameOccluders(const GridContext3D& grid, const SpatialLighting::OccluderBuildOptions& options);
    const std::vector<SpatialLighting::OccluderQuad>& frameOccluderQuads() const { return frame_occluder_quads_; }
    const std::vector<SpatialLighting::OccluderAabb>& frameOccluderAabbs() const { return frame_occluder_aabbs_; }
    const SpatialLighting::OccluderSpatialIndex& frameOccluderSpatialIndex() const { return frame_occluder_index_; }
    const std::vector<SpatialLighting::BlockerGridOccluder>& frameBlockerGrids() const { return frame_blocker_grids_; }

    void BeginAmbientShadeCacheFrame(std::uint64_t render_sequence, float quant_size);
    float ComputeAmbientShadeFactorCached(int shade_slot,
                                          float room_x,
                                          float room_y,
                                          float room_z,
                                          float room_center_x,
                                          float room_center_y,
                                          float room_center_z,
                                          float ao_strength_norm,
                                          float probe_span);

private:
    SpatialLightingSceneProvider() = default;

    const std::vector<std::unique_ptr<ControllerTransform>>* controllers_ = nullptr;
    int shading_controller_index_ = -1;

    bool emitter_relay_mirror_active_ = false;
    EmitterRelayMirror::MirrorFrame emitter_relay_mirror_;
    std::unordered_set<int> emitter_controller_indices_;

    bool frame_occluders_valid_ = false;
    SpatialLighting::OccluderBuildOptions frame_occluder_options_{};
    std::vector<SpatialLighting::OccluderQuad> frame_occluder_quads_;
    std::vector<SpatialLighting::OccluderAabb> frame_occluder_aabbs_;
    SpatialLighting::OccluderSpatialIndex frame_occluder_index_;
    std::vector<SpatialLighting::BlockerGridOccluder> frame_blocker_grids_;

    std::uint64_t shade_cache_epoch_ = 0;
    float shade_cache_quant_ = 1.0f;
    std::unordered_map<std::uint64_t, float> shade_cache_;
    std::unordered_map<int, float> shade_slot_cache_;
};

#endif
