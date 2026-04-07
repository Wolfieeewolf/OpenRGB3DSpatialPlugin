// SPDX-License-Identifier: GPL-2.0-only

#ifndef BREATHINGSPHERE_H
#define BREATHINGSPHERE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class BreathingSphere : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BreathingSphere(QWidget* parent = nullptr);
    ~BreathingSphere();

    EFFECT_REGISTERER_3D("BreathingSphere", "Breathing Sphere", "Spatial", [](){return new BreathingSphere;});

    static std::string const ClassName() { return "BreathingSphere"; }
    static std::string const UIName() { return "Breathing Sphere"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Mode { MODE_SPHERE = 0, MODE_GLOBAL_PULSE, MODE_COUNT };
    static const char* ModeName(int m);
    int breathing_mode = MODE_SPHERE;
    float           progress;
};

#endif
