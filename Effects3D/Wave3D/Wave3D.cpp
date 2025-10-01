/*---------------------------------------------------------*\
| Wave3D.cpp                                                |
|                                                           |
|   3D Wave effect with custom UI controls                 |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Wave3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(Wave3D);
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Wave3D::Wave3D(QWidget* parent) : SpatialEffect3D(parent)
{
    shape_combo = nullptr;
    shape_type = 0;      // Circles
    progress = 0.0f;

    // Initialize with default colors using base class method
    SetFrequency(50);    // Default frequency like RadialRainbow slider2
    SetRainbowMode(true); // Default to rainbow like RadialRainbow

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);  // Blue
    default_colors.push_back(0x0000FF00);  // Green
    default_colors.push_back(0x00FF0000);  // Red
    SetColors(default_colors);
}

Wave3D::~Wave3D()
{
}

EffectInfo3D Wave3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Wave";
    info.effect_description = "3D wave effect with configurable direction and properties";
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
    info.needs_frequency = true;

    return info;
}

void Wave3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Create Wave-specific controls (base has axis/speed/etc) |
    \*---------------------------------------------------------*/
    QWidget* wave_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(wave_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    /*---------------------------------------------------------*\
    | Row 0: Shape (like RadialRainbow)                       |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Shape:"), 0, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Circles");
    shape_combo->addItem("Squares");
    shape_combo->setCurrentIndex(shape_type);
    layout->addWidget(shape_combo, 0, 1);

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(wave_widget);
    }

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave3D::OnWaveParameterChanged);
}

void Wave3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void Wave3D::OnWaveParameterChanged()
{
    /*---------------------------------------------------------*\
    | Update internal parameters                               |
    \*---------------------------------------------------------*/
    if(shape_combo) shape_type = shape_combo->currentIndex();

    /*---------------------------------------------------------*\
    | Emit parameter change signal                             |
    \*---------------------------------------------------------*/
    emit ParametersChanged();
}

RGBColor Wave3D::CalculateColor(float x, float y, float z, float time)
{
    // Debug: Log that we're being called
    static int call_count = 0;
    if(call_count < 5)
    {
        call_count++;
        // Will log to OpenRGB console
    }

    /*---------------------------------------------------------*\
    | Create smooth curves for speed and frequency            |
    \*---------------------------------------------------------*/
    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve; // Quadratic curve for smoother control
    float actual_speed = speed_curve * 200.0f; // Map back to 0-200 range

    float freq_curve = (effect_frequency / 100.0f);
    freq_curve = freq_curve * freq_curve; // Quadratic curve for smoother control
    float actual_frequency = freq_curve * 10.0f; // Map to 0-10 range

    /*---------------------------------------------------------*\
    | Update progress for animation (like RadialRainbow)      |
    \*---------------------------------------------------------*/
    progress = time * (actual_speed * 2.0f);

    /*---------------------------------------------------------*\
    | Calculate wave based on direction type (like LED cube) |
    \*---------------------------------------------------------*/
    float wave_value = 0.0f;
    float freq_scale = actual_frequency * 0.1f;

    switch(effect_axis)
    {
        case AXIS_X: // X Axis Wave (left-to-right with Y/Z amplitude)
            wave_value = sin(x * freq_scale + (effect_reverse ? -progress : progress)) *
                        (1.0f + 0.3f * sin(y * 0.5f) + 0.2f * sin(z * 0.3f));
            break;
        case AXIS_Y: // Y Axis Wave (up-down with X/Z amplitude)
            wave_value = sin(y * freq_scale + (effect_reverse ? -progress : progress)) *
                        (1.0f + 0.3f * sin(x * 0.5f) + 0.2f * sin(z * 0.3f));
            break;
        case AXIS_Z: // Z Axis Wave (front-back with X/Y amplitude)
            wave_value = sin(z * freq_scale + (effect_reverse ? -progress : progress)) *
                        (1.0f + 0.3f * sin(x * 0.5f) + 0.2f * sin(y * 0.3f));
            break;
        case AXIS_RADIAL: // Radial (3D Sphere)
        default:
            if(shape_type == 0) // Circles
            {
                float distance = sqrt(x*x + y*y + z*z);
                wave_value = sin(distance * freq_scale + (effect_reverse ? progress : -progress));
            }
            else // Squares (cube distance)
            {
                float distance = std::max({fabs(x), fabs(y), fabs(z)});
                wave_value = sin(distance * freq_scale + (effect_reverse ? progress : -progress));
            }
            break;
        case AXIS_CUSTOM: // Custom direction vector
        {
            // Project position onto custom direction vector
            float dot_product = x * custom_direction.x + y * custom_direction.y + z * custom_direction.z;
            wave_value = sin(dot_product * freq_scale + (effect_reverse ? -progress : progress)) *
                        (1.0f + 0.3f * sin((x + y + z) * 0.3f)); // Add some 3D complexity
            break;
        }
    }

    /*---------------------------------------------------------*\
    | Convert wave to hue (0-360 degrees)                     |
    \*---------------------------------------------------------*/
    float hue = (wave_value + 1.0f) * 180.0f;
    hue = fmod(hue, 360.0f);
    if(hue < 0.0f) hue += 360.0f;

    /*---------------------------------------------------------*\
    | Get color based on mode                                  |
    \*---------------------------------------------------------*/
    RGBColor final_color;

    if(GetRainbowMode())
    {
        final_color = GetRainbowColor(hue);
    }
    else
    {
        // Use custom colors with position-based selection
        float position = hue / 360.0f;
        final_color = GetColorAtPosition(position);
    }

    /*---------------------------------------------------------*\
    | Apply brightness                                         |
    \*---------------------------------------------------------*/
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = effect_brightness / 100.0f;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}

