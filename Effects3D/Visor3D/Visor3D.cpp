// SPDX-License-Identifier: GPL-2.0-only

#include "Visor3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>

REGISTER_EFFECT_3D(Visor3D);

Visor3D::Visor3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    std::vector<RGBColor> cols;
    cols.push_back(0x000000FF);
    cols.push_back(0x00FF0000);
    SetColors(cols);
}

EffectInfo3D Visor3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Visor (KITT)";
    info.effect_description = "KITT-style sweeping beam back and forth (Larson scanner)";
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

void Visor3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Beam width:"), row, 0);
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
    AddWidgetToParent(w, parent);
}

void Visor3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor Visor3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor Visor3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    int ax = GetPathAxis();
    float axis_val = (ax == 0) ? rot.x : (ax == 1) ? rot.y : rot.z;
    float axis_min = (ax == 0) ? grid.min_x : (ax == 1) ? grid.min_y : grid.min_z;
    float axis_max = (ax == 0) ? grid.max_x : (ax == 1) ? grid.max_y : grid.max_z;
    float span = std::max(axis_max - axis_min, 1e-5f);

    float progress = CalculateProgress(time);
    if(progress > 1.0f) progress = std::fmod(progress, 1.0f);
    if(progress < 0.0f) progress = std::fmod(progress, 1.0f) + 1.0f;
    bool step = (progress < 0.5f);
    float p_step = step ? (2.0f * progress) : (2.0f * (1.0f - progress));
    float beam_center = axis_min + p_step * span;

    float w = std::max(0.05f, std::min(0.5f, beam_width)) * span;
    float hw = w * 0.5f;
    float dist = beam_center - axis_val;

    float intensity = 0.0f;
    RGBColor c;
    if(GetRainbowMode())
    {
        float hue = fmodf(progress * 360.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        c = GetRainbowColor(hue);
    }
    else if(dist < -hw)
        c = GetColorAtPosition(step ? 1.0f : 0.0f);
    else if(dist > hw)
        c = GetColorAtPosition(step ? 0.0f : 1.0f);
    else
    {
        float interp = std::max(0.0f, std::min(1.0f, (hw - dist) / w));
        c = GetColorAtPosition(step ? interp : (1.0f - interp));
    }
    if(dist < -hw)
        intensity = std::max(0.0f, std::min(1.0f, (w + dist) / w));
    else if(dist > hw)
        intensity = std::max(0.0f, std::min(1.0f, 1.0f - (dist - hw) / w));
    else
        intensity = 1.0f;

    if(intensity < 0.01f) return 0x00000000;
    unsigned char r = (unsigned char)((c & 0xFF) * intensity);
    unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * intensity);
    unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Visor3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["beam_width"] = beam_width;
    return j;
}

void Visor3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("beam_width") && settings["beam_width"].is_number())
        beam_width = std::max(0.05f, std::min(0.5f, settings["beam_width"].get<float>()));
}
