// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROTATINGCONESPOTLIGHTS_H
#define ROTATINGCONESPOTLIGHTS_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

#include <array>

class QSlider;
class QComboBox;
class QWidget;
class EffectSliderRow;

class RotatingConeSpotlights : public SpatialEffect3D
{
    Q_OBJECT

public:
    static constexpr int kMaxCones = 4;

    explicit RotatingConeSpotlights(QWidget* parent = nullptr);
    ~RotatingConeSpotlights() override;

    EFFECT_REGISTERER_3D("RotatingConeSpotlights", "Rotating Cone Spotlights", "Spatial", []() {
        return new RotatingConeSpotlights;
    });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Surface {
        SURF_CENTER = 0,
        SURF_REF,
        SURF_CEILING,
        SURF_FLOOR,
        SURF_WALLS,
        SURF_COUNT
    };
    enum MotionMode {
        MOTION_INDEPENDENT = 0,
        MOTION_OPPOSITE,
        MOTION_COUNT
    };
    enum LayoutPreset {
        LAYOUT_AUTO = 0,
        LAYOUT_CENTER,
        LAYOUT_ROW,
        LAYOUT_CORNERS,
        LAYOUT_WALLS,
        LAYOUT_CUSTOM,
        LAYOUT_COUNT
    };

    static const char* SurfaceName(int s);
    static const char* MotionName(int m);
    static const char* LayoutName(int l);

    void ApplyLayoutPreset(int preset);
    void UpdateConeSliderVisibility();
    void UpdateConeSliderLabels();
    void MarkCustomLayout();
    void SyncUiFromState();

    float cone_scale = 0.10132118364233777f;
    float hue01 = 0.0f;
    float motion_rate = 1.0f;
    float wander_amt = 1.0f;
    int cone_count = 2;
    int surface = SURF_CENTER;
    int motion_mode = MOTION_INDEPENDENT;
    int layout_preset = LAYOUT_AUTO;
    std::array<float, kMaxCones> apex_u = {0.5f, 0.25f, 0.5f, 0.75f};
    std::array<float, kMaxCones> apex_v = {0.5f, 0.5f, 0.5f, 0.5f};

    QSlider* cone_slider = nullptr;
    QSlider* hue_slider = nullptr;
    QSlider* motion_slider = nullptr;
    QSlider* wander_slider = nullptr;
    QSlider* count_slider = nullptr;
    QComboBox* surface_combo = nullptr;
    QComboBox* motion_combo = nullptr;
    QComboBox* layout_combo = nullptr;
    QWidget* cone_pos_rows_[kMaxCones] = {};
    EffectSliderRow* apex_u_row_[kMaxCones] = {};
    EffectSliderRow* apex_v_row_[kMaxCones] = {};
    QSlider* apex_u_slider_[kMaxCones] = {};
    QSlider* apex_v_slider_[kMaxCones] = {};
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
