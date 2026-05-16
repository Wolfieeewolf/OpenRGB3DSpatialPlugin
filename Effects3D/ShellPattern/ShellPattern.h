// SPDX-License-Identifier: GPL-2.0-only

#ifndef SHELLPATTERN_H
#define SHELLPATTERN_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Game/StripPatternSurface.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"

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

    static std::string const ClassName() { return "ShellPattern"; }
    static std::string const UIName() { return "Shell Pattern"; }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnParameterChanged();
private:
    enum DisplayMode { DISP_SHELL_Y = 0, DISP_FILL_STRIP, DISP_COUNT };

    static const char* UnfoldModeLabel(int m);
    static const char* DisplayModeLabel(int d);

    float EvaluateKernel(float s01, float phase01, float time_sec, int pattern, float repeats) const;

    int unfold_mode = 0;
    int display_mode = DISP_SHELL_Y;
    int pattern_id = 0;
    int strip_shell_color_style = 0;
    float direction_deg = 0.0f;
    float surface_thickness = 0.0f;
    float strip_repeats = 1.0f;
    float wave_amplitude = 0.2f;
    float edge_fade_pct = 0.0f;

    QComboBox* display_combo = nullptr;
    QSlider* thick_slider = nullptr;
    QLabel* thick_label = nullptr;
    QSlider* amp_slider = nullptr;
    QLabel* amp_label = nullptr;
    QSlider* edge_slider = nullptr;
    QLabel* edge_label = nullptr;
};

#endif
