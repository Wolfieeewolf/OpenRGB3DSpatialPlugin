// SPDX-License-Identifier: GPL-2.0-only

#include "FreqFill3D.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

float FreqFill3D::EvaluateIntensity(float amplitude, float time)
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
    return ApplyAudioIntensity(smoothed, audio_settings);
}

FreqFill3D::FreqFill3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D FreqFill3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Frequency Fill";
    info.effect_description = "Fills room along an axis like a VU meter";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 0;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 1.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_fps_control = false;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.show_path_axis_control = true;
    return info;
}

void FreqFill3D::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    QHBoxLayout* edge_row = new QHBoxLayout();
    edge_row->addWidget(new QLabel("Edge Width:"));
    QSlider* edge_slider = new QSlider(Qt::Horizontal);
    edge_slider->setRange(0, 40);
    edge_slider->setValue((int)(edge_width * 100.0f));
    QLabel* edge_label = new QLabel(QString::number((int)(edge_width * 100)) + "%");
    edge_label->setMinimumWidth(40);
    edge_row->addWidget(edge_slider);
    edge_row->addWidget(edge_label);
    layout->addLayout(edge_row);

    connect(edge_slider, &QSlider::valueChanged, this, [this, edge_label](int v){
        edge_width = v / 100.0f;
        edge_label->setText(QString::number(v) + "%");
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

void FreqFill3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

static float AxisPosition(int axis, float x, float y, float z,
                          float min_x, float max_x,
                          float min_y, float max_y,
                          float min_z, float max_z)
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

RGBColor FreqFill3D::CalculateColor(float x, float y, float z, float time)
{
    float amplitude = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float fill_level = EvaluateIntensity(amplitude, time);

    float pos = AxisPosition(GetPathAxis(), x, y, z, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    float edge = std::max(edge_width, 1e-3f);
    float blend = std::clamp((fill_level - pos) / edge + 0.5f, 0.0f, 1.0f);

    RGBColor lit_color = GetRainbowMode()
        ? GetRainbowColor(pos * 360.0f)
        : GetColorAtPosition(pos);
    RGBColor dark_color = GetColorAtPosition(1.0f);

    RGBColor color = BlendRGBColors(dark_color, lit_color, blend);
    return ScaleRGBColor(color, 0.1f + 0.9f * blend);
}

RGBColor FreqFill3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    float amplitude = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float fill_level = EvaluateIntensity(amplitude, time);

    float pos = AxisPosition(GetPathAxis(), x, y, z,
                             grid.min_x, grid.max_x,
                             grid.min_y, grid.max_y,
                             grid.min_z, grid.max_z);

    float edge = std::max(edge_width, 1e-3f);
    float blend = std::clamp((fill_level - pos) / edge + 0.5f, 0.0f, 1.0f);

    RGBColor lit_color = GetRainbowMode()
        ? GetRainbowColor(pos * 360.0f)
        : GetColorAtPosition(pos);
    RGBColor dark_color = GetColorAtPosition(1.0f);

    RGBColor color = BlendRGBColors(dark_color, lit_color, blend);
    return ScaleRGBColor(color, 0.1f + 0.9f * blend);
}

nlohmann::json FreqFill3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["edge_width"] = edge_width;
    return j;
}

void FreqFill3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("edge_width")) edge_width = settings["edge_width"].get<float>();
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
}

REGISTER_EFFECT_3D(FreqFill3D)
