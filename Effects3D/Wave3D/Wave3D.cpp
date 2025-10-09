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
    default_colors.push_back(0x000000FF);  // Red (0x00BBGGRR format)
    default_colors.push_back(0x0000FF00);  // Green
    default_colors.push_back(0x00FF0000);  // Blue
    SetColors(default_colors);
}

Wave3D::~Wave3D()
{
}

EffectInfo3D Wave3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;  // Using new standardized system
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

    // Standardized parameter scaling
    info.default_speed_scale = 400.0f;      // Normalized speed (0.0-1.0) * 400.0
    info.default_frequency_scale = 10.0f;   // Normalized frequency (0.0-1.0) * 10.0
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
    | Uses standardized boundary helper                        |
    \*---------------------------------------------------------*/
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z))
    {
        return 0x00000000;  // Black - outside effect boundary
    }

    /*---------------------------------------------------------*\
    | Use standardized parameter helpers                       |
    | These apply the correct curves and scaling automatically |
    \*---------------------------------------------------------*/
    float actual_frequency = GetScaledFrequency();

    /*---------------------------------------------------------*\
    | Update progress for animation using universal helper    |
    \*---------------------------------------------------------*/
    progress = CalculateProgress(time);

    /*---------------------------------------------------------*\
    | Calculate wave based on axis and shape type             |
    \*---------------------------------------------------------*/
    float wave_value = 0.0f;
    float size_multiplier = GetNormalizedSize();  // 0.1 to 2.0 (affects spatial frequency)
    float freq_scale = actual_frequency * 0.1f / size_multiplier;  // Larger size = more spread out
    float position = 0.0f;

    /*---------------------------------------------------------*\
    | Calculate position based on selected axis               |
    \*---------------------------------------------------------*/
    switch(effect_axis)
    {
        case AXIS_X:  // Left to Right
            position = rel_x;
            break;
        case AXIS_Y:  // Floor to Ceiling
            position = rel_y;
            break;
        case AXIS_Z:  // Front to Back
            position = rel_z;
            break;
        case AXIS_RADIAL:  // Radial from center
        default:
            if(shape_type == 0)  // Sphere
            {
                position = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            }
            else  // Cube
            {
                position = std::max({fabs(rel_x), fabs(rel_y), fabs(rel_z)});
            }
            break;
    }

    /*---------------------------------------------------------*\
    | Apply reverse if enabled                                 |
    \*---------------------------------------------------------*/
    if(effect_reverse)
    {
        position = -position;
    }

    wave_value = sin(position * freq_scale - progress);

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

