// SPDX-License-Identifier: GPL-2.0-only

#include "Tornado3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(Tornado3D);

Tornado3D::Tornado3D(QWidget* parent) : SpatialEffect3D(parent)
{
    core_radius_slider = nullptr;
    height_slider = nullptr;
    core_radius = 80;
    tornado_height = 250;
    SetRainbowMode(true);
    SetFrequency(50);
}

Tornado3D::~Tornado3D()
{
}

EffectInfo3D Tornado3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Tornado";
    info.effect_description = "Vortex swirl rising around the origin";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_TORNADO;
    info.is_reversible = true;
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

    info.default_speed_scale = 25.0f;  // rotation speed
    info.default_frequency_scale = 6.0f;  // twist density
    info.use_size_parameter = true;

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

void Tornado3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Core Radius:"), 0, 0);
    core_radius_slider = new QSlider(Qt::Horizontal);
    core_radius_slider->setRange(20, 300);
    core_radius_slider->setValue(core_radius);
    core_radius_slider->setToolTip("Tornado core radius (affects base funnel size)");
    layout->addWidget(core_radius_slider, 0, 1);

    layout->addWidget(new QLabel("Height:"), 1, 0);
    height_slider = new QSlider(Qt::Horizontal);
    height_slider->setRange(50, 500);
    height_slider->setValue(tornado_height);
    height_slider->setToolTip("Tornado height (relative to room height)");
    layout->addWidget(height_slider, 1, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(w);
    }

    connect(core_radius_slider, &QSlider::valueChanged, this, &Tornado3D::OnTornadoParameterChanged);
    connect(height_slider, &QSlider::valueChanged, this, &Tornado3D::OnTornadoParameterChanged);
}

void Tornado3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_TORNADO;
}

void Tornado3D::OnTornadoParameterChanged()
{
    if(core_radius_slider) core_radius = core_radius_slider->value();
    if(height_slider) tornado_height = height_slider->value();
    emit ParametersChanged();
}

RGBColor Tornado3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor Tornado3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    float speed = GetScaledSpeed();
    float freq = GetScaledFrequency();
    float size_m = GetNormalizedSize();

    // Room-aware height and radius scaling
    // Use absolute world coordinates for normalization to ensure synchronization across controllers
    float axial = 0.0f;
    EffectAxis use_axis = axis_none ? AXIS_Y : effect_axis;

    // Normalize using absolute world coordinates, not relative coordinates
    // This ensures ALL controllers see the same tornado pattern at the same absolute room position
    switch(use_axis)
    {
        case AXIS_X: 
            axial = (grid.width > 0.001f) ? ((x - grid.min_x) / grid.width) : 0.0f;
            break;
        case AXIS_Y: 
            axial = (grid.height > 0.001f) ? ((y - grid.min_y) / grid.height) : 0.0f;
            break;
        case AXIS_Z: 
            axial = (grid.depth > 0.001f) ? ((z - grid.min_z) / grid.depth) : 0.0f;
            break;
        case AXIS_RADIAL: 
        default: 
            // For radial, normalize height position
            axial = (grid.height > 0.001f) ? ((y - grid.min_y) / grid.height) : 0.0f;
            break;
    }
    axial = fmaxf(0.0f, fminf(1.0f, axial));
    
    // Map normalized axial position to tornado height range
    float height_center = 0.5f;
    float height_range = (tornado_height / 500.0f) * 0.5f; // 0 to 0.5 range
    float h_norm = fmax(0.0f, fmin(1.0f, (axial - (height_center - height_range)) / (2.0f * height_range + 0.0001f)));
    float base_radius = 0.5f * fmin(grid.width, grid.depth); // half of min horizontal span
    // core_radius (20..300) maps to ~4%..60% of base, grows with height
    float core_scale = 0.04f + (core_radius / 300.0f) * 0.56f;
    float funnel_radius = (base_radius * core_scale) * (0.6f + 0.4f * h_norm) * size_m;

    // Swirl angle depends on height and time (twist)
    // Axis selection for rotation: default Y
    float a = 0.0f, rad = 0.0f, along = 0.0f;

    switch(use_axis)
    {
        case AXIS_X: a = atan2f(rel_z, rel_y); rad = sqrtf(rel_y*rel_y + rel_z*rel_z); along = rel_x; break;
        case AXIS_Y: a = atan2f(rel_z, rel_x); rad = sqrtf(rel_x*rel_x + rel_z*rel_z); along = rel_y; break;
        case AXIS_Z: a = atan2f(rel_y, rel_x); rad = sqrtf(rel_x*rel_x + rel_y*rel_y); along = rel_z; break;
        case AXIS_RADIAL:
        default:
            a = atan2f(rel_z, rel_x); rad = sqrtf(rel_x*rel_x + rel_z*rel_z); along = rel_y; break;
    }
    float swirl = a + along * (0.015f * freq) - time * speed * 0.25f;

    // Distance to the funnel wall (ring)
    float ring = fabsf(rad - funnel_radius);
    // Thickness scales with room size to remain visible
    float thickness_base = (grid.width + grid.depth) * 0.01f;
    float ring_thickness = thickness_base * (0.6f + 1.2f * size_m);
    float ring_intensity = fmax(0.0f, 1.0f - ring / ring_thickness);

    // Add azimuthal banding to suggest rotation arms
    float arms = 4.0f + 4.0f * size_m;
    float band = 0.5f * (1.0f + cosf(swirl * arms));

    // Vertical fade outside active height (using normalized axial position)
    // Reuse height_range calculated above
    float y_fade = fmax(0.0f, 1.0f - fabsf(axial - 0.5f) / (height_range + 0.001f));

    float intensity = ring_intensity * (0.5f + 0.5f * band) * y_fade;
    intensity = fmax(0.0f, fmin(1.0f, intensity));

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = 200.0f + swirl * 57.2958f * 0.2f + h_norm * 80.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(0.5f + 0.5f * intensity);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    // Apply intensity (global brightness is applied by PostProcessColorGrid)
    r = (unsigned char)(r * intensity);
    g = (unsigned char)(g * intensity);
    b = (unsigned char)(b * intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Tornado3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["core_radius"] = core_radius;
    j["tornado_height"] = tornado_height;
    return j;
}

void Tornado3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("core_radius")) core_radius = settings["core_radius"];
    if(settings.contains("tornado_height")) tornado_height = settings["tornado_height"];

    if(core_radius_slider) core_radius_slider->setValue(core_radius);
    if(height_slider) height_slider->setValue(tornado_height);
}
