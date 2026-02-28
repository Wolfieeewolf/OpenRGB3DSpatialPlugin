// SPDX-License-Identifier: GPL-2.0-only

#ifndef FIREWORKS3D_H
#define FIREWORKS3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include <vector>

class Fireworks3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Fireworks3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Fireworks3D", "Fireworks", "3D Spatial", [](){ return new Fireworks3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

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
};

#endif
