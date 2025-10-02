/*---------------------------------------------------------*\
| Spin3D.cpp                                                |
|                                                           |
|   3D Spin effect with rotating patterns on room surfaces |
|                                                           |
|   Date: 2025-10-02                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Spin3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(Spin3D);
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Spin3D::Spin3D(QWidget* parent) : SpatialEffect3D(parent)
{
    surface_combo = nullptr;
    arms_slider = nullptr;
    surface_type = 0;   // Default to Floor
    num_arms = 3;
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);  // Red
    default_colors.push_back(0x0000FF00);  // Green
    default_colors.push_back(0x00FF0000);  // Blue
    SetColors(default_colors);
}

Spin3D::~Spin3D()
{
}

EffectInfo3D Spin3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;  // Using new standardized system
    info.effect_name = "3D Spin";
    info.effect_description = "Rotating patterns on room surfaces with multiple arm configurations";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WAVE;  // Reuse WAVE type for now
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

    // Standardized parameter scaling
    info.default_speed_scale = 10.0f;       // (speed/100)² * 200 * 0.05
    info.default_frequency_scale = 10.0f;
    info.use_size_parameter = true;

    // Control visibility (show all controls except frequency)
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;    // Spin doesn't use frequency
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;         // Spin has custom surface selection
    info.show_color_controls = true;

    return info;
}

void Spin3D::SetupCustomUI(QWidget* parent)
{
    QWidget* spin_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(spin_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Surface:"), 0, 0);
    surface_combo = new QComboBox();
    surface_combo->addItem("Floor");
    surface_combo->addItem("Ceiling");
    surface_combo->addItem("Left Wall");
    surface_combo->addItem("Right Wall");
    surface_combo->addItem("Front Wall");
    surface_combo->addItem("Back Wall");
    surface_combo->addItem("Floor & Ceiling");
    surface_combo->addItem("Left & Right Walls");
    surface_combo->addItem("Front & Back Walls");
    surface_combo->addItem("All Walls");
    surface_combo->addItem("Entire Room");
    surface_combo->setCurrentIndex(surface_type);
    layout->addWidget(surface_combo, 0, 1);

    layout->addWidget(new QLabel("Arms:"), 1, 0);
    arms_slider = new QSlider(Qt::Horizontal);
    arms_slider->setRange(1, 8);
    arms_slider->setValue(num_arms);
    layout->addWidget(arms_slider, 1, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(spin_widget);
    }

    connect(surface_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Spin3D::OnSpinParameterChanged);
    connect(arms_slider, &QSlider::valueChanged, this, &Spin3D::OnSpinParameterChanged);
}

void Spin3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void Spin3D::OnSpinParameterChanged()
{
    if(surface_combo) surface_type = surface_combo->currentIndex();
    if(arms_slider) num_arms = arms_slider->value();
    emit ParametersChanged();
}

RGBColor Spin3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | Get effect origin (room center or user head position)   |
    \*---------------------------------------------------------*/
    Vector3D origin = GetEffectOrigin();

    /*---------------------------------------------------------*\
    | Calculate position relative to origin and apply scale   |
    \*---------------------------------------------------------*/
    float scale_factor = GetNormalizedScale();
    float rel_x = (x - origin.x) / scale_factor;
    float rel_y = (y - origin.y) / scale_factor;
    float rel_z = (z - origin.z) / scale_factor;

    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve;
    float actual_speed = speed_curve * 200.0f;

    progress = time * (actual_speed * 0.05f);
    if(effect_reverse) progress = -progress;

    float intensity = 0.0f;

    /*---------------------------------------------------------*\
    | Calculate spinning pattern based on surface             |
    | Note: Positions are room-centered, typically -5 to +5   |
    \*---------------------------------------------------------*/
    switch(surface_type)
    {
        case 0: // Floor (XZ plane, spins around Y axis)
            {
                float angle = atan2(rel_z, rel_x);
                float radius = sqrt(rel_x*rel_x + rel_z*rel_z);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                // Create blade pattern
                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    intensity = 1.0f - (arm_position / blade_width);
                    // Radial fade (strong everywhere, slight fade at far edges)
                    intensity *= fmax(0.3f, 1.0f - radius * 0.05f);
                    // Fade with height (stronger near floor, Y near 0)
                    intensity *= fmax(0.0f, 1.0f - fabs(rel_y) * 0.15f);
                }
            }
            break;

        case 1: // Ceiling (XZ plane, spins around Y axis)
            {
                float angle = atan2(rel_z, rel_x);
                float radius = sqrt(rel_x*rel_x + rel_z*rel_z);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    intensity = 1.0f - (arm_position / blade_width);
                    intensity *= fmax(0.3f, 1.0f - radius * 0.05f);
                    // Fade with height (stronger near ceiling, positive Y)
                    intensity *= fmax(0.0f, (rel_y > 0) ? (1.0f - (5.0f - rel_y) * 0.15f) : 0.0f);
                }
            }
            break;

        case 2: // Left Wall (YZ plane, spins around X axis)
            {
                float angle = atan2(rel_z, rel_y);
                float radius = sqrt(rel_y*rel_y + rel_z*rel_z);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    intensity = 1.0f - (arm_position / blade_width);
                    intensity *= fmax(0.3f, 1.0f - radius * 0.05f);
                    // Fade with X distance (stronger near left wall, negative X)
                    intensity *= fmax(0.0f, (rel_x < 0) ? (1.0f - (5.0f + rel_x) * 0.15f) : 0.0f);
                }
            }
            break;

        case 3: // Right Wall (YZ plane, spins around X axis)
            {
                float angle = atan2(rel_z, rel_y);
                float radius = sqrt(rel_y*rel_y + rel_z*rel_z);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    intensity = 1.0f - (arm_position / blade_width);
                    intensity *= fmax(0.3f, 1.0f - radius * 0.05f);
                    // Fade with X distance (stronger near right wall, positive X)
                    intensity *= fmax(0.0f, (rel_x > 0) ? (1.0f - (5.0f - rel_x) * 0.15f) : 0.0f);
                }
            }
            break;

        case 4: // Front Wall (XY plane, spins around Z axis)
            {
                float angle = atan2(rel_y, rel_x);
                float radius = sqrt(rel_x*rel_x + rel_y*rel_y);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    intensity = 1.0f - (arm_position / blade_width);
                    intensity *= fmax(0.3f, 1.0f - radius * 0.05f);
                    // Fade with Z distance (stronger near front wall, negative Z)
                    intensity *= fmax(0.0f, (rel_z < 0) ? (1.0f - (5.0f + rel_z) * 0.15f) : 0.0f);
                }
            }
            break;

        case 5: // Back Wall (XY plane, spins around Z axis)
            {
                float angle = atan2(rel_y, rel_x);
                float radius = sqrt(rel_x*rel_x + rel_y*rel_y);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    intensity = 1.0f - (arm_position / blade_width);
                    intensity *= fmax(0.3f, 1.0f - radius * 0.05f);
                    // Fade with Z distance (stronger near back wall, positive Z)
                    intensity *= fmax(0.0f, (rel_z > 0) ? (1.0f - (5.0f - rel_z) * 0.15f) : 0.0f);
                }
            }
            break;

        case 6: // Floor & Ceiling
            {
                float angle = atan2(rel_z, rel_x);
                float radius = sqrt(rel_x*rel_x + rel_z*rel_z);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    float blade_intensity = 1.0f - (arm_position / blade_width);
                    blade_intensity *= fmax(0.3f, 1.0f - radius * 0.05f);

                    // Floor component (Y near 0)
                    float floor_intensity = blade_intensity * fmax(0.0f, 1.0f - fabs(rel_y) * 0.15f);
                    // Ceiling component (Y positive)
                    float ceiling_intensity = blade_intensity * fmax(0.0f, (rel_y > 0) ? (1.0f - (5.0f - rel_y) * 0.15f) : 0.0f);

                    intensity = fmax(floor_intensity, ceiling_intensity);
                }
            }
            break;

        case 7: // Left & Right Walls
            {
                float angle = atan2(rel_z, rel_y);
                float radius = sqrt(rel_y*rel_y + rel_z*rel_z);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    float blade_intensity = 1.0f - (arm_position / blade_width);
                    blade_intensity *= fmax(0.3f, 1.0f - radius * 0.05f);

                    // Left wall
                    float left_intensity = blade_intensity * fmax(0.0f, (rel_x < 0) ? (1.0f - (5.0f + rel_x) * 0.15f) : 0.0f);
                    // Right wall
                    float right_intensity = blade_intensity * fmax(0.0f, (rel_x > 0) ? (1.0f - (5.0f - rel_x) * 0.15f) : 0.0f);

                    intensity = fmax(left_intensity, right_intensity);
                }
            }
            break;

        case 8: // Front & Back Walls
            {
                float angle = atan2(rel_y, rel_x);
                float radius = sqrt(rel_x*rel_x + rel_y*rel_y);
                float spin_angle = angle * num_arms - progress;
                float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                if(arm_position < 0) arm_position += 6.28318f / num_arms;

                float blade_width = 0.4f * (6.28318f / num_arms);
                if(arm_position < blade_width)
                {
                    float blade_intensity = 1.0f - (arm_position / blade_width);
                    blade_intensity *= fmax(0.3f, 1.0f - radius * 0.05f);

                    // Front wall (negative Z)
                    float front_intensity = blade_intensity * fmax(0.0f, (rel_z < 0) ? (1.0f - (5.0f + rel_z) * 0.15f) : 0.0f);
                    // Back wall (positive Z)
                    float back_intensity = blade_intensity * fmax(0.0f, (rel_z > 0) ? (1.0f - (5.0f - rel_z) * 0.15f) : 0.0f);

                    intensity = fmax(front_intensity, back_intensity);
                }
            }
            break;

        case 9: // All Walls (not floor/ceiling)
            {
                // Calculate for all 4 walls and take maximum
                float max_intensity = 0.0f;

                // Left/Right walls (YZ plane)
                {
                    float angle = atan2(rel_z, rel_y);
                    float radius = sqrt(rel_y*rel_y + rel_z*rel_z);
                    float spin_angle = angle * num_arms - progress;
                    float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                    if(arm_position < 0) arm_position += 6.28318f / num_arms;

                    float blade_width = 0.4f * (6.28318f / num_arms);
                    if(arm_position < blade_width)
                    {
                        float blade_intensity = 1.0f - (arm_position / blade_width);
                        blade_intensity *= fmax(0.3f, 1.0f - radius * 0.05f);

                        float left = blade_intensity * fmax(0.0f, (rel_x < 0) ? (1.0f - (5.0f + rel_x) * 0.15f) : 0.0f);
                        float right = blade_intensity * fmax(0.0f, (rel_x > 0) ? (1.0f - (5.0f - rel_x) * 0.15f) : 0.0f);
                        max_intensity = fmax(max_intensity, fmax(left, right));
                    }
                }

                // Front/Back walls (XY plane)
                {
                    float angle = atan2(rel_y, rel_x);
                    float radius = sqrt(rel_x*rel_x + rel_y*rel_y);
                    float spin_angle = angle * num_arms - progress;
                    float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                    if(arm_position < 0) arm_position += 6.28318f / num_arms;

                    float blade_width = 0.4f * (6.28318f / num_arms);
                    if(arm_position < blade_width)
                    {
                        float blade_intensity = 1.0f - (arm_position / blade_width);
                        blade_intensity *= fmax(0.3f, 1.0f - radius * 0.05f);

                        float front = blade_intensity * fmax(0.0f, (rel_z < 0) ? (1.0f - (5.0f + rel_z) * 0.15f) : 0.0f);
                        float back = blade_intensity * fmax(0.0f, (rel_z > 0) ? (1.0f - (5.0f - rel_z) * 0.15f) : 0.0f);
                        max_intensity = fmax(max_intensity, fmax(front, back));
                    }
                }

                intensity = max_intensity;
            }
            break;

        case 10: // Entire Room (all surfaces)
        default:
            {
                float max_intensity = 0.0f;

                // Floor/Ceiling (XZ plane)
                {
                    float angle = atan2(rel_z, rel_x);
                    float radius = sqrt(rel_x*rel_x + rel_z*rel_z);
                    float spin_angle = angle * num_arms - progress;
                    float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                    if(arm_position < 0) arm_position += 6.28318f / num_arms;

                    float blade_width = 0.4f * (6.28318f / num_arms);
                    if(arm_position < blade_width)
                    {
                        float blade_intensity = 1.0f - (arm_position / blade_width);
                        blade_intensity *= fmax(0.3f, 1.0f - radius * 0.05f);

                        float floor = blade_intensity * fmax(0.0f, 1.0f - fabs(rel_y) * 0.15f);
                        float ceiling = blade_intensity * fmax(0.0f, (rel_y > 0) ? (1.0f - (5.0f - rel_y) * 0.15f) : 0.0f);
                        max_intensity = fmax(max_intensity, fmax(floor, ceiling));
                    }
                }

                // All Walls
                {
                    // Left/Right walls
                    float angle = atan2(rel_z, rel_y);
                    float radius = sqrt(rel_y*rel_y + rel_z*rel_z);
                    float spin_angle = angle * num_arms - progress;
                    float arm_position = fmod(spin_angle, 6.28318f / num_arms);
                    if(arm_position < 0) arm_position += 6.28318f / num_arms;

                    float blade_width = 0.4f * (6.28318f / num_arms);
                    if(arm_position < blade_width)
                    {
                        float blade_intensity = 1.0f - (arm_position / blade_width);
                        blade_intensity *= fmax(0.3f, 1.0f - radius * 0.05f);

                        float left = blade_intensity * fmax(0.0f, (rel_x < 0) ? (1.0f - (5.0f + rel_x) * 0.15f) : 0.0f);
                        float right = blade_intensity * fmax(0.0f, (rel_x > 0) ? (1.0f - (5.0f - rel_x) * 0.15f) : 0.0f);
                        max_intensity = fmax(max_intensity, fmax(left, right));
                    }

                    // Front/Back walls
                    angle = atan2(rel_y, rel_x);
                    radius = sqrt(rel_x*rel_x + rel_y*rel_y);
                    spin_angle = angle * num_arms - progress;
                    arm_position = fmod(spin_angle, 6.28318f / num_arms);
                    if(arm_position < 0) arm_position += 6.28318f / num_arms;

                    if(arm_position < blade_width)
                    {
                        float blade_intensity = 1.0f - (arm_position / blade_width);
                        blade_intensity *= fmax(0.3f, 1.0f - radius * 0.05f);

                        float front = blade_intensity * fmax(0.0f, (rel_z < 0) ? (1.0f - (5.0f + rel_z) * 0.15f) : 0.0f);
                        float back = blade_intensity * fmax(0.0f, (rel_z > 0) ? (1.0f - (5.0f - rel_z) * 0.15f) : 0.0f);
                        max_intensity = fmax(max_intensity, fmax(front, back));
                    }
                }

                intensity = max_intensity;
            }
            break;
    }

    intensity = fmax(0.0f, fmin(1.0f, intensity));

    /*---------------------------------------------------------*\
    | Get color based on intensity and mode                    |
    \*---------------------------------------------------------*/
    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = progress * 57.2958f + intensity * 120.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(intensity);
    }

    // Apply intensity and brightness
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f) * intensity;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}
