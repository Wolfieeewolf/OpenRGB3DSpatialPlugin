// SPDX-License-Identifier: GPL-2.0-only

#include "Plasma3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(Plasma3D);
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
    info.info_version = 2;  // Using new standardized system
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

    // Standardized parameter scaling
    info.default_speed_scale = 8.0f;        // (speed/100)² * 8.0 (room-scale motion)
    info.default_frequency_scale = 8.0f;    // (freq/100)² * 8.0 (larger features)
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
    pattern_combo->setToolTip("Plasma pattern variant");
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

    /*---------------------------------------------------------*\
    | Calculate primary and secondary coordinates based on axis|
    \*---------------------------------------------------------*/
    float coord1, coord2, coord3;
    switch(effect_axis)
    {
        case AXIS_X:  // Pattern primarily on YZ plane (perpendicular to X - Left/Right axis)
            coord1 = rel_y;
            coord2 = rel_z;
            coord3 = rel_x;
            break;
        case AXIS_Y:  // Pattern primarily on XZ plane (perpendicular to Y - Bottom/Top axis, Y-up)
            coord1 = rel_x;
            coord2 = rel_z;
            coord3 = rel_y;
            break;
        case AXIS_Z:  // Pattern primarily on XY plane (perpendicular to Z - Front/Back axis)
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
    float size_multiplier = GetNormalizedSize();  // 0.1 to 2.0
    float scale = actual_frequency * 0.004f / size_multiplier;  // Room-scale features across the space

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
                    sin(sqrtf(coord1*coord1 + coord2*coord2) * scale * 0.5f + progress * 1.5f) +
                    cos(coord3 * scale * 0.4f + progress * 0.9f);
            }
            break;

        case 1: // Swirl Plasma - Rotating spiral patterns
            {
                float angle = atan2(coord2, coord1);
                float radius = sqrtf(coord1*coord1 + coord2*coord2);

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
                    dist_from_center = sqrtf(coord1*coord1 + coord2*coord2 + coord3*coord3);
                }
                else
                {
                    dist_from_center = sqrtf(coord1*coord1 + coord2*coord2);
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

    // Global brightness is applied by PostProcessColorGrid
    return final_color;
}

// Grid-aware version with room-scale feature sizing
RGBColor Plasma3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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
    float freq_scale = actual_frequency * 0.8f / fmax(0.1f, size_multiplier);

    // Normalize coordinates to 0-1 range based on room bounds
    // This ensures ALL controllers see the same plasma pattern at the same absolute room position
    float norm_x = (grid.width > 0.001f) ? ((x - grid.min_x) / grid.width) : 0.0f;
    float norm_y = (grid.height > 0.001f) ? ((y - grid.min_y) / grid.height) : 0.0f;
    float norm_z = (grid.depth > 0.001f) ? ((z - grid.min_z) / grid.depth) : 0.0f;
    norm_x = fmaxf(0.0f, fminf(1.0f, norm_x));
    norm_y = fmaxf(0.0f, fminf(1.0f, norm_y));
    norm_z = fmaxf(0.0f, fminf(1.0f, norm_z));

    float coord1, coord2, coord3;
    switch(effect_axis)
    {
        case AXIS_X:  coord1 = norm_y; coord2 = norm_z; coord3 = norm_x; break;
        case AXIS_Y:  coord1 = norm_x; coord2 = norm_z; coord3 = norm_y; break;
        case AXIS_Z:  coord1 = norm_x; coord2 = norm_y; coord3 = norm_z; break;
        case AXIS_RADIAL: default: 
        {
            // For radial, use normalized distance from origin
            float max_distance = sqrtf(grid.width*grid.width + grid.height*grid.height + grid.depth*grid.depth) / 2.0f;
            float radial_dist = sqrtf(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            float norm_radial = (max_distance > 0.001f) ? (radial_dist / max_distance) : 0.0f;
            norm_radial = fmaxf(0.0f, fminf(1.0f, norm_radial));
            // Use angle for coord1/coord2, radial distance for coord3
            float angle = atan2(rel_y, rel_x);
            coord1 = (angle + 3.14159f) / (2.0f * 3.14159f); // Normalize angle to 0-1
            coord2 = norm_radial;
            coord3 = norm_radial;
            break;
        }
    }
    if(effect_reverse) coord3 = 1.0f - coord3;

    float plasma_value;
    switch(pattern_type)
    {
        case 0: // Classic
            plasma_value =
                sin((coord1 + progress * 2.0f) * freq_scale * 10.0f) +
                sin((coord2 + progress * 1.7f) * freq_scale * 8.0f) +
                sin((coord1 + coord2 + progress * 1.3f) * freq_scale * 6.0f) +
                cos((coord1 - coord2 + progress * 2.2f) * freq_scale * 7.0f) +
                sin(sqrtf(coord1*coord1 + coord2*coord2) * freq_scale * 5.0f + progress * 1.5f) +
                cos(coord3 * freq_scale * 4.0f + progress * 0.9f);
            break;
        case 1: // Swirl
            {
                float angle = atan2(coord2 - 0.5f, coord1 - 0.5f); // Center at 0.5, 0.5
                float radius = sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f));
                plasma_value =
                    sin(angle * 4.0f + radius * freq_scale * 8.0f + progress * 2.0f) +
                    sin(angle * 3.0f - radius * freq_scale * 6.0f + progress * 1.5f) +
                    cos(angle * 5.0f + radius * freq_scale * 4.0f - progress * 1.8f) +
                    sin(coord3 * freq_scale * 5.0f + progress) +
                    cos((angle * 2.0f + coord3 * freq_scale * 3.0f) + progress * 1.2f);
            }
            break;
        case 2: // Ripple
            {
                float dist_from_center = (effect_axis == AXIS_RADIAL)
                    ? sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f) + (coord3 - 0.5f)*(coord3 - 0.5f))
                    : sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f));
                plasma_value =
                    sin(dist_from_center * freq_scale * 10.0f - progress * 3.0f) +
                    sin(dist_from_center * freq_scale * 15.0f - progress * 2.3f) +
                    cos(dist_from_center * freq_scale * 8.0f + progress * 1.8f) +
                    sin((coord1 + coord2) * freq_scale * 6.0f + progress * 1.2f) +
                    cos(coord3 * freq_scale * 5.0f - progress * 0.7f);
            }
            break;
        case 3: // Organic
        default:
            {
                float flow1 = sin(coord1 * freq_scale * 8.0f + sin(coord2 * freq_scale * 12.0f + progress) + progress * 0.5f);
                float flow2 = cos(coord2 * freq_scale * 9.0f + cos(coord3 * freq_scale * 11.0f + progress * 1.3f));
                float flow3 = sin(coord3 * freq_scale * 7.0f + sin(coord1 * freq_scale * 13.0f + progress * 0.7f));
                float flow4 = cos((coord1 + coord2) * freq_scale * 6.0f + sin(progress * 1.5f));
                float flow5 = sin((coord2 + coord3) * freq_scale * 5.0f + cos(progress * 1.8f));
                plasma_value = flow1 + flow2 + flow3 + flow4 + flow5;
            }
            break;
    }

    plasma_value = (plasma_value + 6.0f) / 12.0f;
    plasma_value = fmax(0.0f, fmin(1.0f, plasma_value));

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(plasma_value * 360.0f + progress * 60.0f)
                                            : GetColorAtPosition(plasma_value);
    // Global brightness is applied by PostProcessColorGrid
    return final_color;
}

nlohmann::json Plasma3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["pattern_type"] = pattern_type;
    return j;
}

void Plasma3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("pattern_type")) pattern_type = settings["pattern_type"];

    if(pattern_combo) pattern_combo->setCurrentIndex(pattern_type);
}
