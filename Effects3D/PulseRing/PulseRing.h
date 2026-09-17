// SPDX-License-Identifier: GPL-2.0-only

#ifndef PULSERING_H
#define PULSERING_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class PulseRing : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit PulseRing(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("PulseRing", "Pulse Ring", "Spatial", [](){ return new PulseRing; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Style { STYLE_PULSE = 0, STYLE_RADIAL_RAINBOW, STYLE_COUNT };
    enum Shape {
        SHAPE_CIRCLE = 0,  // extruded XZ circle (flat expanding ring)
        SHAPE_SPHERE,
        SHAPE_HEXAGON,
        SHAPE_TRIANGLE,
        SHAPE_SQUARE,
        SHAPE_COUNT
    };
    static const char* StyleName(int s);
    static const char* ShapeName(int s);

    int ring_style = STYLE_PULSE;
    int pulse_shape = SHAPE_HEXAGON;
    float ring_thickness = 0.05f;
    float hole_size = 0.08f;
    float pulse_amplitude = 1.0f;
    float direction_deg = 0.0f;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
