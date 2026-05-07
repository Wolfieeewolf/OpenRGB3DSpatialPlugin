// SPDX-License-Identifier: GPL-2.0-only

#ifndef FASTPULSE3D_H
#define FASTPULSE3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class StripKernelColormapPanel;

class FastPulse3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit FastPulse3D(QWidget* parent = nullptr);
    ~FastPulse3D() override;

    EFFECT_REGISTERER_3D("FastPulse3D", "Fast Pulse 3D", "Spatial", []() { return new FastPulse3D; });

    static std::string const ClassName() { return "FastPulse3D"; }
    static std::string const UIName() { return "Fast Pulse 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void SyncStripColormapFromPanel();

private:
    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool fastpulse_strip_cmap_on = false;
    int fastpulse_strip_cmap_kernel = 0;
    float fastpulse_strip_cmap_rep = 4.0f;
    int fastpulse_strip_cmap_unfold = 0;
    float fastpulse_strip_cmap_dir = 0.0f;
    int fastpulse_strip_cmap_color_style = 0;
};

#endif
