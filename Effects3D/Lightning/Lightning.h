// SPDX-License-Identifier: GPL-2.0-only

#ifndef LIGHTNING_H
#define LIGHTNING_H

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

class Lightning : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Lightning(QWidget* parent = nullptr);
    ~Lightning();

    EFFECT_REGISTERER_3D("Lightning", "Lightning", "Spatial", [](){return new Lightning;});

    static std::string const ClassName() { return "Lightning"; }
    static std::string const UIName() { return "Lightning"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnLightningParameterChanged();

private:
    enum Mode { MODE_PLASMA_BALL = 0, MODE_SKY, MODE_COUNT };
    static const char* ModeName(int m);
    static float hash11(float t);

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
    float           arc_aabb_min_x = 0, arc_aabb_min_y = 0, arc_aabb_min_z = 0;
    float           arc_aabb_max_x = 0, arc_aabb_max_y = 0, arc_aabb_max_z = 0;

    int             mode = MODE_PLASMA_BALL;
    float           flash_rate = 0.15f;
    float           flash_duration = 0.08f;
};

#endif
