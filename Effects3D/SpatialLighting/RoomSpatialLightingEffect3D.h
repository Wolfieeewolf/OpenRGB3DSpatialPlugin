// SPDX-License-Identifier: GPL-2.0-only
// Base for spatial lighting effects: light position is fixed in room space.

#ifndef ROOMSPATIALLIGHTINGEFFECT3D_H
#define ROOMSPATIALLIGHTINGEFFECT3D_H

#include "SpatialEffect3D.h"
#include "SpatialRoom/SpatialRoomTypes.h"
#include "SpatialRoom/SpatialRoomDefaults.h"
#include "SpatialLighting/SpatialLightingEngine.h"

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
    virtual void ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const;
    void InvalidateLightingScene() { lighting_scene_epoch_++; }

    bool LightingSceneEpochChanged(std::uint64_t grid_seq) const
    {
        return cached_lighting_grid_seq_ != grid_seq || cached_lighting_ref_epoch_ != lighting_scene_epoch_;
    }

    void MarkLightingSceneBuilt(std::uint64_t grid_seq)
    {
        cached_lighting_grid_seq_ = grid_seq;
        cached_lighting_ref_epoch_ = lighting_scene_epoch_;
    }

private:
    std::uint64_t lighting_scene_epoch_ = 0;
    mutable std::uint64_t cached_lighting_grid_seq_ = 0;
    mutable std::uint64_t cached_lighting_ref_epoch_ = 0;
};

#endif
