// SPDX-License-Identifier: GPL-2.0-only

#ifndef STRIPSHELLPATTERN_H
#define STRIPSHELLPATTERN_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Game/StripPatternSurface.h"
#include "StripShellPatternKernels.h"

class StratumBandPanel;
class QComboBox;
class QSlider;
class QLabel;

class StripShellPattern : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit StripShellPattern(QWidget* parent = nullptr);
    ~StripShellPattern() override;

    EFFECT_REGISTERER_3D("StripShellPattern", "Strip Shell Pattern", "Spatial", []() { return new StripShellPattern; });

    static std::string const ClassName() { return "StripShellPattern"; }
    static std::string const UIName() { return "Strip Shell Pattern"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnParameterChanged();
    void OnStratumBandChanged();

private:
    enum DisplayMode { DISP_SHELL_Y = 0, DISP_FILL_STRIP, DISP_COUNT };

    static const char* UnfoldModeLabel(int m);
    static const char* DisplayModeLabel(int d);

    float EvaluateKernel(float s01, float phase01, float time_sec, int pattern) const;

    int unfold_mode = 0;
    int display_mode = DISP_SHELL_Y;
    int pattern_id = 0;
    int strip_shell_color_style = 0;
    float direction_deg = 0.0f;
    float surface_thickness = 0.0f;
    float strip_repeats = 1.0f;
    float wave_amplitude = 0.2f;
    float edge_fade_pct = 0.0f;

    QComboBox* unfold_combo = nullptr;
    QComboBox* display_combo = nullptr;
    QComboBox* pattern_combo = nullptr;
    QComboBox* color_style_combo = nullptr;
    QSlider* dir_slider = nullptr;
    QLabel* dir_label = nullptr;
    QSlider* thick_slider = nullptr;
    QLabel* thick_label = nullptr;
    QSlider* repeats_slider = nullptr;
    QLabel* repeats_label = nullptr;
    QSlider* amp_slider = nullptr;
    QLabel* amp_label = nullptr;
    QSlider* edge_slider = nullptr;
    QLabel* edge_label = nullptr;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
