// SPDX-License-Identifier: GPL-2.0-only

#ifndef DEPTHTONE3D_H
#define DEPTHTONE3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class QLabel;
class QSlider;
class StripKernelColormapPanel;

class DepthTone3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DepthTone3D(QWidget* parent = nullptr);
    ~DepthTone3D() override;

    EFFECT_REGISTERER_3D("DepthTone3D", "Depth Tone 3D", "Spatial", []() { return new DepthTone3D; });

    static std::string const ClassName() { return "DepthTone3D"; }
    static std::string const UIName() { return "Depth Tone 3D"; }

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
