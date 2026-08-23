// SPDX-License-Identifier: GPL-2.0-only

#ifndef BUBBLES_H
#define BUBBLES_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"
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

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    static constexpr int kMaxGpuBubbles = 48;

    int max_bubbles = 24;
    float bubble_thickness = 0.55f;
    float rise_speed = 1.4f;
    float spawn_interval = 0.95f;
    float max_radius = 1.9f;
    float horizontal_fill = 1.35f;
    float overlap_spacing = 0.65f;
    float launch_randomness = 0.55f;
    float bubble_cache_time = -1e9f;
    std::vector<BubbleCenter3D> bubble_centers_cached;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
