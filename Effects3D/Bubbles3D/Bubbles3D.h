// SPDX-License-Identifier: GPL-2.0-only

#ifndef BUBBLES3D_H
#define BUBBLES3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Bubbles3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Bubbles3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Bubbles3D", "3D Bubbles", "3D Spatial", [](){ return new Bubbles3D; });

    static std::string const ClassName() { return "Bubbles3D"; }
    static std::string const UIName() { return "3D Bubbles"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    int max_bubbles = 12;
    float bubble_thickness = 0.08f;
    float rise_speed = 0.5f;
    float spawn_interval = 0.8f;
    float max_radius = 1.0f;
};

#endif
