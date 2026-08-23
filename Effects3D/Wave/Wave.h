// SPDX-License-Identifier: GPL-2.0-only

#ifndef WAVE_H
#define WAVE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class QComboBox;
class QSlider;

class Wave : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Wave(QWidget* parent = nullptr);
    ~Wave();

    EFFECT_REGISTERER_3D("Wave", "Wave", "Spatial", [](){return new Wave;});

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum WaveStyle { STYLE_SINUS = 0, STYLE_RADIAL, STYLE_LINEAR, STYLE_OCEAN_DRIFT, STYLE_GRADIENT, STYLE_COUNT };
    static const char* WaveStyleName(int s);

    float smoothstep(float edge0, float edge1, float x) const;

    QComboBox* surface_style_combo = nullptr;
    QSlider* surface_thick_slider = nullptr;
    QSlider* surface_freq_slider = nullptr;
    QSlider* surface_amp_slider = nullptr;
    QSlider* surface_dir_slider = nullptr;
    QSlider* surface_edge_fade_slider = nullptr;
    int wave_style = STYLE_SINUS;
    float surface_thickness = 0.08f;
    float wave_frequency = 1.0f;
    float wave_amplitude = 1.0f;
    float wave_direction_deg = 0.0f;
    float surface_edge_fade = 18.0f;

    SpatialVolumeFieldAssist surface_volume_assist_;
};

#endif
