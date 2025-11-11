// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOLEVEL3D_H
#define AUDIOLEVEL3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include <limits>

class AudioLevel3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit AudioLevel3D(QWidget* parent = nullptr);
    ~AudioLevel3D() override = default;

    // Auto-registration hook
    EFFECT_REGISTERER_3D("AudioLevel3D", "Audio Level 3D", "Audio", [](){ return new AudioLevel3D; })

    static std::string const ClassName() { return "AudioLevel3D"; }
    static std::string const UIName() { return "Audio Level 3D"; }

    // SpatialEffect3D overrides
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    // Settings persistence
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    /*---------------------------------------------------------*\
    | Audio-specific parameters                                |
    | (Controlled by standard Audio Controls panel)           |
    \*---------------------------------------------------------*/
    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    float EvaluateIntensity(float amplitude, float time);
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();
};

#endif // AUDIOLEVEL3D_H
