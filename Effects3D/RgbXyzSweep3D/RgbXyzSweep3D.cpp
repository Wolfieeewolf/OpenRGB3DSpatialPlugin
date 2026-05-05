// SPDX-License-Identifier: GPL-2.0-only

#include "RgbXyzSweep3D.h"

#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(RgbXyzSweep3D);

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

inline float Phase01(float time_sec, float cycle_seconds, float speed_mul)
{
    if(cycle_seconds < 1e-4f)
        return 0.f;
    return std::fmod((time_sec * speed_mul) / cycle_seconds + 1000.f, 1.f);
}

inline RGBColor Rgb01(float r, float g, float b)
{
    const int ri = std::clamp((int)std::lround(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::lround(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::lround(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}
} // namespace

RgbXyzSweep3D::RgbXyzSweep3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    SetSpeed(45);
}

RgbXyzSweep3D::~RgbXyzSweep3D() = default;

EffectInfo3D RgbXyzSweep3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Axis Sweep 3D";
    info.effect_description =
        "Diagnostic sweep band: red sweeps X, green sweeps Y, blue sweeps Z. Useful for map orientation checks.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_RGB_XYZ_SWEEP_3D;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = false;
    info.default_speed_scale = 45.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = false;
    return info;
}

void RgbXyzSweep3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);
    QGridLayout* g = new QGridLayout();
    g->setContentsMargins(0, 0, 0, 0);
    vbox->addLayout(g);

    g->addWidget(new QLabel("Band style:"), 0, 0);
    style_slider = new QSlider(Qt::Horizontal);
    style_slider->setRange(0, 1);
    style_slider->setValue(band_style);
    style_slider->setToolTip("0 = sharp band, 1 = smooth band.");
    g->addWidget(style_slider, 0, 1);
    style_label = new QLabel(band_style == 0 ? "Sharp" : "Smooth");
    g->addWidget(style_label, 0, 2);

    connect(style_slider, &QSlider::valueChanged, this, [this](int v) {
        band_style = std::clamp(v, 0, 1);
        if(style_label)
            style_label->setText(band_style == 0 ? "Sharp" : "Smooth");
        emit ParametersChanged();
    });
    AddWidgetToParent(w, parent);
}

void RgbXyzSweep3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_RGB_XYZ_SWEEP_3D;
}

RGBColor RgbXyzSweep3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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

    const float spd = std::max(0.08f, GetScaledSpeed() / 100.0f);
    const float t_now = Phase01(time, 22.222222f, spd); // time(.045)

    int axis = 0;
    float sweep = 0.0f;
    if(t_now < (1.0f / 3.0f))
    {
        axis = 0;
        sweep = t_now * 3.0f;
    }
    else if(t_now < (2.0f / 3.0f))
    {
        axis = 1;
        sweep = (t_now - (1.0f / 3.0f)) * 3.0f;
    }
    else
    {
        axis = 2;
        sweep = (t_now - (2.0f / 3.0f)) * 3.0f;
    }

    const float width = std::clamp(0.03f + GetNormalizedSize() * 0.22f, 0.02f, 0.35f);
    const float range = 1.0f + width * 2.0f;
    const float sweep_full = sweep * range - width;

    float plane = nx;
    float r = 1.0f;
    float g = 0.0f;
    float b = 0.0f;
    if(axis == 1)
    {
        plane = ny;
        r = 0.0f;
        g = 1.0f;
        b = 0.0f;
    }
    else if(axis == 2)
    {
        plane = nz;
        r = 0.0f;
        g = 0.0f;
        b = 1.0f;
    }

    const float d = std::fabs(plane - sweep_full);
    if(d > width)
        return 0x00000000;

    float v = 1.0f;
    if(band_style == 1)
    {
        float frac = (plane - (sweep_full - width)) / (2.0f * width);
        frac = std::clamp(frac, 0.0f, 1.0f);
        v = 0.5f + 0.5f * std::sin(kTwoPi * (frac - 0.25f));
        v = std::clamp(v, 0.0f, 1.0f);
    }
    return Rgb01(r * v, g * v, b * v);
}

nlohmann::json RgbXyzSweep3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["rgb_xyz_sweep_style"] = band_style;
    return j;
}

void RgbXyzSweep3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("rgb_xyz_sweep_style") && settings["rgb_xyz_sweep_style"].is_number_integer())
        band_style = std::clamp(settings["rgb_xyz_sweep_style"].get<int>(), 0, 1);
    if(style_slider)
    {
        style_slider->setValue(band_style);
        if(style_label)
            style_label->setText(band_style == 0 ? "Sharp" : "Smooth");
    }
}
