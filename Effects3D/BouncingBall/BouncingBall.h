// SPDX-License-Identifier: GPL-2.0-only

#ifndef BOUNCINGBALL_H
#define BOUNCINGBALL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include <vector>

struct CachedBall3D
{
    float px, py, pz;
    float vx, vy, vz;
};

class BouncingBall : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BouncingBall(QWidget* parent = nullptr);
    ~BouncingBall();

    EFFECT_REGISTERER_3D("BouncingBall", "Bouncing Ball", "Spatial", [](){return new BouncingBall;});

    static std::string const ClassName() { return "BouncingBall"; }
    static std::string const UIName() { return "Bouncing Ball"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnBallParameterChanged();

private:
    QSlider* elasticity_slider;
    QLabel* elasticity_label;
    QSlider* count_slider;
    QLabel* count_label;
    unsigned int elasticity;
    unsigned int ball_count;

    float ball_cache_time = -1e9f;
    float ball_cache_grid_hash = 0.0f;
    float ball_cache_phys_tag = -1e10f;
    std::vector<CachedBall3D> ball_positions_cached;
};

#endif
