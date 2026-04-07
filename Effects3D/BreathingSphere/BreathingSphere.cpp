// SPDX-License-Identifier: GPL-2.0-only

#include "BreathingSphere.h"

REGISTER_EFFECT_3D(BreathingSphere);

#include <QGridLayout>
#include <QComboBox>
#include "../EffectHelpers.h"

const char* BreathingSphere::ModeName(int m)
{
    switch(m) { case MODE_SPHERE: return "Sphere"; case MODE_GLOBAL_PULSE: return "Global pulse"; default: return "Sphere"; }
}

BreathingSphere::BreathingSphere(QWidget* parent) : SpatialEffect3D(parent)
{
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}


BreathingSphere::~BreathingSphere() = default;


EffectInfo3D BreathingSphere::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Breathing Sphere";
    info.effect_description = "Pulsing sphere with rainbow or custom colors";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_BREATHING_SPHERE;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 20.0f;
    info.default_frequency_scale = 100.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_color_controls = true;

    return info;
}


void BreathingSphere::SetupCustomUI(QWidget* parent)
{
    QWidget* breathing_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(breathing_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Mode:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(breathing_mode, MODE_COUNT - 1)));
    layout->addWidget(mode_combo, row, 1, 1, 2);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        breathing_mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });
    AddWidgetToParent(breathing_widget, parent);
}


void BreathingSphere::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_BREATHING_SPHERE;
}

RGBColor BreathingSphere::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    progress = CalculateProgress(time);
    int mode = std::max(0, std::min(breathing_mode, MODE_COUNT - 1));

    if(mode == MODE_GLOBAL_PULSE)
    {
        float pulse = 0.4f + 0.6f * (0.5f + 0.5f * sinf(progress * rate * 0.2f));
        RGBColor c = GetRainbowMode() ? GetRainbowColor(progress * 60.0f + time * rate * 12.0f) : GetColorAtPosition(0.5f);
        unsigned char r = (unsigned char)((c & 0xFF) * pulse);
        unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * pulse);
        unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * pulse);
        return (b << 16) | (g << 8) | r;
    }

    float size_multiplier = GetNormalizedSize();
    float bounds_r = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float base_scale = 0.45f;
    float sphere_radius = bounds_r * base_scale * size_multiplier * (1.0f + 0.25f * sinf(progress * rate * 0.2f));

    float distance = sqrtf(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    
    float core_intensity = 1.0f - smoothstep(0.0f, sphere_radius * 0.7f, distance);
    float glow_intensity = 0.5f * (1.0f - smoothstep(sphere_radius * 0.7f, sphere_radius * 1.3f, distance));
    float ripple = 0.3f * sinf(distance * (detail / (bounds_r + 0.001f)) * 1.5f - progress * 2.0f);
    ripple = (ripple + 1.0f) * 0.5f;
    
    float ambient = 0.1f * (1.0f - smoothstep(0.0f, sphere_radius * 2.0f, distance));
    
    float sphere_intensity = core_intensity + glow_intensity + ripple * 0.4f + ambient;
    sphere_intensity = fmax(0.0f, fmin(1.0f, sphere_intensity));

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(distance * 30.0f * (0.6f + 0.4f * detail) + time * rate * 12.0f)
                                            : GetColorAtPosition(fmodf(fmin(1.0f, distance / (sphere_radius + 0.001f)) * (0.6f + 0.4f * detail), 1.0f));
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * sphere_intensity);
    g = (unsigned char)(g * sphere_intensity);
    b = (unsigned char)(b * sphere_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json BreathingSphere::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["breathing_mode"] = breathing_mode;
    return j;
}

void BreathingSphere::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("breathing_mode") && settings["breathing_mode"].is_number_integer())
        breathing_mode = std::max(0, std::min(settings["breathing_mode"].get<int>(), MODE_COUNT - 1));
}

