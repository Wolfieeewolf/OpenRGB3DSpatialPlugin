// SPDX-License-Identifier: GPL-2.0-only

#ifndef FREQRIPPLE_H
#define FREQRIPPLE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <vector>
#include <limits>

class StratumBandPanel;
class StripKernelColormapPanel;

class FreqRipple : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit FreqRipple(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("FreqRipple", "Frequency Ripple", "Audio", [](){ return new FreqRipple; })

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
    void TickRipples(float time);
    RGBColor ComputeRippleColor(float dist_norm, float time, const EffectStratumBlend::BandBlendScalars& bb) const;
    float smoothstep(float edge0, float edge1, float x) const;

    struct Ripple
    {
        float birth_time;
        float strength;
    };

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 200);
    std::vector<Ripple> ripples;

    float last_tick_time = std::numeric_limits<float>::lowest();
    float onset_smoothed = 0.0f;
    float onset_hold = 0.0f;

    float trail_width  = 0.35f;
    float decay_rate   = 2.0f;
    float onset_threshold = 0.25f;
    int ripple_edge_shape = 0;  /* 0=Round, 1=Sharp, 2=Square */

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool freqripple_strip_cmap_on = false;
    int freqripple_strip_cmap_kernel = 0;
    float freqripple_strip_cmap_rep = 4.0f;
    int freqripple_strip_cmap_unfold = 0;
    float freqripple_strip_cmap_dir = 0.0f;
    int freqripple_strip_cmap_color_style = 0;
};

#endif
