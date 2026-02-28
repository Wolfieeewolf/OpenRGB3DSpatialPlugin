// SPDX-License-Identifier: GPL-2.0-only

#ifndef FREQFILL3D_H
#define FREQFILL3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include <limits>

class FreqFill3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit FreqFill3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("FreqFill3D", "Frequency Fill", "Audio", [](){ return new FreqFill3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    float EvaluateIntensity(float amplitude, float time);

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();

    float edge_width = 0.08f;
};

#endif
