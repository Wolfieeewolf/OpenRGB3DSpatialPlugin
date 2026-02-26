// SPDX-License-Identifier: GPL-2.0-only

#ifndef BEATPULSE3D_H
#define BEATPULSE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include <limits>

class BeatPulse3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit BeatPulse3D(QWidget* parent = nullptr);
    ~BeatPulse3D() override = default;

    EFFECT_REGISTERER_3D("BeatPulse3D", "Beat Pulse", "Audio", [](){ return new BeatPulse3D; })

    static std::string const ClassName() { return "BeatPulse3D"; }
    static std::string const UIName() { return "Beat Pulse 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 200);
    float EvaluateIntensity(float amplitude, float time);
    float envelope = 0.0f;
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();
};

#endif // BEATPULSE3D_H
