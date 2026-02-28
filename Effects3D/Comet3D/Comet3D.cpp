// SPDX-License-Identifier: GPL-2.0-only

#include "Comet3D.h"
#include "../EffectHelpers.h"
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QWidget>
#include <cmath>
#include <algorithm>

REGISTER_EFFECT_3D(Comet3D);

const char* Comet3D::ModeName(int m)
{
    switch(m) { case MODE_COMET: return "Comet"; case MODE_CHASE: return "Chase (multi)"; case MODE_MARQUEE: return "Marquee"; default: return "Comet"; }
}

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
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.show_path_axis_control = true;
    return info;
}

void Comet3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Mode:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(comet_mode, MODE_COUNT - 1)));
    layout->addWidget(mode_combo, row, 1, 1, 2);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        comet_mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Tail size:"), row, 0);
    QSlider* size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(5, 80);
    size_slider->setValue((int)(comet_size * 100.0f));
    QLabel* size_label = new QLabel(QString::number((int)(comet_size * 100)) + "%");
    size_label->setMinimumWidth(36);
    layout->addWidget(size_slider, row, 1);
    layout->addWidget(size_label, row, 2);
    AddWidgetToParent(w, parent);

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

    int ax = GetPathAxis();
    float axis_val = (ax == 0) ? rotated.x : (ax == 1) ? rotated.y : rotated.z;
    float axis_min = (ax == 0) ? grid.min_x : (ax == 1) ? grid.min_y : grid.min_z;
    float axis_max = (ax == 0) ? grid.max_x : (ax == 1) ? grid.max_y : grid.max_z;
    float span = std::max(axis_max - axis_min, 1e-5f);

    float progress = CalculateProgress(time);
    if(progress > 1.0f) progress = std::fmod(progress, 1.0f);
    if(progress < 0.0f) progress = std::fmod(progress, 1.0f) + 1.0f;

    int mode = std::max(0, std::min(comet_mode, MODE_COUNT - 1));
    float tail_len = comet_size * span;
    float intensity = 0.0f;
    float hue_offset = 0.0f;

    if(mode == MODE_CHASE)
    {
        const int n_comets = 4;
        for(int c = 0; c < n_comets; c++)
        {
            float p = std::fmod(progress + (float)c / (float)n_comets, 1.0f);
            if(p < 0.0f) p += 1.0f;
            float head = axis_min + p * span;
            float distance = head - axis_val;
            if(distance >= 0.0f && distance <= tail_len)
            {
                float i = 1.0f - (distance / tail_len);
                i = i * i * 0.7f;
                if(i > intensity) { intensity = i; hue_offset = (1.0f - distance / tail_len) * 60.0f; }
            }
            else if(distance < 0.0f && distance > -tail_len * 0.2f)
            {
                if(1.0f > intensity) { intensity = 1.0f; hue_offset = 0.0f; }
            }
        }
    }
    else if(mode == MODE_MARQUEE)
    {
        float head = axis_min + progress * span;
        float distance = head - axis_val;
        float band = tail_len * 0.5f;
        if(distance >= 0.0f && distance <= band)
            intensity = 1.0f;
        else if(distance < 0.0f && distance > -band * 0.3f)
            intensity = 0.6f;
        hue_offset = 0.0f;
    }
    else
    {
        float head = axis_min + progress * span;
        float distance = head - axis_val;
        if(distance >= 0.0f && distance <= tail_len)
        {
            intensity = 1.0f - (distance / tail_len);
            intensity = intensity * intensity;
            hue_offset = (1.0f - (distance / tail_len)) * 60.0f;
        }
        else if(distance < 0.0f && distance > -tail_len * 0.2f)
            intensity = 1.0f;
    }

    if(intensity <= 0.0f)
        return 0x00000000;

    RGBColor color = GetRainbowMode()
        ? GetRainbowColor(progress * 360.0f + hue_offset)
        : GetColorAtPosition(progress);
    unsigned char r = (unsigned char)((color & 0xFF) * intensity);
    unsigned char g = (unsigned char)(((color >> 8) & 0xFF) * intensity);
    unsigned char b = (unsigned char)(((color >> 16) & 0xFF) * intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Comet3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["comet_mode"] = comet_mode;
    j["comet_size"] = comet_size;
    return j;
}

void Comet3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("comet_mode") && settings["comet_mode"].is_number_integer())
        comet_mode = std::clamp(settings["comet_mode"].get<int>(), 0, MODE_COUNT - 1);
    if(settings.contains("comet_size") && settings["comet_size"].is_number())
        comet_size = std::clamp(settings["comet_size"].get<float>(), 0.05f, 1.0f);
}
