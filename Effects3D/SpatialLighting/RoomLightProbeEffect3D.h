// SPDX-License-Identifier: GPL-2.0-only
// Static test source for spatial lighting engine (no flicker).

#ifndef ROOMLIGHTPROBEEFFECT3D_H
#define ROOMLIGHTPROBEEFFECT3D_H

#include "RoomSpatialLightingEffect3D.h"
#include "EffectRegisterer3D.h"
#include "SpatialLighting/SpatialLightingEngine.h"

#include <vector>

class RoomLightProbeEffect3D : public RoomSpatialLightingEffect3D
{
    Q_OBJECT

public:
    explicit RoomLightProbeEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("RoomLightProbe", "Room light probe (test)", "Spatial · Lighting",
                          []() { return new RoomLightProbeEffect3D; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private:
    void RebuildScene(const GridContext3D& grid);
    void RefreshOccluders(const GridContext3D& grid) const;
    void ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const override;

    mutable SpatialLighting::RoomScene cached_scene_{};
    mutable std::vector<SpatialLighting::OccluderQuad> occluders_{};
    mutable std::vector<SpatialLighting::OccluderAabb> occluder_aabbs_{};
};

#endif
