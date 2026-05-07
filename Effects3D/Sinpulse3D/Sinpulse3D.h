// SPDX-License-Identifier: GPL-2.0-only

#ifndef SINPULSE3D_H
#define SINPULSE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class QLabel;
class QSlider;
class StripKernelColormapPanel;

/** Harmonic 3D pulse: h = (1 + sin(x·zoom+t1) + cos(y·zoom+t2) + sin(z·zoom+t1−t2))/2, v = h³/2. */
class Sinpulse3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Sinpulse3D(QWidget* parent = nullptr);
    ~Sinpulse3D() override;

    EFFECT_REGISTERER_3D("Sinpulse3D", "Sinpulse 3D", "Spatial", []() { return new Sinpulse3D; });

    static std::string const ClassName() { return "Sinpulse3D"; }
    static std::string const UIName() { return "Sinpulse 3D"; }

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

    /** Extra amplitude on (1 + wave·strength); reference uses 3. */
    float zoom_wobble_strength = 3.0f;

    QSlider* wobble_slider = nullptr;
    QLabel* wobble_label = nullptr;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool sinpulse_strip_cmap_on = false;
    int sinpulse_strip_cmap_kernel = 0;
    float sinpulse_strip_cmap_rep = 4.0f;
    int sinpulse_strip_cmap_unfold = 0;
    float sinpulse_strip_cmap_dir = 0.0f;
    int sinpulse_strip_cmap_color_style = 0;
};

#endif
