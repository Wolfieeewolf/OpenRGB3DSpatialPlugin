// SPDX-License-Identifier: GPL-2.0-only

#ifndef STARFIELD_H
#define STARFIELD_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include <vector>

class StratumBandPanel;
class StripKernelColormapPanel;

class Starfield : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Starfield(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Starfield", "Starfield", "Spatial", [](){ return new Starfield; })

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
    enum Mode { MODE_STARFIELD = 0, MODE_TWINKLE, MODE_COUNT };
    static const char* ModeName(int m);
    int mode = MODE_STARFIELD;
    int num_stars = 70;
    float star_size = 0.06f;
    float drift_amount = 0.0f;
    float twinkle_speed = 0.0f;
    float star_cache_time = -1e9f;
    int star_cache_count = 0;
    std::vector<Vector3D> star_positions_cached;
    float star_aabb_min_x = 0, star_aabb_min_y = 0, star_aabb_min_z = 0;
    float star_aabb_max_x = 0, star_aabb_max_y = 0, star_aabb_max_z = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool starfield_strip_cmap_on = false;
    int starfield_strip_cmap_kernel = 0;
    float starfield_strip_cmap_rep = 4.0f;
    int starfield_strip_cmap_unfold = 0;
    float starfield_strip_cmap_dir = 0.0f;
    int starfield_strip_cmap_color_style = 0;
};

#endif
