// SPDX-License-Identifier: GPL-2.0-only

#include "CubeLayer3D.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

float CubeLayer3D::EvaluateIntensity(float amplitude, float time)
{
    float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    if(std::fabs(time - last_intensity_time) > 1e-4f)
    {
        smoothed = alpha * smoothed + (1.0f - alpha) * amplitude;
        last_intensity_time = time;
    }
    else if(alpha <= 0.0f)
    {
        smoothed = amplitude;
    }
    return ApplyAudioIntensity(std::clamp(smoothed, 0.0f, 1.0f), audio_settings);
}

float CubeLayer3D::AxisPosition(int axis, float x, float y, float z,
                                float min_x, float max_x, float min_y, float max_y, float min_z, float max_z)
{
    float val = 0.0f, lo = 0.0f, hi = 1.0f;
    switch(axis)
    {
        case 0: val = x; lo = min_x; hi = max_x; break;
        case 2: val = z; lo = min_z; hi = max_z; break;
        default: val = y; lo = min_y; hi = max_y; break;
    }
    float range = hi - lo;
    if(range < 1e-5f) return 0.5f;
    return std::clamp((val - lo) / range, 0.0f, 1.0f);
}

CubeLayer3D::CubeLayer3D(QWidget* parent) : SpatialEffect3D(parent)
{
}

EffectInfo3D CubeLayer3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Cube Layer";
    info.effect_description = "One lit layer at a time (LED cube style); layer position follows audio level";
    info.category = "Audio";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 0;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 1.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.show_path_axis_control = true;
    return info;
}

void CubeLayer3D::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
        layout = new QVBoxLayout(parent);

    QHBoxLayout* thick_row = new QHBoxLayout();
    thick_row->addWidget(new QLabel("Layer thickness:"));
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(3, 40);
    thick_slider->setValue((int)(layer_thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(layer_thickness * 100)) + "%");
    thick_label->setMinimumWidth(40);
    thick_row->addWidget(thick_slider);
    thick_row->addWidget(thick_label);
    layout->addLayout(thick_row);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        layer_thickness = v / 100.0f;
        thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* smooth_row = new QHBoxLayout();
    smooth_row->addWidget(new QLabel("Smoothing:"));
    QSlider* smooth_slider = new QSlider(Qt::Horizontal);
    smooth_slider->setRange(0, 99);
    smooth_slider->setValue((int)(audio_settings.smoothing * 100.0f));
    QLabel* smooth_label = new QLabel(QString::number(audio_settings.smoothing, 'f', 2));
    smooth_label->setMinimumWidth(36);
    smooth_row->addWidget(smooth_slider);
    smooth_row->addWidget(smooth_label);
    layout->addLayout(smooth_row);
    connect(smooth_slider, &QSlider::valueChanged, this, [this, smooth_label](int v){
        audio_settings.smoothing = v / 100.0f;
        smooth_label->setText(QString::number(audio_settings.smoothing, 'f', 2));
        emit ParametersChanged();
    });

    QHBoxLayout* boost_row = new QHBoxLayout();
    boost_row->addWidget(new QLabel("Peak Boost:"));
    QSlider* boost_slider = new QSlider(Qt::Horizontal);
    boost_slider->setRange(50, 400);
    boost_slider->setValue((int)(audio_settings.peak_boost * 100.0f));
    QLabel* boost_label = new QLabel(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
    boost_label->setMinimumWidth(44);
    boost_row->addWidget(boost_slider);
    boost_row->addWidget(boost_label);
    layout->addLayout(boost_row);
    connect(boost_slider, &QSlider::valueChanged, this, [this, boost_label](int v){
        audio_settings.peak_boost = v / 100.0f;
        boost_label->setText(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
        emit ParametersChanged();
    });
}

void CubeLayer3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor CubeLayer3D::CalculateColor(float x, float y, float z, float time)
{
    float amplitude = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float layer_pos = EvaluateIntensity(amplitude, time);

    Vector3D origin = GetEffectOrigin();
    Vector3D r = TransformPointByRotation(x, y, z, origin);
    float axis_pos = AxisPosition(GetPathAxis(), r.x, r.y, r.z, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    float sigma = std::max(layer_thickness, 0.02f);
    float d = (axis_pos - layer_pos) / sigma;
    float intensity = std::exp(-d * d * 0.5f);
    intensity = std::clamp(intensity, 0.0f, 1.0f);

    float gradient_pos = layer_pos;
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, intensity);
    color = ScaleRGBColor(color, 0.2f + 0.8f * intensity);
    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(gradient_pos * 360.0f + time * 30.0f)
        : GetColorAtPosition(gradient_pos);
    return ModulateRGBColors(color, user_color);
}

RGBColor CubeLayer3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float amplitude = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float layer_pos = EvaluateIntensity(amplitude, time);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float axis_pos = AxisPosition(GetPathAxis(), rotated_pos.x, rotated_pos.y, rotated_pos.z,
                                   grid.min_x, grid.max_x,
                                   grid.min_y, grid.max_y,
                                   grid.min_z, grid.max_z);
    float sigma = std::max(layer_thickness, 0.02f);
    float d = (axis_pos - layer_pos) / sigma;
    float intensity = std::exp(-d * d * 0.5f);
    intensity = std::clamp(intensity, 0.0f, 1.0f);

    float gradient_pos = layer_pos;
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, intensity);
    color = ScaleRGBColor(color, 0.2f + 0.8f * intensity);
    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(gradient_pos * 360.0f + time * 30.0f)
        : GetColorAtPosition(gradient_pos);
    return ModulateRGBColors(color, user_color);
}

nlohmann::json CubeLayer3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["layer_thickness"] = layer_thickness;
    return j;
}

void CubeLayer3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("layer_thickness")) layer_thickness = std::clamp(settings["layer_thickness"].get<float>(), 0.03f, 0.5f);
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
}

REGISTER_EFFECT_3D(CubeLayer3D)
