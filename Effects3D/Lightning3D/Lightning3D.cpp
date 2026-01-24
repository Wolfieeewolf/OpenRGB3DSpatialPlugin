// SPDX-License-Identifier: GPL-2.0-only

#include "Lightning3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(Lightning3D);

Lightning3D::Lightning3D(QWidget* parent) : SpatialEffect3D(parent)
{
    strike_rate_slider = nullptr;
    strike_rate_label = nullptr;
    branch_slider = nullptr;
    branch_label = nullptr;
    strike_rate = 5; // arches per second
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
    info.effect_name = "3D Plasma Ball";
    info.effect_description = "Plasma ball with electrical arches emanating from origin";
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

    info.default_speed_scale = 20.0f; // arch animation speed
    info.default_frequency_scale = 10.0f; // arch spawn rate influence
    info.use_size_parameter = true;

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

void Lightning3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Arches/sec:"), 0, 0);
    strike_rate_slider = new QSlider(Qt::Horizontal);
    strike_rate_slider->setRange(1, 30);
    strike_rate_slider->setValue(strike_rate);
    strike_rate_slider->setToolTip("Number of arches per second");
    layout->addWidget(strike_rate_slider, 0, 1);
    strike_rate_label = new QLabel(QString::number(strike_rate));
    strike_rate_label->setMinimumWidth(30);
    layout->addWidget(strike_rate_label, 0, 2);

    layout->addWidget(new QLabel("Arches:"), 1, 0);
    branch_slider = new QSlider(Qt::Horizontal);
    branch_slider->setRange(1, 20);
    branch_slider->setValue(branches);
    branch_slider->setToolTip("Number of simultaneous arches");
    layout->addWidget(branch_slider, 1, 1);
    branch_label = new QLabel(QString::number(branches));
    branch_label->setMinimumWidth(30);
    layout->addWidget(branch_label, 1, 2);

    if(parent && parent->layout()) parent->layout()->addWidget(w);

    connect(strike_rate_slider, &QSlider::valueChanged, this, &Lightning3D::OnLightningParameterChanged);
    connect(strike_rate_slider, &QSlider::valueChanged, strike_rate_label, [this](int value) {
        strike_rate_label->setText(QString::number(value));
    });
    connect(branch_slider, &QSlider::valueChanged, this, &Lightning3D::OnLightningParameterChanged);
    connect(branch_slider, &QSlider::valueChanged, branch_label, [this](int value) {
        branch_label->setText(QString::number(value));
    });
}

void Lightning3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_LIGHTNING;
}

void Lightning3D::OnLightningParameterChanged()
{
    if(strike_rate_slider)
    {
        strike_rate = strike_rate_slider->value();
        if(strike_rate_label) strike_rate_label->setText(QString::number(strike_rate));
    }
    if(branch_slider)
    {
        branches = branch_slider->value();
        if(branch_label) branch_label->setText(QString::number(branches));
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

RGBColor Lightning3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor Lightning3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float speed = GetScaledSpeed();
    float size_m = GetNormalizedSize();
    
    // Apply rotation
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;
    
    // Generate plasma ball arches emanating from origin
    // Use frequency control to influence arch spawn rate
    float freq_factor = GetScaledFrequency() * 0.1f; // Frequency influences spawn rate
    float arches_per_sec = (float)strike_rate + freq_factor;
    float arch_interval = 1.0f / arches_per_sec;
    
    float max_intensity = 0.0f;
    RGBColor arch_color = GetRainbowMode() ? GetRainbowColor(200.0f) : ToRGBColor(180, 220, 255);
    
    // Room dimensions
    float room_avg = (grid.width + grid.depth + grid.height) / 3.0f;
    float max_reach = room_avg * 0.6f; // Maximum arch reach
    
    // Arch properties
    float core_width = room_avg * (0.03f + 0.05f * size_m);
    float glow_width = room_avg * (0.10f + 0.12f * size_m);
    float outer_glow_width = room_avg * (0.20f + 0.18f * size_m);
    
    // Check multiple recent arches (last 2 seconds worth)
    int max_arches_to_check = (int)(arches_per_sec * 2.0f) + branches;
    if(max_arches_to_check > 50) max_arches_to_check = 50;
    
    // Generate arches - each branch creates a separate arch
    for(int arch_idx = 0; arch_idx < max_arches_to_check; arch_idx++)
    {
        // Calculate arch spawn time
        float arch_time_offset = (float)(arch_idx / branches) * arch_interval;
        float arch_time = time - arch_time_offset;
        
        if(arch_time < 0) continue;
        
        // Arch age and pulse
        float age = fmodf(arch_time, arch_interval);
        float pulse = 0.5f + 0.5f * sinf(age * 15.0f + arch_idx * 2.0f); // Pulsing effect
        float decay = fmax(0.0f, 1.0f - age * 2.0f); // Fade over 0.5 seconds
        float arch_intensity = pulse * decay;
        if(arch_intensity <= 0.01f) continue;
        
        // Deterministic arch direction (spherical coordinates)
        int branch_idx = arch_idx % branches;
        float arch_seed = (float)(arch_idx * 733 + branch_idx * 577);
        float theta = hash31((int)(arch_seed * 829), 0, 0) * 6.28318f; // Azimuth: 0 to 2π
        float phi = hash31((int)(arch_seed * 997), 0, 0) * 3.14159f;   // Polar: 0 to π
        
        // Arch extends outward from origin
        float arch_length = max_reach * (0.3f + hash31((int)(arch_seed * 733), 0, 0) * 0.7f);
        // Use speed for animation speed (how fast arch extends)
        float arch_progress = fmin(1.0f, age * speed * 0.5f); // How far along the arch
        
        // Arch path with some curvature (plasma ball effect)
        float base_dist = arch_length * arch_progress;
        float curve_amount = sinf(arch_progress * 3.14159f) * 0.2f; // Curved path
        float curve_angle = theta + curve_amount * sinf(phi * 2.0f);
        
        // Arch endpoint in 3D space
        float arch_x = base_dist * sinf(phi) * cosf(curve_angle);
        float arch_y = base_dist * sinf(phi) * sinf(curve_angle);
        float arch_z = base_dist * cosf(phi);
        
        // Add some wobble for plasma effect
        float wobble = sinf(age * 20.0f + arch_seed) * 0.1f * arch_progress;
        arch_x += wobble * cosf(theta);
        arch_y += wobble * sinf(theta);
        arch_z += wobble * 0.5f;
        
        // Distance from LED to arch path
        float dx = rot_rel_x - arch_x;
        float dy = rot_rel_y - arch_y;
        float dz = rot_rel_z - arch_z;
        float dist_to_arch = sqrtf(dx*dx + dy*dy + dz*dz);
        
        // Also check distance along the arch path from origin
        float dist_from_origin = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
        float dist_along_arch = fabsf(dist_from_origin - base_dist);
        
        // Only affect LEDs near the arch
        if(dist_to_arch < outer_glow_width * 2.0f && dist_along_arch < arch_length * 0.3f)
        {
            // Core, glow, and outer glow layers
            float core = fmax(0.0f, 1.0f - dist_to_arch / (core_width + 0.001f));
            float glow = fmax(0.0f, 1.0f - dist_to_arch / (glow_width + 0.001f)) * 0.8f;
            float outer_glow = fmax(0.0f, 1.0f - dist_to_arch / (outer_glow_width + 0.001f)) * 0.4f;
            float intensity = (core + glow + outer_glow) * arch_intensity;
            
            // Fade based on distance from origin (stronger near center)
            float origin_dist_norm = dist_from_origin / max_reach;
            float origin_fade = 1.0f - origin_dist_norm * 0.3f; // Slight fade at edges
            intensity *= fmax(0.5f, origin_fade);
            
            // Increase brightness by 60%
            intensity *= 1.6f;
            intensity = fmax(0.0f, fmin(1.0f, intensity));
            
            if(intensity > max_intensity)
            {
                max_intensity = intensity;
                if(GetRainbowMode())
                {
                    float hue = 200.0f + arch_progress * 160.0f + arch_seed * 30.0f;
                    arch_color = GetRainbowColor(hue);
                }
            }
        }
    }

    unsigned char r = arch_color & 0xFF;
    unsigned char g = (arch_color >> 8) & 0xFF;
    unsigned char b = (arch_color >> 16) & 0xFF;
    // Apply intensity (global brightness is applied by PostProcessColorGrid)
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Lightning3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["strike_rate"] = strike_rate;
    j["branches"] = branches;
    return j;
}

void Lightning3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("strike_rate")) strike_rate = settings["strike_rate"];
    if(settings.contains("branches")) branches = settings["branches"];

    if(strike_rate_slider) strike_rate_slider->setValue(strike_rate);
    if(branch_slider) branch_slider->setValue(branches);
}
