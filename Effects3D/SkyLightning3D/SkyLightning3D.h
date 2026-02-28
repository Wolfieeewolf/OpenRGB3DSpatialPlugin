// SPDX-License-Identifier: GPL-2.0-only

#ifndef SKYLIGHTNING3D_H
#define SKYLIGHTNING3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class SkyLightning3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit SkyLightning3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("SkyLightning3D", "Sky Lightning", "3D Spatial", [](){ return new SkyLightning3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    float flash_rate = 0.15f;
    float flash_duration = 0.08f;
};

#endif
