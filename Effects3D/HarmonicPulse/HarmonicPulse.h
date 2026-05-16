// SPDX-License-Identifier: GPL-2.0-only

#ifndef HARMONICPULSE_H
#define HARMONICPULSE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class QLabel;
class QSlider;
class HarmonicPulse : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit HarmonicPulse(QWidget* parent = nullptr);
    ~HarmonicPulse() override;

    EFFECT_REGISTERER_3D("HarmonicPulse", "Harmonic Pulse", "Spatial", []() { return new HarmonicPulse; });

    static std::string const ClassName() { return "HarmonicPulse"; }
    static std::string const UIName() { return "Harmonic Pulse"; }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    static RGBColor Hsv01ToBgr(float h, float s, float v);

    float zoom_wobble_strength = 0.0f;
    float flow_amount = 1.0f;
    float pulse_contrast = 1.0f;
    float large_setup_boost = 1.0f;

    QSlider* wobble_slider = nullptr;
    QLabel* wobble_label = nullptr;
    QSlider* flow_slider = nullptr;
    QLabel* flow_label = nullptr;
    QSlider* contrast_slider = nullptr;
    QLabel* contrast_label = nullptr;
    QSlider* setup_boost_slider = nullptr;
    QLabel* setup_boost_label = nullptr;
};

#endif
