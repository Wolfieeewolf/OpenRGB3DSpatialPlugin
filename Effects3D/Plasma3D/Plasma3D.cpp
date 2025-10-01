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
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <cmath>

Plasma3D::Plasma3D(QWidget* parent) : SpatialEffect3D(parent)
{
    pattern_combo = nullptr;
    pattern_type = 0;        // Default to Classic plasma
    progress = 0.0f;

    // Set default plasma colors (electric blue to purple)
    std::vector<RGBColor> plasma_colors = {
        0x0000FF00,  // Electric Blue
        0x00FF00FF,  // Purple/Magenta
        0x00FFFF00   // Cyan
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
    // Use universal base class values
    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve;
    float actual_speed = speed_curve * 4.0f;

    float freq_curve = (GetFrequency() / 100.0f);
    freq_curve = freq_curve * freq_curve;
    float actual_frequency = freq_curve * 20.0f;

    // Update progress for animation
    progress = time * actual_speed;

    // Base plasma calculation with different patterns
    float plasma_value;

    switch(pattern_type)
    {
        case 0: // Classic
            plasma_value = sin(x * 0.1f + progress) +
                          sin(y * 0.1f + progress * 1.3f) +
                          sin(z * 0.1f + progress * 0.7f) +
                          sin(sqrt(x*x + y*y + z*z) * 0.05f + progress * 2.0f);
            break;

        case 1: // Swirl
            {
                float angle = atan2(y, x);
                float radius = sqrt(x*x + y*y);
                plasma_value = sin(angle * 3.0f + radius * 0.1f + progress) +
                              sin(z * 0.1f + progress * 1.5f) +
                              sin((radius + z) * 0.08f + progress * 0.8f);
            }
            break;

        case 2: // Ripple
            {
                float dist_from_center = sqrt(x*x + y*y + z*z);
                plasma_value = sin(dist_from_center * 0.2f - progress * 3.0f) +
                              sin(x * 0.15f + y * 0.1f + progress) +
                              sin(z * 0.12f + progress * 1.2f);
            }
            break;

        case 3: // Organic
        default:
            plasma_value = sin(x * 0.08f + sin(y * 0.12f + progress) + progress * 0.5f) +
                          cos(y * 0.09f + cos(z * 0.11f + progress * 1.3f)) +
                          sin(z * 0.07f + sin(x * 0.13f + progress * 0.7f));
            break;
    }

    // Apply frequency scaling
    plasma_value *= actual_frequency * 0.1f;

    // Normalize to 0-1 range
    plasma_value = (plasma_value + 4.0f) / 8.0f;
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

RGBColor Plasma3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)grid;
    return CalculateColor(x, y, z, time);
}