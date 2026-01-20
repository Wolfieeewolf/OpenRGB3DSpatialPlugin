// SPDX-License-Identifier: GPL-2.0-only
// SPDX-License-Identifier: GPL-2.0-only

#include "Wipe3D.h"

REGISTER_EFFECT_3D(Wipe3D);
#include <QGridLayout>
#include <cmath>

Wipe3D::Wipe3D(QWidget* parent) : SpatialEffect3D(parent)
{
    thickness_slider = nullptr;
    shape_combo = nullptr;
    wipe_thickness = 20;     // Default thickness
    edge_shape = 0;          // Default to round edges
    progress = 0.0f;

    // Set default wipe colors (red to white)
    std::vector<RGBColor> wipe_colors = {
        0x000000FF,  // Red
        0x00FFFFFF   // White
    };
    if(GetColors().empty())
    {
        SetColors(wipe_colors);
    }
    SetRainbowMode(false);   // Default to custom colors
}

EffectInfo3D Wipe3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;  // Using new standardized system
    info.effect_name = "3D Wipe";
    info.effect_description = "Progressive sweep effect with configurable thickness and edge shapes";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WIPE;
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
    info.default_speed_scale = 2.0f;        // speed/100 * 2.0
    info.default_frequency_scale = 10.0f;
    info.use_size_parameter = true;

    // Control visibility (show all controls except frequency)
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

void Wipe3D::SetupCustomUI(QWidget* parent)
{
    QWidget* wipe_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(wipe_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    // Wipe-specific controls
    layout->addWidget(new QLabel("Thickness:"), 0, 0);
    thickness_slider = new QSlider(Qt::Horizontal);
    thickness_slider->setRange(5, 100);
    thickness_slider->setValue(wipe_thickness);
    thickness_slider->setToolTip("Wipe edge thickness (higher = wider edge)");
    layout->addWidget(thickness_slider, 0, 1);

    layout->addWidget(new QLabel("Edge Shape:"), 1, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Round");
    shape_combo->addItem("Sharp");
    shape_combo->addItem("Square");
    shape_combo->setCurrentIndex(edge_shape);
    shape_combo->setToolTip("Wipe edge profile");
    layout->addWidget(shape_combo, 1, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(wipe_widget);
    }

    // Connect signals
    connect(thickness_slider, &QSlider::valueChanged, this, &Wipe3D::OnWipeParameterChanged);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Wipe3D::OnWipeParameterChanged);
}

void Wipe3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WIPE;
}

void Wipe3D::OnWipeParameterChanged()
{
    if(thickness_slider) wipe_thickness = thickness_slider->value();
    if(shape_combo) edge_shape = shape_combo->currentIndex();
    emit ParametersChanged();
}

RGBColor Wipe3D::CalculateColor(float x, float y, float z, float time)
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
    progress = fmod(CalculateProgress(time), 2.0f);
    if(progress > 1.0f) progress = 2.0f - progress;

    /*---------------------------------------------------------*\
    | Calculate position based on selected axis               |
    \*---------------------------------------------------------*/
    float position;
    switch(effect_axis)
    {
        case AXIS_X:  // Left to Right wipe
            position = rel_x;
            break;
        case AXIS_Y:  // Bottom to Top wipe (Y-up)
            position = rel_y;
            break;
        case AXIS_Z:  // Front to Back wipe
        default:
            position = rel_z;
            break;
        case AXIS_RADIAL:  // Radial wipe from center
            position = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            break;
    }

    /*---------------------------------------------------------*\
    | Apply reverse if enabled                                 |
    \*---------------------------------------------------------*/
    if(effect_reverse)
    {
        position = -position;
    }

    /*---------------------------------------------------------*\
    | Normalize position to 0-1 range                          |
    \*---------------------------------------------------------*/
    position = (position + 100.0f) / 200.0f;
    position = fmax(0.0f, fmin(1.0f, position));

    // Calculate wipe edge with thickness
    float edge_distance = fabs(position - progress);
    float thickness_factor = wipe_thickness / 100.0f;

    float intensity;
    switch(edge_shape)
    {
        case 0: // Round
            intensity = 1.0f - smoothstep(0.0f, thickness_factor, edge_distance);
            break;
        case 1: // Sharp
            intensity = edge_distance < thickness_factor * 0.5f ? 1.0f : 0.0f;
            break;
        case 2: // Square
        default:
            intensity = edge_distance < thickness_factor ? 1.0f : 0.0f;
            break;
    }

    // Get color using universal base class methods
    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = progress * 360.0f + time * 30.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(progress);
    }

    // Apply intensity (global brightness is applied by PostProcessColorGrid)
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    r = (unsigned char)(r * intensity);
    g = (unsigned char)(g * intensity);
    b = (unsigned char)(b * intensity);

    return (b << 16) | (g << 8) | r;
}

RGBColor Wipe3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | NOTE: All coordinates (x, y, z) are in GRID UNITS       |
    | One grid unit equals the configured grid scale          |
    | (default 10mm). GridContext3D uses the same units.      |
    |                                                          |
    | Since RequiresWorldSpaceCoordinates() returns true,     |
    | these are WORLD coordinates (world_position), which     |
    | include controller rotation. This is CRITICAL for       |
    | room-based effects:                                      |
    |                                                          |
    | - World coordinates position LEDs in room space after   |
    |   rotation is applied                                    |
    | - A keyboard rotated 90° will have its LEDs positioned  |
    |   in world space according to that rotation             |
    |                                                          |
    | IMPORTANT: The 'grid' parameter passed here is world_grid|
    | (based on world_bounds), but for room-based effects we  |
    | need to normalize by room bounds. However, since we    |
    | only receive one grid context, we'll use the grid       |
    | bounds we receive, which should represent the actual    |
    | extent of LEDs in world space (room space after        |
    | rotation). This gives us room-absolute wipe direction.  |
    \*---------------------------------------------------------*/

    /*---------------------------------------------------------*\
    | For room-based effects, we use absolute world coordinates|
    | and normalize directly by room bounds. This matches the   |
    | old working version's approach.                           |
    |                                                          |
    | The old version:                                         |
    | - Uses world_position directly (includes rotation)        |
    | - Normalizes with fixed range: (position + 100) / 200    |
    |                                                          |
    | The new version should:                                  |
    | - Use world_position directly (includes rotation)        |
    | - Normalize by room bounds: (x - grid.min_x) / grid.width|
    \*---------------------------------------------------------*/

    /*---------------------------------------------------------*\
    | Check if LED is within scaled effect radius             |
    | Calculate relative position for boundary check           |
    \*---------------------------------------------------------*/
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;  // Black - outside effect boundary
    }

    /*---------------------------------------------------------*\
    | Update progress                                          |
    \*---------------------------------------------------------*/
    progress = fmod(CalculateProgress(time), 2.0f);
    if(progress > 1.0f) progress = 2.0f - progress;

    /*---------------------------------------------------------*\
    | Calculate position based on axis                         |
    |                                                          |
    | ROOM-BASED EFFECT: Use absolute world coordinates and   |
    | normalize by room bounds. This ensures the wipe goes    |
    | left-to-right in room space regardless of controller     |
    | rotation.                                                 |
    |                                                          |
    | World coordinates (x, y, z) already include rotation,   |
    | so LEDs are positioned correctly in room space.          |
    | Normalizing by room bounds gives us room-absolute       |
    | position [0, 1] from left wall to right wall.            |
    \*---------------------------------------------------------*/
    
    float position;
    switch(effect_axis)
    {
        case AXIS_X:  // Left wall to Right wall wipe
            // Use absolute X coordinate, normalize by room width
            // Maps from [grid.min_x, grid.max_x] to [0, 1]
            if(grid.width > 0.001f)
            {
                position = (x - grid.min_x) / grid.width;
            }
            else
            {
                position = 0.0f;
            }
            break;
        case AXIS_Y:  // Floor to Ceiling wipe
            // Use absolute Y coordinate, normalize by room height
            // Maps from [grid.min_y, grid.max_y] to [0, 1]
            if(grid.height > 0.001f)
            {
                position = (y - grid.min_y) / grid.height;
            }
            else
            {
                position = 0.0f;
            }
            break;
        case AXIS_Z:  // Front to Back wipe
            // Use absolute Z coordinate, normalize by room depth
            // Maps from [grid.min_z, grid.max_z] to [0, 1]
            if(grid.depth > 0.001f)
            {
                position = (z - grid.min_z) / grid.depth;
            }
            else
            {
                position = 0.0f;
            }
            break;
        case AXIS_RADIAL:  // Radial wipe from room center
        default:
            {
                // Distance from room center (origin)
                float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
                // Normalize to room diagonal (maximum possible distance from center)
                float max_distance = sqrt(grid.width*grid.width +
                                        grid.height*grid.height +
                                        grid.depth*grid.depth) / 2.0f;
                if(max_distance > 0.001f)
                {
                    position = distance / max_distance;
                }
                else
                {
                    position = 0.0f;
                }
            }
            break;
    }
    
    // Clamp to valid range [0, 1]
    position = fmaxf(0.0f, fminf(1.0f, position));

    /*---------------------------------------------------------*\
    | Apply reverse if enabled                                 |
    \*---------------------------------------------------------*/
    if(effect_reverse)
    {
        position = 1.0f - position;
    }

    // Position is now 0.0-1.0, no need to clamp
    // Calculate wipe edge with thickness
    float edge_distance = fabs(position - progress);
    float thickness_factor = wipe_thickness / 100.0f;

    float intensity;
    switch(edge_shape)
    {
        case 0: // Round
            intensity = 1.0f - smoothstep(0.0f, thickness_factor, edge_distance);
            break;
        case 1: // Sharp
            intensity = edge_distance < thickness_factor * 0.5f ? 1.0f : 0.0f;
            break;
        case 2: // Square
        default:
            intensity = edge_distance < thickness_factor ? 1.0f : 0.0f;
            break;
    }

    // Get color
    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = progress * 360.0f + time * 30.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(progress);
    }

    // Apply intensity (global brightness is applied by PostProcessColorGrid)
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    r = (unsigned char)(r * intensity);
    g = (unsigned char)(g * intensity);
    b = (unsigned char)(b * intensity);

    return (b << 16) | (g << 8) | r;
}

nlohmann::json Wipe3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["wipe_thickness"] = wipe_thickness;
    j["edge_shape"] = edge_shape;
    return j;
}

void Wipe3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("wipe_thickness")) wipe_thickness = settings["wipe_thickness"];
    if(settings.contains("edge_shape")) edge_shape = settings["edge_shape"];

    if(thickness_slider) thickness_slider->setValue(wipe_thickness);
    if(shape_combo) shape_combo->setCurrentIndex(edge_shape);
}

// Helper function for smooth interpolation
float Wipe3D::smoothstep(float edge0, float edge1, float x)
{
    float t = fmax(0.0f, fmin(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}
