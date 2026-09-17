// SPDX-License-Identifier: GPL-2.0-only

#ifndef BOUNCINGBALL_H
#define BOUNCINGBALL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class BouncingBall : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BouncingBall(QWidget* parent = nullptr);
    ~BouncingBall();

    EFFECT_REGISTERER_3D("BouncingBall", "Bouncing Ball", "Spatial", [](){return new BouncingBall;});

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    static constexpr unsigned int kMaxGpuBalls = 32u;

    QSlider* count_slider = nullptr;
    unsigned int ball_count;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
