// SPDX-License-Identifier: GPL-2.0-only

#include "BouncingBall3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(BouncingBall3D);

namespace
{
// Calculate ball position with proper gravity-based bouncing physics
// Returns position and velocity after bouncing
void CalculateBouncingBall(float& pos_x, float& pos_y, float& pos_z,
                          float& vel_x, float& vel_y, float& vel_z,
                          float dt, float gravity, float elasticity,
                          float xmin, float xmax, float ymin, float ymax, float zmin, float zmax)
{
    // Apply gravity to Y velocity
    vel_y -= gravity * dt;
    
    // Update positions
    pos_x += vel_x * dt;
    pos_y += vel_y * dt;
    pos_z += vel_z * dt;
    
    // Bounce off walls with energy loss (elasticity)
    if(pos_x <= xmin)
    {
        pos_x = xmin;
        vel_x = -vel_x * elasticity;
    }
    else if(pos_x >= xmax)
    {
        pos_x = xmax;
        vel_x = -vel_x * elasticity;
    }
    
    if(pos_y <= ymin)
    {
        pos_y = ymin;
        vel_y = -vel_y * elasticity;
        // Prevent getting stuck on floor
        if(fabsf(vel_y) < 0.01f && pos_y <= ymin + 0.01f)
        {
            vel_y = 0.5f; // Small upward push
        }
    }
    else if(pos_y >= ymax)
    {
        pos_y = ymax;
        vel_y = -vel_y * elasticity;
    }
    
    if(pos_z <= zmin)
    {
        pos_z = zmin;
        vel_z = -vel_z * elasticity;
    }
    else if(pos_z >= zmax)
    {
        pos_z = zmax;
        vel_z = -vel_z * elasticity;
    }
}

float HashFloat01(unsigned int seed)
{
    unsigned int value = seed ^ 0x27D4EB2D;
    value = (value ^ 61U) ^ (value >> 16U);
    value = value + (value << 3U);
    value = value ^ (value >> 4U);
    value = value * 0x27D4EB2D;
    value = value ^ (value >> 15U);
    return (value & 0xFFFFU) / 65535.0f;
}
}

BouncingBall3D::BouncingBall3D(QWidget* parent) : SpatialEffect3D(parent)
{
    size_slider = nullptr;
    size_label = nullptr;
    elasticity_slider = nullptr;
    elasticity_label = nullptr;
    count_slider = nullptr;
    count_label = nullptr;
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
    info.effect_description = "Single ball bouncing in the room with glow";
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
    info.show_size_control = false;  // custom "Ball Size" slider used instead
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
    size_label = new QLabel(QString::number(ball_size));
    size_label->setMinimumWidth(30);
    layout->addWidget(size_label, 0, 2);

    layout->addWidget(new QLabel("Elasticity:"), 1, 0);
    elasticity_slider = new QSlider(Qt::Horizontal);
    elasticity_slider->setRange(10, 100);
    elasticity_slider->setValue(elasticity);
    elasticity_slider->setToolTip("Bounce elasticity (higher = higher bounces)");
    layout->addWidget(elasticity_slider, 1, 1);
    elasticity_label = new QLabel(QString::number(elasticity));
    elasticity_label->setMinimumWidth(30);
    layout->addWidget(elasticity_label, 1, 2);

    layout->addWidget(new QLabel("Balls:"), 2, 0);
    count_slider = new QSlider(Qt::Horizontal);
    count_slider->setRange(1, 50);
    count_slider->setValue(ball_count);
    count_slider->setToolTip("Number of balls (1..50)");
    layout->addWidget(count_slider, 2, 1);
    count_label = new QLabel(QString::number(ball_count));
    count_label->setMinimumWidth(30);
    layout->addWidget(count_label, 2, 2);

    AddWidgetToParent(w, parent);

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
    if(size_slider)
    {
        ball_size = size_slider->value();
        if(size_label) size_label->setText(QString::number(ball_size));
    }
    if(elasticity_slider)
    {
        elasticity = elasticity_slider->value();
        if(elasticity_label) elasticity_label->setText(QString::number(elasticity));
    }
    if(count_slider)
    {
        ball_count = count_slider->value();
        if(count_label) count_label->setText(QString::number(ball_count));
    }
    emit ParametersChanged();
}

RGBColor BouncingBall3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor BouncingBall3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    // Multi-ball physics-based bouncing with gravity
    float speed = GetScaledSpeed();
    float e = fmax(0.1f, elasticity / 100.0f); // 0.1..1.0 elasticity (energy retention)

    // Ball radius scales with room size for visibility across any room
    float size_m = GetNormalizedSize();
    float room_avg = (grid.width + grid.depth + grid.height) / 3.0f;
    float R = room_avg * (0.002f + (ball_size / 150.0f) * 0.28f) * size_m;

    // Bounding box with ball radius padding
    float xmin = grid.min_x + R;
    float xmax = grid.max_x - R;
    float ymin = grid.min_y + R;
    float ymax = grid.max_y - R;
    float zmin = grid.min_z + R;
    float zmax = grid.max_z - R;

    // Gravity strength (scales with room size and speed)
    float gravity = room_avg * 0.8f * (0.3f + speed * 0.02f);

    float max_intensity = 0.0f;
    float hue_for_max = 120.0f; // matrix-green base hue

    unsigned int N = ball_count == 0 ? 1u : ball_count;
    for(unsigned int k = 0; k < N; k++)
    {
        // Initial positions pseudo-random within room (start higher up for better bouncing)
        float p0x = xmin + HashFloat01(k * 131U) * (xmax - xmin);
        float p0y = ymin + HashFloat01(k * 313U) * 0.3f + (ymax - ymin) * 0.5f; // Start in upper half
        float p0z = zmin + HashFloat01(k * 919U) * (zmax - zmin);

        // Initial velocity - mostly horizontal with some upward component
        float v0x = (HashFloat01(k * 733U) * 2.0f - 1.0f) * (0.3f + speed * 0.05f) * room_avg;
        float v0y = HashFloat01(k * 577U) * 0.5f * room_avg * (0.3f + speed * 0.05f); // Upward initial velocity
        float v0z = (HashFloat01(k * 829U) * 2.0f - 1.0f) * (0.3f + speed * 0.05f) * room_avg;

        // Simulate physics step-by-step to current time
        // Wrap time to prevent simulation cutoff (keeps ball moving indefinitely)
        // Use a 20-second cycle to keep simulation bounded while maintaining motion
        float wrapped_time = fmodf(time, 20.0f);
        
        float pos_x = p0x;
        float pos_y = p0y;
        float pos_z = p0z;
        float vel_x = v0x;
        float vel_y = v0y;
        float vel_z = v0z;

        // Time step - balance between accuracy and performance
        float dt = 0.08f;
        float sim_time = 0.0f;
        int max_steps = (int)(wrapped_time / dt) + 1;
        // With wrapped time, max_steps is always <= 250 (20s / 0.08s), limit for safety
        if(max_steps > 250) max_steps = 250;

        // Run physics simulation
        for(int step = 0; step < max_steps && sim_time < wrapped_time; step++)
        {
            float step_dt = fminf(dt, wrapped_time - sim_time);
            CalculateBouncingBall(pos_x, pos_y, pos_z, vel_x, vel_y, vel_z,
                                step_dt, gravity, e,
                                xmin, xmax, ymin, ymax, zmin, zmax);
            sim_time += step_dt;
        }

        // Final ball position
        float bx = pos_x;
        float by = pos_y;
        float bz = pos_z;

        // Distance from LED to ball center
        float dx = (x - bx);
        float dy = (y - by);
        float dz = (z - bz);
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        // Enhanced glow with larger, brighter particles for sparse LEDs
        // Make balls more visible on disconnected LED strips
        float core_radius = R * 0.8f; // Larger core
        float glow_radius = R * 2.0f; // Much larger glow for sparse LEDs
        float core_glow = fmax(0.0f, 1.0f - dist / (core_radius + 0.001f));
        float outer_glow = 0.7f * fmax(0.0f, 1.0f - dist / (glow_radius + 0.001f)); // Brighter outer glow
        float intensity = powf(core_glow, 0.9f) + outer_glow; // Softer core falloff
        // Ensure minimum visibility for whole-room presence on sparse LEDs
        if(intensity < 0.05f && dist <= glow_radius) intensity = 0.05f;
        
        // Increase brightness by 60% (multiply by 1.6)
        intensity *= 1.6f;

        // Clamp intensity before comparison
        intensity = fmax(0.0f, fmin(1.0f, intensity));
        
        if(intensity > max_intensity)
        {
            max_intensity = intensity;
            // Hue per-ball from velocity direction/time
            float hue = fmodf((atan2f(vel_z, vel_x) * 57.2958f) + time * 20.0f, 360.0f);
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
