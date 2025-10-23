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

    /*---------------------------------------------------------*\
    | Auto-registration system                                 |
    \*---------------------------------------------------------*/
    EFFECT_REGISTERER_3D("SpectrumBars3D", "Spectrum Bars 3D", "Audio", [](){ return new SpectrumBars3D; })

    static std::string const ClassName() { return "SpectrumBars3D"; }
    static std::string const UIName() { return "Spectrum Bars 3D"; }

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    /*---------------------------------------------------------*\
    | Settings persistence                                     |
    \*---------------------------------------------------------*/
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    /*---------------------------------------------------------*\
    | Audio-specific parameters                                |
    | (Controlled by standard Audio Controls panel)           |
    \*---------------------------------------------------------*/
    int low_hz = 20;
    int high_hz = 20000;
    int band_start = 0; // inclusive (auto-calculated from low_hz)
    int band_end = -1;  // inclusive (-1 = auto to last, auto-calculated from high_hz)
    float smoothing = 0.6f;
    float smoothed = 0.0f;
    float falloff = 1.0f;
};

#endif // SPECTRUMBARS3D_H
