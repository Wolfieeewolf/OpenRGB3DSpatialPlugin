// SPDX-License-Identifier: GPL-2.0-only

#ifndef SHELLPATTERN_H
#define SHELLPATTERN_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Game/StripPatternSurface.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class QComboBox;
class QSlider;
class QLabel;

class ShellPattern : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit ShellPattern(QWidget* parent = nullptr);
    ~ShellPattern() override;

    EFFECT_REGISTERER_3D("ShellPattern", "Shell Pattern", "Spatial", []() { return new ShellPattern; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnParameterChanged();
private:
    enum DisplayMode {
        DISP_SHELL_Y = 0,
        DISP_FILL_STRIP,
        DISP_SHELL_RADIAL_XZ,
        DISP_CONTOUR,
        DISP_BARS,
        DISP_RIPPLES,
        DISP_DROPLETS,
        DISP_FIREWORKS,
        DISP_EXPLOSION,
        DISP_RAIN,
        DISP_COUNT
    };

    static const char* UnfoldModeLabel(int m);
    static const char* DisplayModeLabel(int d);

    int unfold_mode = 0;
    int display_mode = DISP_FILL_STRIP;
    int pattern_id = 0;
    float direction_deg = 0.0f;
    float surface_thickness = 0.14f;
    float strip_repeats = 2.0f;
    float wave_amplitude = 0.85f;
    float edge_fade_pct = 0.0f;

    QComboBox* display_combo = nullptr;
    QComboBox* unfold_combo = nullptr;
    QComboBox* pattern_combo = nullptr;
    QSlider*   thick_slider  = nullptr;
    QSlider*   amp_slider    = nullptr;
    QSlider*   edge_slider   = nullptr;
    QSlider*   direction_slider = nullptr;
    QSlider*   repeats_slider = nullptr;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
