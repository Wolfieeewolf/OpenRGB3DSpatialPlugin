// SPDX-License-Identifier: GPL-2.0-only

#include "Wipe3D.h"

REGISTER_EFFECT_3D(Wipe3D);
#include <QGridLayout>
#include <cmath>

Wipe3D::Wipe3D(QWidget* parent) : SpatialEffect3D(parent)
{
    thickness_slider = nullptr;
    thickness_label = nullptr;
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
    // Rotation controls are in base class
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
    thickness_label = new QLabel(QString::number(wipe_thickness));
    thickness_label->setMinimumWidth(30);
    layout->addWidget(thickness_label, 0, 2);

    layout->addWidget(new QLabel("Edge Shape:"), 1, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Round");
    shape_combo->addItem("Sharp");
    shape_combo->addItem("Square");
    shape_combo->setCurrentIndex(edge_shape);
    shape_combo->setToolTip("Wipe edge profile");
    layout->addWidget(shape_combo, 1, 1);

    AddWidgetToParent(wipe_widget, parent);

    // Connect signals
    connect(thickness_slider, &QSlider::valueChanged, this, &Wipe3D::OnWipeParameterChanged);
    connect(thickness_slider, &QSlider::valueChanged, thickness_label, [this](int value) {
        thickness_label->setText(QString::number(value));
    });
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Wipe3D::OnWipeParameterChanged);
}

void Wipe3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WIPE;
}

void Wipe3D::OnWipeParameterChanged()
{
    if(thickness_slider)
    {
        wipe_thickness = thickness_slider->value();
        if(thickness_label) thickness_label->setText(QString::number(wipe_thickness));
    }
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
    | Apply rotation transformation to LED position            |
    | This rotates the effect pattern around the origin       |
    \*---------------------------------------------------------*/
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;

    /*---------------------------------------------------------*\
    | Calculate position based on rotated coordinates         |
    | Wipe moves along rotated X-axis by default              |
    \*---------------------------------------------------------*/
    float position = rot_rel_x;

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
    | IMPORTANT: The 'grid' parameter passed here is selected |
    | by the renderer based on effect requirements. For most  |
    | room-locked effects we normalize against room-aligned   |
    | bounds (stable) even when sampling world coordinates.   |
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
    | Apply rotation transformation to LED position            |
    | This rotates the effect pattern around the origin       |
    \*---------------------------------------------------------*/
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    /*---------------------------------------------------------*\
    | Calculate position based on rotated coordinates         |
    | Wipe moves along rotated X-axis by default              |
    \*---------------------------------------------------------*/
    float position;
    if(grid.width > 0.001f)
    {
        position = (rot_rel_x + grid.width * 0.5f) / grid.width;
    }
    else
    {
        position = 0.0f;
    }
    
    // For radial wipe, use distance from origin
    // (Uncomment if you want radial option)
    /*
    {
        // Distance from room center (origin)
        float distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
        // Normalize to room diagonal (maximum possible distance from center)
        float max_distance = sqrtf(grid.width*grid.width +
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
    */
    
    // Clamp to valid range [0, 1]
    position = fmaxf(0.0f, fminf(1.0f, position));

    // Position is now 0.0-1.0, no need to clamp
    // Calculate wipe edge with thickness
    float edge_distance = fabs(position - progress);
    float thickness_factor = wipe_thickness / 100.0f;

    float intensity;
    switch(edge_shape)
    {
        case 0: // Round - enhanced with glow
            {
                float core = 1.0f - smoothstep(0.0f, thickness_factor * 0.6f, edge_distance);
                float glow = 0.4f * (1.0f - smoothstep(thickness_factor * 0.6f, thickness_factor * 1.2f, edge_distance));
                intensity = fmin(1.0f, core + glow);
            }
            break;
        case 1: // Sharp
            intensity = edge_distance < thickness_factor * 0.5f ? 1.0f : 0.0f;
            break;
        case 2: // Square
        default:
            intensity = edge_distance < thickness_factor ? 1.0f : 0.0f;
            break;
    }

    // Add depth-based enhancement for 3D immersion
    float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    float max_radius = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
    float depth_factor = 1.0f;
    if(max_radius > 0.001f)
    {
        float normalized_dist = fmin(1.0f, radial_distance / max_radius);
        // Soft distance fade - keeps wipe visible across whole room
        depth_factor = 0.5f + 0.5f * (1.0f - normalized_dist * 0.5f);
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

    // Apply intensity with depth factor (global brightness is applied by PostProcessColorGrid)
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    r = (unsigned char)(r * intensity * depth_factor);
    g = (unsigned char)(g * intensity * depth_factor);
    b = (unsigned char)(b * intensity * depth_factor);

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
