// SPDX-License-Identifier: GPL-2.0-only

#include "Wave3D.h"


// Register this effect with the effect manager
REGISTER_EFFECT_3D(Wave3D);
#include <QGridLayout>
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
    // Rotation controls are in base class
    info.show_color_controls = true;

    return info;
}

void Wave3D::SetupCustomUI(QWidget* parent)
{
    // Create Wave-specific controls (base has axis/speed/etc)
    QWidget* wave_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(wave_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    // Row 0: Shape (like RadialRainbow)
    layout->addWidget(new QLabel("Shape:"), 0, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Circles");
    shape_combo->addItem("Squares");
    shape_combo->setCurrentIndex(shape_type);
    layout->addWidget(shape_combo, 0, 1);

    // Add to parent layout
    AddWidgetToParent(wave_widget, parent);

    // Connect signals
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave3D::OnWaveParameterChanged);
}

void Wave3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void Wave3D::OnWaveParameterChanged()
{
    // Update internal parameters
    if(shape_combo) shape_type = shape_combo->currentIndex();

    // Emit parameter change signalemit ParametersChanged();
}

RGBColor Wave3D::CalculateColor(float x, float y, float z, float time)
{
    // Get effect origin (room center or user head position)
    Vector3D origin = GetEffectOrigin();

    // Calculate position relative to origin
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

    // Update progress for animation using universal helper
    progress = CalculateProgress(time);

    /*---------------------------------------------------------*\
    | Apply rotation transformation to LED position            |
    | This rotates the effect pattern around the origin       |
    \*---------------------------------------------------------*/
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    /*---------------------------------------------------------*\
    | Calculate wave based on rotated coordinates             |
    | Wave propagates along rotated X-axis by default         |
    \*---------------------------------------------------------*/
    float wave_value = 0.0f;
    float size_multiplier = GetNormalizedSize();  // 0.1 to 2.0 (affects spatial frequency)
    float freq_scale = actual_frequency * 0.1f / size_multiplier;  // Larger size = more spread out
    float position = 0.0f;

    /*---------------------------------------------------------*\
    | For radial waves, use distance from origin              |
    | For directional waves, use rotated X coordinate         |
    \*---------------------------------------------------------*/
    if(shape_type == 0)  // Radial (Sphere)
    {
        position = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    }
    else  // Directional (use rotated X-axis)
    {
        position = rot_rel_x;
    }

    wave_value = sin(position * freq_scale - progress);

    // Convert wave to hue (0-360 degrees)
    float hue = (wave_value + 1.0f) * 180.0f;
    hue = fmod(hue, 360.0f);
    if(hue < 0.0f) hue += 360.0f;

    // Get color based on mode
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

    // Global brightness is applied by PostProcessColorGrid
    return final_color;
}

RGBColor Wave3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | NOTE: All coordinates (x, y, z) are in GRID UNITS       |
    | One grid unit equals the configured grid scale          |
    | (default 10mm). GridContext3D uses the same units.      |
    \*---------------------------------------------------------*/

    /*---------------------------------------------------------*\
    | Get effect origin using grid-aware helper               |
    | Automatically uses grid.center for room center mode     |
    \*---------------------------------------------------------*/
    Vector3D origin = GetEffectOriginGrid(grid);

    // Calculate position relative to origin
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    /*---------------------------------------------------------*\
    | Check if LED is within scaled effect radius             |
    | Uses room-aware boundary checking                        |
    \*---------------------------------------------------------*/
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;  // Black - outside effect boundary
    }

    // Use standardized parameter helpers
    float actual_frequency = GetScaledFrequency();

    // Update progress for animation
    progress = CalculateProgress(time);

    /*---------------------------------------------------------*\
    | Apply rotation transformation to LED position            |
    | This rotates the effect pattern around the origin       |
    \*---------------------------------------------------------*/
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    /*---------------------------------------------------------*\
    | Calculate wave based on rotated coordinates             |
    | IMPORTANT: Normalize position to 0-1 for consistent     |
    | wave density regardless of room size                     |
    \*---------------------------------------------------------*/
    float wave_value = 0.0f;
    float size_multiplier = GetNormalizedSize();
    float freq_scale = actual_frequency * 0.1f / size_multiplier;
    float normalized_position = 0.0f;

    /*---------------------------------------------------------*\
    | For radial waves, use distance from origin              |
    | For directional waves, use rotated X coordinate         |
    \*---------------------------------------------------------*/
    if(shape_type == 0)  // Radial (Sphere)
    {
        float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
        float max_radius = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
        if(max_radius > 0.001f)
        {
            normalized_position = radial_distance / max_radius;
        }
        else
        {
            normalized_position = 0.0f;
        }
    }
    else  // Directional (use rotated X-axis)
    {
        if(grid.width > 0.001f)
        {
            // Normalize rotated X position against room width
            normalized_position = (rot_rel_x + grid.width * 0.5f) / grid.width;
        }
        else
        {
            normalized_position = 0.0f;
        }
    }

    // Clamp to valid range [0, 1] to avoid NaNs/inf propagating into sin()
    normalized_position = fmaxf(0.0f, fminf(1.0f, normalized_position));

    // Use normalized position (0-1) directly - it's already normalized across the entire room
    // This ensures ALL controllers see the same wave pattern at the same absolute room position
    // freq_scale already accounts for frequency, so we don't need room-size scaling
    wave_value = sin(normalized_position * freq_scale * 10.0f - progress);

    // Add depth-based enhancement for 3D immersion
    float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    float max_radius = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
    float depth_factor = 1.0f;
    if(max_radius > 0.001f)
    {
        // Soft distance fade - keeps effect visible across whole room
        float normalized_dist = fmin(1.0f, radial_distance / max_radius);
        depth_factor = 0.4f + 0.6f * (1.0f - normalized_dist * 0.7f); // Gentle fade, never fully black
    }

    // Enhance wave with secondary harmonics for richer visuals
    float wave_enhanced = wave_value * 0.7f + 0.3f * sin(normalized_position * freq_scale * 20.0f - progress * 1.5f);
    wave_enhanced = fmax(-1.0f, fmin(1.0f, wave_enhanced));

    // Convert wave to hue (0-360 degrees)
    float hue = (wave_enhanced + 1.0f) * 180.0f;
    hue = fmod(hue, 360.0f);
    if(hue < 0.0f) hue += 360.0f;

    // Get color based on mode
    RGBColor final_color;

    if(GetRainbowMode())
    {
        final_color = GetRainbowColor(hue);
    }
    else
    {
        float color_position = hue / 360.0f;
        final_color = GetColorAtPosition(color_position);
    }

    // Apply depth factor for immersive 3D feel
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * depth_factor);
    g = (unsigned char)(g * depth_factor);
    b = (unsigned char)(b * depth_factor);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Wave3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["shape_type"] = shape_type;
    return j;
}

void Wave3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("shape_type"))
    {
        shape_type = settings["shape_type"].get<int>();
        if(shape_combo)
        {
            shape_combo->setCurrentIndex(shape_type);
        }
    }
}
