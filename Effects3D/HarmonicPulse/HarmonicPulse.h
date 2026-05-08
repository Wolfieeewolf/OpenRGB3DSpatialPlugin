// SPDX-License-Identifier: GPL-2.0-only

#ifndef HARMONICPULSE_H
#define HARMONICPULSE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class QLabel;
class QSlider;
class StripKernelColormapPanel;

/** Harmonic 3D pulse: h = (1 + sin(x·zoom+t1) + cos(y·zoom+t2) + sin(z·zoom+t1−t2))/2, v = h³/2. */
class HarmonicPulse : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit HarmonicPulse(QWidget* parent = nullptr);
    ~HarmonicPulse() override;

    EFFECT_REGISTERER_3D("HarmonicPulse", "Harmonic Pulse", "Spatial", []() { return new HarmonicPulse; });

    static std::string const ClassName() { return "HarmonicPulse"; }
    static std::string const UIName() { return "Harmonic Pulse"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void SyncStripColormapFromPanel();

private:
    static RGBColor Hsv01ToBgr(float h, float s, float v);

    /** Extra amplitude on (1 + wave·strength). */
    float zoom_wobble_strength = 0.0f;
    /** Multiplies motion pace beyond shared speed/frequency controls. */
    float flow_amount = 1.0f;
    /** Extra brightness contrast shaping for pulses. */
    float pulse_contrast = 1.0f;
    /** Scales lattice density based on grid size to keep large rigs lively. */
    float large_setup_boost = 1.0f;

    QSlider* wobble_slider = nullptr;
    QLabel* wobble_label = nullptr;
    QSlider* flow_slider = nullptr;
    QLabel* flow_label = nullptr;
    QSlider* contrast_slider = nullptr;
    QLabel* contrast_label = nullptr;
    QSlider* setup_boost_slider = nullptr;
    QLabel* setup_boost_label = nullptr;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool harmonic_strip_cmap_on = false;
    int harmonic_strip_cmap_kernel = 0;
    float harmonic_strip_cmap_rep = 4.0f;
    int harmonic_strip_cmap_unfold = 0;
    float harmonic_strip_cmap_dir = 0.0f;
    int harmonic_strip_cmap_color_style = 0;
};

#endif
