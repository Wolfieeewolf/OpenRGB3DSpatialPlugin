// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOPULSE_H
#define AUDIOPULSE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <limits>
#include <vector>

class StratumBandPanel;
class StripKernelColormapPanel;

class AudioPulse : public SpatialEffect3D
{
    Q_OBJECT

public:
    AudioPulse(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("AudioPulse", "Audio Pulse", "Audio", [](){ return new AudioPulse; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams&) override;

    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    struct PulseData
    {
        float birth_time = 0.0f;
        float strength   = 0.0f;
    };

    static float Hash01(float x, float y, float z, unsigned int salt);

    void TickPulses(float time);
    float HeightFade01(float height_norm) const;
    float SampleBeatShell(float distance,
                          float radius_basis,
                          float time,
                          const EffectStratumBlend::BandBlendScalars& bb,
                          float height_fade,
                          float x,
                          float y,
                          float z) const;
    float ParticleDebrisAt(float x,
                           float y,
                           float z,
                           float burst_phase,
                           float distance,
                           float shell_radius,
                           int salt) const;

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 200);

    std::vector<PulseData> pulses;
    float onset_smoothed = 0.0f;
    float onset_hold = 0.0f;
    float onset_threshold = 0.38f;
    float last_tick_time = std::numeric_limits<float>::lowest();
    int particle_amount = 40;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool audiopulse_strip_cmap_on = false;
    int audiopulse_strip_cmap_kernel = 0;
    float audiopulse_strip_cmap_rep = 4.0f;
    int audiopulse_strip_cmap_unfold = 0;
    float audiopulse_strip_cmap_dir = 0.0f;
    int audiopulse_strip_cmap_color_style = 0;
};

#endif
