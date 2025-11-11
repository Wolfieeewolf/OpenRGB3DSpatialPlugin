// SPDX-License-Identifier: GPL-2.0-only

#ifndef RAIN3D_H
#define RAIN3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Rain3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Rain3D(QWidget* parent = nullptr);
    ~Rain3D();

    EFFECT_REGISTERER_3D("Rain3D", "3D Rain", "3D Spatial", [](){return new Rain3D;});

    static std::string const ClassName() { return "Rain3D"; }
    static std::string const UIName() { return "3D Rain"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    /*---------------------------------------------------------*\
    | Settings persistence                                     |
    \*---------------------------------------------------------*/
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnRainParameterChanged();

private:
    QSlider*        density_slider;
    QSlider*        wind_slider;
    unsigned int    rain_density;    // 1-100 (drops per area)
    int             wind;            // -50..50 lateral drift
};

#endif

