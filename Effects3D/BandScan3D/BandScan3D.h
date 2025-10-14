/*---------------------------------------------------------*\
| BandScan3D.h                                              |
|                                                           |
|   Scans through spectrum bands across space               |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef BANDSCAN3D_H
#define BANDSCAN3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"

class BandScan3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit BandScan3D(QWidget* parent = nullptr);
    ~BandScan3D() override;

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    EFFECT_REGISTERER_3D("BandScan3D", "Band Scan 3D", "Audio", [](){ return new BandScan3D; })

private:
    int band_start = 0; // inclusive
    int band_end = -1;  // inclusive
    float smoothing = 0.6f;
    float smoothed = 0.0f;
    float falloff = 1.0f;
};

#endif // BANDSCAN3D_H
