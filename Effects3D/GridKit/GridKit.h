// SPDX-License-Identifier: GPL-2.0-only

#ifndef GRIDKIT_H
#define GRIDKIT_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

/** LED-cube style soft volumes: plane sweep, wireframe, boxes, Rubik cube, roam, send, lightning. */
class GridKit : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit GridKit(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("GridKit", "Grid Kit", "Spatial", [](){ return new GridKit; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Mode {
        MODE_PLANE_SWEEP = 0,
        MODE_WIREFRAME,
        MODE_MOVING_BOXES,
        MODE_RUBIK,
        MODE_SPHERE_ROAM,
        MODE_AXIS_SEND,
        MODE_LIGHTNING,
        MODE_COUNT
    };
    static const char* ModeName(int m);

    int mode = MODE_PLANE_SWEEP;
    int travel_axis = 1; /* 0=X 1=Y 2=Z */
    float thickness = 0.14f;
    float density = 0.65f;
    float sway = 0.70f;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
