// SPDX-License-Identifier: GPL-2.0-only

#ifndef DEPTHTONE_H
#define DEPTHTONE_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class QLabel;
class QSlider;
class DepthTone : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DepthTone(QWidget* parent = nullptr);
    ~DepthTone() override;

    EFFECT_REGISTERER_3D("DepthTone", "Depth Tone", "Spatial", []() { return new DepthTone; });

    static std::string const ClassName() { return "DepthTone"; }
    static std::string const UIName() { return "Depth Tone"; }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    int depth_tone_count = 2;
    QSlider* depth_tones_slider = nullptr;
    QLabel* depth_tones_label = nullptr;
};

#endif
