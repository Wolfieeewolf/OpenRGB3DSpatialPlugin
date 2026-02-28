// SPDX-License-Identifier: GPL-2.0-only

#include "ColorWheel3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>

REGISTER_EFFECT_3D(ColorWheel3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ColorWheel3D::ColorWheel3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
}

EffectInfo3D ColorWheel3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Color Wheel";
    info.effect_description = "Rotating rainbow from center";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
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
    info.show_plane_control = true;
    return info;
}

void ColorWheel3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Direction:"), row, 0);
    QComboBox* dir_combo = new QComboBox();
    dir_combo->addItem("Clockwise");
    dir_combo->addItem("Counter-clockwise");
    dir_combo->setCurrentIndex(direction);
    layout->addWidget(dir_combo, row, 1, 1, 2);
    connect(dir_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        direction = std::max(0, std::min(1, idx));
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void ColorWheel3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor ColorWheel3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor ColorWheel3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = rot.x - origin.x, ly = rot.y - origin.y, lz = rot.z - origin.z;

    int pl = GetPlane();
    float angle;
    if(pl == 0) angle = atan2f(lz, lx);
    else if(pl == 1) angle = atan2f(lx, ly);
    else angle = atan2f(lz, ly);

    float dir = (direction == 0) ? 1.0f : -1.0f;
    float hue = fmodf(angle * (180.0f / (float)M_PI) + progress * 360.0f * dir, 360.0f);
    if(hue < 0.0f) hue += 360.0f;

    return GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(hue / 360.0f);
}

nlohmann::json ColorWheel3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["direction"] = direction;
    return j;
}

void ColorWheel3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("direction") && settings["direction"].is_number_integer())
        direction = std::max(0, std::min(1, settings["direction"].get<int>()));
}
