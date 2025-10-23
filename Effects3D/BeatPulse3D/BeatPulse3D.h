/*---------------------------------------------------------*\
| BeatPulse3D.h                                             |
|                                                           |
|   Bass-driven global pulse                                |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef BEATPULSE3D_H
#define BEATPULSE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"

class BeatPulse3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit BeatPulse3D(QWidget* parent = nullptr);
    ~BeatPulse3D() override;

    /*---------------------------------------------------------*\
    | Auto-registration system                                 |
    \*---------------------------------------------------------*/
    EFFECT_REGISTERER_3D("BeatPulse3D", "Beat Pulse 3D", "Audio", [](){ return new BeatPulse3D; })

    static std::string const ClassName() { return "BeatPulse3D"; }
    static std::string const UIName() { return "Beat Pulse 3D"; }

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
    float envelope = 0.0f; // decay envelope
    int low_hz = 20;
    int high_hz = 200;
    float smoothing = 0.6f; // controls decay rate
    float falloff = 1.0f;
};

#endif // BEATPULSE3D_H
