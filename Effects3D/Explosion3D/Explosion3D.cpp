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
    type_combo = nullptr;
    explosion_intensity = 75;
    progress = 0.0f;
    explosion_type = 0;

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
    info.default_speed_scale = 35.0f;       // (speed/100)² * 35 (room-scale expansion)
    info.default_frequency_scale = 60.0f;   // (freq/100)² * 60 (less fine noise)
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
    intensity_slider->setToolTip("Explosion energy (affects radius and wave thickness)");
    layout->addWidget(intensity_slider, 0, 1);

    layout->addWidget(new QLabel("Type:"), 1, 0);
    type_combo = new QComboBox();
    type_combo->setToolTip("Explosion type behavior");
    type_combo->addItem("Standard");
    type_combo->addItem("Nuke");
    type_combo->addItem("Land Mine");
    type_combo->addItem("Bomb");
    type_combo->addItem("Wall Bounce");
    type_combo->setCurrentIndex(explosion_type);
    layout->addWidget(type_combo, 1, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(explosion_widget);
    }

    connect(intensity_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Explosion3D::OnExplosionParameterChanged);
}

void Explosion3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_EXPLOSION;
}

void Explosion3D::OnExplosionParameterChanged()
{
    if(intensity_slider) explosion_intensity = intensity_slider->value();
    if(type_combo) explosion_type = type_combo->currentIndex();
    emit ParametersChanged();
}

RGBColor Explosion3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | NOTE: All coordinates (x, y, z) are in GRID UNITS       |
    | 1 grid unit = 10mm. LED positions use grid units.       |
    \*---------------------------------------------------------*/

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
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z))
    {
        return 0x00000000;  // Black - outside effect boundary
    }

    float actual_frequency = GetScaledFrequency();
    progress = CalculateProgress(time);

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
        case AXIS_Y:  // Directional explosion along Y-axis (Front to Back)
            distance = fabs(rel_y) + sqrt(rel_x*rel_x + rel_z*rel_z) * 0.3f;
            break;
        case AXIS_Z:  // Directional explosion along Z-axis (Floor to Ceiling)
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

// Grid-aware version using real room center and room-relative boundary
RGBColor Explosion3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    // Origin based on reference mode (room center/user/custom)
    Vector3D origin = GetEffectOriginGrid(grid);

    // Position relative to origin
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    // Respect room-relative boundary via scale slider and room size
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    float actual_frequency = GetScaledFrequency();
    progress = CalculateProgress(time);

    float size_multiplier = GetNormalizedSize();
    float freq_scale = actual_frequency * 0.01f / size_multiplier;

    float distance;
    switch(effect_axis)
    {
        case AXIS_X:
            distance = fabs(rel_x) + sqrt(rel_y*rel_y + rel_z*rel_z) * 0.3f;
            break;
        case AXIS_Y:
            distance = fabs(rel_y) + sqrt(rel_x*rel_x + rel_z*rel_z) * 0.3f;
            break;
        case AXIS_Z:
            distance = fabs(rel_z) + sqrt(rel_x*rel_x + rel_y*rel_y) * 0.3f;
            break;
        case AXIS_RADIAL:
        default:
            distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            break;
    }

    // Type-specific distance shaping (Land Mine flattens vertical component)
    if(explosion_type == 2)
    {
        float vz = rel_z * 0.35f;
        distance = sqrtf(rel_x*rel_x + rel_y*rel_y + vz*vz);
    }

    // Room-scale expansion: base radius and wave thickness
    float explosion_radius = progress * (explosion_intensity * 0.25f) * size_multiplier;
    float wave_thickness = (8.0f + explosion_intensity * 0.08f) * size_multiplier;
    // Nuke: much larger radius and thicker wave
    if(explosion_type == 1)
    {
        explosion_radius *= 1.8f;
        wave_thickness   *= 1.5f;
    }
    // Wall Bounce: ping-pong radius between center and walls
    if(explosion_type == 4)
    {
        float max_extent = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
        float travel = explosion_radius;
        float period = fmax(0.1f, max_extent);
        float t = fmodf(travel, 2.0f * period);
        explosion_radius = (t <= period) ? t : (2.0f * period - t);
    }

    float primary_wave = 1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, distance);
    primary_wave *= exp(-fabs(distance - explosion_radius) * 0.1f);

    float secondary_radius = explosion_radius * 0.7f;
    float secondary_wave = 1.0f - smoothstep(secondary_radius - wave_thickness * 0.5f, secondary_radius + wave_thickness * 0.5f, distance);
    secondary_wave *= exp(-fabs(distance - secondary_radius) * 0.15f) * 0.6f;

    float shock_detail = 0.2f * sinf(distance * freq_scale * 8.0f - progress * 4.0f);
    // Bomb: add directional lobes
    if(explosion_type == 3)
    {
        float ang = atan2f(rel_y, rel_x);
        shock_detail *= (0.6f + 0.4f * fabsf(cosf(ang * 4.0f)));
    }
    shock_detail *= expf(-distance * 0.1f);

    float explosion_intensity_final = primary_wave + secondary_wave + shock_detail;
    explosion_intensity_final = fmax(0.0f, fmin(1.0f, explosion_intensity_final));

    if(distance < explosion_radius * 0.3f)
    {
        float core_intensity = 1.0f - (distance / (explosion_radius * 0.3f));
        explosion_intensity_final = fmax(explosion_intensity_final, core_intensity * 0.8f);
    }

    float hue_base = (explosion_type == 1 ? 30.0f : 60.0f);
    RGBColor final_color = GetRainbowMode()
        ? GetRainbowColor(fmax(0.0f, hue_base - (explosion_intensity_final * 60.0f) + progress * 10.0f))
        : GetColorAtPosition(explosion_intensity_final);

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (GetBrightness() / 100.0f) * explosion_intensity_final;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}
