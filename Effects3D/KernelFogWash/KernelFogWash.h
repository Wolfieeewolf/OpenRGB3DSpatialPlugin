// SPDX-License-Identifier: GPL-2.0-only

#ifndef KERNELFOGWASH_H
#define KERNELFOGWASH_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StripKernelColormapPanel;
class StratumBandPanel;

/** Low-frequency strip kernel drives hue; height or radial distance drives brightness ("colored mist"). */
class KernelFogWash : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit KernelFogWash(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("KernelFogWash", "Kernel Fog Wash", "Spatial", []() { return new KernelFogWash; });

    static std::string const ClassName() { return "KernelFogWash"; }
    static std::string const UIName() { return "Kernel Fog Wash"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    int v_mode = 0; /* 0 = height (Y), 1 = distance from origin */
    float mist_contrast = 0.65f;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool kfog_strip_cmap_on = true;
    int kfog_strip_cmap_kernel = 0;
    float kfog_strip_cmap_rep = 4.0f;
    int kfog_strip_cmap_unfold = 0;
    float kfog_strip_cmap_dir = 0.0f;
    int kfog_strip_cmap_color_style = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
