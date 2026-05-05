// SPDX-License-Identifier: GPL-2.0-only

#ifndef XORCERY3D_H
#define XORCERY3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class Xorcery3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Xorcery3D(QWidget* parent = nullptr);
    ~Xorcery3D() override;

    EFFECT_REGISTERER_3D("Xorcery3D", "Bitwarp 3D", "Spatial", []() { return new Xorcery3D; });

    static std::string const ClassName() { return "Xorcery3D"; }
    static std::string const UIName() { return "Bitwarp 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
};

#endif
