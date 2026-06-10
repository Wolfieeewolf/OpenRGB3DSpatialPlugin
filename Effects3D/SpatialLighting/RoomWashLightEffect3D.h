// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMWASHLIGHTEFFECT3D_H
#define ROOMWASHLIGHTEFFECT3D_H

#include "RoomSpatialLightingEffect3D.h"
#include "EffectRegisterer3D.h"

class RoomWashLightEffect3D : public RoomSpatialLightingEffect3D
{
    Q_OBJECT

public:
    explicit RoomWashLightEffect3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("RoomWashLight", "Room wash light", "Spatial · Lighting",
                          []() { return new RoomWashLightEffect3D; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private:
    void ApplyLiveShadeSettings(SpatialLighting::RoomScene& scene) const override;
};

#endif
