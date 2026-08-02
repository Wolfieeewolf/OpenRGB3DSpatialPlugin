// SPDX-License-Identifier: GPL-2.0-only

#include "HexLattice.h"
#include "HexLatticeVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"

#include <QComboBox>
#include "EffectUiRows.h"
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
    volume_assist_.setFragmentBody(QString::fromUtf8(HexLatticeVolumeFieldGlsl()));
    // Higher atlas res than most effects: hex walls are sub-cell features and
    // blur away at 18^3.
    volume_assist_.setResolution(20); // was 28 — atlas cost is n³; 20 keeps hex edges readable
}

HexLattice::~HexLattice() = default;

void HexLattice::EvaluateHexFieldCpu(float nx, float ny, float nz, float flow_t, float hue_t,
                                     float detail_norm, float* out_v, float* out_h01) const
{
    // Mirrors HexLatticeVolumeFieldGlsl — keep the two in sync.
    const auto mod_pos = [](float v, float m) {
        float r = std::fmod(v, m);
        if(r < 0.0f)
            r += m;
        return r;
    };
    const auto smstep = [](float e0, float e1, float x) {
        const float t = std::clamp((x - e0) / std::max(e1 - e0, 1e-5f), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    const float base_scale = std::max(0.2f, GetNormalizedSize());
    const float flow_mode_mul[3] = {0.68f, 1.0f, 1.55f};
    const float flow_mul = flow_mode_mul[std::clamp(flow_mode, 0, 2)];
    const float turbulence = std::clamp(turbulence_amount, 0.0f, 2.0f);
    const float pulse_amt = std::clamp(pulse_amount, 0.0f, 2.0f);

    const float breathe = 1.0f + (Wave01(flow_t * 0.30f) - 0.5f) * 0.35f * breathing_amount;
    const float cells = std::min(12.0f, (5.0f + 7.0f * detail_norm) / base_scale * breathe);

    float ux = nx + turbulence * 0.07f * std::sin(kTwoPi * (ny * 0.8f + flow_t * 0.11f));
    float uy = nz + turbulence * 0.07f * std::cos(kTwoPi * (ny * 0.8f - flow_t * 0.09f));
    ux = ux * cells + flow_t * flow_mul * 0.22f;
    uy = uy * cells + flow_t * flow_mul * 0.31f;

    const float ry = 1.7320508f;
    const float ax = mod_pos(ux, 1.0f) - 0.5f;
    const float ay = mod_pos(uy, ry) - ry * 0.5f;
    const float bx = mod_pos(ux - 0.5f, 1.0f) - 0.5f;
    const float by = mod_pos(uy - ry * 0.5f, ry) - ry * 0.5f;
    float gx = ax;
    float gy = ay;
    if(ax * ax + ay * ay >= bx * bx + by * by)
    {
        gx = bx;
        gy = by;
    }
    const float idx = ux - gx;
    const float idy = uy - gy;

    // Hex metric: 0 at cell centre, 0.5 on the wall.
    const float hd = std::max(std::fabs(gx) * 0.5f + std::fabs(gy) * 0.8660254f, std::fabs(gx));
    const float edge_w = 0.20f - 0.08f * detail_norm;
    const float edge = smstep(0.5f - edge_w, 0.5f - edge_w * 0.15f, hd);

    float hcell = std::sin(idx * 12.9898f + idy * 78.233f) * 43758.547f;
    hcell -= std::floor(hcell);
    const float pulse = Wave01(flow_t * (0.20f + 0.35f * hcell) * flow_mul + hcell);
    const float cell_fill = (0.06f + 0.30f * pulse * pulse_amt) * (1.0f - edge);

    *out_v = std::clamp(edge * (0.80f + 0.20f * pulse) + cell_fill, 0.0f, 1.0f);

    float h = idx * 0.045f + idy * 0.030f + hcell * 0.18f + (ny - 0.5f) * 0.10f + hue_t;
    *out_h01 = h - std::floor(h);
}

EffectInfo3D HexLattice::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Hex Lattice";
    info.effect_description =
        "Honeycomb lattice of hex prisms: glowing cell walls with pulsing interiors. "
        "Speed drives motion, Frequency cycles hue, Detail sets hex count, Size scales the cells.";
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
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectSliderRow* breathing_amount_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Breathing amount:"), 0, 200, (int)std::lround(breathing_amount * 100.0f));
    breathing_amount_row->setObjectName(QStringLiteral("breathingAmountRow"));
    breathing_amount_slider = breathing_amount_row->slider();
    breathing_amount_row->bindValueChanged(
        this,
        [this](int v) { breathing_amount = std::clamp(v / 100.0f, 0.0f, 2.0f); },
        pct_format,
        on_changed);

    EffectSliderRow* pulse_amount_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Pulse amount:"), 0, 200, (int)std::lround(pulse_amount * 100.0f));
    pulse_amount_row->setObjectName(QStringLiteral("pulseAmountRow"));
    pulse_amount_slider = pulse_amount_row->slider();
    pulse_amount_row->bindValueChanged(
        this,
        [this](int v) { pulse_amount = std::clamp(v / 100.0f, 0.0f, 2.0f); },
        pct_format,
        on_changed);

    EffectLabeledComboRow* flow_mode_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Flow mode:"));
    flow_mode_row->setObjectName(QStringLiteral("flowModeRow"));
    flow_mode_combo = flow_mode_row->combo();
    flow_mode_combo->addItem(QStringLiteral("Calm"));
    flow_mode_combo->addItem(QStringLiteral("Active"));
    flow_mode_combo->addItem(QStringLiteral("Aggressive"));
    flow_mode_combo->setCurrentIndex(std::clamp(flow_mode, 0, 2));
    flow_mode_combo->setToolTip(QStringLiteral("Changes overall flow intensity and directional drift style."));
    connect(flow_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        flow_mode = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    EffectSliderRow* turbulence_amount_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Turbulence amount:"), 0, 200, (int)std::lround(turbulence_amount * 100.0f));
    turbulence_amount_row->setObjectName(QStringLiteral("turbulenceAmountRow"));
    turbulence_amount_slider = turbulence_amount_row->slider();
    turbulence_amount_row->bindValueChanged(
        this,
        [this](int v) { turbulence_amount = std::clamp(v / 100.0f, 0.0f, 2.0f); },
        pct_format,
        on_changed);

    AddWidgetToParent(w, parent);
}

void HexLattice::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    const float speed_norm = std::max(0.05f, GetNormalizedSpeed());
    const float freq_norm = std::max(0.05f, GetNormalizedFrequency());
    const float detail_norm = std::max(0.05f, GetNormalizedDetail());
    const float flow_mode_mul[3] = {0.68f, 1.0f, 1.55f};
    const float flow_mul = flow_mode_mul[std::clamp(flow_mode, 0, 2)];
    // Speed drives lattice motion; Frequency drives hue cycling only.
    const float flow_t = time_sec * (0.15f + speed_norm * 1.10f);
    const float hue_t = time_sec * (0.04f + freq_norm * 0.45f);
    float ox = 0.5f, oy = 0.5f, oz = 0.5f;
    PackEffectOrigin01(grid, GetEffectOriginGrid(grid), &ox, &oy, &oz);
    const float vp[11] = {
        flow_t,
        hue_t,
        detail_norm,
        std::max(0.2f, GetNormalizedSize()),
        breathing_amount,
        pulse_amount,
        turbulence_amount,
        flow_mul,
        ox,
        oy,
        oz
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 11);
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

    const float flow_t = time * (0.15f + speed_norm * 1.10f);
    const float hue_t = time * (0.04f + freq_norm * 0.45f);

    float v = 0.0f;
    float h01 = 0.0f;
    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
        v = samp.x();
        h01 = samp.y();
    }
    else
    {
        float ox = 0.5f, oy = 0.5f, oz = 0.5f;
        PackEffectOrigin01(grid, origin, &ox, &oy, &oz);
        const float lnx = std::clamp(nx - ox + 0.5f, 0.0f, 1.0f);
        const float lny = std::clamp(ny - oy + 0.5f, 0.0f, 1.0f);
        const float lnz = std::clamp(nz - oz + 0.5f, 0.0f, 1.0f);
        EvaluateHexFieldCpu(lnx, lny, lnz, flow_t, hue_t, detail_norm, &v, &h01);
    }

    RGBColor c = 0x00000000;
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
        c = ResolveStripKernelFinalColor(SpatialPatternKernelClamp(GetEffectStripColormapKernel()), p01, time);
    }
    else if(GetRainbowMode())
    {
        c = Hsv01ToBgr(h01, 1.0f, 1.0f);
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
        breathing_amount_slider->setValue((int)std::lround(breathing_amount * 100.0f));
    if(pulse_amount_slider)
        pulse_amount_slider->setValue((int)std::lround(pulse_amount * 100.0f));
    if(flow_mode_combo)
        flow_mode_combo->setCurrentIndex(std::clamp(flow_mode, 0, 2));
    if(turbulence_amount_slider)
        turbulence_amount_slider->setValue((int)std::lround(turbulence_amount * 100.0f));
}
