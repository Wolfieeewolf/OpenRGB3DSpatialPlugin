// SPDX-License-Identifier: GPL-2.0-only

#ifndef WAVE_H
#define WAVE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StratumBandPanel;
class StripKernelColormapPanel;
class QComboBox;
class QSlider;
class QLabel;

class Wave : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Wave(QWidget* parent = nullptr);
    ~Wave();

    EFFECT_REGISTERER_3D("Wave", "Wave", "Spatial", [](){return new Wave;});

    static std::string const ClassName() { return "Wave"; }
    static std::string const UIName() { return "Wave"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnWaveParameterChanged();
    void OnModeChanged();
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    enum Mode { MODE_LINE = 0, MODE_SURFACE, MODE_COUNT };
    enum WaveStyle { STYLE_SINUS = 0, STYLE_RADIAL, STYLE_LINEAR, STYLE_OCEAN_DRIFT, STYLE_GRADIENT, STYLE_COUNT };
    static const char* ModeName(int m);
    static const char* WaveStyleName(int s);

    float smoothstep(float edge0, float edge1, float x) const;

    int mode = MODE_LINE;
    QComboBox* style_combo = nullptr;
    QWidget* line_controls = nullptr;
    QWidget* surface_controls = nullptr;

    QComboBox* shape_combo = nullptr;
    QComboBox* edge_shape_combo = nullptr;
    QSlider* thickness_slider = nullptr;
    QLabel* thickness_label = nullptr;
    int shape_type = 0;
    int edge_shape = 0;
    int wave_thickness = 30;

    QComboBox* surface_style_combo = nullptr;
    QSlider* surface_thick_slider = nullptr;
    QLabel* surface_thick_label = nullptr;
    QSlider* surface_freq_slider = nullptr;
    QLabel* surface_freq_label = nullptr;
    QSlider* surface_amp_slider = nullptr;
    QLabel* surface_amp_label = nullptr;
    QSlider* surface_dir_slider = nullptr;
    QLabel* surface_dir_label = nullptr;
    QSlider* surface_edge_fade_slider = nullptr;
    QLabel* surface_edge_fade_label = nullptr;
    int wave_style = STYLE_SINUS;
    float surface_thickness = 0.08f;
    float wave_frequency = 1.0f;
    float wave_amplitude = 1.0f;
    float wave_direction_deg = 0.0f;
    float surface_edge_fade = 18.0f;

    float progress = 0.0f;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool wave_strip_cmap_on = false;
    int wave_strip_cmap_kernel = 0;
    float wave_strip_cmap_rep = 4.0f;
    int wave_strip_cmap_unfold = 0;
    float wave_strip_cmap_dir = 0.0f;
    int wave_strip_cmap_color_style = 0;
};

#endif
