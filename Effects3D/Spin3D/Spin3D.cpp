// SPDX-License-Identifier: GPL-2.0-only

#include "Spin3D.h"
#include <QGridLayout>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(Spin3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Spin3D::Spin3D(QWidget* parent) : SpatialEffect3D(parent)
{
    arms_slider = nullptr;
    arms_label = nullptr;
    num_arms = 3;
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

Spin3D::~Spin3D()
{
}

EffectInfo3D Spin3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Spin";
    info.effect_description = "Rotating pattern with configurable arms";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WAVE;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = false;

    info.default_speed_scale = 25.0f;
    info.default_frequency_scale = 6.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_color_controls = true;

    return info;
}

void Spin3D::SetupCustomUI(QWidget* parent)
{
    QWidget* spin_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(spin_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Arms:"), 0, 0);
    arms_slider = new QSlider(Qt::Horizontal);
    arms_slider->setRange(1, 8);
    arms_slider->setValue(num_arms);
    arms_slider->setToolTip("Number of spinning arms radiating from origin");
    layout->addWidget(arms_slider, 0, 1);
    arms_label = new QLabel(QString::number(num_arms));
    arms_label->setMinimumWidth(30);
    layout->addWidget(arms_label, 0, 2);

    AddWidgetToParent(spin_widget, parent);

    connect(arms_slider, &QSlider::valueChanged, this, &Spin3D::OnSpinParameterChanged);
}

void Spin3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void Spin3D::OnSpinParameterChanged()
{
    if(arms_slider)
    {
        num_arms = arms_slider->value();
        if(arms_label) arms_label->setText(QString::number(num_arms));
    }
    emit ParametersChanged();
}

RGBColor Spin3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

float Spin3D::Clamp01(float value)
{
    if(value < 0.0f)
    {
        return 0.0f;
    }
    if(value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

RGBColor Spin3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    progress = CalculateProgress(time);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    float angle = atan2(rot_rel_z, rot_rel_x);
    float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    float max_radius = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
    float radial_fade = 1.0f;
    if(max_radius > 0.001f)
    {
        float normalized_dist = fmin(1.0f, radial_distance / max_radius);
        radial_fade = 0.35f + 0.65f * (1.0f - normalized_dist * 0.6f);
    }
    
    unsigned int arms = (num_arms == 0U) ? 1U : num_arms;
    float spin_angle = angle * (float)arms - progress;
    float period = 6.28318f / (float)arms;
    float arm_position = fmod(spin_angle, period);
    if(arm_position < 0.0f)
    {
        arm_position += period;
    }
    
    float blade_width = 0.4f * period;
    float blade_core = (arm_position < blade_width) ? (1.0f - (arm_position / blade_width)) : 0.0f;
    float blade_glow = 0.0f;
    if(arm_position < blade_width * 1.5f)
    {
        float glow_dist = fabs(arm_position - blade_width * 0.5f) / (blade_width * 0.5f);
        blade_glow = 0.3f * (1.0f - glow_dist);
    }
    float blade = fmin(1.0f, blade_core + blade_glow);
    
    float ambient = 0.08f * radial_fade;
    
    float intensity = blade * radial_fade + ambient;
    intensity = fmax(0.0f, fmin(1.0f, intensity));

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(progress * 57.2958f + intensity * 120.0f)
                                            : GetColorAtPosition(intensity);
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * intensity);
    g = (unsigned char)(g * intensity);
    b = (unsigned char)(b * intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Spin3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["num_arms"] = num_arms;
    return j;
}

void Spin3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("num_arms") && settings["num_arms"].is_number_integer())
        num_arms = std::max(1, std::min(8, settings["num_arms"].get<int>()));

    if(arms_slider) arms_slider->setValue(num_arms);
}
