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

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(DNAHelix3D);

#include <QGridLayout>
#include "../EffectHelpers.h"

DNAHelix3D::DNAHelix3D(QWidget* parent) : SpatialEffect3D(parent)
{
    radius_slider = nullptr;
    helix_radius = 180;      // Larger default helix radius for room-scale
    progress = 0.0f;

    // Set up default DNA base pair colors (0x00BBGGRR format), allow user override via universal controls
    std::vector<RGBColor> dna_colors = {
        0x000000FF,  // Red (Adenine)
        0x0000FFFF,  // Yellow (Thymine)
        0x0000FF00,  // Green (Guanine)
        0x00FF0000   // Blue (Cytosine)
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
    info.info_version = 2;  // Using new standardized system
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
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    // Standardized parameter scaling
    info.default_speed_scale = 10.0f;       // (speed/100)² * 200 * 0.05
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
    /*---------------------------------------------------------*\
    | NOTE: All coordinates (x, y, z) are in GRID UNITS       |
    | One grid unit equals the configured grid scale          |
    | (default 10mm). LED positions use grid units.           |
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
    // Room-scale: lower spatial frequency, larger radius scale
    float freq_scale = actual_frequency * 0.004f / size_multiplier;
    float radius_scale = helix_radius * 0.08f * size_multiplier;  // Bigger helix coils across the room

    /*---------------------------------------------------------*\
    | Calculate helix based on selected axis                  |
    \*---------------------------------------------------------*/
    float radial_distance, angle, helix_height;
    float coord1, coord2, coord_along_helix;

    switch(effect_axis)
    {
        case AXIS_X:  // Helix along X-axis (Left to Right)
            radial_distance = sqrt(rel_y*rel_y + rel_z*rel_z);
            angle = atan2(rel_z, rel_y);
            helix_height = rel_x * freq_scale + progress;
            coord1 = rel_y;
            coord2 = rel_z;
            coord_along_helix = rel_x;
            break;
        case AXIS_Y:  // Helix along Y-axis (Bottom to Top, Y-up)
            radial_distance = sqrt(rel_x*rel_x + rel_z*rel_z);
            angle = atan2(rel_z, rel_x);
            helix_height = rel_y * freq_scale + progress;
            coord1 = rel_x;
            coord2 = rel_z;
            coord_along_helix = rel_y;
            break;
        case AXIS_Z:  // Helix along Z-axis (Front to Back)
        default:
            radial_distance = sqrt(rel_x*rel_x + rel_y*rel_y);
            angle = atan2(rel_y, rel_x);
            helix_height = rel_z * freq_scale + progress;
            coord1 = rel_x;
            coord2 = rel_y;
            coord_along_helix = rel_z;
            break;
        case AXIS_RADIAL:  // Radial helix from center
            radial_distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            angle = atan2(rel_y, rel_x);
            helix_height = radial_distance * freq_scale + progress;
            coord1 = rel_x;
            coord2 = rel_y;
            coord_along_helix = rel_z;
            break;
    }

    /*---------------------------------------------------------*\
    | Calculate the two DNA strands (double helix)            |
    \*---------------------------------------------------------*/
    float helix1_angle = angle + helix_height;
    float helix1_c1 = radius_scale * cos(helix1_angle);
    float helix1_c2 = radius_scale * sin(helix1_angle);
    float helix1_distance = sqrt((coord1 - helix1_c1)*(coord1 - helix1_c1) + (coord2 - helix1_c2)*(coord2 - helix1_c2));

    float helix2_angle = angle + helix_height + 3.14159f;
    float helix2_c1 = radius_scale * cos(helix2_angle);
    float helix2_c2 = radius_scale * sin(helix2_angle);
    float helix2_distance = sqrt((coord1 - helix2_c1)*(coord1 - helix2_c1) + (coord2 - helix2_c2)*(coord2 - helix2_c2));

    /*---------------------------------------------------------*\
    | Create thicker, glowing strands with outer glow         |
    \*---------------------------------------------------------*/
    float strand_core_thickness = 6.0f + radius_scale * 0.25f;
    float strand_glow_thickness = 16.0f + radius_scale * 0.5f;

    // Core brightness (solid strand)
    float helix1_core = 1.0f - smoothstep(0.0f, strand_core_thickness, helix1_distance);
    float helix2_core = 1.0f - smoothstep(0.0f, strand_core_thickness, helix2_distance);

    // Outer glow (softer falloff)
    float helix1_glow = (1.0f - smoothstep(strand_core_thickness, strand_glow_thickness, helix1_distance)) * 0.5f;
    float helix2_glow = (1.0f - smoothstep(strand_core_thickness, strand_glow_thickness, helix2_distance)) * 0.5f;

    float helix1_intensity = helix1_core + helix1_glow;
    float helix2_intensity = helix2_core + helix2_glow;

    /*---------------------------------------------------------*\
    | Add base pairs (rungs) with better spacing and glow     |
    \*---------------------------------------------------------*/
    float base_pair_frequency = freq_scale * 1.2f;  // Fewer, larger base pairs in room-scale
    float base_pair_phase = fmod(coord_along_helix * base_pair_frequency + progress * 0.5f, 6.28318f);

    // Create discrete base pairs at regular intervals
    float base_pair_active = exp(-fmod(base_pair_phase, 6.28318f / 3.0f) * 8.0f);  // Sharper pulses
    float base_pair_connection = 0.0f;

    if(base_pair_active > 0.1f && radial_distance < radius_scale * 1.8f)
    {
        // Create horizontal rung connecting the two strands
        float rung_distance = fabs(radial_distance - radius_scale);
        float rung_thickness = 1.5f + radius_scale * 0.2f;

        float rung_intensity = 1.0f - smoothstep(0.0f, rung_thickness, rung_distance);
        float rung_glow = (1.0f - smoothstep(rung_thickness, rung_thickness * 2.0f, rung_distance)) * 0.4f;

        base_pair_connection = (rung_intensity + rung_glow) * base_pair_active;
    }

    /*---------------------------------------------------------*\
    | Add major and minor grooves (realistic DNA feature)     |
    \*---------------------------------------------------------*/
    float groove_angle = fmod(angle - helix_height * 0.5f, 6.28318f);
    float major_groove = exp(-fabs(groove_angle - 3.14159f) * 2.0f) * 0.15f;  // Darker region
    float minor_groove = exp(-fabs(groove_angle) * 3.0f) * 0.1f;  // Slightly darker region

    float groove_effect = 1.0f - (major_groove + minor_groove);

    /*---------------------------------------------------------*\
    | Combine all DNA elements                                 |
    \*---------------------------------------------------------*/
    float strand_intensity = fmax(helix1_intensity, helix2_intensity);
    float total_intensity = (strand_intensity + base_pair_connection) * groove_effect;

    // Add subtle pulsing energy effect along strands
    float energy_pulse = 0.15f * sin(helix_height * 4.0f - progress * 3.0f) * strand_intensity;
    total_intensity += energy_pulse;

    total_intensity = fmax(0.0f, fmin(1.0f, total_intensity * 1.3f));

    /*---------------------------------------------------------*\
    | Color the different DNA components                       |
    \*---------------------------------------------------------*/
    RGBColor final_color;
    if(GetRainbowMode())
    {
        // Rainbow colors spiral along the helix with base pairs in complementary colors
        float hue = helix_height * 50.0f;

        // Base pairs get offset hue for contrast
        if(base_pair_connection > 0.3f)
        {
            hue += 180.0f;  // Complementary color for base pairs
        }

        final_color = GetRainbowColor(hue);
    }
    else
    {
        // Color strands vs base pairs differently for better visibility
        if(base_pair_connection > strand_intensity * 0.5f)
        {
            // Base pairs use second color
            float position = (GetColors().size() > 1) ? 0.7f : 0.5f;
            final_color = GetColorAtPosition(position);
        }
        else
        {
            // Strands use gradient based on position along helix
            float position = fmod(helix_height * 0.3f, 1.0f);
            final_color = GetColorAtPosition(position);
        }
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
    /*---------------------------------------------------------*\
    | DNA Helix is a 3D spatial effect - simply delegate to    |
    | the standard CalculateColor implementation               |
    \*---------------------------------------------------------*/
    (void)grid;  // Unused parameter
    return CalculateColor(x, y, z, time);
}

nlohmann::json DNAHelix3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["helix_radius"] = helix_radius;
    return j;
}

void DNAHelix3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("helix_radius")) helix_radius = settings["helix_radius"];

    if(radius_slider) radius_slider->setValue(helix_radius);
}
