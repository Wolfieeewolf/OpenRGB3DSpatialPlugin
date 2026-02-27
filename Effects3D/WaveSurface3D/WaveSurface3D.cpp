// SPDX-License-Identifier: GPL-2.0-only

#include "WaveSurface3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>

REGISTER_EFFECT_3D(WaveSurface3D);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

WaveSurface3D::WaveSurface3D(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D WaveSurface3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Wave Surface";
    info.effect_description = "3D wave surface (Mega-Cube Sinus style): height = sin(phase + radius), rotating";
    info.category = "3D Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
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
    return info;
}

void WaveSurface3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Surface thickness:"), row, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 100);
    thick_slider->setValue((int)(surface_thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(surface_thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, row, 1);
    layout->addWidget(thick_label, row, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        surface_thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Wave frequency:"), row, 0);
    QSlider* freq_slider = new QSlider(Qt::Horizontal);
    freq_slider->setRange(3, 30);
    freq_slider->setValue((int)(wave_frequency * 10.0f));
    QLabel* freq_label = new QLabel(QString::number(wave_frequency, 'f', 1));
    freq_label->setMinimumWidth(36);
    layout->addWidget(freq_slider, row, 1);
    layout->addWidget(freq_label, row, 2);
    connect(freq_slider, &QSlider::valueChanged, this, [this, freq_label](int v){
        wave_frequency = v / 10.0f;
        if(freq_label) freq_label->setText(QString::number(wave_frequency, 'f', 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Wave amplitude:"), row, 0);
    QSlider* amp_slider = new QSlider(Qt::Horizontal);
    amp_slider->setRange(20, 200);
    amp_slider->setValue((int)(wave_amplitude * 100.0f));
    QLabel* amp_label = new QLabel(QString::number((int)(wave_amplitude * 100)) + "%");
    amp_label->setMinimumWidth(36);
    layout->addWidget(amp_slider, row, 1);
    layout->addWidget(amp_label, row, 2);
    connect(amp_slider, &QSlider::valueChanged, this, [this, amp_label](int v){
        wave_amplitude = v / 100.0f;
        if(amp_label) amp_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Wave travel speed:"), row, 0);
    QSlider* travel_slider = new QSlider(Qt::Horizontal);
    travel_slider->setRange(0, 200);
    travel_slider->setValue((int)(wave_travel_speed * 100.0f));
    QLabel* travel_label = new QLabel(QString::number(wave_travel_speed, 'f', 2));
    travel_label->setMinimumWidth(36);
    layout->addWidget(travel_slider, row, 1);
    layout->addWidget(travel_label, row, 2);
    connect(travel_slider, &QSlider::valueChanged, this, [this, travel_label](int v){
        wave_travel_speed = v / 100.0f;
        if(travel_label) travel_label->setText(QString::number(wave_travel_speed, 'f', 2));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Wave direction:"), row, 0);
    QSlider* dir_slider = new QSlider(Qt::Horizontal);
    dir_slider->setRange(0, 360);
    dir_slider->setValue((int)wave_direction_deg);
    QLabel* dir_label = new QLabel(QString::number((int)wave_direction_deg) + "°");
    dir_label->setMinimumWidth(36);
    layout->addWidget(dir_slider, row, 1);
    layout->addWidget(dir_label, row, 2);
    connect(dir_slider, &QSlider::valueChanged, this, [this, dir_label](int v){
        wave_direction_deg = (float)v;
        if(dir_label) dir_label->setText(QString::number(v) + "°");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void WaveSurface3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor WaveSurface3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor WaveSurface3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress_val = CalculateProgress(time);
    float phase = progress_val * (float)(2.0 * M_PI);
    float half = 0.5f * std::max(grid.width, std::max(grid.height, grid.depth)) * GetNormalizedScale();
    if(half < 1e-5f) half = 1.0f;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = (rot.x - origin.x) / half;
    float ly = (rot.y - origin.y) / half;
    float lz = (rot.z - origin.z) / half;

    float r = sqrtf(lx*lx + lz*lz);
    float freq = std::max(0.2f, std::min(4.0f, wave_frequency));
    float amp = std::max(0.2f, std::min(2.0f, wave_amplitude));
    float dir_rad = wave_direction_deg * (float)(M_PI / 180.0);
    float wave_pos = (float)(cos(dir_rad) * lx + sin(dir_rad) * lz);
    // Traveling wave: phase moves along wave_pos over time (surface moves in direction)
    float travel = wave_travel_speed * time * (float)(2.0 * M_PI);
    float surface_y = amp * sinf(phase + freq * r + wave_pos * 2.0f + travel);
    float d = fabsf(ly - surface_y);
    float sigma = std::max(surface_thickness, 0.02f);
    const float d_cutoff = 3.0f * sigma * std::max(1.0f, amp);
    if(d > d_cutoff) return 0x00000000;
    float intensity = expf(-d * d / (sigma * sigma));
    intensity = fminf(1.0f, intensity);

    float hue = fmodf((surface_y / amp + 1.0f) * 90.0f + progress_val * 60.0f, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    float pos_norm = (surface_y / amp + 1.0f) * 0.5f;
    RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(std::max(0.0f, std::min(1.0f, pos_norm)));
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json WaveSurface3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["surface_thickness"] = surface_thickness;
    j["wave_frequency"] = wave_frequency;
    j["wave_amplitude"] = wave_amplitude;
    j["wave_travel_speed"] = wave_travel_speed;
    j["wave_direction_deg"] = wave_direction_deg;
    return j;
}

void WaveSurface3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("surface_thickness") && settings["surface_thickness"].is_number())
    { float v = settings["surface_thickness"].get<float>(); surface_thickness = std::max(0.02f, std::min(1.0f, v)); }
    if(settings.contains("wave_frequency") && settings["wave_frequency"].is_number())
    { float v = settings["wave_frequency"].get<float>(); wave_frequency = std::max(0.2f, std::min(4.0f, v)); }
    if(settings.contains("wave_amplitude") && settings["wave_amplitude"].is_number())
    { float v = settings["wave_amplitude"].get<float>(); wave_amplitude = std::max(0.2f, std::min(2.0f, v)); }
    if(settings.contains("wave_travel_speed") && settings["wave_travel_speed"].is_number())
    { float v = settings["wave_travel_speed"].get<float>(); wave_travel_speed = std::max(0.0f, std::min(2.0f, v)); }
    if(settings.contains("wave_direction_deg") && settings["wave_direction_deg"].is_number())
    { float v = settings["wave_direction_deg"].get<float>(); wave_direction_deg = fmodf(v + 360.0f, 360.0f); }
}
