// SPDX-License-Identifier: GPL-2.0-only

#ifndef COMPLEMENTS3D_H
#define COMPLEMENTS3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class QLabel;
class QSlider;
class StripKernelColormapPanel;

class Complements3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Complements3D(QWidget* parent = nullptr);
    ~Complements3D() override;

    EFFECT_REGISTERER_3D("Complements3D", "Complements 3D", "Spatial", []() { return new Complements3D; });

    static std::string const ClassName() { return "Complements3D"; }
    static std::string const UIName() { return "Complements 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void SyncStripColormapFromPanel();

private:
    int depth_tone_count = 2;
    QSlider* depth_tones_slider = nullptr;
    QLabel* depth_tones_label = nullptr;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool depth_tone_strip_cmap_on = false;
    int depth_tone_strip_cmap_kernel = 0;
    float depth_tone_strip_cmap_rep = 4.0f;
    int depth_tone_strip_cmap_unfold = 0;
    float depth_tone_strip_cmap_dir = 0.0f;
    int depth_tone_strip_cmap_color_style = 0;
};

#endif
