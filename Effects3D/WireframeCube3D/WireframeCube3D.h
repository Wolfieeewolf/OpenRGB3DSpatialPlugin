// SPDX-License-Identifier: GPL-2.0-only

#ifndef WIREFRAMECUBE3D_H
#define WIREFRAMECUBE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class WireframeCube3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit WireframeCube3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("WireframeCube3D", "Wireframe Cube", "3D Spatial", [](){ return new WireframeCube3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    static float PointToSegmentDistance(float px, float py, float pz,
                                        float ax, float ay, float az,
                                        float bx, float by, float bz);

    float thickness = 0.08f;
    float line_brightness = 1.0f;

    float cube_cache_time = -1e9f;
    float cube_corners[8][3];
    float cached_angle_deg = 0.0f;
};

#endif
