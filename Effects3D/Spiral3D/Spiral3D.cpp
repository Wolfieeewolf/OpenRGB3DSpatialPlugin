// SPDX-License-Identifier: GPL-2.0-only

#include "Spiral3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(Spiral3D);
#include <QGridLayout>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Spiral3D::Spiral3D(QWidget* parent) : SpatialEffect3D(parent)
{
    arms_slider = nullptr;
    pattern_combo = nullptr;
    gap_slider = nullptr;
    num_arms = 3;
    pattern_type = 0;   // Default to Smooth
    gap_size = 30;      // Default gap size
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

Spiral3D::~Spiral3D()
{
}

EffectInfo3D Spiral3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;  // Using new standardized system
    info.effect_name = "3D Spiral";
    info.effect_description = "Animated spiral pattern with configurable arm count";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_SPIRAL;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = true;
    info.needs_frequency = false;

    // Standardized parameter scaling
    // Room-scale spirals: larger arms and slower spatial twist by default
    info.default_speed_scale = 35.0f;       // (speed/100)² * 35 (clear rotation at mid speeds)
    info.default_frequency_scale = 40.0f;   // fewer twists/kinks across the room
    info.use_size_parameter = true;

    // Control visibility (show all controls except frequency - has custom)
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;     // Show standard frequency control
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;
    info.show_color_controls = true;

    return info;
}

void Spiral3D::SetupCustomUI(QWidget* parent)
{
    QWidget* spiral_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(spiral_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Pattern:"), 0, 0);
    pattern_combo = new QComboBox();
    pattern_combo->addItem("Smooth Spiral");
    pattern_combo->addItem("Pinwheel");
    pattern_combo->addItem("Sharp Blades");
    pattern_combo->setCurrentIndex(pattern_type);
    pattern_combo->setToolTip("Spiral pattern style");
    layout->addWidget(pattern_combo, 0, 1);

    layout->addWidget(new QLabel("Arms:"), 1, 0);
    arms_slider = new QSlider(Qt::Horizontal);
    arms_slider->setRange(2, 8);
    arms_slider->setValue(num_arms);
    arms_slider->setToolTip("Number of spiral arms");
    layout->addWidget(arms_slider, 1, 1);

    layout->addWidget(new QLabel("Gap Size:"), 2, 0);
    gap_slider = new QSlider(Qt::Horizontal);
    gap_slider->setRange(10, 80);
    gap_slider->setValue(gap_size);
    gap_slider->setToolTip("Gap size between blades");
    layout->addWidget(gap_slider, 2, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(spiral_widget);
    }

    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Spiral3D::OnSpiralParameterChanged);
    connect(arms_slider, &QSlider::valueChanged, this, &Spiral3D::OnSpiralParameterChanged);
    connect(gap_slider, &QSlider::valueChanged, this, &Spiral3D::OnSpiralParameterChanged);
}

void Spiral3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_SPIRAL;
}

void Spiral3D::OnSpiralParameterChanged()
{
    if(pattern_combo) pattern_type = pattern_combo->currentIndex();
    if(arms_slider) num_arms = arms_slider->value();
    if(gap_slider) gap_size = gap_slider->value();
    emit ParametersChanged();
}

RGBColor Spiral3D::CalculateColor(float x, float y, float z, float time)
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
    | Uses standardized boundary helper                        |
    \*---------------------------------------------------------*/
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z))
    {
        return 0x00000000;  // Black - outside effect boundary
    }

    /*---------------------------------------------------------*\
    | Use standardized parameter helpers                       |
    \*---------------------------------------------------------*/
    float actual_frequency = GetScaledFrequency();

    progress = CalculateProgress(time);
    float size_multiplier = GetNormalizedSize();  // 0.1 to 2.0
    // Room-scale: reduce spatial frequency to expand arm spacing
    float freq_scale = actual_frequency * 0.003f / size_multiplier;

    /*---------------------------------------------------------*\
    | Calculate spiral based on selected axis                 |
    \*---------------------------------------------------------*/
    float radius, angle, twist_coord;

    switch(effect_axis)
    {
        case AXIS_X:  // Spiral along X-axis (Left to Right)
            radius = sqrtf(rel_y*rel_y + rel_z*rel_z);
            angle = atan2(rel_z, rel_y);
            twist_coord = rel_x;
            break;
        case AXIS_Y:  // Spiral along Y-axis (Bottom to Top, Y-up)
            radius = sqrtf(rel_x*rel_x + rel_z*rel_z);
            angle = atan2(rel_z, rel_x);
            twist_coord = rel_y;
            break;
        case AXIS_Z:  // Spiral along Z-axis (Front to Back)
        default:
            radius = sqrtf(rel_x*rel_x + rel_y*rel_y);
            angle = atan2(rel_y, rel_x);
            twist_coord = rel_z;
            break;
        case AXIS_RADIAL:  // Radial spiral from center
            radius = sqrtf(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            angle = atan2(rel_y, rel_x) + atan2(rel_z, sqrtf(rel_x*rel_x + rel_y*rel_y));
            twist_coord = radius;
            break;
    }

    /*---------------------------------------------------------*\
    | Apply reverse if enabled                                 |
    \*---------------------------------------------------------*/
    if(effect_reverse)
    {
        angle = -angle;
    }

    float z_twist = twist_coord * 0.3f;
    float spiral_angle = angle * num_arms + radius * freq_scale + z_twist - progress;

    /*---------------------------------------------------------*\
    | Calculate intensity based on pattern type               |
    \*---------------------------------------------------------*/
    float spiral_value;
    float gap_factor = gap_size / 100.0f;

    switch(pattern_type)
    {
        case 0: // Smooth Spiral - Original smooth flowing spiral
            spiral_value = sin(spiral_angle) * (1.0f + 0.4f * cos(twist_coord * freq_scale + progress * 0.7f));
            {
                float secondary_spiral = cos(spiral_angle * 0.5f + twist_coord * freq_scale * 1.5f + progress * 1.2f) * 0.3f;
                spiral_value += secondary_spiral;
            }
            spiral_value = (spiral_value + 1.5f) / 3.0f;
            spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));
            break;

        case 1: // Pinwheel - Distinct blades with dark gaps
            {
                // Normalize angle to 0-2π range per arm
                float arm_angle = fmod(spiral_angle, 6.28318f / num_arms);
                if(arm_angle < 0) arm_angle += 6.28318f / num_arms;

                // Calculate blade width (inverse of gap)
                float blade_width = (1.0f - gap_factor) * (6.28318f / num_arms);

                // Sharp edges for distinct blades
                if(arm_angle < blade_width)
                {
                    // Inside blade - smooth gradient
                    float blade_position = arm_angle / blade_width;
                    spiral_value = 0.5f + 0.5f * cos(blade_position * 3.14159f);  // Peak in middle
                }
                else
                {
                    // In gap - completely dark
                    spiral_value = 0.0f;
                }

                // Add radial gradient for depth
                float radial_fade = 1.0f - exp(-radius * freq_scale * 0.5f);
                spiral_value *= radial_fade;
            }
            break;

        case 2: // Sharp Blades - Very defined, laser-like arms
        default:
            {
                float arm_angle = fmod(spiral_angle, 6.28318f / num_arms);
                if(arm_angle < 0) arm_angle += 6.28318f / num_arms;

                float blade_width = (1.0f - gap_factor) * (6.28318f / num_arms);

                if(arm_angle < blade_width)
                {
                    // Sharp peak in center with quick falloff
                    float blade_position = fabs(arm_angle - blade_width * 0.5f) / (blade_width * 0.5f);
                    spiral_value = 1.0f - blade_position * blade_position;  // Sharp peak
                }
                else
                {
                    spiral_value = 0.0f;
                }

                // Add pulsing energy along blades
                float energy_pulse = 0.2f * sin(radius * freq_scale * 2.0f - progress * 2.0f);
                spiral_value = fmax(0.0f, spiral_value + energy_pulse);
            }
            break;
    }

    /*---------------------------------------------------------*\
    | Get color based on spiral value and position            |
    \*---------------------------------------------------------*/
    RGBColor final_color;

    // For pinwheel modes, color each arm differently
    if((pattern_type == 1 || pattern_type == 2) && !GetRainbowMode())
    {
        // Determine which arm we're on
        float arm_index = fmod(spiral_angle / (6.28318f / num_arms), num_arms);
        if(arm_index < 0) arm_index += num_arms;
        float color_position = arm_index / num_arms;

        final_color = GetColorAtPosition(color_position);
    }
    else if(GetRainbowMode())
    {
        // Rainbow mode - smooth color transition
        float hue = spiral_angle * 57.2958f + progress * 20.0f;  // Convert radians to degrees
        final_color = GetRainbowColor(hue);
    }
    else
    {
        // Smooth gradient based on spiral value
        final_color = GetColorAtPosition(spiral_value);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    return (b << 16) | (g << 8) | r;
}

// Grid-aware version with room-sized spirals
RGBColor Spiral3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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
    float freq_scale = actual_frequency * 0.15f / fmax(0.1f, size_multiplier);

    float radius, angle;
    switch(effect_axis)
    {
        case AXIS_X: radius = sqrt(rel_y*rel_y + rel_z*rel_z); angle = atan2(rel_z, rel_y); break;
        case AXIS_Y: radius = sqrt(rel_x*rel_x + rel_z*rel_z); angle = atan2(rel_z, rel_x); break;
        case AXIS_Z: default: radius = sqrt(rel_x*rel_x + rel_y*rel_y); angle = atan2(rel_y, rel_x); break;
        case AXIS_RADIAL: radius = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z); angle = atan2(rel_y, rel_x) + atan2(rel_z, sqrt(rel_x*rel_x + rel_y*rel_y)); break;
    }

    if(effect_reverse) angle = -angle;

    // Normalize radius and twist_coord consistently against room bounds
    // This ensures ALL controllers see the same spiral pattern at the same absolute room position
    float max_distance = sqrtf(grid.width*grid.width + grid.height*grid.height + grid.depth*grid.depth) / 2.0f;
    float norm_radius = (max_distance > 0.001f) ? (radius / max_distance) : 0.0f;
    norm_radius = fmaxf(0.0f, fminf(1.0f, norm_radius));
    
    // Normalize twist_coord based on axis
    float norm_twist = 0.0f;
    switch(effect_axis)
    {
        case AXIS_X: norm_twist = (grid.width > 0.001f) ? ((x - grid.min_x) / grid.width) : 0.0f; break;
        case AXIS_Y: norm_twist = (grid.height > 0.001f) ? ((y - grid.min_y) / grid.height) : 0.0f; break;
        case AXIS_Z: default: norm_twist = (grid.depth > 0.001f) ? ((z - grid.min_z) / grid.depth) : 0.0f; break;
        case AXIS_RADIAL: norm_twist = norm_radius; break;
    }
    norm_twist = fmaxf(0.0f, fminf(1.0f, norm_twist));
    
    float z_twist = norm_twist * freq_scale * 3.0f;
    float spiral_angle = angle * num_arms + norm_radius * (actual_frequency * 6.0f) + z_twist - progress;

    float spiral_value;
    float gap_factor = gap_size / 100.0f;

    switch(pattern_type)
    {
        case 0: // Smooth
            spiral_value = sin(spiral_angle) * (1.0f + 0.4f * cos(norm_twist * freq_scale * 3.0f + progress * 0.7f));
            spiral_value += 0.3f * cos(spiral_angle * 0.5f + norm_twist * freq_scale * 4.5f + progress * 1.2f);
            spiral_value = (spiral_value + 1.5f) / 3.0f;
            spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));
            break;
        case 1: // Pinwheel
            {
                float arm_angle = fmod(spiral_angle, 6.28318f / num_arms);
                if(arm_angle < 0) arm_angle += 6.28318f / num_arms;
                float blade_width = (1.0f - gap_factor) * (6.28318f / num_arms);
                if(arm_angle < blade_width)
                {
                    float blade_position = arm_angle / blade_width;
                    spiral_value = 0.5f + 0.5f * cos(blade_position * 3.14159f);
                }
                else
                {
                    spiral_value = 0.0f;
                }
                float radial_fade = 1.0f - exp(-norm_radius * (actual_frequency * 0.8f));
                spiral_value *= radial_fade;
            }
            break;
        case 2: // Sharp Blades
        default:
            {
                float arm_angle = fmod(spiral_angle, 6.28318f / num_arms);
                if(arm_angle < 0) arm_angle += 6.28318f / num_arms;
                float blade_width = (1.0f - gap_factor) * (6.28318f / num_arms);
                if(arm_angle < blade_width)
                {
                    float blade_position = fabs(arm_angle - blade_width * 0.5f) / (blade_width * 0.5f);
                    spiral_value = 1.0f - blade_position * blade_position;
                }
                else
                {
                    spiral_value = 0.0f;
                }
                float energy_pulse = 0.2f * sin(norm_radius * (actual_frequency * 1.2f) - progress * 2.0f);
                spiral_value = fmax(0.0f, spiral_value + energy_pulse);
            }
            break;
    }

    RGBColor final_color;
    if((pattern_type == 1 || pattern_type == 2) && !GetRainbowMode())
    {
        float arm_index = fmod(spiral_angle / (6.28318f / num_arms), (float)num_arms);
        if(arm_index < 0) arm_index += num_arms;
        final_color = GetColorAtPosition(arm_index / (float)num_arms);
    }
    else if(GetRainbowMode())
    {
        float hue = spiral_angle * 57.2958f + progress * 20.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(spiral_value);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Spiral3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["num_arms"] = num_arms;
    j["pattern_type"] = pattern_type;
    j["gap_size"] = gap_size;
    return j;
}

void Spiral3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("num_arms"))
    {
        num_arms = settings["num_arms"].get<unsigned int>();
        if(arms_slider)
        {
            arms_slider->setValue(num_arms);
        }
    }
    if(settings.contains("pattern_type"))
    {
        pattern_type = settings["pattern_type"].get<int>();
        if(pattern_combo)
        {
            pattern_combo->setCurrentIndex(pattern_type);
        }
    }
    if(settings.contains("gap_size"))
    {
        gap_size = settings["gap_size"].get<unsigned int>();
        if(gap_slider)
        {
            gap_slider->setValue(gap_size);
        }
    }
}
