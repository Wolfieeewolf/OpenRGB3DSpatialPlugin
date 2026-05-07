// SPDX-License-Identifier: GPL-2.0-only

#ifndef BOUNCER3D_H
#define BOUNCER3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

#include <vector>

class QSlider;
class QLabel;

class Bouncer3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Bouncer3D(QWidget* parent = nullptr);
    ~Bouncer3D() override;

    EFFECT_REGISTERER_3D("Bouncer3D", "Bouncer 3D", "Spatial", []() { return new Bouncer3D; });

    static std::string const ClassName() { return "Bouncer3D"; }
    static std::string const UIName() { return "Bouncer 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnBallCountChanged();

private:
    struct BallState
    {
        float x;
        float y;
        float z;
        float vx;
        float vy;
        float vz;
        float hue01;
    };

    void EnsureBallStates(unsigned int count);
    void ResetBallStates();
    void StepSimulation(float dt_seconds);

    QSlider* ball_count_slider = nullptr;
    QLabel* ball_count_label = nullptr;

    unsigned int ball_count = 6;
    std::vector<BallState> balls_;
    float last_time_ = -1e9f;
    bool needs_reset_ = true;
};

#endif
