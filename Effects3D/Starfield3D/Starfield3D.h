// SPDX-License-Identifier: GPL-2.0-only

#ifndef STARFIELD3D_H
#define STARFIELD3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include <vector>

class Starfield3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Starfield3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Starfield3D", "Starfield", "3D Spatial", [](){ return new Starfield3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    int num_stars = 70;
    float star_size = 0.06f;
    float drift_amount = 0.0f;
    float twinkle_speed = 0.0f;
    float star_speed_mult = 1.0f;
    // Cache rotated star positions once per frame (major FPS win)
    float star_cache_time = -1e9f;
    int star_cache_count = 0;
    std::vector<Vector3D> star_positions_cached;
};

#endif
