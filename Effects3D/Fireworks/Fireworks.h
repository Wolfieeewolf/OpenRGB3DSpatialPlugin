// SPDX-License-Identifier: GPL-2.0-only

#ifndef FIREWORKS_H
#define FIREWORKS_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include <vector>

class Fireworks : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Fireworks(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Fireworks", "Fireworks", "Spatial", [](){ return new Fireworks; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    enum FireworkType { TYPE_SINGLE = 0, TYPE_BIG_EXPLOSION, TYPE_ROMAN_CANDLE, TYPE_SPINNER, TYPE_FOUNTAIN, TYPE_RANDOM, TYPE_COUNT };
    static const char* TypeName(int t);
    enum BurstStyle { BURST_AUTO = 0, BURST_PEONY, BURST_CHRYSANTHEMUM, BURST_WILLOW, BURST_PALM, BURST_CROSSETTE, BURST_COUNT };
    static const char* BurstStyleName(int s);

    float particle_size = 0.08f;
    int num_debris = 40;
    int firework_type = TYPE_SINGLE;
    int burst_style = BURST_AUTO;
    int num_simultaneous = 2;
    float show_density = 1.0f;
    float show_variety = 0.7f;
    float launch_trail_amount = 0.8f;
    float burst_flash_amount = 0.7f;
    float ember_amount = 0.9f;
    float ember_hang_time = 1.0f;
    float gravity_strength = 1.0f;
    float decay_speed = 2.8f;
    static constexpr float CYCLE_DURATION = 5.0f;
    static constexpr float MISSILE_DURATION = 1.0f;

    struct CachedParticle { float px, py, pz; float vx, vy, vz; float decay; float hue; float sparkle; };
    float particle_cache_time = -1e9f;
    std::vector<CachedParticle> particle_cache;
    float particle_aabb_min_x = 0, particle_aabb_min_y = 0, particle_aabb_min_z = 0;
    float particle_aabb_max_x = 0, particle_aabb_max_y = 0, particle_aabb_max_z = 0;
};

#endif
