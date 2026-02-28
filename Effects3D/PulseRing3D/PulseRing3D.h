// SPDX-License-Identifier: GPL-2.0-only

#ifndef PULSERING3D_H
#define PULSERING3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class PulseRing3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit PulseRing3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("PulseRing3D", "Pulse Ring", "3D Spatial", [](){ return new PulseRing3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Style { STYLE_PULSE_RING = 0, STYLE_RADIAL_RAINBOW, STYLE_COUNT };
    static const char* StyleName(int s);
    int ring_style = STYLE_PULSE_RING;
    float ring_thickness = 0.12f;
    float hole_size = 0.15f;
    float pulse_frequency = 1.2f;
    float pulse_amplitude = 1.0f;
    float direction_deg = 0.0f;
};

#endif
