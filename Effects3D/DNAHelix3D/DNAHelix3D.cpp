/*---------------------------------------------------------*\
| DNAHelix3D.cpp                                            |
|                                                           |
|   3D DNA Helix effect with enhanced controls             |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "DNAHelix3D.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <cmath>

// Helper function for smooth interpolation
static float smoothstep(float edge0, float edge1, float x)
{
    float t = fmax(0.0f, fmin(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

DNAHelix3D::DNAHelix3D(QWidget* parent) : SpatialEffect3D(parent)
{
    radius_slider = nullptr;
    helix_radius = 75;       // Default helix radius
    progress = 0.0f;

    // Set up default DNA base pair colors, but allow user override via universal controls
    std::vector<RGBColor> dna_colors = {
        0x000000FF,  // Blue (Adenine)
        0x0000FFFF,  // Yellow (Thymine)
        0x0000FF00,  // Green (Guanine)
        0x00FF0000   // Red (Cytosine)
    };
    // Only set default colors if none are set yet
    if(GetColors().empty())
    {
        SetColors(dna_colors);
    }
    SetFrequency(50);        // Default DNA pitch frequency
    SetRainbowMode(false);   // Default to custom colors so users can see the DNA colors
}

DNAHelix3D::~DNAHelix3D()
{
}

EffectInfo3D DNAHelix3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D DNA Helix";
    info.effect_description = "Double helix pattern with base pairs and rainbow colors";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_DNA_HELIX;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    return info;
}

void DNAHelix3D::SetupCustomUI(QWidget* parent)
{
    QWidget* dna_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(dna_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    // Only DNA-specific control: Helix Radius
    layout->addWidget(new QLabel("Helix Radius:"), 0, 0);
    radius_slider = new QSlider(Qt::Horizontal);
    radius_slider->setRange(20, 150);
    radius_slider->setValue(helix_radius);
    layout->addWidget(radius_slider, 0, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(dna_widget);
    }

    // Connect signals (only for DNA-specific controls)
    connect(radius_slider, &QSlider::valueChanged, this, &DNAHelix3D::OnDNAParameterChanged);
}

void DNAHelix3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_DNA_HELIX;
}

void DNAHelix3D::OnDNAParameterChanged()
{
    if(radius_slider) helix_radius = radius_slider->value();
    emit ParametersChanged();
}

RGBColor DNAHelix3D::CalculateColor(float x, float y, float z, float time)
{
    // Use universal base class values
    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve;
    float actual_speed = speed_curve * 200.0f;

    float freq_curve = (GetFrequency() / 100.0f);
    freq_curve = freq_curve * freq_curve;
    float actual_frequency = freq_curve * 100.0f;

    // Update progress for animation
    progress = time * (actual_speed * 0.05f);
    float freq_scale = actual_frequency * 0.02f;
    float radius_scale = helix_radius * 0.02f;

    // Calculate distance from Z-axis (center of DNA)
    float radial_distance = sqrt(x*x + y*y);
    float angle = atan2(y, x);

    // Create double helix - two intertwined spirals
    float helix_height = z * freq_scale + progress;

    // First helix (major groove)
    float helix1_angle = angle + helix_height;
    float helix1_x = radius_scale * cos(helix1_angle);
    float helix1_y = radius_scale * sin(helix1_angle);
    float helix1_distance = sqrt((x - helix1_x)*(x - helix1_x) + (y - helix1_y)*(y - helix1_y));

    // Second helix (180 degrees offset)
    float helix2_angle = angle + helix_height + 3.14159f;
    float helix2_x = radius_scale * cos(helix2_angle);
    float helix2_y = radius_scale * sin(helix2_angle);
    float helix2_distance = sqrt((x - helix2_x)*(x - helix2_x) + (y - helix2_y)*(y - helix2_y));

    // Calculate helix strand intensities with smooth falloff
    float strand_thickness = 2.5f + radius_scale * 0.5f;
    float helix1_intensity = 1.0f - smoothstep(0.0f, strand_thickness, helix1_distance);
    float helix2_intensity = 1.0f - smoothstep(0.0f, strand_thickness, helix2_distance);

    // Add base pair connections (rungs of the DNA ladder)
    float base_pair_frequency = freq_scale * 2.0f;
    float base_pair_phase = sin(z * base_pair_frequency + progress * 0.5f);
    float base_pair_connection = 0.0f;

    if(base_pair_phase > 0.5f && radial_distance < radius_scale * 1.5f)
    {
        float connection_strength = (base_pair_phase - 0.5f) / 0.5f;
        float radial_falloff = 1.0f - smoothstep(0.0f, radius_scale * 1.5f, radial_distance);
        base_pair_connection = connection_strength * radial_falloff * 0.8f;
    }

    // Combine all DNA elements with boosted intensity
    float total_intensity = fmax(helix1_intensity, helix2_intensity) + base_pair_connection;
    total_intensity = fmax(0.0f, fmin(1.0f, total_intensity * 1.5f));

    // Add minor groove effects for realism
    float minor_groove = 0.1f * sin(helix_height * 2.0f) * exp(-radial_distance * 0.5f);
    total_intensity += minor_groove;

    // Ensure minimum visibility
    if(total_intensity < 0.1f)
    {
        float fallback_distance = sqrt(x*x + y*y);
        if(fallback_distance < helix_radius * 0.03f)
        {
            total_intensity = 0.2f;
        }
    }

    // Get color using universal base class methods
    RGBColor final_color;
    if(GetRainbowMode())
    {
        // Rainbow colors that shift along the helix
        float hue = helix_height * 30.0f + radial_distance * 50.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        // Use different colors for different parts of the DNA
        float color_selector = helix_height * 0.5f + base_pair_phase;
        final_color = GetColorAtPosition(color_selector);
    }

    // Apply intensity and brightness
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f) * total_intensity;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}

RGBColor DNAHelix3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)grid;
    return CalculateColor(x, y, z, time);
}