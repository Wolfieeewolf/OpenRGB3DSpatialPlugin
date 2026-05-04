// SPDX-License-Identifier: GPL-2.0-only

#ifndef BUBBLES_H
#define BUBBLES_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include <vector>

class StratumBandPanel;
class StripKernelColormapPanel;

struct BubbleCenter3D
{
    float cx, cy, cz, radius;
};

class Bubbles : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Bubbles(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Bubbles", "Bubbles", "Spatial", [](){ return new Bubbles; });

    static std::string const ClassName() { return "Bubbles"; }
    static std::string const UIName() { return "Bubbles"; }

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
    int max_bubbles = 12;
    float bubble_thickness = 0.08f;
    float rise_speed = 0.5f;
    float spawn_interval = 0.8f;
    float max_radius = 1.0f;
    float bubble_cache_time = -1e9f;
    std::vector<BubbleCenter3D> bubble_centers_cached;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool bubbles_strip_cmap_on = false;
    int bubbles_strip_cmap_kernel = 0;
    float bubbles_strip_cmap_rep = 4.0f;
    int bubbles_strip_cmap_unfold = 0;
    float bubbles_strip_cmap_dir = 0.0f;
    int bubbles_strip_cmap_color_style = 0;
};

#endif
