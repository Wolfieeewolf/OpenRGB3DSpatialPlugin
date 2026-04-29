// SPDX-License-Identifier: GPL-2.0-only

#ifndef FREQFILL_H
#define FREQFILL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <limits>

class StratumBandPanel;

class FreqFill : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit FreqFill(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("FreqFill", "Frequency Fill", "Audio", [](){ return new FreqFill; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();

private:
    float EvaluateIntensity(float amplitude, float time);

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();

    float edge_width = 0.08f;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
