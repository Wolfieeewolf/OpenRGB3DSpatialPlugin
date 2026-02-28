// SPDX-License-Identifier: GPL-2.0-only

#ifndef MOVINGPANES3D_H
#define MOVINGPANES3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class MovingPanes3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit MovingPanes3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("MovingPanes3D", "Moving Panes", "3D Spatial", [](){ return new MovingPanes3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    int num_divisions = 4;
};

#endif
