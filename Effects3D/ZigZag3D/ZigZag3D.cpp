// SPDX-License-Identifier: GPL-2.0-only

#include "ZigZag3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>

REGISTER_EFFECT_3D(ZigZag3D);

const char* ZigZag3D::ModeName(int m)
{
    switch(m) { case MODE_ZIGZAG: return "ZigZag (snake)"; case MODE_MARQUEE: return "Marquee (band)"; default: return "ZigZag"; }
}

ZigZag3D::ZigZag3D(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D ZigZag3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "ZigZag";
    info.effect_description = "Snake path through the room (converted from OpenRGB ZigZag)";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
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
    info.show_path_axis_control = true;
    return info;
}

void ZigZag3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Mode:"), row, 0);
    QComboBox* mode_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::max(0, std::min(path_mode, MODE_COUNT - 1)));
    layout->addWidget(mode_combo, row, 1, 1, 2);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        path_mode = std::max(0, std::min(idx, MODE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Tail length:"), row, 0);
    QSlider* tail_slider = new QSlider(Qt::Horizontal);
    tail_slider->setRange(10, 80);
    tail_slider->setValue((int)(tail_length * 100.0f));
    QLabel* tail_label = new QLabel(QString::number((int)(tail_length * 100)) + "%");
    tail_label->setMinimumWidth(36);
    layout->addWidget(tail_slider, row, 1);
    layout->addWidget(tail_label, row, 2);
    connect(tail_slider, &QSlider::valueChanged, this, [this, tail_label](int v){
        tail_length = v / 100.0f;
        if(tail_label) tail_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void ZigZag3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor ZigZag3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor ZigZag3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    if(progress > 1.0f) progress = std::fmod(progress, 1.0f);
    if(progress < 0.0f) progress = std::fmod(progress, 1.0f) + 1.0f;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = (rot.x - origin.x) / std::max(0.001f, grid.width);
    float ly = (rot.y - origin.y) / std::max(0.001f, grid.height);
    float lz = (rot.z - origin.z) / std::max(0.001f, grid.depth);
    lx = (lx + 1.0f) * 0.5f;
    ly = (ly + 1.0f) * 0.5f;
    lz = (lz + 1.0f) * 0.5f;
    lx = std::max(0.0f, std::min(1.0f, lx));
    ly = std::max(0.0f, std::min(1.0f, ly));
    lz = std::max(0.0f, std::min(1.0f, lz));

    const int n_cols = 24;
    const int n_rows = 24;
    float prim = 0.0f, sec = 0.0f;
    int axis = GetPathAxis();
    if(axis == 0) { prim = lx; sec = ly; }
    else if(axis == 1) { prim = ly; sec = lz; }
    else { prim = lz; sec = lx; }

    float col_cont = prim * (float)n_cols;
    float row_cont = sec * (float)n_rows;
    int seg = (int)col_cont;
    seg = std::max(0, std::min(n_cols - 1, seg));
    float local = (seg % 2 == 0) ? row_cont : ((float)n_rows - row_cont);
    float path_pos = (float)(seg * n_rows) + local;
    path_pos /= (float)(n_cols * n_rows);
    path_pos = std::max(0.0f, std::min(1.0f, path_pos));

    float tail = std::max(0.1f, std::min(0.8f, tail_length));
    float distance = 1.0f;
    if(path_mode == MODE_MARQUEE)
    {
        float band = tail * 0.5f;
        if(path_pos > progress || path_pos < progress - band) return 0x00000000;
        distance = 1.0f - 0.3f * (progress - path_pos) / band;
        distance = std::max(0.7f, std::min(1.0f, distance));
    }
    else
    {
        if(path_pos > progress) return 0x00000000;
        float dist_in_tail = progress - path_pos;
        if(dist_in_tail > tail) return 0x00000000;
        distance = 1.0f - (dist_in_tail / tail);
        distance = distance * distance * distance;
    }

    float hue = fmodf(path_pos * 360.0f - time * 50.0f, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(path_pos);
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * distance)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * distance)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * distance)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json ZigZag3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["path_mode"] = path_mode;
    j["tail_length"] = tail_length;
    return j;
}

void ZigZag3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("path_mode") && settings["path_mode"].is_number_integer())
        path_mode = std::max(0, std::min(settings["path_mode"].get<int>(), MODE_COUNT - 1));
    if(settings.contains("tail_length") && settings["tail_length"].is_number())
        tail_length = std::max(0.1f, std::min(0.8f, settings["tail_length"].get<float>()));
}
