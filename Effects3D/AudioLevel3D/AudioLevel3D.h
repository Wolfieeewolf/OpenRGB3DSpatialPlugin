/*---------------------------------------------------------*\
| AudioLevel3D.h                                            |
|                                                           |
|   Basic audio-reactive effect: overall level -> brightness|
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef AUDIOLEVEL3D_H
#define AUDIOLEVEL3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"

class AudioLevel3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit AudioLevel3D(QWidget* parent = nullptr);
    ~AudioLevel3D() override;

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;

    // Registration
    EFFECT_REGISTERER_3D("AudioLevel3D", "Audio Level 3D", "Audio", [](){ return new AudioLevel3D; })

    // Per-effect band filter
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;
private:
    int low_hz = 20;
    int high_hz = 20000;
    float smoothing = 0.6f;   // 0..0.99 per-effect smoothing
    float smoothed = 0.0f;
    float falloff = 1.0f;     // gamma-like shaping 0.2..5.0 (1.0 neutral)
};

#endif // AUDIOLEVEL3D_H
