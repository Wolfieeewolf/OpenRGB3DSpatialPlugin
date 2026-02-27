// SPDX-License-Identifier: GPL-2.0-only

#ifndef COMET3D_H
#define COMET3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Comet3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Comet3D(QWidget* parent = nullptr);
    ~Comet3D() override = default;

    EFFECT_REGISTERER_3D("Comet3D", "Comet", "3D Spatial", [](){ return new Comet3D; });

    static std::string const ClassName() { return "Comet3D"; }
    static std::string const UIName() { return "Comet"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    int comet_axis = 1;
    float comet_size = 0.25f;
};

#endif
