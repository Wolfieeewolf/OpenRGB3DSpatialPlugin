// SPDX-License-Identifier: GPL-2.0-only

#ifndef BEATPULSE_H
#define BEATPULSE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <limits>
#include <vector>

class StratumBandPanel;
class StripKernelColormapPanel;

class BeatPulse : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit BeatPulse(QWidget* parent = nullptr);
    ~BeatPulse() override = default;

    EFFECT_REGISTERER_3D("BeatPulse", "Beat Pulse", "Audio", [](){ return new BeatPulse; })

    static std::string const ClassName() { return "BeatPulse"; }
    static std::string const UIName() { return "Beat Pulse"; }

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
    struct PulseData
    {
        float birth_time = 0.0f;
        float strength   = 0.0f;
    };
    void TickPulses(float time);
    float SamplePulseField(float radial_norm,
                           float height_norm,
                           float time,
                           const EffectStratumBlend::BandBlendScalars& bb) const;

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 200);
    float EvaluateIntensity(float amplitude, float time);
    float envelope = 0.0f;
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();

    std::vector<PulseData> pulses;
    float onset_smoothed = 0.0f;
    float onset_hold = 0.0f;
    float onset_threshold = 0.35f;
    float last_tick_time = std::numeric_limits<float>::lowest();

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool beatpulse_strip_cmap_on = false;
    int beatpulse_strip_cmap_kernel = 0;
    float beatpulse_strip_cmap_rep = 4.0f;
    int beatpulse_strip_cmap_unfold = 0;
    float beatpulse_strip_cmap_dir = 0.0f;
    int beatpulse_strip_cmap_color_style = 0;
};

#endif // BEATPULSE_H
