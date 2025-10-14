/*---------------------------------------------------------*\
| SpectrumBars3D.h                                          |
|                                                           |
|   Audio spectrum -> vertical bars along axis              |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPECTRUMBARS3D_H
#define SPECTRUMBARS3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"

class SpectrumBars3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit SpectrumBars3D(QWidget* parent = nullptr);
    ~SpectrumBars3D() override;

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    EFFECT_REGISTERER_3D("SpectrumBars3D", "Spectrum Bars 3D", "Audio", [](){ return new SpectrumBars3D; })

private:
    int band_start = 0; // inclusive
    int band_end = -1;  // inclusive (-1 = auto to last)
    float smoothing = 0.6f;
    float smoothed = 0.0f;
    float falloff = 1.0f;
};

#endif // SPECTRUMBARS3D_H
