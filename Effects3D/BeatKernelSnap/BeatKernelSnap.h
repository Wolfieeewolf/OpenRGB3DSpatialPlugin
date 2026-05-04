// SPDX-License-Identifier: GPL-2.0-only

#ifndef BEATKERNELSNAP_H
#define BEATKERNELSNAP_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include <limits>

class StripKernelColormapPanel;
class StratumBandPanel;

/** On each beat, hold or jump strip-kernel phase01 while spatial kernel sampling stays live. */
class BeatKernelSnap : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BeatKernelSnap(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("BeatKernelSnap", "Beat Kernel Snap", "Audio", []() { return new BeatKernelSnap; });

    static std::string const ClassName() { return "BeatKernelSnap"; }
    static std::string const UIName() { return "Beat Kernel Snap"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    void TickSnap(float time);

    float running_phase01 = 0.0f;
    float snapped_phase01 = 0.0f;
    bool holding = false;
    float hold_remaining = 0.0f;
    float last_tick_time = std::numeric_limits<float>::lowest();
    float onset_smoothed = 0.0f;
    float onset_hold = 0.0f;
    float onset_threshold = 0.32f;
    float hold_duration = 0.14f;
    int snap_mode = 0; /* 0 = freeze to running phase, 1 = jump by 0.5 */
    float phase_advance_scale = 0.08f;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool bksnap_strip_cmap_on = true;
    int bksnap_strip_cmap_kernel = 0;
    float bksnap_strip_cmap_rep = 4.0f;
    int bksnap_strip_cmap_unfold = 0;
    float bksnap_strip_cmap_dir = 0.0f;
    int bksnap_strip_cmap_color_style = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
