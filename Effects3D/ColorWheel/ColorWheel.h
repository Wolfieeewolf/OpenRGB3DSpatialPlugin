// SPDX-License-Identifier: GPL-2.0-only

#ifndef COLORWHEEL_H
#define COLORWHEEL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class QComboBox;

class ColorWheel : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit ColorWheel(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("ColorWheel", "Color Wheel", "Spatial", [](){ return new ColorWheel; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    int direction = 0;
    int hue_geometry_mode = 0;
};

#endif
