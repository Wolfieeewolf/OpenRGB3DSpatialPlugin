// SPDX-License-Identifier: GPL-2.0-only

#ifndef FREQRIPPLE3D_H
#define FREQRIPPLE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include <vector>
#include <limits>

class FreqRipple3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit FreqRipple3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("FreqRipple3D", "Frequency Ripple", "Audio", [](){ return new FreqRipple3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    void TickRipples(float time);
    RGBColor ComputeRippleColor(float dist_norm, float time) const;

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

    float trail_width  = 0.18f;
    float decay_rate   = 2.0f;
    float onset_threshold = 0.55f;
};

#endif
