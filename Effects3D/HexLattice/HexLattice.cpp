// SPDX-License-Identifier: GPL-2.0-only

#include "HexLattice.h"
#include "HexLatticeVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
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

    Vector3D rot{x, y, z};
    const float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    float v = 0.0f;
    float h01 = 0.0f;
    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
        v = samp.x();
        h01 = samp.y();
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
        SpatialLayerCore::Basis basis;
        SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
        SpatialLayerCore::MapperSettings map;
        SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), std::max(0.05f, GetScaledDetail()));

        SpatialLayerCore::SamplePoint sp{};
        sp.grid_x = x;
        sp.grid_y = y;
        sp.grid_z = z;
        sp.origin_x = origin.x;
        sp.origin_y = origin.y;
        sp.origin_z = origin.z;

        float hue = ApplySpatialRainbowHue(h01 * 360.0f, h01, basis, sp, map, time, &grid);
        c = GetRainbowColor(std::fmod(hue + 720.0f, 360.0f));
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
