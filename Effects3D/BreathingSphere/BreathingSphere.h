// SPDX-License-Identifier: GPL-2.0-only

#ifndef BREATHINGSPHERE_H
#define BREATHINGSPHERE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class BreathingSphere : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BreathingSphere(QWidget* parent = nullptr);
    ~BreathingSphere();

    EFFECT_REGISTERER_3D("BreathingSphere", "Breathing Shape", "Spatial", [](){return new BreathingSphere;});

    static std::string const ClassName() { return "BreathingSphere"; }
    static std::string const UIName() { return "Breathing Shape"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Shape {
        SHAPE_SPHERE = 0,
        SHAPE_SQUARE,
        SHAPE_RECTANGLE,
        SHAPE_TRIANGLE,
        SHAPE_PENTAGON,
        SHAPE_WHOLE_ROOM,
        SHAPE_COUNT
    };
    enum EdgeProfile {
        EDGE_SMOOTH = 0,
        EDGE_SHARP,
        EDGE_FEATHERED,
        EDGE_RING,
        EDGE_COUNT
    };
    static const char* ShapeName(int s);
    static const char* EdgeName(int e);

    int breathing_shape = SHAPE_SPHERE;
    int edge_profile = EDGE_SMOOTH;
    int breath_pulse_pct = 40;
    int center_hole_pct = 0;
    float progress;
};

#endif
