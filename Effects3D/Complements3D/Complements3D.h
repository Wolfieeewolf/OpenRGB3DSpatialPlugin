// SPDX-License-Identifier: GPL-2.0-only

#ifndef COMPLEMENTS3D_H
#define COMPLEMENTS3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class Complements3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Complements3D(QWidget* parent = nullptr);
    ~Complements3D() override;

    EFFECT_REGISTERER_3D("Complements3D", "Dual Tone Depth 3D", "Spatial", []() { return new Complements3D; });

    static std::string const ClassName() { return "Complements3D"; }
    static std::string const UIName() { return "Dual Tone Depth 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
};

#endif
