/*---------------------------------------------------------*\
| Plasma3D.cpp                                              |
|                                                           |
|   3D Plasma effect with enhanced controls                |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Plasma3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(Plasma3D);
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <cmath>

Plasma3D::Plasma3D(QWidget* parent) : SpatialEffect3D(parent)
{
    pattern_combo = nullptr;
    pattern_type = 0;        // Default to Classic plasma
    progress = 0.0f;

    // Set default plasma colors
    std::vector<RGBColor> plasma_colors = {
        0x0000FF00,  // Green (0x00BBGGRR format)
        0x00FF00FF,  // Magenta/Purple
        0x00FFFF00   // Yellow
    };
    // Only set default colors if none are set yet
    if(GetColors().empty())
    {
        SetColors(plasma_colors);
    }
    SetFrequency(60);        // Default plasma frequency
    SetRainbowMode(false);   // Default to custom colors
}

Plasma3D::~Plasma3D()
{
}

EffectInfo3D Plasma3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Plasma";
    info.effect_description = "Animated plasma effect with configurable patterns and complexity";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_PLASMA;
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
    return info;
}

void Plasma3D::SetupCustomUI(QWidget* parent)
{
    QWidget* plasma_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(plasma_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    // Only Plasma-specific control: Pattern Type
    layout->addWidget(new QLabel("Pattern:"), 0, 0);
    pattern_combo = new QComboBox();
    pattern_combo->addItem("Classic");
    pattern_combo->addItem("Swirl");
    pattern_combo->addItem("Ripple");
    pattern_combo->addItem("Organic");
    pattern_combo->setCurrentIndex(pattern_type);
    layout->addWidget(pattern_combo, 0, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(plasma_widget);
    }

    // Connect signals (only for Plasma-specific controls)
    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Plasma3D::OnPlasmaParameterChanged);
}

void Plasma3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_PLASMA;
}

void Plasma3D::OnPlasmaParameterChanged()
{
    if(pattern_combo) pattern_type = pattern_combo->currentIndex();
    emit ParametersChanged();
}

RGBColor Plasma3D::CalculateColor(float x, float y, float z, float time)
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

    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve;
    float actual_speed = speed_curve * 4.0f;

    float freq_curve = (GetFrequency() / 100.0f);
    freq_curve = freq_curve * freq_curve;
    float actual_frequency = freq_curve * 20.0f;

    progress = time * actual_speed;

    /*---------------------------------------------------------*\
    | Calculate primary and secondary coordinates based on axis|
    \*---------------------------------------------------------*/
    float coord1, coord2, coord3;
    switch(effect_axis)
    {
        case AXIS_X:  // Pattern primarily on YZ plane (Left/Right walls - perpendicular to X)
            coord1 = rel_y;
            coord2 = rel_z;
            coord3 = rel_x;
            break;
        case AXIS_Y:  // Pattern primarily on XZ plane (Floor/Ceiling - perpendicular to Y)
            coord1 = rel_x;
            coord2 = rel_z;
            coord3 = rel_y;
            break;
        case AXIS_Z:  // Pattern primarily on XY plane (Front/Back walls - perpendicular to Z)
            coord1 = rel_x;
            coord2 = rel_y;
            coord3 = rel_z;
            break;
        case AXIS_RADIAL:  // Full 3D pattern
        default:
            coord1 = rel_x;
            coord2 = rel_y;
            coord3 = rel_z;
            break;
    }

    /*---------------------------------------------------------*\
    | Apply reverse if enabled                                 |
    \*---------------------------------------------------------*/
    if(effect_reverse)
    {
        coord3 = -coord3;
    }

    float plasma_value;
    float scale = actual_frequency * 0.015f;

    switch(pattern_type)
    {
        case 0: // Classic Plasma - Multiple overlapping sine waves
            {
                // Create classic plasma with 4-6 overlapping waves
                plasma_value =
                    sin((coord1 + progress * 2.0f) * scale) +
                    sin((coord2 + progress * 1.7f) * scale * 0.8f) +
                    sin((coord1 + coord2 + progress * 1.3f) * scale * 0.6f) +
                    cos((coord1 - coord2 + progress * 2.2f) * scale * 0.7f) +
                    sin(sqrt(coord1*coord1 + coord2*coord2) * scale * 0.5f + progress * 1.5f) +
                    cos(coord3 * scale * 0.4f + progress * 0.9f);
            }
            break;

        case 1: // Swirl Plasma - Rotating spiral patterns
            {
                float angle = atan2(coord2, coord1);
                float radius = sqrt(coord1*coord1 + coord2*coord2);

                plasma_value =
                    sin(angle * 4.0f + radius * scale * 0.8f + progress * 2.0f) +
                    sin(angle * 3.0f - radius * scale * 0.6f + progress * 1.5f) +
                    cos(angle * 5.0f + radius * scale * 0.4f - progress * 1.8f) +
                    sin(coord3 * scale * 0.5f + progress) +
                    cos((angle * 2.0f + coord3 * scale * 0.3f) + progress * 1.2f);
            }
            break;

        case 2: // Ripple Plasma - Concentric waves
            {
                float dist_from_center;
                if(effect_axis == AXIS_RADIAL)
                {
                    dist_from_center = sqrt(coord1*coord1 + coord2*coord2 + coord3*coord3);
                }
                else
                {
                    dist_from_center = sqrt(coord1*coord1 + coord2*coord2);
                }

                plasma_value =
                    sin(dist_from_center * scale - progress * 3.0f) +
                    sin(dist_from_center * scale * 1.5f - progress * 2.3f) +
                    cos(dist_from_center * scale * 0.8f + progress * 1.8f) +
                    sin((coord1 + coord2) * scale * 0.6f + progress * 1.2f) +
                    cos(coord3 * scale * 0.5f - progress * 0.7f);
            }
            break;

        case 3: // Organic Plasma - Flowing liquid-like patterns
        default:
            {
                // Nested sine waves for organic flowing effect
                float flow1 = sin(coord1 * scale * 0.8f + sin(coord2 * scale * 1.2f + progress) + progress * 0.5f);
                float flow2 = cos(coord2 * scale * 0.9f + cos(coord3 * scale * 1.1f + progress * 1.3f));
                float flow3 = sin(coord3 * scale * 0.7f + sin(coord1 * scale * 1.3f + progress * 0.7f));
                float flow4 = cos((coord1 + coord2) * scale * 0.6f + sin(progress * 1.5f));
                float flow5 = sin((coord2 + coord3) * scale * 0.5f + cos(progress * 1.8f));

                plasma_value = flow1 + flow2 + flow3 + flow4 + flow5;
            }
            break;
    }

    // Normalize to 0-1 range
    // With 5-6 overlapping waves, range is approximately -6 to +6
    plasma_value = (plasma_value + 6.0f) / 12.0f;
    plasma_value = fmax(0.0f, fmin(1.0f, plasma_value));

    // Get color using universal base class methods
    RGBColor final_color;
    if(GetRainbowMode())
    {
        // Rainbow colors that shift with plasma value
        float hue = plasma_value * 360.0f + progress * 60.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        // Use different colors based on plasma value
        final_color = GetColorAtPosition(plasma_value);
    }

    // Apply brightness
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f);
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}

