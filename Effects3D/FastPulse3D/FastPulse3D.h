// SPDX-License-Identifier: GPL-2.0-only

#ifndef FASTPULSE3D_H
#define FASTPULSE3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class FastPulse3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit FastPulse3D(QWidget* parent = nullptr);
    ~FastPulse3D() override;

    EFFECT_REGISTERER_3D("FastPulse3D", "Rapid Pulse 3D", "Spatial", []() { return new FastPulse3D; });

    static std::string const ClassName() { return "FastPulse3D"; }
    static std::string const UIName() { return "Rapid Pulse 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
};

#endif
