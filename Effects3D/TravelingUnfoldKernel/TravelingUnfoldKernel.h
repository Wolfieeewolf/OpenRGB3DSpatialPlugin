// SPDX-License-Identifier: GPL-2.0-only

#ifndef TRAVELINGUNFOLDKERNEL_H
#define TRAVELINGUNFOLDKERNEL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StripKernelColormapPanel;
class StratumBandPanel;

/** Strip kernel with animated unfold mode and/or direction so the pattern "unwraps" through the room. */
class TravelingUnfoldKernel : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit TravelingUnfoldKernel(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("TravelingUnfoldKernel", "Traveling Unfold Kernel", "Spatial", []() {
        return new TravelingUnfoldKernel;
    });

    static std::string const ClassName() { return "TravelingUnfoldKernel"; }
    static std::string const UIName() { return "Traveling Unfold Kernel"; }

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
    float dir_spin_deg_per_sec = 12.0f;
    float unfold_cycle_sec = 0.0f; /* 0 = no step cycling */

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool tunf_strip_cmap_on = true;
    int tunf_strip_cmap_kernel = 0;
    float tunf_strip_cmap_rep = 4.0f;
    int tunf_strip_cmap_unfold = 0;
    float tunf_strip_cmap_dir = 0.0f;
    int tunf_strip_cmap_color_style = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
