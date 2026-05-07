// SPDX-License-Identifier: GPL-2.0-only

#ifndef BREATHINGSPHERE_H
#define BREATHINGSPHERE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StratumBandPanel;
class StripKernelColormapPanel;

class BreathingSphere : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BreathingSphere(QWidget* parent = nullptr);
    ~BreathingSphere();

    EFFECT_REGISTERER_3D("BreathingSphere", "Breathing Sphere", "Spatial", [](){return new BreathingSphere;});

    static std::string const ClassName() { return "BreathingSphere"; }
    static std::string const UIName() { return "Breathing Sphere"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

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

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool breathing_strip_cmap_on = false;
    int breathing_strip_cmap_kernel = 0;
    float breathing_strip_cmap_rep = 4.0f;
    int breathing_strip_cmap_unfold = 0;
    float breathing_strip_cmap_dir = 0.0f;
    int breathing_strip_cmap_color_style = 0;
};

#endif
