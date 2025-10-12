/*---------------------------------------------------------*\
| BouncingBall3D.cpp                                       |
|                                                          |
|   Single bouncing ball with glow                         |
|                                                          |
|   SPDX-License-Identifier: GPL-2.0-only                  |
\*---------------------------------------------------------*/

#include "BouncingBall3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(BouncingBall3D);

BouncingBall3D::BouncingBall3D(QWidget* parent) : SpatialEffect3D(parent)
{
    size_slider = nullptr;
    elasticity_slider = nullptr;
    ball_size = 40;
    elasticity = 70;
    SetRainbowMode(true);
}

BouncingBall3D::~BouncingBall3D() {}

EffectInfo3D BouncingBall3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Bouncing Ball";
    info.effect_description = "Single ball bouncing in room with glow";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_BOUNCING_BALL;
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
    info.needs_frequency = false;

    info.default_speed_scale = 10.0f; // motion speed
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void BouncingBall3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Ball Size:"), 0, 0);
    size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(10, 150);
    size_slider->setValue(ball_size);
    layout->addWidget(size_slider, 0, 1);

    layout->addWidget(new QLabel("Elasticity:"), 1, 0);
    elasticity_slider = new QSlider(Qt::Horizontal);
    elasticity_slider->setRange(10, 100);
    elasticity_slider->setValue(elasticity);
    layout->addWidget(elasticity_slider, 1, 1);

    if(parent && parent->layout()) parent->layout()->addWidget(w);

    connect(size_slider, &QSlider::valueChanged, this, &BouncingBall3D::OnBallParameterChanged);
    connect(elasticity_slider, &QSlider::valueChanged, this, &BouncingBall3D::OnBallParameterChanged);
}

void BouncingBall3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_BOUNCING_BALL;
}

void BouncingBall3D::OnBallParameterChanged()
{
    if(size_slider) ball_size = size_slider->value();
    if(elasticity_slider) elasticity = elasticity_slider->value();
    emit ParametersChanged();
}

RGBColor BouncingBall3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor BouncingBall3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    // Deterministic ball path: bounce between floor and ceiling with parabolic segments.
    float speed = GetScaledSpeed();
    float period = fmax(1.5f, 3.0f - speed * 0.02f); // seconds per bounce
    float t = fmodf(time, period);
    float e = elasticity / 100.0f; // 0.1..1.0

    // Vertical motion: y = -a*(t - T/2)^2 + H
    float H = grid.height * 0.45f; // peak
    float a = (4.0f * H) / (period * period);
    float y_ball = -a * (t - period * 0.5f) * (t - period * 0.5f) + H;

    // Horizontal circular drift around origin
    float radius_xy = (grid.width + grid.depth) * 0.1f;
    float ang = time * (0.5f + speed * 0.05f);
    float x_ball = cosf(ang) * radius_xy;
    float z_ball = sinf(ang) * radius_xy;

    // Ball radius from slider
    float R = (ball_size * 0.03f) * GetNormalizedSize();

    float dx = rel_x - x_ball;
    float dy = rel_y - (y_ball - origin.y);
    float dz = rel_z - z_ball;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);

    float glow = fmax(0.0f, 1.0f - dist / (R + 0.001f));
    float intensity = powf(glow, 1.5f);

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(ang * 57.2958f) : GetColorAtPosition(0.5f);
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    float bf = (GetBrightness() / 100.0f) * intensity;
    r = (unsigned char)(r * bf);
    g = (unsigned char)(g * bf);
    b = (unsigned char)(b * bf);
    return (b << 16) | (g << 8) | r;
}

