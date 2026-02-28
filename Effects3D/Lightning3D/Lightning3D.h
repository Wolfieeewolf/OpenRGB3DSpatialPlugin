// SPDX-License-Identifier: GPL-2.0-only

#ifndef LIGHTNING3D_H
#define LIGHTNING3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "LEDPosition3D.h"
#include <vector>

struct PlasmaArc3D
{
    Vector3D start;
    Vector3D end;
    float birth_time;
    float duration;
    unsigned int seed;
};

class Lightning3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Lightning3D(QWidget* parent = nullptr);
    ~Lightning3D();

    EFFECT_REGISTERER_3D("Lightning3D", "Plasma Ball", "3D Spatial", [](){return new Lightning3D;});

    static std::string const ClassName() { return "Lightning3D"; }
    static std::string const UIName() { return "Plasma Ball"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnLightningParameterChanged();

private:
    void UpdateArchCache(float time, const GridContext3D& grid);
    Vector3D RandomPointOnGlass(const GridContext3D& grid, unsigned int seed);
    static float DistToSegment(float px, float py, float pz,
                              float ax, float ay, float az,
                              float bx, float by, float bz);
    static float HashF(unsigned int seed);

    QSlider*        strike_rate_slider;
    QLabel*         strike_rate_label;
    QSlider*        branch_slider;
    QLabel*         branch_label;
    unsigned int    strike_rate;
    unsigned int    branches;

    float           cache_time;
    float           cache_grid_hash;
    std::vector<PlasmaArc3D> cached_arches;
};

#endif
