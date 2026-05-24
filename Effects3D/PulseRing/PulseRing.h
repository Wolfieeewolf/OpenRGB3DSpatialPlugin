// SPDX-License-Identifier: GPL-2.0-only

#ifndef PULSERING_H
#define PULSERING_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class PulseRing : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit PulseRing(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("PulseRing", "Pulse Ring", "Spatial", [](){ return new PulseRing; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    enum Style { STYLE_PULSE_RING = 0, STYLE_RADIAL_RAINBOW, STYLE_COUNT };
    static const char* StyleName(int s);
    int ring_style = STYLE_PULSE_RING;
    float ring_thickness = 0.12f;
    float hole_size = 0.15f;
    float pulse_amplitude = 1.0f;
    float direction_deg = 0.0f;
};

#endif
