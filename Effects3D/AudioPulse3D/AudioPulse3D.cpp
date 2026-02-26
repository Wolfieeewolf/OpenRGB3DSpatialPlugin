// SPDX-License-Identifier: GPL-2.0-only

#include "AudioPulse3D.h"
#include <QLabel>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <cmath>
#include <algorithm>

float AudioPulse3D::EvaluateIntensity(float amplitude, float time)
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

AudioPulse3D::AudioPulse3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D AudioPulse3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Audio Pulse";
    info.effect_description = "Room brightness pulses from a chosen frequency band";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = false;
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
    return info;
}

void AudioPulse3D::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    QHBoxLayout* hz_row = new QHBoxLayout();
    hz_row->addWidget(new QLabel("Low Hz:"));
    QSpinBox* low_spin = new QSpinBox();
    low_spin->setRange(1, 20000);
    low_spin->setValue(audio_settings.low_hz);
    hz_row->addWidget(low_spin);
    hz_row->addWidget(new QLabel("High Hz:"));
    QSpinBox* high_spin = new QSpinBox();
    high_spin->setRange(1, 20000);
    high_spin->setValue(audio_settings.high_hz);
    hz_row->addWidget(high_spin);
    layout->addLayout(hz_row);

    connect(low_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        audio_settings.low_hz = v;
        emit ParametersChanged();
    });
    connect(high_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        audio_settings.high_hz = v;
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

    QHBoxLayout* falloff_row = new QHBoxLayout();
    falloff_row->addWidget(new QLabel("Falloff:"));
    QSlider* falloff_slider = new QSlider(Qt::Horizontal);
    falloff_slider->setRange(20, 500);
    falloff_slider->setValue((int)(audio_settings.falloff * 100.0f));
    QLabel* falloff_label = new QLabel(QString::number(audio_settings.falloff, 'f', 1));
    falloff_label->setMinimumWidth(36);
    falloff_row->addWidget(falloff_slider);
    falloff_row->addWidget(falloff_label);
    layout->addLayout(falloff_row);

    connect(falloff_slider, &QSlider::valueChanged, this, [this, falloff_label](int v){
        audio_settings.falloff = v / 100.0f;
        falloff_label->setText(QString::number(audio_settings.falloff, 'f', 1));
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

    QCheckBox* radial_check = new QCheckBox("Radial Fade");
    radial_check->setChecked(use_radial);
    connect(radial_check, &QCheckBox::toggled, this, [this](bool checked){
        use_radial = checked;
        emit ParametersChanged();
    });
    layout->addWidget(radial_check);
}

void AudioPulse3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor AudioPulse3D::CalculateColor(float x, float y, float z, float time)
{
    float amplitude = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    float distance = 0.0f;
    if(use_radial)
    {
        Vector3D origin = GetEffectOrigin();
        float dx = x - origin.x;
        float dy = y - origin.y;
        float dz = z - origin.z;
        distance = std::clamp(std::sqrt(dx*dx + dy*dy + dz*dz) / 0.75f, 0.0f, 1.0f);
    }

    float brightness = use_radial ? intensity * (1.0f - distance * 0.5f) : intensity;
    brightness = std::clamp(brightness, 0.0f, 1.0f);

    RGBColor color = ComposeAudioGradientColor(audio_settings, use_radial ? (1.0f - distance) : 0.5f, intensity);
    color = ScaleRGBColor(color, 0.25f + 0.75f * brightness);

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(CalculateProgress(time) * 360.0f)
        : GetColorAtPosition(0.0f);
    return ModulateRGBColors(color, user_color);
}

RGBColor AudioPulse3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    float amplitude = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    float distance = 0.0f;
    if(use_radial)
    {
        Vector3D origin = GetEffectOriginGrid(grid);
        float dx = x - origin.x;
        float dy = y - origin.y;
        float dz = z - origin.z;
        float max_radius = 0.5f * std::max({grid.width, grid.height, grid.depth});
        distance = std::clamp(std::sqrt(dx*dx + dy*dy + dz*dz) / std::max(max_radius, 1e-5f), 0.0f, 1.0f);
    }

    float brightness = use_radial ? intensity * (1.0f - distance * 0.5f) : intensity;
    brightness = std::clamp(brightness, 0.0f, 1.0f);

    RGBColor color = ComposeAudioGradientColor(audio_settings, use_radial ? (1.0f - distance) : 0.5f, intensity);
    color = ScaleRGBColor(color, 0.25f + 0.75f * brightness);

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(CalculateProgress(time) * 360.0f)
        : GetColorAtPosition(0.0f);
    return ModulateRGBColors(color, user_color);
}

nlohmann::json AudioPulse3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["use_radial"] = use_radial;
    return j;
}

void AudioPulse3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("use_radial"))
    {
        use_radial = settings["use_radial"].get<bool>();
    }
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
}

REGISTER_EFFECT_3D(AudioPulse3D)
