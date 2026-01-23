// SPDX-License-Identifier: GPL-2.0-only

#include "BouncingBall3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(BouncingBall3D);

namespace
{
float ReflectPosition(float p0, float velocity, float time_value, float min_value, float max_value)
{
    float length = max_value - min_value;
    if(length <= 0.0001f)
    {
        return min_value;
    }

    float relative = (p0 - min_value) + velocity * time_value;
    float double_length = 2.0f * length;
    float wrapped = fmodf(relative, double_length);
    if(wrapped < 0.0f)
    {
        wrapped += double_length;
    }
    return (wrapped <= length) ? (min_value + wrapped) : (max_value - (wrapped - length));
}

float HashFloat01(unsigned int seed)
{
    unsigned int value = seed ^ 0x27d4eb2d;
    value = (value ^ 61U) ^ (value >> 16U);
    value = value + (value << 3U);
    value = value ^ (value >> 4U);
    value = value * 0x27d4eb2d;
    value = value ^ (value >> 15U);
    return (value & 0xFFFFU) / 65535.0f;
}
}

BouncingBall3D::BouncingBall3D(QWidget* parent) : SpatialEffect3D(parent)
{
    size_slider = nullptr;
    elasticity_slider = nullptr;
    count_slider = nullptr;
    ball_size = 40;
    elasticity = 70;
    ball_count = 1;
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
    // Rotation controls are in base class
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
    size_slider->setToolTip("Ball radius (room-aware)");
    layout->addWidget(size_slider, 0, 1);

    layout->addWidget(new QLabel("Elasticity:"), 1, 0);
    elasticity_slider = new QSlider(Qt::Horizontal);
    elasticity_slider->setRange(10, 100);
    elasticity_slider->setValue(elasticity);
    elasticity_slider->setToolTip("Bounce elasticity (higher = higher bounces)");
    layout->addWidget(elasticity_slider, 1, 1);

    layout->addWidget(new QLabel("Balls:"), 2, 0);
    count_slider = new QSlider(Qt::Horizontal);
    count_slider->setRange(1, 50);
    count_slider->setValue(ball_count);
    count_slider->setToolTip("Number of balls (1..50)");
    layout->addWidget(count_slider, 2, 1);

    if(parent && parent->layout()) parent->layout()->addWidget(w);

    connect(size_slider, &QSlider::valueChanged, this, &BouncingBall3D::OnBallParameterChanged);
    connect(elasticity_slider, &QSlider::valueChanged, this, &BouncingBall3D::OnBallParameterChanged);
    connect(count_slider, &QSlider::valueChanged, this, &BouncingBall3D::OnBallParameterChanged);
}

void BouncingBall3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_BOUNCING_BALL;
}

void BouncingBall3D::OnBallParameterChanged()
{
    if(size_slider) ball_size = size_slider->value();
    if(elasticity_slider) elasticity = elasticity_slider->value();
    if(count_slider) ball_count = count_slider->value();
    emit ParametersChanged();
}

RGBColor BouncingBall3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor BouncingBall3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{

    // Multi-ball elastic reflections off room bounds (all axes)
    float speed = GetScaledSpeed();
    float e = fmax(0.1f, elasticity / 100.0f); // 0.1..1.0

    // Ball radius scales with room size for visibility across any room
    float size_m = GetNormalizedSize();
    float room_avg = (grid.width + grid.depth + grid.height) / 3.0f;
    float R = room_avg * (0.002f + (ball_size / 150.0f) * 0.28f) * size_m;

    float xmin = grid.min_x + R;
    float xmax = grid.max_x - R;
    float ymin = grid.min_y + R;
    float ymax = grid.max_y - R;
    float zmin = grid.min_z + R;
    float zmax = grid.max_z - R;

    float max_intensity = 0.0f;
    float hue_for_max = 120.0f; // matrix-green base hue

    unsigned int N = ball_count == 0 ? 1u : ball_count;
    for(unsigned int k = 0; k < N; k++)
    {
        // Initial positions pseudo-random within room
        float p0x = xmin + HashFloat01(k * 131U) * (xmax - xmin);
        float p0y = ymin + HashFloat01(k * 313U) * (ymax - ymin);
        float p0z = zmin + HashFloat01(k * 919U) * (zmax - zmin);

        // Velocity direction (unit-ish), speed factor scales with speed and elasticity
        float ax = HashFloat01(k * 733U) * 2.0f - 1.0f;
        float ay = HashFloat01(k * 577U) * 2.0f - 1.0f;
        float az = HashFloat01(k * 829U) * 2.0f - 1.0f;
        float norm = sqrtf(ax*ax + ay*ay + az*az);
        if(norm < 0.0001f) norm = 1.0f;
        ax /= norm; ay /= norm; az /= norm;
        float base_speed = 0.5f + 1.5f * HashFloat01(k * 997U);
        float vmag = base_speed * (0.2f + speed * 0.03f) * (0.6f + 0.8f * e);

        // Position at time with elastic reflections
        float bx = ReflectPosition(p0x, ax * vmag, time, xmin, xmax);
        float by = ReflectPosition(p0y, ay * vmag, time, ymin, ymax);
        float bz = ReflectPosition(p0z, az * vmag, time, zmin, zmax);

        // Distance from LED to ball center
        float dx = (x - bx);
        float dy = (y - by);
        float dz = (z - bz);
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        float glow = fmax(0.0f, 1.0f - dist / (R + 0.001f));
        float intensity = powf(glow, 1.2f);
        if(intensity < 0.02f && dist <= R * 1.2f) intensity = 0.02f;

        if(intensity > max_intensity)
        {
            max_intensity = intensity;
            // Hue per-ball from velocity direction/time
            float hue = fmodf((atan2f(az, ax) * 57.2958f) + time * 20.0f, 360.0f);
            hue_for_max = hue;
        }
    }

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(hue_for_max) : GetColorAtPosition(0.5f);
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    // Apply intensity (global brightness is applied by PostProcessColorGrid)
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json BouncingBall3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["ball_size"] = ball_size;
    j["elasticity"] = elasticity;
    j["ball_count"] = ball_count;
    return j;
}

void BouncingBall3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("ball_size")) ball_size = settings["ball_size"];
    if(settings.contains("elasticity")) elasticity = settings["elasticity"];
    if(settings.contains("ball_count")) ball_count = settings["ball_count"];

    if(size_slider) size_slider->setValue(ball_size);
    if(elasticity_slider) elasticity_slider->setValue(elasticity);
    if(count_slider) count_slider->setValue(ball_count);
}
