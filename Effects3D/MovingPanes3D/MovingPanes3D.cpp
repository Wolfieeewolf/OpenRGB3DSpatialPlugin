// SPDX-License-Identifier: GPL-2.0-only

#include "MovingPanes3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>

REGISTER_EFFECT_3D(MovingPanes3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static RGBColor lerp_color(RGBColor a, RGBColor b, float t)
{
    t = std::max(0.0f, std::min(1.0f, t));
    int ar = a & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF;
    int br = b & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF;
    int r = (int)(ar + (br - ar) * t);
    int g = (int)(ag + (bg - ag) * t);
    int b_ = (int)(ab + (bb - ab) * t);
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b_ = std::max(0, std::min(255, b_));
    return (RGBColor)((b_ << 16) | (g << 8) | r);
}

MovingPanes3D::MovingPanes3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    std::vector<RGBColor> cols;
    cols.push_back(0x000000FF);
    cols.push_back(0x00FF0000);
    SetColors(cols);
}

EffectInfo3D MovingPanes3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Moving Panes";
    info.effect_description = "Symmetrical moving color panes";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 2;
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
    info.show_path_axis_control = true;
    return info;
}

void MovingPanes3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Divisions:"), row, 0);
    QSlider* div_slider = new QSlider(Qt::Horizontal);
    div_slider->setRange(2, 16);
    div_slider->setValue(num_divisions);
    QLabel* div_label = new QLabel(QString::number(num_divisions));
    div_label->setMinimumWidth(36);
    layout->addWidget(div_slider, row, 1);
    layout->addWidget(div_label, row, 2);
    connect(div_slider, &QSlider::valueChanged, this, [this, div_label](int v){
        num_divisions = v;
        if(div_label) div_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void MovingPanes3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor MovingPanes3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor MovingPanes3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = (rot.x - origin.x + grid.width * 0.5f) / std::max(0.001f, grid.width);
    float ly = (rot.y - origin.y + grid.height * 0.5f) / std::max(0.001f, grid.height);
    float lz = (rot.z - origin.z + grid.depth * 0.5f) / std::max(0.001f, grid.depth);
    lx = std::max(0.0f, std::min(1.0f, lx));
    ly = std::max(0.0f, std::min(1.0f, ly));
    lz = std::max(0.0f, std::min(1.0f, lz));

    int ax = GetPathAxis();
    float prim = (ax == 0) ? lx : (ax == 1) ? ly : lz;
    float sec = (ax == 0) ? ly : (ax == 1) ? lz : lx;
    int ndiv = std::max(2, std::min(16, num_divisions));
    float zone_size = 1.0f / (float)ndiv;
    int zone = (int)(prim / zone_size);
    zone = std::max(0, std::min(ndiv - 1, zone));
    int zone_id = zone % 2;
    float t = progress;
    float s = 0.5f * (1.0f + sinf(sec * (float)M_PI * 4.0f + (zone_id ? 1.0f : -1.0f) * t * (float)(2.0 * M_PI) + (float)M_PI * 0.25f));

    RGBColor c0, c1;
    if(GetRainbowMode())
    {
        c0 = GetRainbowColor(progress * 60.0f + zone * 30.0f);
        c1 = GetRainbowColor(progress * 60.0f + zone * 30.0f + 180.0f);
    }
    else
    {
        const std::vector<RGBColor>& cols = GetColors();
        c0 = (cols.size() > 0) ? cols[0] : 0x000000FF;
        c1 = (cols.size() > 1) ? cols[1] : 0x00FF0000;
    }
    return lerp_color(zone_id ? c1 : c0, zone_id ? c0 : c1, s);
}

nlohmann::json MovingPanes3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["num_divisions"] = num_divisions;
    return j;
}

void MovingPanes3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("num_divisions") && settings["num_divisions"].is_number_integer())
        num_divisions = std::max(2, std::min(16, settings["num_divisions"].get<int>()));
}
