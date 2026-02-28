// SPDX-License-Identifier: GPL-2.0-only

#include "SkyLightning3D.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

REGISTER_EFFECT_3D(SkyLightning3D);

static float hash11(float t)
{
    float s = sinf(t * 12.9898f) * 43758.5453f;
    return s - floorf(s);
}

SkyLightning3D::SkyLightning3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
}

EffectInfo3D SkyLightning3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Sky Lightning";
    info.effect_description = "Real sky lightning: occasional bright flashes from above illuminating the room.";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_SKY_LIGHTNING;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
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
    info.show_scale_control = false;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void SkyLightning3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;

    layout->addWidget(new QLabel("Flash rate:"), row, 0);
    QSlider* rate_slider = new QSlider(Qt::Horizontal);
    rate_slider->setRange(5, 50);
    rate_slider->setValue((int)(flash_rate * 100.0f));
    QLabel* rate_label = new QLabel(QString::number(flash_rate, 'f', 2));
    rate_label->setMinimumWidth(36);
    layout->addWidget(rate_slider, row, 1);
    layout->addWidget(rate_label, row, 2);
    connect(rate_slider, &QSlider::valueChanged, this, [this, rate_label](int v){
        flash_rate = v / 100.0f;
        rate_label->setText(QString::number(flash_rate, 'f', 2));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Flash duration:"), row, 0);
    QSlider* dur_slider = new QSlider(Qt::Horizontal);
    dur_slider->setRange(2, 25);
    dur_slider->setValue((int)(flash_duration * 100.0f));
    QLabel* dur_label = new QLabel(QString::number(flash_duration * 1000.0f, 'f', 0) + " ms");
    dur_label->setMinimumWidth(50);
    layout->addWidget(dur_slider, row, 1);
    layout->addWidget(dur_label, row, 2);
    connect(dur_slider, &QSlider::valueChanged, this, [this, dur_label](int v){
        flash_duration = v / 100.0f;
        dur_label->setText(QString::number(flash_duration * 1000.0f, 'f', 0) + " ms");
        emit ParametersChanged();
    });
    row++;

    AddWidgetToParent(w, parent);
}

void SkyLightning3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_SKY_LIGHTNING;
}

RGBColor SkyLightning3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor SkyLightning3D::CalculateColorGrid(float /*x*/, float y, float /*z*/, float time, const GridContext3D& grid)
{
    float rate = std::max(0.05f, std::min(0.5f, flash_rate));
    float interval = 1.0f / rate;
    float dur = std::max(0.02f, std::min(0.25f, flash_duration));

    float cycle = floorf(time / interval);
    float flash_offset = hash11(cycle) * interval * 0.6f;
    float phase_in_cycle = time - cycle * interval;
    float flash_phase = phase_in_cycle - flash_offset;

    float intensity = 0.0f;
    if(flash_phase >= 0.0f && flash_phase < dur)
    {
        float rise = (flash_phase < dur * 0.15f) ? (flash_phase / (dur * 0.15f)) : 1.0f;
        float fall = (flash_phase > dur * 0.6f) ? (1.0f - (flash_phase - dur * 0.6f) / (dur * 0.4f)) : 1.0f;
        intensity = rise * fall;
    }

    if(intensity <= 0.001f)
        return 0x00000000;

    float norm_y = (grid.height > 0.001f) ? ((y - grid.min_y) / grid.height) : 0.5f;
    norm_y = std::max(0.0f, std::min(1.0f, norm_y));
    float sky_factor = 0.6f + 0.4f * norm_y;

    RGBColor base;
    if(GetRainbowMode())
        base = GetRainbowColor(time * 50.0f + norm_y * 100.0f);
    else
    {
        const std::vector<RGBColor>& cols = GetColors();
        base = (cols.size() > 0) ? cols[0] : 0x00FFFFFF;
    }
    int r = base & 0xFF, g = (base >> 8) & 0xFF, b = (base >> 16) & 0xFF;
    r = (int)(r * intensity * sky_factor);
    g = (int)(g * intensity * sky_factor);
    b = (int)(b * intensity * sky_factor);
    r = std::min(255, r); g = std::min(255, g); b = std::min(255, b);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json SkyLightning3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["flash_rate"] = flash_rate;
    j["flash_duration"] = flash_duration;
    return j;
}

void SkyLightning3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("flash_rate") && settings["flash_rate"].is_number())
        flash_rate = std::max(0.05f, std::min(0.5f, settings["flash_rate"].get<float>()));
    if(settings.contains("flash_duration") && settings["flash_duration"].is_number())
        flash_duration = std::max(0.02f, std::min(0.25f, settings["flash_duration"].get<float>()));
}
