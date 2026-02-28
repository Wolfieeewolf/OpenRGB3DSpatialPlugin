// SPDX-License-Identifier: GPL-2.0-only

#ifndef VISOR3D_H
#define VISOR3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Visor3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Visor3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Visor3D", "Visor (KITT)", "3D Spatial", [](){ return new Visor3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    float beam_width = 0.15f;
};

#endif
