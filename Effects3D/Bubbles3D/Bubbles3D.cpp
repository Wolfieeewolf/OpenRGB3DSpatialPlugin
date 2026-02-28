// SPDX-License-Identifier: GPL-2.0-only

#include "Bubbles3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

REGISTER_EFFECT_3D(Bubbles3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float hash_f(unsigned int seed, unsigned int salt)
{
    unsigned int v = seed * 73856093u ^ salt * 19349663u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
}

Bubbles3D::Bubbles3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetFrequency(50);
}

EffectInfo3D Bubbles3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "3D Bubbles";
    info.effect_description = "Rising expanding spheres (like OpenRGB Bubbles) – bubbles spawn from floor and rise";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void Bubbles3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Max bubbles:"), row, 0);
    QSlider* max_slider = new QSlider(Qt::Horizontal);
    max_slider->setRange(4, 30);
    max_slider->setValue(max_bubbles);
    QLabel* max_label = new QLabel(QString::number(max_bubbles));
    max_label->setMinimumWidth(36);
    layout->addWidget(max_slider, row, 1);
    layout->addWidget(max_label, row, 2);
    connect(max_slider, &QSlider::valueChanged, this, [this, max_label](int v){
        max_bubbles = v;
        if(max_label) max_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Ring thickness:"), row, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 100);
    thick_slider->setValue((int)(bubble_thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(bubble_thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, row, 1);
    layout->addWidget(thick_label, row, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        bubble_thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Rise speed:"), row, 0);
    QSlider* rise_slider = new QSlider(Qt::Horizontal);
    rise_slider->setRange(20, 200);
    rise_slider->setValue((int)(rise_speed * 100.0f));
    QLabel* rise_label = new QLabel(QString::number(rise_speed, 'f', 2));
    rise_label->setMinimumWidth(36);
    layout->addWidget(rise_slider, row, 1);
    layout->addWidget(rise_label, row, 2);
    connect(rise_slider, &QSlider::valueChanged, this, [this, rise_label](int v){
        rise_speed = v / 100.0f;
        if(rise_label) rise_label->setText(QString::number(rise_speed, 'f', 2));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Spawn rate:"), row, 0);
    QSlider* spawn_slider = new QSlider(Qt::Horizontal);
    spawn_slider->setRange(30, 200);
    spawn_slider->setValue((int)(spawn_interval * 100.0f));
    QLabel* spawn_label = new QLabel(QString::number(spawn_interval, 'f', 2));
    spawn_label->setMinimumWidth(36);
    layout->addWidget(spawn_slider, row, 1);
    layout->addWidget(spawn_label, row, 2);
    connect(spawn_slider, &QSlider::valueChanged, this, [this, spawn_label](int v){
        spawn_interval = v / 100.0f;
        if(spawn_label) spawn_label->setText(QString::number(spawn_interval, 'f', 2));
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void Bubbles3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor Bubbles3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor Bubbles3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float half = 0.5f * std::max(grid.width, std::max(grid.height, grid.depth)) * GetNormalizedScale();
    if(half < 1e-5f) half = 1.0f;
    float speed_scale = GetScaledSpeed() * 0.015f;
    int n_bub = std::max(4, std::min(30, max_bubbles));
    float thick = std::max(0.02f, std::min(4.0f, bubble_thickness * half));
    float rise = std::max(0.2f, std::min(2.0f, rise_speed)) * speed_scale * half;
    float interval = std::max(0.3f, std::min(2.0f, spawn_interval));
    float max_r = std::max(0.5f, std::min(2.0f, max_radius)) * half;

    float max_intensity = 0.0f;
    float best_hue = 0.0f;

    for(int i = 0; i < n_bub; i++)
    {
        float phase = fmodf(time + (float)i * interval, interval * (float)n_bub);
        float radius = (phase / interval) * max_r * 0.4f;
        float cx = origin.x + hash_f((unsigned int)(i * 1000), 1u) * half * 0.6f;
        float cy = origin.y - half * 0.5f + fmodf(time * rise * 0.5f + (float)i * 0.3f, half * 2.0f) - half;
        float cz = origin.z + hash_f((unsigned int)(i * 1000), 2u) * half * 0.6f;

        float dx = x - cx;
        float dy = y - cy;
        float dz = z - cz;
        float dist_sq = dx*dx + dy*dy + dz*dz;
        float far = radius + thick * 4.0f;
        if(dist_sq > far * far) continue;
        float dist = sqrtf(dist_sq);
        float shallow = fabsf(dist - radius) / thick;
        float value = (shallow < 0.01f) ? 1.0f : 1.0f / (1.0f + shallow * shallow);
        value = fmaxf(0.0f, fminf(1.0f, value));

        if(value > max_intensity)
        {
            max_intensity = value;
            best_hue = fmodf((float)i * 40.0f + time * 30.0f, 360.0f);
            if(best_hue < 0.0f) best_hue += 360.0f;
        }
    }

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(best_hue) : GetColorAtPosition(0.5f);
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Bubbles3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["max_bubbles"] = max_bubbles;
    j["bubble_thickness"] = bubble_thickness;
    j["rise_speed"] = rise_speed;
    j["spawn_interval"] = spawn_interval;
    j["max_radius"] = max_radius;
    return j;
}

void Bubbles3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("max_bubbles") && settings["max_bubbles"].is_number_integer())
        max_bubbles = std::max(4, std::min(30, settings["max_bubbles"].get<int>()));
    if(settings.contains("bubble_thickness") && settings["bubble_thickness"].is_number())
        bubble_thickness = std::max(0.02f, std::min(1.0f, settings["bubble_thickness"].get<float>()));
    if(settings.contains("rise_speed") && settings["rise_speed"].is_number())
        rise_speed = std::max(0.2f, std::min(2.0f, settings["rise_speed"].get<float>()));
    if(settings.contains("spawn_interval") && settings["spawn_interval"].is_number())
        spawn_interval = std::max(0.3f, std::min(2.0f, settings["spawn_interval"].get<float>()));
    if(settings.contains("max_radius") && settings["max_radius"].is_number())
        max_radius = std::max(0.5f, std::min(2.0f, settings["max_radius"].get<float>()));
}
