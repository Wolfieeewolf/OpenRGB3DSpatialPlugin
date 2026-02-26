// SPDX-License-Identifier: GPL-2.0-only

#include "Rain3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(Rain3D);

Rain3D::Rain3D(QWidget* parent) : SpatialEffect3D(parent)
{
    density_slider = nullptr;
    density_label = nullptr;
    wind_slider = nullptr;
    wind_label = nullptr;
    rain_density = 50;
    wind = 0;

    SetRainbowMode(false);
    SetFrequency(50);
}

Rain3D::~Rain3D()
{
}

EffectInfo3D Rain3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Rain";
    info.effect_description = "Falling rain with wind drift";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_RAIN;
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

    // Room-scale defaults: visible motion and spacing
    info.default_speed_scale = 30.0f;
    info.default_frequency_scale = 8.0f;
    info.use_size_parameter = true;

    // Show standard controls
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

void Rain3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Density:"), 0, 0);
    density_slider = new QSlider(Qt::Horizontal);
    density_slider->setRange(5, 100);
    density_slider->setValue(rain_density);
    density_slider->setToolTip("Rain density (higher = more drops)");
    layout->addWidget(density_slider, 0, 1);
    density_label = new QLabel(QString::number(rain_density));
    density_label->setMinimumWidth(30);
    layout->addWidget(density_label, 0, 2);

    layout->addWidget(new QLabel("Wind:"), 1, 0);
    wind_slider = new QSlider(Qt::Horizontal);
    wind_slider->setRange(-50, 50);
    wind_slider->setValue(wind);
    wind_slider->setToolTip("Wind drift (left/right)");
    layout->addWidget(wind_slider, 1, 1);
    wind_label = new QLabel(QString::number(wind));
    wind_label->setMinimumWidth(30);
    layout->addWidget(wind_label, 1, 2);

    AddWidgetToParent(w, parent);

    connect(density_slider, &QSlider::valueChanged, this, &Rain3D::OnRainParameterChanged);
    connect(wind_slider, &QSlider::valueChanged, this, &Rain3D::OnRainParameterChanged);
}

void Rain3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_RAIN;
}

void Rain3D::OnRainParameterChanged()
{
    if(density_slider)
    {
        rain_density = density_slider->value();
        if(density_label) density_label->setText(QString::number(rain_density));
    }
    if(wind_slider)
    {
        wind = wind_slider->value();
        if(wind_label) wind_label->setText(QString::number(wind));
    }
    emit ParametersChanged();
}

static inline float hash31(int x, int y, int z)
{
    // Small integer hash to pseudo-randomize per-LED behavior deterministically
    int n = x * 73856093 ^ y * 19349663 ^ z * 83492791;
    n = (n << 13) ^ n;
    return 0.5f * (1.0f + (float)((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

RGBColor Rain3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor Rain3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float speed = GetScaledSpeed();
    float size_m = GetNormalizedSize();
    
    // Apply rotation - rain falls along rotated Y-axis
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;
    
    // Wind drift in rotated space
    float wind_drift = (float)wind * 0.02f;
    
    // Generate discrete drops deterministically based on time
    // Number of drops scales with density
    int num_drops = 5 + (rain_density * 15) / 100; // 5-20 drops
    float max_intensity = 0.0f;
    RGBColor drop_color = GetRainbowMode() ? GetRainbowColor(200.0f) : GetColorAtPosition(0.5f);
    
    // Room dimensions for drop generation
    float room_width = grid.width;
    float room_height = grid.height;
    float room_depth = grid.depth;
    
    // Drop size scales with room and size parameter
    float drop_size = (room_width + room_depth + room_height) / 3.0f * (0.01f + 0.03f * size_m);
    float trail_length = drop_size * 1.5f; // Trail behind drop
    
    // Fall speed
    float fall_speed = speed * 0.5f;
    
    for(int i = 0; i < num_drops; i++)
    {
        // Deterministic drop generation based on time and index
        // Each drop has a unique seed
        float drop_seed = (float)(i * 131 + 313);
        float drop_x_seed = hash31((int)(drop_seed * 733), 0, 0);
        float drop_z_seed = hash31((int)(drop_seed * 919), 0, 0);
        float speed_mult = 0.8f + drop_x_seed * 0.4f; // Vary drop speeds
        
        // Initial X/Z position (spread across room)
        float drop_x = -room_width * 0.5f + drop_x_seed * room_width;
        float drop_z = -room_depth * 0.5f + drop_z_seed * room_depth;
        
        // Apply wind drift
        drop_x += time * wind_drift;
        
        // Y position - drops fall from top
        float drop_y_start = room_height * 0.5f;
        float drop_y = drop_y_start - (time * fall_speed * speed_mult);
        
        // Wrap drops that fall below room
        float wrap_height = room_height + drop_size * 2.0f;
        drop_y = fmodf(drop_y + wrap_height, wrap_height) - wrap_height * 0.5f;
        
        // Distance from LED to drop center (in rotated space)
        float dx = rot_rel_x - drop_x;
        float dy = rot_rel_y - drop_y;
        float dz = rot_rel_z - drop_z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        
        // Check if LED is in drop's trail (behind the drop)
        float dist_along_fall = dy; // Positive = below drop, negative = above
        float dist_lateral = sqrtf(dx*dx + dz*dz);
        
        float intensity = 0.0f;
        
        // Drop head (bright core)
        if(dist < drop_size && dist_along_fall > -drop_size * 0.3f)
        {
            float head_dist = dist / drop_size;
            intensity = fmax(intensity, 1.0f - head_dist);
        }
        
        // Drop body (main trail)
        if(dist_along_fall >= 0 && dist_along_fall <= trail_length && dist_lateral < drop_size)
        {
            float body_dist = dist_lateral / drop_size;
            float trail_fade = 1.0f - (dist_along_fall / trail_length);
            intensity = fmax(intensity, (1.0f - body_dist) * trail_fade * 0.8f);
        }
        
        // Glow around drop
        if(dist < drop_size * 2.0f)
        {
            float glow_dist = dist / (drop_size * 2.0f);
            intensity = fmax(intensity, (1.0f - glow_dist) * 0.3f);
        }
        
        if(intensity > max_intensity)
        {
            max_intensity = intensity;
            // Vary drop color slightly
            if(GetRainbowMode())
            {
                float hue = 200.0f + drop_x_seed * 60.0f + time * 5.0f;
                drop_color = GetRainbowColor(hue);
            }
        }
    }
    
    // Increase brightness by 60% (multiply by 1.6)
    max_intensity *= 1.6f;
    max_intensity = fmax(0.0f, fmin(1.0f, max_intensity));

    unsigned char r = drop_color & 0xFF;
    unsigned char g = (drop_color >> 8) & 0xFF;
    unsigned char b = (drop_color >> 16) & 0xFF;
    // Apply intensity (global brightness is applied by PostProcessColorGrid)
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Rain3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["rain_density"] = rain_density;
    j["wind"] = wind;
    return j;
}

void Rain3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("rain_density")) rain_density = settings["rain_density"];
    if(settings.contains("wind")) wind = settings["wind"];

    if(density_slider) density_slider->setValue(rain_density);
    if(wind_slider) wind_slider->setValue(wind);
}
