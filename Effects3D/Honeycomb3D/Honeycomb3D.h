// SPDX-License-Identifier: GPL-2.0-only

#ifndef HONEYCOMB3D_H
#define HONEYCOMB3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Honeycomb3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Honeycomb3D(QWidget* parent = nullptr);
    ~Honeycomb3D() override;

    EFFECT_REGISTERER_3D("Honeycomb3D", "Hex Lattice 3D", "Spatial", []() { return new Honeycomb3D; });

    static std::string const ClassName() { return "Honeycomb3D"; }
    static std::string const UIName() { return "Hex Lattice 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    static RGBColor Hsv01ToBgr(float h, float s, float v);
};

#endif
