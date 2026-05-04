// SPDX-License-Identifier: GPL-2.0-only

#ifndef DUALKERNELBLEND_H
#define DUALKERNELBLEND_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StripKernelColormapPanel;
class StratumBandPanel;

/** Blend two strip kernels (or kernel vs palette when a source is off) by Y, slider, or stratum band. */
class DualKernelBlend : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DualKernelBlend(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("DualKernelBlend", "Dual Kernel Blend", "Spatial", []() { return new DualKernelBlend; });

    static std::string const ClassName() { return "DualKernelBlend"; }
    static std::string const UIName() { return "Dual Kernel Blend"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanels();

private:
    float SamplePalette01ForSide(bool strip_on,
                                 int kern,
                                 float rep,
                                 int unfold,
                                 float dir,
                                 float phase01,
                                 float time,
                                 const GridContext3D& grid,
                                 float size_m,
                                 const Vector3D& origin,
                                 const Vector3D& rot,
                                 float spatial_palette_driver) const;

    int blend_mode = 0; /* 0=Y, 1=slider, 2=stratum mid band weight */
    float blend_slider = 0.5f;

    StripKernelColormapPanel* strip_a_panel = nullptr;
    bool dual_a_on = true;
    int dual_a_kernel = 0;
    float dual_a_rep = 4.0f;
    int dual_a_unfold = 0;
    float dual_a_dir = 0.0f;
    int dual_a_color_style = 0;

    StripKernelColormapPanel* strip_b_panel = nullptr;
    bool dual_b_on = true;
    int dual_b_kernel = 1;
    float dual_b_rep = 4.0f;
    int dual_b_unfold = 0;
    float dual_b_dir = 0.0f;
    int dual_b_color_style = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
