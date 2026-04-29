// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOLEVEL_H
#define AUDIOLEVEL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <limits>

class StratumBandPanel;

class AudioLevel : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit AudioLevel(QWidget* parent = nullptr);
    ~AudioLevel() override = default;

    EFFECT_REGISTERER_3D("AudioLevel", "Audio Level", "Audio", [](){ return new AudioLevel; })

    static std::string const ClassName() { return "AudioLevel"; }
    static std::string const UIName() { return "Audio Level"; }

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
    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    float EvaluateIntensity(float amplitude, float time);
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();
    float wave_amount = 0.06f;
    float edge_soft = 0.08f;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif // AUDIOLEVEL_H
