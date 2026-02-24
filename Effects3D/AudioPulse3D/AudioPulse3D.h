/*---------------------------------------------------------*\
| AudioPulse3D.h                                            |
|                                                           |
|   Simple audio-reactive pulse effect                      |
|   Designed for frequency range system                     |
|                                                           |
|   Date: 2026-01-27                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef AUDIOPULSE3D_H
#define AUDIOPULSE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class AudioPulse3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    AudioPulse3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("AudioPulse3D", "Audio Pulse", "Audio", [](){ return new AudioPulse3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;

    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    float audio_level = 0.0f;
    float pulse_intensity = 1.0f;
    bool use_radial = true;
};

#endif
