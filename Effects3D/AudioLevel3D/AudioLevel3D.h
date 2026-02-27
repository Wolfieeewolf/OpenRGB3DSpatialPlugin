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

    EFFECT_REGISTERER_3D("AudioLevel3D", "Audio Level", "Audio", [](){ return new AudioLevel3D; })

    static std::string const ClassName() { return "AudioLevel3D"; }
    static std::string const UIName() { return "Audio Level 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    float EvaluateIntensity(float amplitude, float time);
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();
    int fill_axis = 1;       // 0=X, 1=Y, 2=Z – axis along which level "fills"
    float wave_amount = 0.06f;  // boundary wave amplitude 0..1
    float edge_soft = 0.08f;    // soft edge width 0..1
};

#endif // AUDIOLEVEL3D_H
