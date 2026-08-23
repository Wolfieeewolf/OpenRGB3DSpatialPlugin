// SPDX-License-Identifier: GPL-2.0-only

#ifndef SURFACEAMBIENT_H
#define SURFACEAMBIENT_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "SpatialVolumeFieldAssist.h"

#include <cstdint>

class SurfaceAmbient : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit SurfaceAmbient(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("SurfaceAmbient", "Surface Ambient", "Spatial", [](){ return new SurfaceAmbient; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    enum Style { STYLE_FIRE = 0, STYLE_WATER, STYLE_SLIME, STYLE_LAVA, STYLE_EMBER, STYLE_OCEAN, STYLE_STEAM, STYLE_COUNT };
    enum Motion {
        MOTION_FIELD = 0,
        MOTION_WATERFALL,
        MOTION_RAIN,
        MOTION_DRIP,
        MOTION_FIRE_RISE,
        MOTION_WAVES,
        MOTION_PULSE,
        MOTION_COUNT
    };
    enum SurfaceMask {
        SURF_FLOOR  = 1,
        SURF_CEIL   = 2,
        SURF_WALL_XM = 4,
        SURF_WALL_XP = 8,
        SURF_WALL_ZM = 16,
        SURF_WALL_ZP = 32
    };
    static const char* StyleName(int s);
    static const char* MotionName(int m);
    /** role: 0 floor, 1 ceiling, 2 wall. alongA/alongB + up01 in [0,1]. */
    static float EvalPresetField(int style, int role, float alongA, float alongB, float up01,
                                 float time, float freq, float speed, float* sparse_mul);
    static float ApplySpatialMotion(int motion, int role, float alongA, float alongB, float up01,
                                    float time, float speed, float base);

    int style = STYLE_FIRE;
    int motion = MOTION_FIELD;
    int wall_kernel_id = 0;
    bool kernel_on_wall = false;
    float wall_kernel_repeats = 2.0f;
    float height_pct = 0.45f;
    float thickness = 0.08f;

    SpatialVolumeFieldAssist volume_assist_;
};

#endif
