// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPIN3D_H
#define SPIN3D_H

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Spin3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Spin3D(QWidget* parent = nullptr);
    ~Spin3D();

    /*---------------------------------------------------------*\
    | Auto-registration system                                 |
    \*---------------------------------------------------------*/
    EFFECT_REGISTERER_3D("Spin3D", "3D Spin", "3D Spatial", [](){return new Spin3D;});

    static std::string const ClassName() { return "Spin3D"; }
    static std::string const UIName() { return "3D Spin"; }

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
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
    void OnSpinParameterChanged();

private:
    /*---------------------------------------------------------*\
    | Spin-specific controls                                   |
    \*---------------------------------------------------------*/
    QSlider*        arms_slider;        // Number of spinning arms

    /*---------------------------------------------------------*\
    | Spin-specific parameters                                 |
    \*---------------------------------------------------------*/
    unsigned int    num_arms;
    float           progress;

    static float Clamp01(float value);
};

#endif
