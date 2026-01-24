// SPDX-License-Identifier: GPL-2.0-only

#include "BreathingSphere3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(BreathingSphere3D);

#include <QGridLayout>
#include "../EffectHelpers.h"

BreathingSphere3D::BreathingSphere3D(QWidget* parent) : SpatialEffect3D(parent)
{
    size_slider = nullptr;
    size_label = nullptr;
    sphere_size = 120;   // Larger default for room-scale
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
    // Rotation controls are in base class
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
    size_label = new QLabel(QString::number(sphere_size));
    size_label->setMinimumWidth(30);
    layout->addWidget(size_label, 0, 2);

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
    if(size_slider)
    {
        sphere_size = size_slider->value();
        if(size_label) size_label->setText(QString::number(sphere_size));
    }
    emit ParametersChanged();
}


RGBColor BreathingSphere3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

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

// Grid-aware version with radius proportional to room size
RGBColor BreathingSphere3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
}


    float actual_frequency = GetScaledFrequency();
    progress = CalculateProgress(time);

    float size_multiplier = GetNormalizedSize();
    float half_diag = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
    // Sphere base radius spans a large portion of the room, scales with slider
    float base_scale = 0.15f + (sphere_size / 200.0f) * 0.55f; // ~15%..70% of half-diagonal
    float sphere_radius = half_diag * base_scale * size_multiplier * (1.0f + 0.25f * sinf(progress * actual_frequency * 0.2f));

    float distance = sqrtf(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    
    // Enhanced sphere with core, glow, and ripple layers for immersive 3D effect
    float core_intensity = 1.0f - smoothstep(0.0f, sphere_radius * 0.7f, distance);
    float glow_intensity = 0.5f * (1.0f - smoothstep(sphere_radius * 0.7f, sphere_radius * 1.3f, distance));
    float ripple = 0.3f * sinf(distance * (actual_frequency / (half_diag + 0.001f)) * 1.5f - progress * 2.0f);
    ripple = (ripple + 1.0f) * 0.5f; // Normalize ripple to 0-1
    
    // Add subtle ambient glow for whole-room presence
    float ambient = 0.1f * (1.0f - smoothstep(0.0f, sphere_radius * 2.0f, distance));
    
    float sphere_intensity = core_intensity + glow_intensity + ripple * 0.4f + ambient;
    sphere_intensity = fmax(0.0f, fmin(1.0f, sphere_intensity));

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(distance * 30.0f + progress * 30.0f)
                                            : GetColorAtPosition(fmin(1.0f, distance / (sphere_radius + 0.001f)));
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    // Apply intensity (global brightness is applied by PostProcessColorGrid)
    r = (unsigned char)(r * sphere_intensity);
    g = (unsigned char)(g * sphere_intensity);
    b = (unsigned char)(b * sphere_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json BreathingSphere3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["sphere_size"] = sphere_size;
    return j;
}

void BreathingSphere3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("sphere_size"))
    {
        sphere_size = settings["sphere_size"].get<unsigned int>();
        if(size_slider)
        {
            size_slider->setValue(sphere_size);
        }
    }
}

