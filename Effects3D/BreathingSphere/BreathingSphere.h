// SPDX-License-Identifier: GPL-2.0-only

#ifndef BREATHINGSPHERE_H
#define BREATHINGSPHERE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class BreathingSphere : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BreathingSphere(QWidget* parent = nullptr);
    ~BreathingSphere();

    EFFECT_REGISTERER_3D("BreathingSphere", "Breathing Sphere", "Spatial", [](){return new BreathingSphere;});

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
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
    /** Soft vs Crisp only (legacy Feathered→Soft, Crisp ring→Crisp on load). */
    enum EdgeProfile {
        EDGE_SOFT = 0,
        EDGE_CRISP = 1,
        EDGE_COUNT = 2,
        /* Legacy saved IDs (not shown in UI): */
        EDGE_LEGACY_FEATHERED = 2,
        EDGE_LEGACY_RING = 3
    };
    static const char* ShapeName(int s);
    static const char* EdgeName(int e);
    static int NormalizeEdgeProfile(int e);

    int breathing_shape = SHAPE_SPHERE;
    int edge_profile = EDGE_CRISP;
    int breath_pulse_pct = 55;
    int center_hole_pct = 0;
    float progress;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
