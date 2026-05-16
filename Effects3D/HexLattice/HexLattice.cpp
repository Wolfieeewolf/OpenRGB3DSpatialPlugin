// SPDX-License-Identifier: GPL-2.0-only

#include "HexLattice.h"
#include "SpatialKernelColormap.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"

#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(HexLattice);

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

inline float Phase01(float time_sec, float cycle_seconds, float speed_mul)
{
    if(cycle_seconds < 1e-4f)
        return 0.f;
    return std::fmod((time_sec * speed_mul) / cycle_seconds + 1000.f, 1.f);
}

inline float Wave01(float x01)
{
    return 0.5f + 0.5f * std::sin(kTwoPi * x01);
}

inline float Triangle01(float x01)
{
    const float f = x01 - std::floor(x01);
    return 1.0f - std::fabs(2.0f * f - 1.0f);
}
}

RGBColor HexLattice::Hsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
        h += 1.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    const float hf = h * 6.0f;
    const int i = (int)std::floor(hf) % 6;
    const float f = hf - std::floor(hf);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);
    switch(i)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }

    const int ri = std::clamp((int)std::lround(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::lround(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::lround(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}

HexLattice::HexLattice(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(35);
    SetFrequency(12);
}

HexLattice::~HexLattice() = default;

EffectInfo3D HexLattice::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Hex Lattice";
    info.effect_description =
        "Hex lattice 3D field. Blends sin/cos in XYZ with animated zoom and triangular hue shaping.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_HEX_LATTICE;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_speed_scale = 35.0f;
    info.default_frequency_scale = 12.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.supports_strip_colormap = true;

    return info;
}

void HexLattice::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* grid_layout = new QGridLayout(w);
    int row = 0;

    grid_layout->addWidget(new QLabel("Breathing amount:"), row, 0);
    breathing_amount_slider = new QSlider(Qt::Horizontal);
    breathing_amount_slider->setRange(0, 200);
    breathing_amount_slider->setValue((int)std::lround(breathing_amount * 100.0f));
    breathing_amount_label = new QLabel(QString::number((int)std::lround(breathing_amount * 100.0f)) + "%");
    breathing_amount_label->setMinimumWidth(40);
    grid_layout->addWidget(breathing_amount_slider, row, 1);
    grid_layout->addWidget(breathing_amount_label, row, 2);
    ++row;

    grid_layout->addWidget(new QLabel("Pulse amount:"), row, 0);
    pulse_amount_slider = new QSlider(Qt::Horizontal);
    pulse_amount_slider->setRange(0, 200);
    pulse_amount_slider->setValue((int)std::lround(pulse_amount * 100.0f));
    pulse_amount_label = new QLabel(QString::number((int)std::lround(pulse_amount * 100.0f)) + "%");
    pulse_amount_label->setMinimumWidth(40);
    grid_layout->addWidget(pulse_amount_slider, row, 1);
    grid_layout->addWidget(pulse_amount_label, row, 2);
    ++row;

    grid_layout->addWidget(new QLabel("Flow mode:"), row, 0);
    flow_mode_combo = new QComboBox();
    flow_mode_combo->addItem("Calm");
    flow_mode_combo->addItem("Active");
    flow_mode_combo->addItem("Aggressive");
    flow_mode_combo->setCurrentIndex(std::clamp(flow_mode, 0, 2));
    flow_mode_combo->setToolTip("Changes overall flow intensity and directional drift style.");
    grid_layout->addWidget(flow_mode_combo, row, 1, 1, 2);
    ++row;

    grid_layout->addWidget(new QLabel("Turbulence amount:"), row, 0);
    turbulence_amount_slider = new QSlider(Qt::Horizontal);
    turbulence_amount_slider->setRange(0, 200);
    turbulence_amount_slider->setValue((int)std::lround(turbulence_amount * 100.0f));
    turbulence_amount_label = new QLabel(QString::number((int)std::lround(turbulence_amount * 100.0f)) + "%");
    turbulence_amount_label->setMinimumWidth(40);
    grid_layout->addWidget(turbulence_amount_slider, row, 1);
    grid_layout->addWidget(turbulence_amount_label, row, 2);

    connect(breathing_amount_slider, &QSlider::valueChanged, this, [this](int v) {
        breathing_amount = std::clamp(v / 100.0f, 0.0f, 2.0f);
        if(breathing_amount_label)
            breathing_amount_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    connect(pulse_amount_slider, &QSlider::valueChanged, this, [this](int v) {
        pulse_amount = std::clamp(v / 100.0f, 0.0f, 2.0f);
        if(pulse_amount_label)
            pulse_amount_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    connect(flow_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        flow_mode = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    connect(turbulence_amount_slider, &QSlider::valueChanged, this, [this](int v) {
        turbulence_amount = std::clamp(v / 100.0f, 0.0f, 2.0f);
        if(turbulence_amount_label)
            turbulence_amount_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
AddWidgetToParent(w, parent);
}

void HexLattice::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_HEX_LATTICE;
}

RGBColor HexLattice::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    const float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    const float speed_norm = std::max(0.05f, GetNormalizedSpeed());
    const float freq_norm = std::max(0.05f, GetNormalizedFrequency());
    const float detail_norm = std::max(0.05f, GetNormalizedDetail());

    const float flow_mode_mul[3] = {0.68f, 1.0f, 1.55f};
    const float flow_mul = flow_mode_mul[std::clamp(flow_mode, 0, 2)];
    const float flow_rate = (0.15f + speed_norm * (0.90f + 1.20f * freq_norm)) * flow_mul;
    const float flow_t = time * flow_rate;

    const float base_scale = std::max(0.2f, GetNormalizedSize());
    const float lattice_density = 2.4f + freq_norm * (3.0f + 4.0f * detail_norm);
    const float breathe_base = Wave01(flow_t * (0.22f + 0.30f * speed_norm));
    const float breathe = 1.0f + ((breathe_base - 0.5f) * 0.96f) * breathing_amount;
    const float zoom = lattice_density * base_scale * breathe;

    const float px = kTwoPi * (flow_t * (0.30f + 0.85f * detail_norm));
    const float py = kTwoPi * (flow_t * (0.24f + 0.65f * freq_norm) + 0.17f);
    const float pz = kTwoPi * (flow_t * (0.19f + 0.75f * speed_norm) + 0.41f);
    const float drift_x = flow_t * (0.18f + 0.45f * freq_norm);
    const float drift_y = flow_t * (0.14f + 0.35f * detail_norm);
    const float drift_z = flow_t * (0.22f + 0.30f * speed_norm);

    const float turbulence = std::clamp(turbulence_amount, 0.0f, 2.0f);
    const float harmonic = 0.55f + 0.90f * detail_norm + 0.75f * turbulence;
    const float swirl = turbulence * (0.07f + 0.11f * detail_norm);
    float h = 0.0f;
    h += std::sin((nx + drift_x + swirl * std::sin(py)) * zoom + px);
    h += std::cos((ny - drift_y + swirl * std::cos(pz)) * (zoom * (0.95f + 0.35f * freq_norm)) + py);
    h += std::sin((nz + drift_z + swirl * std::sin(px)) * (zoom * (1.05f + 0.40f * detail_norm)) + pz);
    h += 0.65f * std::sin((nx + ny - nz) * (zoom * harmonic) + (px - py) * 0.6f);
    h += 0.45f * std::cos((nx - ny + nz) * (zoom * (0.8f + 0.6f * detail_norm + 0.5f * turbulence)) + (py + pz) * 0.5f);
    h /= 3.10f;

    float v = Wave01(h * (1.0f + 0.9f * detail_norm) + flow_t * (0.04f + 0.10f * freq_norm));
    const float contrast = 1.2f + detail_norm * 2.2f + pulse_amount * 0.8f;
    v = std::pow(std::clamp(v, 0.0f, 1.0f), contrast);
    const float glow = 0.20f + (0.28f + 0.34f * pulse_amount) * Wave01(flow_t * (0.35f + 0.45f * speed_norm));
    v = std::clamp(v + glow * (0.10f + 0.16f * detail_norm + 0.16f * pulse_amount), 0.0f, 1.0f);

    float hue_flow = flow_t * (0.03f + 0.10f * freq_norm);
    h = Triangle01(h * (0.8f + 1.2f * detail_norm)) * 0.55f + hue_flow;

    RGBColor c = 0x00000000;
    const float h01 = h - std::floor(h);
    if(UseEffectStripColormap())
    {
        float p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                               GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               h01,
                                               time,
                                               grid,
                                               GetNormalizedSize(),
                                               origin,
                                               rot);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        c = ResolveStripKernelFinalColor(*this,
                                         SpatialPatternKernelClamp(GetEffectStripColormapKernel()),
                                         p01,
                                         GetEffectStripColormapColorStyle(),
                                         time,
                                         flow_rate * 6.0f);
    }
    else if(GetRainbowMode())
    {
        c = Hsv01ToBgr(h, 1.0f, 1.0f);
    }
    else
    {
        c = GetColorAtPosition(h01);
    }

    int r = (int)((float)(c & 0xFF) * v);
    int g = (int)((float)((c >> 8) & 0xFF) * v);
    int b = (int)((float)((c >> 16) & 0xFF) * v);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json HexLattice::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["hexlattice_breathing_amount"] = breathing_amount;
    j["hexlattice_pulse_amount"] = pulse_amount;
    j["hexlattice_flow_mode"] = flow_mode;
    j["hexlattice_turbulence_amount"] = turbulence_amount;
    return j;
}

void HexLattice::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("hexlattice_breathing_amount") && settings["hexlattice_breathing_amount"].is_number())
        breathing_amount = std::clamp(settings["hexlattice_breathing_amount"].get<float>(), 0.0f, 2.0f);
    if(settings.contains("hexlattice_pulse_amount") && settings["hexlattice_pulse_amount"].is_number())
        pulse_amount = std::clamp(settings["hexlattice_pulse_amount"].get<float>(), 0.0f, 2.0f);
    if(settings.contains("hexlattice_flow_mode") && settings["hexlattice_flow_mode"].is_number_integer())
        flow_mode = std::clamp(settings["hexlattice_flow_mode"].get<int>(), 0, 2);
    if(settings.contains("hexlattice_turbulence_amount") && settings["hexlattice_turbulence_amount"].is_number())
        turbulence_amount = std::clamp(settings["hexlattice_turbulence_amount"].get<float>(), 0.0f, 2.0f);

    if(breathing_amount_slider)
    {
        const int v = (int)std::lround(breathing_amount * 100.0f);
        breathing_amount_slider->setValue(v);
        if(breathing_amount_label)
            breathing_amount_label->setText(QString::number(v) + "%");
    }
    if(pulse_amount_slider)
    {
        const int v = (int)std::lround(pulse_amount * 100.0f);
        pulse_amount_slider->setValue(v);
        if(pulse_amount_label)
            pulse_amount_label->setText(QString::number(v) + "%");
    }
    if(flow_mode_combo)
        flow_mode_combo->setCurrentIndex(std::clamp(flow_mode, 0, 2));
    if(turbulence_amount_slider)
    {
        const int v = (int)std::lround(turbulence_amount * 100.0f);
        turbulence_amount_slider->setValue(v);
        if(turbulence_amount_label)
            turbulence_amount_label->setText(QString::number(v) + "%");
    }
}
