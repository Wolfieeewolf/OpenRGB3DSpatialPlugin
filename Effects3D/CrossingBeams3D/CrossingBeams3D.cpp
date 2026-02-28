// SPDX-License-Identifier: GPL-2.0-only

#include "CrossingBeams3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

REGISTER_EFFECT_3D(CrossingBeams3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CrossingBeams3D::CrossingBeams3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    std::vector<RGBColor> cols;
    cols.push_back(0x000000FF);
    cols.push_back(0x0000FF00);
    SetColors(cols);
}

EffectInfo3D CrossingBeams3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Crossing Beams";
    info.effect_description = "Two beams moving horizontally and vertically that cross";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 8.0f;
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

void CrossingBeams3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Beam thickness:"), row, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 20);
    thick_slider->setValue((int)(beam_thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(beam_thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, row, 1);
    layout->addWidget(thick_label, row, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        beam_thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Glow:"), row, 0);
    QSlider* glow_slider = new QSlider(Qt::Horizontal);
    glow_slider->setRange(10, 100);
    glow_slider->setValue((int)(glow * 100.0f));
    QLabel* glow_label = new QLabel(QString::number((int)(glow * 100)) + "%");
    glow_label->setMinimumWidth(36);
    layout->addWidget(glow_slider, row, 1);
    layout->addWidget(glow_label, row, 2);
    connect(glow_slider, &QSlider::valueChanged, this, [this, glow_label](int v){
        glow = v / 100.0f;
        if(glow_label) glow_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void CrossingBeams3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor CrossingBeams3D::CalculateColor(float, float, float, float) { return 0x00000000; }

static unsigned char screen_blend(unsigned char a, unsigned char b)
{
    return (unsigned char)(255 - ((255 - a) * (255 - b) / 255));
}

RGBColor CrossingBeams3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    float sine_x = sinf(progress * (float)M_PI);
    float sine_y = sinf(progress * (float)M_PI * 1.3f);

    float half_w = grid.width * 0.5f;
    float half_h = grid.height * 0.5f;
    float x_progress = origin.x + sine_x * half_w;
    float y_progress = origin.y + sine_y * half_h;

    float dist_x = fabsf(x - x_progress);
    float dist_y = fabsf(y - y_progress);
    float thick = std::max(0.02f, std::min(0.2f, beam_thickness)) * std::max(grid.width, grid.height);
    float glow_val = std::max(0.1f, std::min(1.0f, glow));
    float dx_pct = (dist_x > thick) ? powf(dist_x / std::max(0.001f, grid.width), 0.01f * glow_val * 100.0f) : 0.0f;
    float dy_pct = (dist_y > thick) ? powf(dist_y / std::max(0.001f, grid.height), 0.01f * glow_val * 100.0f) : 0.0f;
    dx_pct = std::min(1.0f, dx_pct);
    dy_pct = std::min(1.0f, dy_pct);

    RGBColor c1, c2;
    if(GetRainbowMode())
    {
        c1 = GetRainbowColor(progress * 120.0f);
        c2 = GetRainbowColor(progress * 120.0f + 180.0f);
    }
    else
    {
        const std::vector<RGBColor>& cols = GetColors();
        c1 = (cols.size() > 0) ? cols[0] : 0x000000FF;
        c2 = (cols.size() > 1) ? cols[1] : 0x0000FF00;
    }
    float v1 = 1.0f - dx_pct;
    float v2 = 1.0f - dy_pct;

    unsigned char r1 = (unsigned char)((c1 & 0xFF) * v1);
    unsigned char g1 = (unsigned char)(((c1 >> 8) & 0xFF) * v1);
    unsigned char b1 = (unsigned char)(((c1 >> 16) & 0xFF) * v1);
    unsigned char r2 = (unsigned char)((c2 & 0xFF) * v2);
    unsigned char g2 = (unsigned char)(((c2 >> 8) & 0xFF) * v2);
    unsigned char b2 = (unsigned char)(((c2 >> 16) & 0xFF) * v2);
    unsigned char r = screen_blend(r1, r2);
    unsigned char g = screen_blend(g1, g2);
    unsigned char b = screen_blend(b1, b2);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json CrossingBeams3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["beam_thickness"] = beam_thickness;
    j["glow"] = glow;
    return j;
}

void CrossingBeams3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("beam_thickness") && settings["beam_thickness"].is_number())
        beam_thickness = std::max(0.02f, std::min(0.2f, settings["beam_thickness"].get<float>()));
    if(settings.contains("glow") && settings["glow"].is_number())
        glow = std::max(0.1f, std::min(1.0f, settings["glow"].get<float>()));
}
