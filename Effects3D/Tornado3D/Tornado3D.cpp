/*---------------------------------------------------------*\
| Tornado3D.cpp                                            |
|                                                          |
|   Room-scale vortex/tornado effect                       |
|                                                          |
|   SPDX-License-Identifier: GPL-2.0-only                  |
\*---------------------------------------------------------*/

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
    info.show_axis_control = false;
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
    layout->addWidget(core_radius_slider, 0, 1);

    layout->addWidget(new QLabel("Height:"), 1, 0);
    height_slider = new QSlider(Qt::Horizontal);
    height_slider->setRange(50, 500);
    height_slider->setValue(tornado_height);
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

    // Funnel radius increases with height
    float h_norm = fmax(0.0f, fmin(1.0f, (rel_y + tornado_height * 0.005f) / (tornado_height * 0.01f + 0.001f)));
    float funnel_radius = (core_radius * 0.02f + h_norm * core_radius * 0.03f) * size_m;

    // Swirl angle depends on height and time (twist)
    float angle = atan2(rel_z, rel_x);
    float radius = sqrt(rel_x*rel_x + rel_z*rel_z);
    float swirl = angle + rel_y * (0.05f * freq) - time * speed * 0.2f;

    // Distance to the funnel wall (ring)
    float ring = fabsf(radius - funnel_radius);
    float ring_thickness = 0.6f + 0.8f * size_m;
    float ring_intensity = fmax(0.0f, 1.0f - ring / ring_thickness);

    // Add azimuthal banding to suggest rotation arms
    float arms = 4.0f + 4.0f * size_m;
    float band = 0.5f * (1.0f + cosf(swirl * arms));

    // Vertical fade outside active height
    float y_fade = fmax(0.0f, 1.0f - fabsf(rel_y) / (tornado_height * 0.01f + 0.001f));

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
    float brightness_factor = (GetBrightness() / 100.0f) * intensity;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);
    return (b << 16) | (g << 8) | r;
}

