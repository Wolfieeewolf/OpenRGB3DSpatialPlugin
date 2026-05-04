// SPDX-License-Identifier: GPL-2.0-only

#ifndef FIREWORKS_H
#define FIREWORKS_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include <vector>

class StratumBandPanel;
class StripKernelColormapPanel;

class Fireworks : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Fireworks(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Fireworks", "Fireworks", "Spatial", [](){ return new Fireworks; })

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
    enum FireworkType { TYPE_SINGLE = 0, TYPE_BIG_EXPLOSION, TYPE_ROMAN_CANDLE, TYPE_SPINNER, TYPE_FOUNTAIN, TYPE_RANDOM, TYPE_COUNT };
    static const char* TypeName(int t);

    float particle_size = 0.08f;
    int num_debris = 40;
    int firework_type = TYPE_SINGLE;
    int num_simultaneous = 2;
    float gravity_strength = 1.0f;
    float decay_speed = 2.8f;
    static constexpr float CYCLE_DURATION = 5.0f;
    static constexpr float MISSILE_DURATION = 1.0f;

    struct CachedParticle { float px, py, pz; float decay; float hue; };
    float particle_cache_time = -1e9f;
    std::vector<CachedParticle> particle_cache;
    float particle_aabb_min_x = 0, particle_aabb_min_y = 0, particle_aabb_min_z = 0;
    float particle_aabb_max_x = 0, particle_aabb_max_y = 0, particle_aabb_max_z = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool fireworks_strip_cmap_on = false;
    int fireworks_strip_cmap_kernel = 0;
    float fireworks_strip_cmap_rep = 4.0f;
    int fireworks_strip_cmap_unfold = 0;
    float fireworks_strip_cmap_dir = 0.0f;
    int fireworks_strip_cmap_color_style = 0;
};

#endif
