// SPDX-License-Identifier: GPL-2.0-only

#include "HarmonicPulse.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include <QColor>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(HarmonicPulse);

namespace
{
inline float Phase01(float time_sec, float cycle_seconds, float speed_mul)
{
    if(cycle_seconds < 1e-4f)
        return 0.f;
    return std::fmod((time_sec * speed_mul) / cycle_seconds + 1000.f, 1.f);
}
}

RGBColor HarmonicPulse::Hsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
        h += 1.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    float r = 0, g = 0, b = 0;
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - (float)i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch(i % 6)
    {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:
        r = v;
        g = p;
        b = q;
        break;
    }
    int ri = (int)std::lround(r * 255.0f);
    int gi = (int)std::lround(g * 255.0f);
    int bi = (int)std::lround(b * 255.0f);
    ri = std::clamp(ri, 0, 255);
    gi = std::clamp(gi, 0, 255);
    bi = std::clamp(bi, 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}

HarmonicPulse::HarmonicPulse(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(55);
    SetRainbowMode(false);
}

HarmonicPulse::~HarmonicPulse() = default;

EffectInfo3D HarmonicPulse::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Harmonic Pulse";
    info.effect_description =
        "Harmonic 3D pulse: interference of sin/cos on normalized x,y,z with optional zoom wobble, "
        "flow control, and pulse contrast tuning for both compact and large LED volumes.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_HARMONIC_PULSE;
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

void HarmonicPulse::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);
    QGridLayout* g = new QGridLayout();
    int row = 0;

    g->addWidget(new QLabel("Zoom wobble:"), row, 0);
    wobble_slider = new QSlider(Qt::Horizontal);
    wobble_slider->setRange(0, 600);
    wobble_slider->setValue((int)std::lround(zoom_wobble_strength * 100.0f));
    wobble_label = new QLabel(QString::number(zoom_wobble_strength, 'f', 2));
    wobble_slider->setToolTip("Amplitude k in zoom = 1 + wave·k. Use 0 for stable scale.");
    g->addWidget(wobble_slider, row, 1);
    g->addWidget(wobble_label, row, 2);
    connect(wobble_slider, &QSlider::valueChanged, this, [this](int v) {
        zoom_wobble_strength = std::clamp(v / 100.0f, 0.0f, 6.0f);
        if(wobble_label)
            wobble_label->setText(QString::number(zoom_wobble_strength, 'f', 2));
        emit ParametersChanged();
    });
    row++;

    g->addWidget(new QLabel("Flow amount:"), row, 0);
    flow_slider = new QSlider(Qt::Horizontal);
    flow_slider->setRange(30, 300);
    flow_slider->setValue((int)std::lround(flow_amount * 100.0f));
    flow_label = new QLabel(QString::number((int)std::lround(flow_amount * 100.0f)) + "%");
    flow_slider->setToolTip("Scales motion speed independent of shared speed/frequency.");
    g->addWidget(flow_slider, row, 1);
    g->addWidget(flow_label, row, 2);
    connect(flow_slider, &QSlider::valueChanged, this, [this](int v) {
        flow_amount = std::clamp(v / 100.0f, 0.3f, 3.0f);
        if(flow_label)
            flow_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    g->addWidget(new QLabel("Pulse contrast:"), row, 0);
    contrast_slider = new QSlider(Qt::Horizontal);
    contrast_slider->setRange(40, 280);
    contrast_slider->setValue((int)std::lround(pulse_contrast * 100.0f));
    contrast_label = new QLabel(QString::number((int)std::lround(pulse_contrast * 100.0f)) + "%");
    contrast_slider->setToolTip("Higher values sharpen peaks; lower values soften pulses.");
    g->addWidget(contrast_slider, row, 1);
    g->addWidget(contrast_label, row, 2);
    connect(contrast_slider, &QSlider::valueChanged, this, [this](int v) {
        pulse_contrast = std::clamp(v / 100.0f, 0.4f, 2.8f);
        if(contrast_label)
            contrast_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    g->addWidget(new QLabel("Large setup boost:"), row, 0);
    setup_boost_slider = new QSlider(Qt::Horizontal);
    setup_boost_slider->setRange(0, 200);
    setup_boost_slider->setValue((int)std::lround(large_setup_boost * 100.0f));
    setup_boost_label = new QLabel(QString::number((int)std::lround(large_setup_boost * 100.0f)) + "%");
    setup_boost_slider->setToolTip("Adds density scaling from volume size so large installations stay active.");
    g->addWidget(setup_boost_slider, row, 1);
    g->addWidget(setup_boost_label, row, 2);
    connect(setup_boost_slider, &QSlider::valueChanged, this, [this](int v) {
        large_setup_boost = std::clamp(v / 100.0f, 0.0f, 2.0f);
        if(setup_boost_label)
            setup_boost_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    vbox->addLayout(g);
AddWidgetToParent(w, parent);
}

void HarmonicPulse::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_HARMONIC_PULSE;
}

RGBColor HarmonicPulse::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    const float spd = std::max(0.08f, GetScaledSpeed() / 100.0f);
    const float rate = std::max(0.08f, GetScaledFrequency() / 50.0f);
    const float motion = spd * rate * std::clamp(flow_amount, 0.3f, 3.0f);

    const float t1 = Phase01(time, 20.0f, motion) * TWO_PI;
    const float t2 = Phase01(time, 11.111111f, motion) * TWO_PI;
    const float zw = Phase01(time, 5.0f, motion) * TWO_PI;
    const float wave = 0.5f + 0.5f * std::sin(zw);
    const float zoom = 1.0f + wave * std::clamp(zoom_wobble_strength, 0.0f, 6.0f);

    const float size_mul = std::max(0.2f, GetNormalizedSize());
    const float volume_points = std::max(1.0f, grid.width * grid.height * grid.depth);
    const float volume_scale = std::cbrt(volume_points) / 8.0f;
    const float setup_density = 1.0f + std::clamp(large_setup_boost, 0.0f, 2.0f) * std::clamp(volume_scale - 1.0f, 0.0f, 1.8f);
    const float xf = nx * zoom * size_mul * setup_density;
    const float yf = ny * zoom * size_mul * setup_density;
    const float zf = nz * zoom * size_mul * setup_density;

    float h = (1.0f + std::sin(xf + t1) + std::cos(yf + t2) + std::sin(zf + t1 - t2)) * 0.5f;
    float val = std::clamp(0.5f * h * h * h, 0.0f, 1.0f);
    val = std::pow(val, std::clamp(pulse_contrast, 0.4f, 2.8f));

    float h01 = std::fmod(h, 1.0f);
    if(h01 < 0.0f)
        h01 += 1.0f;

    const float rainbow_rate = rate * 12.0f;

    if(UseEffectStripColormap())
    {
        const float size_m = GetNormalizedSize();
        const float ph01 =
            std::fmod(CalculateProgress(time) * 0.35f + time * rainbow_rate * 0.01f + h01 * 0.2f + 1.f, 1.f);
        float pal01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                 GetEffectStripColormapRepeats(),
                                                 GetEffectStripColormapUnfold(),
                                                 GetEffectStripColormapDirectionDeg(),
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rot);
        pal01 = ApplyVoxelDriveToPalette01(pal01, x, y, z, time, grid);
        const int kid = SpatialPatternKernelClamp(GetEffectStripColormapKernel());
        RGBColor c = ResolveStripKernelFinalColor(*this,
                                                  kid,
                                                  std::clamp(pal01, 0.0f, 1.0f),
                                                  GetEffectStripColormapColorStyle(),
                                                  time,
                                                  rainbow_rate * 0.02f);
        const int cr = (int)(c & 0xFF);
        const int cg = (int)((c >> 8) & 0xFF);
        const int cb = (int)((c >> 16) & 0xFF);
        QColor qc = QColor::fromRgb(cr, cg, cb);
        const QColor hsv = qc.toHsv();
        const float ch = static_cast<float>(hsv.hueF());
        const float cv = static_cast<float>(hsv.valueF());
        const float hue01_kernel = (ch >= 0.0f) ? std::fmod(ch + 1.0f, 1.0f) : h01;
        return Hsv01ToBgr(hue01_kernel, 1.0f, std::clamp(val * cv, 0.0f, 1.0f));
    }

    if(GetRainbowMode())
    {
        float hh = std::fmod(h01 + time * rainbow_rate * 0.01f + CalculateProgress(time) * 0.25f, 1.0f);
        if(hh < 0.0f)
            hh += 1.0f;
        return Hsv01ToBgr(hh, 1.0f, val);
    }

    RGBColor c = GetColorAtPosition(h01);
    int r = (int)((float)(c & 0xFF) * val);
    int g = (int)((float)((c >> 8) & 0xFF) * val);
    int b = (int)((float)((c >> 16) & 0xFF) * val);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json HarmonicPulse::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["harmonic_zoom_wobble"] = zoom_wobble_strength;
    j["harmonic_flow_amount"] = flow_amount;
    j["harmonic_pulse_contrast"] = pulse_contrast;
    j["harmonic_large_setup_boost"] = large_setup_boost;
return j;
}

void HarmonicPulse::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("harmonic_zoom_wobble") && settings["harmonic_zoom_wobble"].is_number())
        zoom_wobble_strength =
            std::clamp(settings["harmonic_zoom_wobble"].get<float>(), 0.0f, 6.0f);
    if(settings.contains("harmonic_flow_amount") && settings["harmonic_flow_amount"].is_number())
        flow_amount = std::clamp(settings["harmonic_flow_amount"].get<float>(), 0.3f, 3.0f);
    if(settings.contains("harmonic_pulse_contrast") && settings["harmonic_pulse_contrast"].is_number())
        pulse_contrast = std::clamp(settings["harmonic_pulse_contrast"].get<float>(), 0.4f, 2.8f);
    if(settings.contains("harmonic_large_setup_boost") && settings["harmonic_large_setup_boost"].is_number())
        large_setup_boost = std::clamp(settings["harmonic_large_setup_boost"].get<float>(), 0.0f, 2.0f);
if(wobble_slider)
    {
        wobble_slider->setValue((int)std::lround(zoom_wobble_strength * 100.0f));
        if(wobble_label)
            wobble_label->setText(QString::number(zoom_wobble_strength, 'f', 2));
    }
    if(flow_slider)
    {
        const int v = (int)std::lround(flow_amount * 100.0f);
        flow_slider->setValue(v);
        if(flow_label)
            flow_label->setText(QString::number(v) + "%");
    }
    if(contrast_slider)
    {
        const int v = (int)std::lround(pulse_contrast * 100.0f);
        contrast_slider->setValue(v);
        if(contrast_label)
            contrast_label->setText(QString::number(v) + "%");
    }
    if(setup_boost_slider)
    {
        const int v = (int)std::lround(large_setup_boost * 100.0f);
        setup_boost_slider->setValue(v);
        if(setup_boost_label)
            setup_boost_label->setText(QString::number(v) + "%");
    }
}
