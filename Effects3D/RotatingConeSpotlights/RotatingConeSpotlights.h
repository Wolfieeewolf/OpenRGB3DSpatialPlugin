// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROTATINGCONESPOTLIGHTS_H
#define ROTATINGCONESPOTLIGHTS_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class QSlider;
class QLabel;
class RotatingConeSpotlights : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit RotatingConeSpotlights(QWidget* parent = nullptr);
    ~RotatingConeSpotlights() override;

    EFFECT_REGISTERER_3D("RotatingConeSpotlights", "Rotating Cone Spotlights", "Spatial", []() {
        return new RotatingConeSpotlights;
    });

    static std::string const ClassName() { return "RotatingConeSpotlights"; }
    static std::string const UIName() { return "Rotating Cone Spotlights"; }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    static void SetupRotationMatrix(float ux, float uy, float uz, float angle_rad, float R[3][3]);
    static void Rotate3D(float x, float y, float z, const float R[3][3], float* rx, float* ry, float* rz);
    static RGBColor Hsv01ToBgr(float h, float s, float v);

    float cone_scale = 0.10132118364233777f;
    float hue01 = 0.0f;
    float motion_rate = 1.0f;

    QSlider* cone_slider = nullptr;
    QLabel* cone_label = nullptr;
    QSlider* hue_slider = nullptr;
    QLabel* hue_label = nullptr;
    QSlider* motion_slider = nullptr;
    QLabel* motion_label = nullptr;
};

#endif
