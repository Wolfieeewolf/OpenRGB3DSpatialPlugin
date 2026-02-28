// SPDX-License-Identifier: GPL-2.0-only

#include "Wave3D.h"
#include <QGridLayout>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_EFFECT_3D(Wave3D);

Wave3D::Wave3D(QWidget* parent) : SpatialEffect3D(parent)
{
    shape_combo = nullptr;
    shape_type = 0;
    progress = 0.0f;
    SetFrequency(50);
    SetRainbowMode(true);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

Wave3D::~Wave3D() {}

EffectInfo3D Wave3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Wave";
    info.effect_description = "Wave pattern with configurable direction and speed";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WAVE;
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
    info.needs_frequency = true;
    info.default_speed_scale = 400.0f;
    info.default_frequency_scale = 10.0f;
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

void Wave3D::SetupCustomUI(QWidget* parent)
{
    QWidget* wave_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(wave_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Shape:"), 0, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Circles");
    shape_combo->addItem("Squares");
    shape_combo->addItem("Lines");
    shape_combo->addItem("Diagonal");
    shape_combo->setCurrentIndex(shape_type);
    layout->addWidget(shape_combo, 0, 1);
    AddWidgetToParent(wave_widget, parent);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave3D::OnWaveParameterChanged);
}

void Wave3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void Wave3D::OnWaveParameterChanged()
{
    if(shape_combo) shape_type = shape_combo->currentIndex();
    emit ParametersChanged();
}

RGBColor Wave3D::CalculateColor(float x, float y, float z, float time)
{
    Vector3D origin = GetEffectOrigin();
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z))
    {
        return 0x00000000;
    }

    float actual_frequency = GetScaledFrequency();
    progress = CalculateProgress(time);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;
    float wave_value = 0.0f;
    float size_multiplier = GetNormalizedSize();
    float freq_scale = actual_frequency * 0.1f / size_multiplier;
    float position = 0.0f;
    if(shape_type == 0)
        position = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    else if(shape_type == 1)
        position = rot_rel_x;
    else if(shape_type == 2)
        position = rot_rel_y;
    else
        position = rot_rel_x + rot_rel_z;

    wave_value = sin(position * freq_scale - progress);
    float hue = (wave_value + 1.0f) * 180.0f;
    hue = fmod(hue, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    RGBColor final_color;
    if(GetRainbowMode())
        final_color = GetRainbowColor(hue);
    else
        final_color = GetColorAtPosition(hue / 360.0f);
    return final_color;
}

RGBColor Wave3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float actual_frequency = GetScaledFrequency();
    progress = CalculateProgress(time);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;
    float wave_value = 0.0f;
    float size_multiplier = GetNormalizedSize();
    float freq_scale = actual_frequency * 0.1f / size_multiplier;
    float normalized_position = 0.0f;
    if(shape_type == 0)
    {
        float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
        float max_radius = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
        normalized_position = (max_radius > 0.001f) ? (radial_distance / max_radius) : 0.0f;
    }
    else if(shape_type == 1)
        normalized_position = (grid.width > 0.001f) ? ((rot_rel_x + grid.width * 0.5f) / grid.width) : 0.0f;
    else if(shape_type == 2)
        normalized_position = (grid.height > 0.001f) ? ((rot_rel_y + grid.height * 0.5f) / grid.height) : 0.0f;
    else
    {
        float diag = rot_rel_x + rot_rel_z;
        float max_d = grid.width + grid.depth;
        normalized_position = (max_d > 0.001f) ? ((diag + max_d * 0.5f) / max_d) : 0.0f;
    }
    normalized_position = fmaxf(0.0f, fminf(1.0f, normalized_position));
    wave_value = sin(normalized_position * freq_scale * 10.0f - progress);
    float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    float max_radius = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
    float depth_factor = 1.0f;
    if(max_radius > 0.001f)
    {
        float normalized_dist = fmin(1.0f, radial_distance / max_radius);
        depth_factor = 0.4f + 0.6f * (1.0f - normalized_dist * 0.7f);
    }
    float wave_enhanced = wave_value * 0.7f + 0.3f * sin(normalized_position * freq_scale * 20.0f - progress * 1.5f);
    wave_enhanced = fmax(-1.0f, fmin(1.0f, wave_enhanced));
    float hue = (wave_enhanced + 1.0f) * 180.0f;
    hue = fmod(hue, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    RGBColor final_color;
    if(GetRainbowMode())
        final_color = GetRainbowColor(hue);
    else
        final_color = GetColorAtPosition(hue / 360.0f);
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * depth_factor);
    g = (unsigned char)(g * depth_factor);
    b = (unsigned char)(b * depth_factor);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Wave3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["shape_type"] = shape_type;
    return j;
}

void Wave3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("shape_type") && settings["shape_type"].is_number_integer())
    {
        shape_type = std::max(0, std::min(3, settings["shape_type"].get<int>()));
        if(shape_combo)
            shape_combo->setCurrentIndex(shape_type);
    }
}
