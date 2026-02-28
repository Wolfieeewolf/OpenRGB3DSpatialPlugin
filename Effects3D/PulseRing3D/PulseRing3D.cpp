// SPDX-License-Identifier: GPL-2.0-only

#include "PulseRing3D.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>

REGISTER_EFFECT_3D(PulseRing3D);

const char* PulseRing3D::StyleName(int s)
{
    switch(s) { case STYLE_PULSE_RING: return "Pulse Ring"; case STYLE_RADIAL_RAINBOW: return "Radial Rainbow"; default: return "Pulse Ring"; }
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PulseRing3D::PulseRing3D(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D PulseRing3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Pulse Ring";
    info.effect_description = "Pulsing donut rings that expand from the center outward, leaving a hole in the middle";
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

void PulseRing3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Style:"), row, 0);
    QComboBox* style_combo = new QComboBox();
    for(int s = 0; s < STYLE_COUNT; s++) style_combo->addItem(StyleName(s));
    style_combo->setCurrentIndex(std::max(0, std::min(ring_style, STYLE_COUNT - 1)));
    layout->addWidget(style_combo, row, 1, 1, 2);
    connect(style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        ring_style = std::max(0, std::min(idx, STYLE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Ring thickness:"), row, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 100);
    thick_slider->setValue((int)(ring_thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(ring_thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, row, 1);
    layout->addWidget(thick_label, row, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        ring_thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Hole size:"), row, 0);
    QSlider* hole_slider = new QSlider(Qt::Horizontal);
    hole_slider->setRange(0, 80);
    hole_slider->setValue((int)(hole_size * 100.0f));
    QLabel* hole_label = new QLabel(QString::number((int)(hole_size * 100)) + "%");
    hole_label->setMinimumWidth(36);
    layout->addWidget(hole_slider, row, 1);
    layout->addWidget(hole_label, row, 2);
    connect(hole_slider, &QSlider::valueChanged, this, [this, hole_label](int v){
        hole_size = v / 100.0f;
        if(hole_label) hole_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Pulse frequency:"), row, 0);
    QSlider* freq_slider = new QSlider(Qt::Horizontal);
    freq_slider->setRange(3, 30);
    freq_slider->setValue((int)(pulse_frequency * 10.0f));
    QLabel* freq_label = new QLabel(QString::number(pulse_frequency, 'f', 1));
    freq_label->setMinimumWidth(36);
    layout->addWidget(freq_slider, row, 1);
    layout->addWidget(freq_label, row, 2);
    connect(freq_slider, &QSlider::valueChanged, this, [this, freq_label](int v){
        pulse_frequency = v / 10.0f;
        if(freq_label) freq_label->setText(QString::number(pulse_frequency, 'f', 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Pulse amplitude:"), row, 0);
    QSlider* amp_slider = new QSlider(Qt::Horizontal);
    amp_slider->setRange(20, 200);
    amp_slider->setValue((int)(pulse_amplitude * 100.0f));
    QLabel* amp_label = new QLabel(QString::number((int)(pulse_amplitude * 100)) + "%");
    amp_label->setMinimumWidth(36);
    layout->addWidget(amp_slider, row, 1);
    layout->addWidget(amp_label, row, 2);
    connect(amp_slider, &QSlider::valueChanged, this, [this, amp_label](int v){
        pulse_amplitude = v / 100.0f;
        if(amp_label) amp_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Direction:"), row, 0);
    QSlider* dir_slider = new QSlider(Qt::Horizontal);
    dir_slider->setRange(0, 360);
    dir_slider->setValue((int)direction_deg);
    QLabel* dir_label = new QLabel(QString::number((int)direction_deg) + "°");
    dir_label->setMinimumWidth(36);
    layout->addWidget(dir_slider, row, 1);
    layout->addWidget(dir_label, row, 2);
    connect(dir_slider, &QSlider::valueChanged, this, [this, dir_label](int v){
        direction_deg = (float)v;
        if(dir_label) dir_label->setText(QString::number(v) + "°");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void PulseRing3D::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor PulseRing3D::CalculateColor(float, float, float, float) { return 0x00000000; }

RGBColor PulseRing3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    float half = 0.5f * std::max(grid.width, std::max(grid.height, grid.depth)) * GetNormalizedScale();
    if(half < 1e-5f) half = 1.0f;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = (rot.x - origin.x) / half;
    float lz = (rot.z - origin.z) / half;
    float r = sqrtf(lx * lx + lz * lz);
    float hole_r = std::max(0.0f, std::min(0.8f, hole_size));
    float max_r = 1.0f;
    float usable = std::max(0.01f, max_r - hole_r);
    float pos_norm = (r - hole_r) / usable;
    pos_norm = std::max(0.0f, std::min(1.0f, pos_norm));

    int style = std::max(0, std::min(ring_style, STYLE_COUNT - 1));
    float intensity = 1.0f;

    if(style == STYLE_RADIAL_RAINBOW)
    {
        if(r < hole_r - 0.02f) return 0x00000000;
        intensity = 1.0f;
    }
    else
    {
        float phase = progress * (float)(2.0 * M_PI);
        float freq = std::max(0.3f, std::min(3.0f, pulse_frequency));
        float amp = std::max(0.2f, std::min(2.0f, pulse_amplitude));
        float sigma = std::max(ring_thickness, 0.02f);
        float phase_offset = direction_deg / 360.0f;
        float expand_progress = fmodf(progress + phase_offset, 1.0f);
        float ring_center = hole_r + expand_progress * usable;
        float d = fabsf(r - ring_center);
        const float d_cutoff = 3.0f * sigma * std::max(1.0f, amp);
        if(d > d_cutoff) return 0x00000000;
        if(r < hole_r - 0.02f) return 0x00000000;
        intensity = expf(-d * d / (sigma * sigma));
        float pulse_mod = 0.5f + 0.5f * sinf(phase * freq);
        intensity *= amp * pulse_mod;
    }
    intensity = std::min(1.0f, std::max(0.0f, intensity));

    float hue = fmodf(pos_norm * 360.0f + progress * (style == STYLE_RADIAL_RAINBOW ? 30.0f : 80.0f), 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(pos_norm);
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json PulseRing3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["ring_style"] = ring_style;
    j["ring_thickness"] = ring_thickness;
    j["hole_size"] = hole_size;
    j["pulse_frequency"] = pulse_frequency;
    j["pulse_amplitude"] = pulse_amplitude;
    j["direction_deg"] = direction_deg;
    return j;
}

void PulseRing3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("ring_style") && settings["ring_style"].is_number_integer())
        ring_style = std::max(0, std::min(settings["ring_style"].get<int>(), STYLE_COUNT - 1));
    if(settings.contains("ring_thickness") && settings["ring_thickness"].is_number())
    { float v = settings["ring_thickness"].get<float>(); ring_thickness = std::max(0.02f, std::min(1.0f, v)); }
    if(settings.contains("hole_size") && settings["hole_size"].is_number())
    { float v = settings["hole_size"].get<float>(); hole_size = std::max(0.0f, std::min(0.8f, v)); }
    if(settings.contains("pulse_frequency") && settings["pulse_frequency"].is_number())
    { float v = settings["pulse_frequency"].get<float>(); pulse_frequency = std::max(0.3f, std::min(3.0f, v)); }
    if(settings.contains("pulse_amplitude") && settings["pulse_amplitude"].is_number())
    { float v = settings["pulse_amplitude"].get<float>(); pulse_amplitude = std::max(0.2f, std::min(2.0f, v)); }
    if(settings.contains("direction_deg") && settings["direction_deg"].is_number())
    { float v = settings["direction_deg"].get<float>(); direction_deg = fmodf(v + 360.0f, 360.0f); }
}
