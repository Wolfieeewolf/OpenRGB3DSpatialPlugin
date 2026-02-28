// SPDX-License-Identifier: GPL-2.0-only

#include "Beam3D.h"
#include "../EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>

REGISTER_EFFECT_3D(Beam3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned char screen_blend(unsigned char a, unsigned char b)
{
    return (unsigned char)(255 - ((255 - a) * (255 - b) / 255));
}

const char* Beam3D::ModeName(int m)
{
    switch(m) {
    case MODE_CROSSING: return "Crossing";
    case MODE_ROTATING: return "Rotating";
    default: return "Crossing";
    }
}

Beam3D::Beam3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    std::vector<RGBColor> cols;
    cols.push_back(0x000000FF);
    cols.push_back(0x0000FF00);
    SetColors(cols);
}

EffectInfo3D Beam3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Beam";
    info.effect_description = "Crossing beams (X+Y) or rotating beam in a plane";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 10.0f;
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

void Beam3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;

    layout->addWidget(new QLabel("Style:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(this->mode, MODE_COUNT - 1)));
    layout->addWidget(mode_combo, row, 1, 1, 2);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        this->mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Beam thickness (Crossing):"), row, 0);
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

    layout->addWidget(new QLabel("Beam width (Rotating):"), row, 0);
    QSlider* width_slider = new QSlider(Qt::Horizontal);
    width_slider->setRange(5, 50);
    width_slider->setValue((int)(beam_width * 100.0f));
    QLabel* width_label = new QLabel(QString::number((int)(beam_width * 100)) + "%");
    width_label->setMinimumWidth(36);
    layout->addWidget(width_slider, row, 1);
    layout->addWidget(width_label, row, 2);
    connect(width_slider, &QSlider::valueChanged, this, [this, width_label](int v){
        beam_width = v / 100.0f;
        if(width_label) width_label->setText(QString::number(v) + "%");
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

void Beam3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor Beam3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor Beam3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    int m = std::max(0, std::min(this->mode, MODE_COUNT - 1));

    if(m == MODE_CROSSING)
    {
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
        float v1 = 1.0f - dx_pct, v2 = 1.0f - dy_pct;
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

    float beam_angle = progress * (float)(2.0 * M_PI);
    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = rot.x - origin.x, ly = rot.y - origin.y, lz = rot.z - origin.z;
    int pl = GetPlane();
    float point_angle;
    if(pl == 0) point_angle = atan2f(lz, lx);
    else if(pl == 1) point_angle = atan2f(lx, ly);
    else point_angle = atan2f(lz, ly);
    float diff = fmodf(point_angle - beam_angle + (float)M_PI, (float)(2.0 * M_PI));
    if(diff < 0.0f) diff += (float)(2.0 * M_PI);
    diff -= (float)M_PI;
    float abs_diff = fabsf(diff);
    float width = std::max(0.05f, std::min(0.5f, beam_width)) * (float)M_PI;
    float glow_val = std::max(0.1f, std::min(1.0f, glow));
    float intensity;
    if(abs_diff <= width * 0.5f)
        intensity = 1.0f;
    else if(abs_diff <= width)
        intensity = 1.0f - (abs_diff - width * 0.5f) / (width * 0.5f);
    else
        intensity = powf(1.0f - fminf(1.0f, (abs_diff - width) / ((float)M_PI * glow_val)), 2.0f);
    if(intensity < 0.01f) return 0x00000000;
    float hue = fmodf(progress * 60.0f, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(progress);
    unsigned char r = (unsigned char)((c & 0xFF) * intensity);
    unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * intensity);
    unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Beam3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["mode"] = this->mode;
    j["beam_thickness"] = beam_thickness;
    j["beam_width"] = beam_width;
    j["glow"] = glow;
    return j;
}

void Beam3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("mode") && settings["mode"].is_number_integer())
        this->mode = std::clamp(settings["mode"].get<int>(), 0, MODE_COUNT - 1);
    else if(settings.contains("plane_axis") && !settings.contains("beam_thickness"))
        this->mode = MODE_ROTATING;
    if(settings.contains("beam_thickness") && settings["beam_thickness"].is_number())
        beam_thickness = std::max(0.02f, std::min(0.2f, settings["beam_thickness"].get<float>()));
    if(settings.contains("beam_width") && settings["beam_width"].is_number())
        beam_width = std::max(0.05f, std::min(0.5f, settings["beam_width"].get<float>()));
    if(settings.contains("glow") && settings["glow"].is_number())
        glow = std::max(0.1f, std::min(1.0f, settings["glow"].get<float>()));
}
