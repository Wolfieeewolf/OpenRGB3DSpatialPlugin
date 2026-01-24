// SPDX-License-Identifier: GPL-2.0-only

#ifndef BOUNCINGBALL3D_H
#define BOUNCINGBALL3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class BouncingBall3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BouncingBall3D(QWidget* parent = nullptr);
    ~BouncingBall3D();

    EFFECT_REGISTERER_3D("BouncingBall3D", "3D Bouncing Ball", "3D Spatial", [](){return new BouncingBall3D;});

    static std::string const ClassName() { return "BouncingBall3D"; }
    static std::string const UIName() { return "3D Bouncing Ball"; }

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
    void OnBallParameterChanged();

private:
    QSlider* size_slider;
    QLabel* size_label;
    QSlider* elasticity_slider;
    QLabel* elasticity_label;
    QSlider* count_slider;
    QLabel* count_label;
    unsigned int ball_size;   // radius factor
    unsigned int elasticity;  // 10..100
    unsigned int ball_count;  // number of balls (1..50)
};

#endif
