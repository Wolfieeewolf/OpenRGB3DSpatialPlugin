// SPDX-License-Identifier: GPL-2.0-only

#include "RotatingBeam3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>

REGISTER_EFFECT_3D(RotatingBeam3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

RotatingBeam3D::RotatingBeam3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
}

EffectInfo3D RotatingBeam3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Rotating Beam";
    info.effect_description = "A beam that rotates in a plane";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
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
    info.show_plane_control = true;
    return info;
}

void RotatingBeam3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Beam width:"), row, 0);
    QSlider* width_slider = new QSlider(Qt::Horizontal);
    width_slider->setRange(5, 50);
    width_slider->setValue((int)(beam_width * 100.0f));
    QLabel* width_label = new QLabel(QString::number((int)(beam_width * 100)) + "%");
    width_label->setMinimumWidth(36);
    layout->addWidget(width_slider, row, 1);
    layout->addWidget(width_label, row, 2);
    connect(width_slider, &QSlider::valueChanged, this, [this, width_label](int v){
        beam_width = v / 100.0f;
        if(width_label) width_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Glow:"), row, 0);
    QSlider* glow_slider = new QSlider(Qt::Horizontal);
    glow_slider->setRange(10, 100);
    glow_slider->setValue((int)(glow * 100.0f));
    QLabel* glow_label = new QLabel(QString::number((int)(glow * 100)) + "%");
    glow_label->setMinimumWidth(36);
    layout->addWidget(glow_slider, row, 1);
    layout->addWidget(glow_label, row, 2);
    connect(glow_slider, &QSlider::valueChanged, this, [this, glow_label](int v){
        glow = v / 100.0f;
        if(glow_label) glow_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void RotatingBeam3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor RotatingBeam3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor RotatingBeam3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    float beam_angle = progress * (float)(2.0 * M_PI);

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = rot.x - origin.x;
    float ly = rot.y - origin.y;
    float lz = rot.z - origin.z;

    int pl = GetPlane();
    float point_angle;
    if(pl == 0) point_angle = atan2f(lz, lx);
    else if(pl == 1) point_angle = atan2f(lx, ly);
    else point_angle = atan2f(lz, ly);

    float diff = fmodf(point_angle - beam_angle + (float)M_PI, (float)(2.0 * M_PI));
    if(diff < 0.0f) diff += (float)(2.0 * M_PI);
    diff -= (float)M_PI;
    float abs_diff = fabsf(diff);

    float width = std::max(0.05f, std::min(0.5f, beam_width)) * (float)M_PI;
    float glow_val = std::max(0.1f, std::min(1.0f, glow));
    float intensity;
    if(abs_diff <= width * 0.5f)
        intensity = 1.0f;
    else if(abs_diff <= width)
        intensity = 1.0f - (abs_diff - width * 0.5f) / (width * 0.5f);
    else
        intensity = powf(1.0f - fminf(1.0f, (abs_diff - width) / ((float)M_PI * glow_val)), 2.0f);

    if(intensity < 0.01f) return 0x00000000;
    float hue = fmodf(progress * 60.0f, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(progress);
    unsigned char r = (unsigned char)((c & 0xFF) * intensity);
    unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * intensity);
    unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json RotatingBeam3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["beam_width"] = beam_width;
    j["glow"] = glow;
    return j;
}

void RotatingBeam3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("beam_width") && settings["beam_width"].is_number())
        beam_width = std::max(0.05f, std::min(0.5f, settings["beam_width"].get<float>()));
    if(settings.contains("glow") && settings["glow"].is_number())
        glow = std::max(0.1f, std::min(1.0f, settings["glow"].get<float>()));
}
