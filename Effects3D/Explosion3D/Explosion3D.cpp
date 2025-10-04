/*---------------------------------------------------------*\
| Explosion3D.cpp                                           |
|                                                           |
|   3D Explosion effect with shockwave animation           |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Explosion3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(Explosion3D);

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include "../EffectHelpers.h"

Explosion3D::Explosion3D(QWidget* parent) : SpatialEffect3D(parent)
{
    intensity_slider = nullptr;
    explosion_intensity = 75;
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);  // Red (0x00BBGGRR format)
    default_colors.push_back(0x0000FFFF);  // Yellow
    default_colors.push_back(0x00FF0000);  // Blue
    SetColors(default_colors);
}

Explosion3D::~Explosion3D()
{
}

EffectInfo3D Explosion3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;  // Using new standardized system
    info.effect_name = "3D Explosion";
    info.effect_description = "Expanding shockwave explosion with multiple wave layers";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_EXPLOSION;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    // Standardized parameter scaling
    info.default_speed_scale = 20.0f;       // (speed/100)² * 200 * 0.1
    info.default_frequency_scale = 100.0f;  // (freq/100)² * 100
    info.use_size_parameter = true;

    // Control visibility (show all controls)
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;
    info.show_color_controls = true;

    return info;
}

void Explosion3D::SetupCustomUI(QWidget* parent)
{
    QWidget* explosion_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(explosion_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Intensity:"), 0, 0);
    intensity_slider = new QSlider(Qt::Horizontal);
    intensity_slider->setRange(10, 200);
    intensity_slider->setValue(explosion_intensity);
    layout->addWidget(intensity_slider, 0, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(explosion_widget);
    }

    connect(intensity_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
}

void Explosion3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_EXPLOSION;
}

void Explosion3D::OnExplosionParameterChanged()
{
    if(intensity_slider) explosion_intensity = intensity_slider->value();
    emit ParametersChanged();
}

RGBColor Explosion3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | Get effect origin (room center or user head position)   |
    \*---------------------------------------------------------*/
    Vector3D origin = GetEffectOrigin();

    /*---------------------------------------------------------*\
    | Calculate position relative to origin                    |
    \*---------------------------------------------------------*/
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    /*---------------------------------------------------------*\
    | Check if LED is within scaled effect radius             |
    \*---------------------------------------------------------*/
    float scale_radius = GetNormalizedScale() * 10.0f;
    float distance_from_origin = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    if(distance_from_origin > scale_radius)
    {
        return 0x00000000;  // Black - outside effect boundary
    }

    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve;
    float actual_speed = speed_curve * 200.0f;

    float freq_curve = (GetFrequency() / 100.0f);
    freq_curve = freq_curve * freq_curve;
    float actual_frequency = freq_curve * 100.0f;

    progress = time * (actual_speed * 0.1f);
    if(effect_reverse) progress = -progress;

    float size_multiplier = GetNormalizedSize();  // 0.1 to 2.0
    float freq_scale = actual_frequency * 0.01f / size_multiplier;

    /*---------------------------------------------------------*\
    | Calculate distance based on axis (spherical vs directional)|
    \*---------------------------------------------------------*/
    float distance;
    switch(effect_axis)
    {
        case AXIS_X:  // Directional explosion along X-axis (Left to Right)
            distance = fabs(rel_x) + sqrt(rel_y*rel_y + rel_z*rel_z) * 0.3f;
            break;
        case AXIS_Y:  // Directional explosion along Y-axis (Floor to Ceiling)
            distance = fabs(rel_y) + sqrt(rel_x*rel_x + rel_z*rel_z) * 0.3f;
            break;
        case AXIS_Z:  // Directional explosion along Z-axis (Front to Back)
            distance = fabs(rel_z) + sqrt(rel_x*rel_x + rel_y*rel_y) * 0.3f;
            break;
        case AXIS_RADIAL:  // Spherical explosion
        default:
            distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            break;
    }

    float explosion_radius = progress * (explosion_intensity * 0.1f) * size_multiplier;
    float wave_thickness = (3.0f + explosion_intensity * 0.05f) * size_multiplier;

    float primary_wave = 1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, distance);
    primary_wave *= exp(-fabs(distance - explosion_radius) * 0.1f);

    float secondary_radius = explosion_radius * 0.7f;
    float secondary_wave = 1.0f - smoothstep(secondary_radius - wave_thickness * 0.5f, secondary_radius + wave_thickness * 0.5f, distance);
    secondary_wave *= exp(-fabs(distance - secondary_radius) * 0.15f) * 0.6f;

    float shock_detail = 0.2f * sin(distance * freq_scale * 8.0f - progress * 4.0f);
    shock_detail *= exp(-distance * 0.1f);

    float explosion_intensity_final = primary_wave + secondary_wave + shock_detail;
    explosion_intensity_final = fmax(0.0f, fmin(1.0f, explosion_intensity_final));

    if(distance < explosion_radius * 0.3f)
    {
        float core_intensity = 1.0f - (distance / (explosion_radius * 0.3f));
        explosion_intensity_final = fmax(explosion_intensity_final, core_intensity * 0.8f);
    }

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = 60.0f - (explosion_intensity_final * 60.0f) + progress * 10.0f;
        hue = fmax(0.0f, hue);
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(explosion_intensity_final);
    }

    // Apply intensity and brightness
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f) * explosion_intensity_final;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}