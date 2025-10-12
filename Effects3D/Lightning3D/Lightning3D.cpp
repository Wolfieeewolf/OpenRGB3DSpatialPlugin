/*---------------------------------------------------------*\
| Lightning3D.cpp                                          |
|                                                          |
|   Room-scale lightning strike effect                     |
|                                                          |
|   SPDX-License-Identifier: GPL-2.0-only                  |
\*---------------------------------------------------------*/

#include "Lightning3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(Lightning3D);

Lightning3D::Lightning3D(QWidget* parent) : SpatialEffect3D(parent)
{
    strike_rate_slider = nullptr;
    branch_slider = nullptr;
    strike_rate = 10; // ~10 per min baseline
    branches = 3;
    SetRainbowMode(false);
}

Lightning3D::~Lightning3D()
{
}

EffectInfo3D Lightning3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Lightning";
    info.effect_description = "Lightning strikes with branching and flash";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_LIGHTNING;
    info.is_reversible = false;
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

    info.default_speed_scale = 40.0f; // flash timing speed
    info.default_frequency_scale = 10.0f; // branching density influence
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

void Lightning3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Strike Rate (per min):"), 0, 0);
    strike_rate_slider = new QSlider(Qt::Horizontal);
    strike_rate_slider->setRange(1, 60);
    strike_rate_slider->setValue(strike_rate);
    layout->addWidget(strike_rate_slider, 0, 1);

    layout->addWidget(new QLabel("Branches:"), 1, 0);
    branch_slider = new QSlider(Qt::Horizontal);
    branch_slider->setRange(1, 8);
    branch_slider->setValue(branches);
    layout->addWidget(branch_slider, 1, 1);

    if(parent && parent->layout()) parent->layout()->addWidget(w);

    connect(strike_rate_slider, &QSlider::valueChanged, this, &Lightning3D::OnLightningParameterChanged);
    connect(branch_slider, &QSlider::valueChanged, this, &Lightning3D::OnLightningParameterChanged);
}

void Lightning3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_LIGHTNING;
}

void Lightning3D::OnLightningParameterChanged()
{
    if(strike_rate_slider) strike_rate = strike_rate_slider->value();
    if(branch_slider) branches = branch_slider->value();
    emit ParametersChanged();
}

static inline float prand3(int x, int y, int z)
{
    int n = x * 15731 ^ y * 789221 ^ z * 1376312589;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f) * 0.5f + 0.5f;
}

RGBColor Lightning3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor Lightning3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);

    // Global flash envelope based on strike chance
    float strikes_per_sec = strike_rate / 60.0f + GetScaledSpeed() * 0.05f;
    float phase = time * strikes_per_sec;
    float flash = fmax(0.0f, 1.0f - fmodf(phase, 1.0f) * 15.0f); // brief flash at start of each period

    // Main bolt path roughly from high Y toward origin with some lateral wobble
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    // Normalize space
    float height = grid.height > 0 ? grid.height : 10.0f;
    float y_norm = (rel_y + height * 0.5f) / (height + 0.001f); // 0..1 from bottom to top

    // Bolt center line parametric: lateral offset per height
    float wobble = sinf(y_norm * 40.0f + time * 10.0f) * (0.2f + 0.1f * branches);
    float cx = wobble * (grid.width * 0.1f);
    float cz = cosf(y_norm * 37.0f + time * 9.0f) * (0.2f + 0.1f * branches) * (grid.depth * 0.1f);

    float dx = rel_x - cx;
    float dz = rel_z - cz;
    float radial = sqrtf(dx*dx + dz*dz);

    // Core and glow falloff
    float core = fmax(0.0f, 1.0f - radial / (0.4f + 0.2f * branches));
    float glow = fmax(0.0f, 1.0f - radial / (1.0f + 0.4f * branches)) * 0.5f;
    float intensity = (core + glow) * flash;

    // Branching: modulate with angle bands to simulate branches
    float angle = atan2f(dz, dx);
    float bands = fabsf(cosf(angle * branches * 2.0f + time * 5.0f));
    intensity *= 0.7f + 0.3f * bands;

    intensity = fmax(0.0f, fmin(1.0f, intensity));

    // Lightning color or custom
    RGBColor final_color;
    if(GetRainbowMode())
    {
        final_color = GetRainbowColor(200.0f + flash * 160.0f);
    }
    else
    {
        // Bright white/blue tint
        final_color = ToRGBColor(180, 220, 255);
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

