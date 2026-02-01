// SPDX-License-Identifier: GPL-2.0-only

#ifndef LIGHTNING3D_H
#define LIGHTNING3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Lightning3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Lightning3D(QWidget* parent = nullptr);
    ~Lightning3D();

    EFFECT_REGISTERER_3D("Lightning3D", "3D Lightning", "3D Spatial", [](){return new Lightning3D;});

    static std::string const ClassName() { return "Lightning3D"; }
    static std::string const UIName() { return "3D Lightning"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    // Settings persistence
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnLightningParameterChanged();

private:
    QSlider*        strike_rate_slider;
    QLabel*         strike_rate_label;
    QSlider*        branch_slider;
    QLabel*         branch_label;
    unsigned int    strike_rate;   // arches per second
    unsigned int    branches;      // number of simultaneous arches
};

#endif

