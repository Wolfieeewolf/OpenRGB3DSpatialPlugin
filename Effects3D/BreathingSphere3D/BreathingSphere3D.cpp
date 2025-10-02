/*---------------------------------------------------------*\
| BreathingSphere3D.cpp                                     |
|                                                           |
|   3D Breathing Sphere effect - pulsing sphere from origin |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "BreathingSphere3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(BreathingSphere3D);
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float smoothstep(float edge0, float edge1, float x)
{
    float t = fmax(0.0f, fmin(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

BreathingSphere3D::BreathingSphere3D(QWidget* parent) : SpatialEffect3D(parent)
{
    size_slider = nullptr;
    sphere_size = 50;
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);  // Red (0x00BBGGRR format)
    default_colors.push_back(0x0000FF00);  // Green
    default_colors.push_back(0x00FF0000);  // Blue
    SetColors(default_colors);
}

BreathingSphere3D::~BreathingSphere3D()
{
}

EffectInfo3D BreathingSphere3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;  // Using new standardized system
    info.effect_name = "3D Breathing Sphere";
    info.effect_description = "Pulsing sphere effect with rainbow and custom colors";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_BREATHING_SPHERE;
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

void BreathingSphere3D::SetupCustomUI(QWidget* parent)
{
    QWidget* breathing_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(breathing_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Size:"), 0, 0);
    size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(10, 200);
    size_slider->setValue(sphere_size);
    layout->addWidget(size_slider, 0, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(breathing_widget);
    }

    connect(size_slider, &QSlider::valueChanged, this, &BreathingSphere3D::OnBreathingParameterChanged);
}

void BreathingSphere3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_BREATHING_SPHERE;
}

void BreathingSphere3D::OnBreathingParameterChanged()
{
    if(size_slider) sphere_size = size_slider->value();
    emit ParametersChanged();
}

RGBColor BreathingSphere3D::CalculateColor(float x, float y, float z, float time)
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
    | Calculate distance based on axis (sphere vs ellipsoid)  |
    \*---------------------------------------------------------*/
    float distance;
    switch(effect_axis)
    {
        case AXIS_X:  // Ellipsoid stretched along X-axis (Left to Right)
            distance = sqrt((rel_x * 0.5f)*(rel_x * 0.5f) + rel_y*rel_y + rel_z*rel_z);
            break;
        case AXIS_Y:  // Ellipsoid stretched along Y-axis (Floor to Ceiling)
            distance = sqrt(rel_x*rel_x + (rel_y * 0.5f)*(rel_y * 0.5f) + rel_z*rel_z);
            break;
        case AXIS_Z:  // Ellipsoid stretched along Z-axis (Front to Back)
            distance = sqrt(rel_x*rel_x + rel_y*rel_y + (rel_z * 0.5f)*(rel_z * 0.5f));
            break;
        case AXIS_RADIAL:  // Perfect sphere
        default:
            distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            break;
    }

    float sphere_radius = (sphere_size * 0.1f) * size_multiplier * (1.0f + 0.5f * sin(progress * freq_scale));

    float sphere_intensity = 1.0f - smoothstep(0.0f, sphere_radius, distance);

    float pulse_wave = 0.3f * sin(distance * freq_scale * 2.0f - progress * 2.0f);
    sphere_intensity += pulse_wave;

    sphere_intensity = fmax(0.0f, fmin(1.0f, sphere_intensity));

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = distance * 50.0f + progress * 30.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        float color_position = (distance * 0.1f + progress * 0.05f);
        final_color = GetColorAtPosition(color_position);
    }

    // Apply intensity and brightness
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f) * sphere_intensity;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}