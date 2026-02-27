// SPDX-License-Identifier: GPL-2.0-only

#include "Comet3D.h"
#include "../EffectHelpers.h"
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>
#include <algorithm>

REGISTER_EFFECT_3D(Comet3D);

Comet3D::Comet3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(50);
    SetRainbowMode(true);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    SetColors(default_colors);
}

EffectInfo3D Comet3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Comet";
    info.effect_description = "A comet that travels along an axis through the room with a fading tail";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_COMET;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 1.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void Comet3D::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
        layout = new QVBoxLayout(parent);

    QHBoxLayout* axis_row = new QHBoxLayout();
    axis_row->addWidget(new QLabel("Axis:"));
    QComboBox* axis_combo = new QComboBox();
    axis_combo->addItem("X (left → right)", 0);
    axis_combo->addItem("Y (floor → ceiling)", 1);
    axis_combo->addItem("Z (front → back)", 2);
    axis_combo->setCurrentIndex(comet_axis);
    axis_row->addWidget(axis_combo);
    layout->addLayout(axis_row);

    connect(axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, axis_combo](int){
        comet_axis = axis_combo->currentData().toInt();
        emit ParametersChanged();
    });

    QHBoxLayout* size_row = new QHBoxLayout();
    size_row->addWidget(new QLabel("Tail size:"));
    QSlider* size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(5, 80);
    size_slider->setValue((int)(comet_size * 100.0f));
    QLabel* size_label = new QLabel(QString::number((int)(comet_size * 100)) + "%");
    size_label->setMinimumWidth(36);
    size_row->addWidget(size_slider);
    size_row->addWidget(size_label);
    layout->addLayout(size_row);

    connect(size_slider, &QSlider::valueChanged, this, [this, size_label](int v){
        comet_size = v / 100.0f;
        if(size_label) size_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
}

void Comet3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_COMET;
}

RGBColor Comet3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor Comet3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rotated = TransformPointByRotation(x, y, z, origin);

    float axis_val = (comet_axis == 0) ? rotated.x : (comet_axis == 1) ? rotated.y : rotated.z;
    float axis_min = (comet_axis == 0) ? grid.min_x : (comet_axis == 1) ? grid.min_y : grid.min_z;
    float axis_max = (comet_axis == 0) ? grid.max_x : (comet_axis == 1) ? grid.max_y : grid.max_z;
    float span = std::max(axis_max - axis_min, 1e-5f);

    float progress = CalculateProgress(time);
    if(progress > 1.0f) progress = std::fmod(progress, 1.0f);
    if(progress < 0.0f) progress = std::fmod(progress, 1.0f) + 1.0f;
    float head = axis_min + progress * span;

    float tail_len = comet_size * span;
    float distance = head - axis_val;

    float intensity = 0.0f;
    if(distance >= 0.0f && distance <= tail_len)
    {
        intensity = 1.0f - (distance / tail_len);
        intensity = intensity * intensity;
    }
    else if(distance < 0.0f && distance > -tail_len * 0.2f)
    {
        intensity = 1.0f;
    }

    if(intensity <= 0.0f)
        return 0x00000000;

    float hue_offset = (1.0f - (distance / tail_len)) * 60.0f;
    RGBColor color = GetRainbowMode()
        ? GetRainbowColor(progress * 360.0f + hue_offset)
        : GetColorAtPosition(progress);
    unsigned char r = (color & 0xFF) * intensity;
    unsigned char g = ((color >> 8) & 0xFF) * intensity;
    unsigned char b = ((color >> 16) & 0xFF) * intensity;
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Comet3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["comet_axis"] = comet_axis;
    j["comet_size"] = comet_size;
    return j;
}

void Comet3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("comet_axis") && settings["comet_axis"].is_number_integer())
        comet_axis = std::clamp(settings["comet_axis"].get<int>(), 0, 2);
    if(settings.contains("comet_size") && settings["comet_size"].is_number())
        comet_size = std::clamp(settings["comet_size"].get<float>(), 0.05f, 1.0f);
}
