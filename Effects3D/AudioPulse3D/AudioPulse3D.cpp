/*---------------------------------------------------------*\
| AudioPulse3D.cpp                                          |
|                                                           |
|   Simple audio-reactive pulse effect                      |
|   Designed for frequency range system                     |
|                                                           |
|   Date: 2026-01-27                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "AudioPulse3D.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "Colors.h"
#include <QLabel>
#include <QCheckBox>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <cmath>

AudioPulse3D::AudioPulse3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D AudioPulse3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Audio Pulse";
    info.effect_description = "Simple brightness pulse based on audio level";
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
    QVBoxLayout* layout = new QVBoxLayout(parent);
    
    QHBoxLayout* intensity_row = new QHBoxLayout();
    intensity_row->addWidget(new QLabel("Pulse Intensity:"));
    QSlider* intensity_slider = new QSlider(Qt::Horizontal);
    intensity_slider->setRange(10, 200);
    intensity_slider->setValue((int)(pulse_intensity * 100.0f));
    connect(intensity_slider, &QSlider::valueChanged, this, [this](int value) {
        pulse_intensity = value / 100.0f;
        emit ParametersChanged();
    });
    intensity_row->addWidget(intensity_slider);
    QLabel* intensity_label = new QLabel(QString::number(pulse_intensity, 'f', 2) + "x");
    intensity_label->setMinimumWidth(50);
    connect(intensity_slider, &QSlider::valueChanged, [intensity_label](int value) {
        intensity_label->setText(QString::number(value / 100.0f, 'f', 2) + "x");
    });
    intensity_row->addWidget(intensity_label);
    layout->addLayout(intensity_row);
    
    QCheckBox* radial_check = new QCheckBox("Radial Fade");
    radial_check->setChecked(use_radial);
    connect(radial_check, &QCheckBox::toggled, this, [this](bool checked) {
        use_radial = checked;
        emit ParametersChanged();
    });
    layout->addWidget(radial_check);
}

void AudioPulse3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor AudioPulse3D::CalculateColor(float x, float y, float z, float /*time*/)
{
    float distance = 0.0f;
    if(use_radial)
    {
        Vector3D origin = GetEffectOrigin();
        float dx = x - origin.x;
        float dy = y - origin.y;
        float dz = z - origin.z;
        distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        distance = std::min(1.0f, distance / 1.0f);
    }
    
    float brightness = audio_level * pulse_intensity;
    if(use_radial)
    {
        brightness *= (1.0f - distance * 0.5f);
    }
    brightness = std::min(1.0f, std::max(0.0f, brightness));
    
    RGBColor user_color = GetColorAtPosition(0.0f);
    return ScaleRGBColor(user_color, brightness);
}

RGBColor AudioPulse3D::CalculateColorGrid(float x, float y, float z, float /*time*/, const GridContext3D& grid)
{
    float distance = 0.0f;
    if(use_radial)
    {
        Vector3D origin = GetEffectOriginGrid(grid);
        float dx = x - origin.x;
        float dy = y - origin.y;
        float dz = z - origin.z;
        float max_radius = 0.5f * std::max({grid.width, grid.height, grid.depth});
        distance = std::sqrt(dx*dx + dy*dy + dz*dz) / max_radius;
        distance = std::min(1.0f, distance);
    }
    
    float brightness = audio_level * pulse_intensity;
    if(use_radial)
    {
        brightness *= (1.0f - distance * 0.5f);
    }
    brightness = std::min(1.0f, std::max(0.0f, brightness));
    
    RGBColor user_color = GetColorAtPosition(0.0f);
    return ScaleRGBColor(user_color, brightness);
}

nlohmann::json AudioPulse3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["pulse_intensity"] = pulse_intensity;
    j["use_radial"] = use_radial;
    return j;
}

void AudioPulse3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    
    if(settings.contains("pulse_intensity"))
    {
        pulse_intensity = settings["pulse_intensity"].get<float>();
    }
    if(settings.contains("use_radial"))
    {
        use_radial = settings["use_radial"].get<bool>();
    }
    if(settings.contains("audio_level"))
    {
        audio_level = settings["audio_level"].get<float>();
    }
}

REGISTER_EFFECT_3D(AudioPulse3D)
