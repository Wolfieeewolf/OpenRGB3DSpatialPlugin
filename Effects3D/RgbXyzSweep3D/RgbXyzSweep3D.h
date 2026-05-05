// SPDX-License-Identifier: GPL-2.0-only

#ifndef RGBXYZSWEEP3D_H
#define RGBXYZSWEEP3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class QLabel;
class QSlider;

class RgbXyzSweep3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit RgbXyzSweep3D(QWidget* parent = nullptr);
    ~RgbXyzSweep3D() override;

    EFFECT_REGISTERER_3D("RgbXyzSweep3D", "Axis Sweep 3D", "Spatial", []() { return new RgbXyzSweep3D; });

    static std::string const ClassName() { return "RgbXyzSweep3D"; }
    static std::string const UIName() { return "Axis Sweep 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    int band_style = 1; // 0 sharp, 1 smooth
    QSlider* style_slider = nullptr;
    QLabel* style_label = nullptr;
};

#endif
