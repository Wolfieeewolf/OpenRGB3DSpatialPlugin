// SPDX-License-Identifier: GPL-2.0-only

#ifndef CROSSINGBEAMS3D_H
#define CROSSINGBEAMS3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class CrossingBeams3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit CrossingBeams3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("CrossingBeams3D", "Crossing Beams", "3D Spatial", [](){ return new CrossingBeams3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    float beam_thickness = 0.08f;
    float glow = 0.5f;
};

#endif
