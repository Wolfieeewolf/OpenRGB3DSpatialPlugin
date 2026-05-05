// SPDX-License-Identifier: GPL-2.0-only

#ifndef RGBXYZOCTANTS3D_H
#define RGBXYZOCTANTS3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class RgbXyzOctants3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit RgbXyzOctants3D(QWidget* parent = nullptr);
    ~RgbXyzOctants3D() override;

    EFFECT_REGISTERER_3D("RgbXyzOctants3D", "Axis Octants 3D", "Spatial", []() { return new RgbXyzOctants3D; });

    static std::string const ClassName() { return "RgbXyzOctants3D"; }
    static std::string const UIName() { return "Axis Octants 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
};

#endif
