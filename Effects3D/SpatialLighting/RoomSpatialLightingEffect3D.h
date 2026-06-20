// SPDX-License-Identifier: GPL-2.0-only
// Base for spatial lighting effects: light position is fixed in room space.

#ifndef ROOMSPATIALLIGHTINGEFFECT3D_H
#define ROOMSPATIALLIGHTINGEFFECT3D_H

#include "SpatialEffect3D.h"
#include "SpatialRoom/SpatialRoomTypes.h"
#include "SpatialRoom/SpatialRoomDefaults.h"
#include "SpatialLighting/OccluderSpatialIndex.h"
#include "SpatialLighting/SpatialLightingEngine.h"
#include "RoomSpatialLightingUi.h"

#include <vector>

/** Stack reference = viewer only; do not warp LED sample coords around the anchor. */
class RoomSpatialLightingEffect3D : public SpatialEffect3D
{
public:
    using SpatialEffect3D::SpatialEffect3D;

    SpatialRoom::SpatialRoomMode GetSpatialRoomMode() const override
    {
        return SpatialRoom::SpatialRoomMode::SpatialLighting;
    }

    SpatialRoom::SpatialRoomCapabilities GetSpatialRoomCapabilities() const override
    {
        return SpatialRoom::DefaultCapabilitiesForMode(SpatialRoom::SpatialRoomMode::SpatialLighting);
    }

    bool SkipsSpatialSampleWarp() const override { return true; }
    bool RequiresWorldSpaceCoordinates() const override { return false; }
    bool UsesSpatialSamplingQuantization() const override { return false; }

protected:
    RoomSpatialLightingUi::RoomSpatialLightParams room_light_{};

    virtual void ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const;
    virtual float RoomLightEmissiveMul() const { return 1.0f; }
    virtual float RoomLightDirectMul() const { return 0.85f; }
    void InvalidateLightingScene() { lighting_scene_epoch_++; }
    void MarkRoomLightPlacementDirty();

    bool LightingSceneEpochChanged(std::uint64_t grid_seq) const
    {
        return cached_lighting_grid_seq_ != grid_seq || cached_lighting_ref_epoch_ != lighting_scene_epoch_;
    }

    void MarkLightingSceneBuilt(std::uint64_t grid_seq)
    {
        cached_lighting_grid_seq_ = grid_seq;
        cached_lighting_ref_epoch_ = lighting_scene_epoch_;
    }

    void RefreshRoomLightOccluders(const GridContext3D& grid) const;
    void UpdateRoomLightSource(const GridContext3D& grid, RGBColor color) const;
    void RebuildRoomLightScene(const GridContext3D& grid, RGBColor color);
    RGBColor ShadeRoomLightAt(float x, float y, float z, const GridContext3D& grid, RGBColor source_color);
    SpatialLighting::Vec3 ResolvedRoomLightPosition(const GridContext3D& grid) const;

    mutable SpatialLighting::RoomScene cached_scene_{};
    mutable std::vector<SpatialLighting::OccluderQuad> occluders_{};
    mutable std::vector<SpatialLighting::OccluderAabb> occluder_aabbs_{};
    mutable SpatialLighting::OccluderSpatialIndex occluder_aabb_index_{};
    mutable std::vector<SpatialLighting::BlockerGridOccluder> blocker_grids_{};
    mutable SpatialLighting::RoomBlockerField room_blocker_field_{};

private:
    SpatialLighting::Vec3 fixed_light_pos_{};
    bool fixed_light_valid_ = false;
    std::uint64_t lighting_scene_epoch_ = 0;
    mutable std::uint64_t cached_lighting_grid_seq_ = 0;
    mutable std::uint64_t cached_lighting_ref_epoch_ = 0;
};

#endif
