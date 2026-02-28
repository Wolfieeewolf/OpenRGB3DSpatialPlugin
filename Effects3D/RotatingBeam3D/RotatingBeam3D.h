// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROTATINGBEAM3D_H
#define ROTATINGBEAM3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class RotatingBeam3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit RotatingBeam3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("RotatingBeam3D", "Rotating Beam", "3D Spatial", [](){ return new RotatingBeam3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    float beam_width = 0.15f;
    float glow = 0.5f;
};

#endif
