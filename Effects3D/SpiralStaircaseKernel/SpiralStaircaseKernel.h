// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPIRALSTAIRCASEKERNEL_H
#define SPIRALSTAIRCASEKERNEL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StripKernelColormapPanel;
class StratumBandPanel;

/** Helix-style phase so strip kernels read as a path climbing through the volume. */
class SpiralStaircaseKernel : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit SpiralStaircaseKernel(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("SpiralStaircaseKernel", "Spiral Staircase Kernel", "Spatial", []() {
        return new SpiralStaircaseKernel;
    });

    static std::string const ClassName() { return "SpiralStaircaseKernel"; }
    static std::string const UIName() { return "Spiral Staircase Kernel"; }

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
    float turn_weight = 2.0f;
    float pitch_weight = 1.25f;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool spiral_strip_cmap_on = true;
    int spiral_strip_cmap_kernel = 0;
    float spiral_strip_cmap_rep = 4.0f;
    int spiral_strip_cmap_unfold = 0;
    float spiral_strip_cmap_dir = 0.0f;
    int spiral_strip_cmap_color_style = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
