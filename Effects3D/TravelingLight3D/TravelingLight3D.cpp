// SPDX-License-Identifier: GPL-2.0-only

#include "TravelingLight3D.h"
#include "../EffectHelpers.h"
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QGridLayout>
#include <cmath>
#include <algorithm>

REGISTER_EFFECT_3D(TravelingLight3D);

const char* TravelingLight3D::ModeName(int m)
{
    switch(m) {
    case MODE_COMET: return "Comet";
    case MODE_ZIGZAG: return "ZigZag (snake)";
    case MODE_KITT: return "KITT Scanner";
    default: return "Comet";
    }
}

TravelingLight3D::TravelingLight3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

EffectInfo3D TravelingLight3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Traveling Light";
    info.effect_description = "Comet, ZigZag snake, or KITT-style scanner beam traveling through the room";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_COMET;
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

void TravelingLight3D::SetupCustomUI(QWidget* parent)
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

    layout->addWidget(new QLabel("Tail/beam size:"), row, 0);
    QSlider* size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(5, 80);
    size_slider->setValue((int)(tail_size * 100.0f));
    QLabel* size_label = new QLabel(QString::number((int)(tail_size * 100)) + "%");
    size_label->setMinimumWidth(36);
    layout->addWidget(size_slider, row, 1);
    layout->addWidget(size_label, row, 2);
    connect(size_slider, &QSlider::valueChanged, this, [this, size_label](int v){
        tail_size = v / 100.0f;
        if(size_label) size_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Beam width (KITT):"), row, 0);
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

void TravelingLight3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_COMET;
}

RGBColor TravelingLight3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor TravelingLight3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rotated = TransformPointByRotation(x, y, z, origin);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    if(progress > 1.0f) progress = std::fmod(progress, 1.0f);
    if(progress < 0.0f) progress = std::fmod(progress, 1.0f) + 1.0f;

    int m = std::max(0, std::min(this->mode, MODE_COUNT - 1));
    int ax = GetPathAxis();

    if(m == MODE_KITT)
    {
        float axis_val = (ax == 0) ? rotated.x : (ax == 1) ? rotated.y : rotated.z;
        float axis_min = (ax == 0) ? grid.min_x : (ax == 1) ? grid.min_y : grid.min_z;
        float axis_max = (ax == 0) ? grid.max_x : (ax == 1) ? grid.max_y : grid.max_z;
        float span = std::max(axis_max - axis_min, 1e-5f);
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

    if(m == MODE_ZIGZAG)
    {
        float lx = (rotated.x - origin.x) / std::max(0.001f, grid.width);
        float ly = (rotated.y - origin.y) / std::max(0.001f, grid.height);
        float lz = (rotated.z - origin.z) / std::max(0.001f, grid.depth);
        lx = std::max(0.0f, std::min(1.0f, (lx + 1.0f) * 0.5f));
        ly = std::max(0.0f, std::min(1.0f, (ly + 1.0f) * 0.5f));
        lz = std::max(0.0f, std::min(1.0f, (lz + 1.0f) * 0.5f));
        float prim = 0.0f, sec = 0.0f;
        if(ax == 0) { prim = lx; sec = ly; }
        else if(ax == 1) { prim = ly; sec = lz; }
        else { prim = lz; sec = lx; }
        const int n_cols = 24, n_rows = 24;
        float col_cont = prim * (float)n_cols;
        float row_cont = sec * (float)n_rows;
        int seg = std::max(0, std::min(n_cols - 1, (int)col_cont));
        float local = (seg % 2 == 0) ? row_cont : ((float)n_rows - row_cont);
        float path_pos = ((float)(seg * n_rows) + local) / (float)(n_cols * n_rows);
        path_pos = std::max(0.0f, std::min(1.0f, path_pos));
        float tail = std::max(0.1f, std::min(0.8f, tail_size));
        float distance = 1.0f;
        if(path_pos > progress) return 0x00000000;
        float dist_in_tail = progress - path_pos;
        if(dist_in_tail > tail) return 0x00000000;
        distance = 1.0f - (dist_in_tail / tail);
        distance = distance * distance * distance;
        float hue = fmodf(path_pos * 360.0f - time * 50.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(path_pos);
        int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * distance)));
        int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * distance)));
        int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * distance)));
        return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
    }

    float axis_val = (ax == 0) ? rotated.x : (ax == 1) ? rotated.y : rotated.z;
    float axis_min = (ax == 0) ? grid.min_x : (ax == 1) ? grid.min_y : grid.min_z;
    float axis_max = (ax == 0) ? grid.max_x : (ax == 1) ? grid.max_y : grid.max_z;
    float span = std::max(axis_max - axis_min, 1e-5f);
    float tail_len = tail_size * span;
    float head = axis_min + progress * span;
    float dist = head - axis_val;
    float intensity = 0.0f;
    float hue_offset = 0.0f;
    if(dist >= 0.0f && dist <= tail_len)
    {
        intensity = 1.0f - (dist / tail_len);
        intensity = intensity * intensity;
        hue_offset = (1.0f - (dist / tail_len)) * 60.0f;
    }
    else if(dist < 0.0f && dist > -tail_len * 0.2f)
        intensity = 1.0f;
    if(intensity <= 0.0f) return 0x00000000;
    RGBColor color = GetRainbowMode()
        ? GetRainbowColor(progress * 360.0f + hue_offset)
        : GetColorAtPosition(progress);
    unsigned char r = (unsigned char)((color & 0xFF) * intensity);
    unsigned char g = (unsigned char)(((color >> 8) & 0xFF) * intensity);
    unsigned char b = (unsigned char)(((color >> 16) & 0xFF) * intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json TravelingLight3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["mode"] = this->mode;
    j["tail_size"] = tail_size;
    j["beam_width"] = beam_width;
    return j;
}

void TravelingLight3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("mode") && settings["mode"].is_number_integer())
        this->mode = std::clamp(settings["mode"].get<int>(), 0, MODE_COUNT - 1);
    else if(settings.contains("comet_mode") && settings["comet_mode"].is_number_integer())
        this->mode = 0;
    else if(settings.contains("path_mode") && settings["path_mode"].is_number_integer())
        this->mode = 1;
    else if(settings.contains("sweep_axis"))
        this->mode = 2;
    if(settings.contains("tail_size") && settings["tail_size"].is_number())
        tail_size = std::clamp(settings["tail_size"].get<float>(), 0.05f, 1.0f);
    else if(settings.contains("comet_size") && settings["comet_size"].is_number())
        tail_size = std::clamp(settings["comet_size"].get<float>(), 0.05f, 1.0f);
    else if(settings.contains("tail_length") && settings["tail_length"].is_number())
        tail_size = std::clamp(settings["tail_length"].get<float>(), 0.1f, 0.8f);
    if(settings.contains("beam_width") && settings["beam_width"].is_number())
        beam_width = std::clamp(settings["beam_width"].get<float>(), 0.05f, 0.5f);
}
