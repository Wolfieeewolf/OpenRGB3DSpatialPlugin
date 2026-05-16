// SPDX-License-Identifier: GPL-2.0-only

#ifndef SHARPPULSE_H
#define SHARPPULSE_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class SharpPulse : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit SharpPulse(QWidget* parent = nullptr);
    ~SharpPulse() override;

    EFFECT_REGISTERER_3D("SharpPulse", "Sharp Pulse", "Spatial", []() { return new SharpPulse; });

    static std::string const ClassName() { return "SharpPulse"; }
    static std::string const UIName() { return "Sharp Pulse"; }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
};

#endif
