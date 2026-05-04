// SPDX-License-Identifier: GPL-2.0-only

#ifndef BOUNCINGBALL_H
#define BOUNCINGBALL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include <vector>

class StratumBandPanel;
class StripKernelColormapPanel;

struct CachedBall3D
{
    float px, py, pz;
    float vx, vy, vz;
    float floor_bounce_vy;
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
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    QSlider* count_slider;
    QLabel* count_label;
    unsigned int ball_count;

    float ball_cache_grid_hash = 0.0f;
    int ball_cache_phys_key = 0x7fffffff;
    std::vector<CachedBall3D> ball_positions_cached;
    /* Monotonic simulation time (effect time * phase rate); no wrap — state advances forever. */
    float ball_physics_sim_t = 0.f;
    float ball_last_integrated_wall_time = -1e9f;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool bouncingball_strip_cmap_on = false;
    int bouncingball_strip_cmap_kernel = 0;
    float bouncingball_strip_cmap_rep = 4.0f;
    int bouncingball_strip_cmap_unfold = 0;
    float bouncingball_strip_cmap_dir = 0.0f;
    int bouncingball_strip_cmap_color_style = 0;
};

#endif
