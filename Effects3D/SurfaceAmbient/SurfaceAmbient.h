// SPDX-License-Identifier: GPL-2.0-only

#ifndef SURFACEAMBIENT_H
#define SURFACEAMBIENT_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "SpatialVolumeFieldAssist.h"

#include <cstdint>

class QComboBox;
class QWidget;

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

private:
    enum Style {
        STYLE_NONE = 0,
        STYLE_FIRE,
        STYLE_WATER,
        STYLE_SLIME,
        STYLE_LAVA,
        STYLE_EMBER,
        STYLE_OCEAN,
        STYLE_STEAM,
        STYLE_COUNT
    };
    enum Motion {
        MOTION_SOFT = 0,   // gentle undulation (used when Preset is None)
        MOTION_WATERFALL,
        MOTION_RAIN,
        MOTION_DRIP,
        MOTION_FIRE_RISE,
        MOTION_WAVES,
        MOTION_PULSE,
        MOTION_COUNT
    };

    static const char* StyleName(int s);
    static const char* MotionName(int m);
    bool HasLockedPreset() const { return style > STYLE_NONE && style < STYLE_COUNT; }
    void UpdateMotionUiEnabled();
    RGBColor PresetColor(float plasma01, float time, float speed_mul, float stratum_phase01) const;

    int style = STYLE_FIRE;
    int motion = MOTION_SOFT;
    /* Shell soft-edge only — depth/feature size come from global Scale / Size. */
    float thickness = 0.08f;

    QComboBox* style_combo_ = nullptr;
    QComboBox* motion_combo_ = nullptr;
    QWidget* motion_row_ = nullptr;

    SpatialVolumeFieldAssist volume_assist_;
};

#endif
