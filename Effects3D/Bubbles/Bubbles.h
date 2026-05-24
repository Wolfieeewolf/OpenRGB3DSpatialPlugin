// SPDX-License-Identifier: GPL-2.0-only

#ifndef BUBBLES_H
#define BUBBLES_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include <vector>

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

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    int max_bubbles = 28;
    float bubble_thickness = 0.55f;
    float rise_speed = 2.4f;
    float spawn_interval = 0.60f;
    float max_radius = 1.9f;
    float horizontal_fill = 1.35f;
    float overlap_spacing = 0.65f;
    float launch_randomness = 0.75f;
    float bubble_cache_time = -1e9f;
    std::vector<BubbleCenter3D> bubble_centers_cached;
};

#endif
