// SPDX-License-Identifier: GPL-2.0-only

#ifndef TRAVELINGLIGHT_H
#define TRAVELINGLIGHT_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "SpatialVolumeFieldAssist.h"

#include <cstdint>

class QWidget;
class QComboBox;
class QSlider;

class TravelingLight : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit TravelingLight(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("TravelingLight", "Traveling Light", "Spatial", [](){ return new TravelingLight; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Mode {
        MODE_COMET = 0,
        MODE_CHASE,
        MODE_MARQUEE,
        MODE_ZIGZAG,
        MODE_KITT,
        MODE_WIPE,
        MODE_MOVING_PANES,
        MODE_CROSSING,
        MODE_ROTATING,
        MODE_WAVE_FRONTS,
        MODE_COUNT
    };
    static const char* ModeName(int m);
    float smoothstep(float edge0, float edge1, float x) const;
    void UpdateWaveFrontsControlsVisible();

    int mode = MODE_COMET;
    float glow = 0.5f;
    int wipe_edge_shape = 0;
    int num_divisions = 4;

    // Wave Fronts (ported from old Wave Line)
    int front_shape = 0;       // Circles / Squares / Lines / Diagonal
    int front_edge = 0;        // Round / Sharp / Square
    int front_thickness = 30;  // 5–100
    QWidget* wave_fronts_panel = nullptr;
    QComboBox* front_shape_combo = nullptr;
    QComboBox* front_edge_combo = nullptr;
    QSlider* front_thickness_slider = nullptr;

    SpatialVolumeFieldAssist volume_assist_;
};

#endif
