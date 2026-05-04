// SPDX-License-Identifier: GPL-2.0-only

#ifndef TORNADO_H
#define TORNADO_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StratumBandPanel;
class StripKernelColormapPanel;

class Tornado : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Tornado(QWidget* parent = nullptr);
    ~Tornado();

    EFFECT_REGISTERER_3D("Tornado", "Tornado", "Spatial", [](){return new Tornado;});

    static std::string const ClassName() { return "Tornado"; }
    static std::string const UIName() { return "Tornado"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnTornadoParameterChanged();
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    QSlider* core_radius_slider;
    QLabel* core_radius_label;
    QSlider* height_slider;
    QLabel* height_label;
    unsigned int core_radius;
    unsigned int tornado_height;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool tornado_strip_cmap_on = false;
    int tornado_strip_cmap_kernel = 0;
    float tornado_strip_cmap_rep = 4.0f;
    int tornado_strip_cmap_unfold = 0;
    float tornado_strip_cmap_dir = 0.0f;
    int tornado_strip_cmap_color_style = 0;
};

#endif

