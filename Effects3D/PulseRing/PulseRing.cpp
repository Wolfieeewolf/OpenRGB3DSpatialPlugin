// SPDX-License-Identifier: GPL-2.0-only

#include "PulseRing.h"
#include "SpatialKernelColormap.h"
#include "EffectHelpers.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

REGISTER_EFFECT_3D(PulseRing);

const char* PulseRing::StyleName(int s)
{
    switch(s) { case STYLE_PULSE_RING: return "Pulse Ring"; case STYLE_RADIAL_RAINBOW: return "Radial Rainbow"; default: return "Pulse Ring"; }
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PulseRing::PulseRing(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D PulseRing::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Pulse Ring";
    info.effect_description =
        "Pulsing donut / radial rainbow; optional floor/mid/ceiling band tuning for motion and spatial freq";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 8.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void PulseRing::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("PulseRingEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* style_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Style:"));
    style_row->setObjectName(QStringLiteral("styleRow"));
    QComboBox* style_combo = style_row->combo();
    for(int s = 0; s < STYLE_COUNT; s++)
    {
        style_combo->addItem(StyleName(s));
    }
    style_combo->setCurrentIndex(std::max(0, std::min(ring_style, STYLE_COUNT - 1)));
    style_combo->setToolTip(QStringLiteral("Pulse Ring = expanding donut; Radial Rainbow = hue by angle from center."));
    style_combo->setItemData(0, QStringLiteral("Timed rings with configurable thickness and inner hole."), Qt::ToolTipRole);
    style_combo->setItemData(1,
                             QStringLiteral("Rainbow locked to horizontal angle—strong on floors and rings of LEDs."),
                             Qt::ToolTipRole);
    connect(style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        ring_style = std::max(0, std::min(idx, STYLE_COUNT - 1));
        emit ParametersChanged();
    });

    EffectSliderRow* ring_thickness_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Ring thickness:"),
        2,
        100,
        (int)(ring_thickness * 100.0f),
        QStringLiteral("Gaussian width of the ring band (Pulse Ring style only)."));
    ring_thickness_row->setObjectName(QStringLiteral("ringThicknessRow"));
    ring_thickness_row->bindValueChanged(
        this, [this](int v) { ring_thickness = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* hole_size_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Hole size:"),
        0,
        80,
        (int)(hole_size * 100.0f),
        QStringLiteral("Dark core radius inside the ring (fraction of horizontal span)."));
    hole_size_row->setObjectName(QStringLiteral("holeSizeRow"));
    hole_size_row->bindValueChanged(
        this, [this](int v) { hole_size = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* pulse_amplitude_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Pulse amplitude:"),
        20,
        200,
        (int)(pulse_amplitude * 100.0f),
        QStringLiteral("How strongly the ring front modulates (Pulse Ring style)."));
    pulse_amplitude_row->setObjectName(QStringLiteral("pulseAmplitudeRow"));
    pulse_amplitude_row->bindValueChanged(
        this, [this](int v) { pulse_amplitude = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* direction_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Direction:"),
        0,
        360,
        (int)direction_deg,
        QStringLiteral("Rotates where the expanding ring starts in the cycle (Pulse Ring style)."));
    direction_row->setObjectName(QStringLiteral("directionRow"));
    direction_row->bindValueChanged(
        this,
        [this](int v) { direction_deg = (float)v; },
        [](int v) { return QString::number(v) + QStringLiteral("\u00B0"); },
        on_changed);

    AddWidgetToParent(w, parent);
}

void PulseRing::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor PulseRing::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress_raw = CalculateProgress(time);
    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);

    float progress = progress_raw * bb.speed_mul;
    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    constexpr float kRingHorizontalFill = 1.0f;
    float hw = std::max(1e-6f, e.hw * kRingHorizontalFill);
    float hd = std::max(1e-6f, e.hd * kRingHorizontalFill);
    float r_corner = EffectGridHorizontalRadialNormXZ(rot.x - origin.x, rot.z - origin.z, hw, hd);
    float r = EffectGridHorizontalRadialNorm01(r_corner);
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
        float detail = std::max(0.05f, GetScaledDetail());
        float freq = std::max(0.3f, std::min(6.0f, GetScaledFrequency() * 0.15f * bb.tight_mul));
        float amp = std::max(0.2f, std::min(2.0f, pulse_amplitude));
        float sigma = std::max(ring_thickness, 0.02f);
        float phase_offset = direction_deg / 360.0f + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);
        float expand_progress = fmodf(progress + phase_offset, 1.0f);
        float ring_center = hole_r + expand_progress * usable;
        float d = fabsf(r - ring_center);
        const float d_cutoff = 3.0f * sigma * std::max(1.0f, amp);
        if(d > d_cutoff) return 0x00000000;
        if(r < hole_r - 0.02f) return 0x00000000;
        intensity = expf(-d * d / ((sigma / (0.6f + 0.4f * detail)) * (sigma / (0.6f + 0.4f * detail))));
        float pulse_mod = 0.5f + 0.5f * sinf(phase * freq);
        intensity *= amp * pulse_mod;
    }
    intensity = std::min(1.0f, std::max(0.0f, intensity));

    float azimuth_deg = atan2f(rot.z - origin.z, rot.x - origin.x) * 57.2957795f;

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.10f, 0.05f, 0.22f);
    map.center_size = std::clamp(0.11f + 0.24f * GetNormalizedScale(), 0.06f, 0.52f);
    map.directional_sharpness = std::clamp(1.05f + std::max(0.05f, GetScaledDetail()) * 0.08f, 0.9f, 2.3f);

    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;

    const float pr_phase01 = std::fmod(progress + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);
    float strip_p01 = 0.0f;
    if(UseEffectStripColormap())
    {
        strip_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                               GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               pr_phase01,
                                               time,
                                               grid,
                                               GetNormalizedSize(),
                                               origin,
                                               rot);
    }

    RGBColor c;
    if(UseEffectStripColormap())
    {
        float p01v = ApplyVoxelDriveToPalette01(strip_p01, x, y, z, time, grid);
        c = ResolveStripKernelFinalColor(*this, GetEffectStripColormapKernel(), p01v, GetEffectStripColormapColorStyle(), time,
                                          GetScaledFrequency() * 12.0f);
    }
    else if(GetRainbowMode())
    {
        float hue = fmodf(
            pos_norm * 360.0f
            + 0.22f * azimuth_deg
            + progress * (style == STYLE_RADIAL_RAINBOW ? 36.0f : 84.0f)
            + (float)style * 52.0f
            + direction_deg * (style == STYLE_PULSE_RING ? 0.85f : 0.30f)
            + (style == STYLE_PULSE_RING ? intensity * 55.0f : 0.0f),
            360.0f);
        if(hue < 0.0f) hue += 360.0f;
        hue = ApplySpatialRainbowHue(hue, pos_norm, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        c = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(pos_norm, basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        c = GetColorAtPosition(p);
    }
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json PulseRing::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["ring_style"] = ring_style;
    j["ring_thickness"] = ring_thickness;
    j["hole_size"] = hole_size;
    j["pulse_amplitude"] = pulse_amplitude;
    j["direction_deg"] = direction_deg;
return j;
}

void PulseRing::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("ring_style") && settings["ring_style"].is_number_integer())
        ring_style = std::max(0, std::min(settings["ring_style"].get<int>(), STYLE_COUNT - 1));
    if(settings.contains("ring_thickness") && settings["ring_thickness"].is_number())
    { float v = settings["ring_thickness"].get<float>(); ring_thickness = std::max(0.02f, std::min(1.0f, v)); }
    if(settings.contains("hole_size") && settings["hole_size"].is_number())
    { float v = settings["hole_size"].get<float>(); hole_size = std::max(0.0f, std::min(0.8f, v)); }
    if(settings.contains("pulse_amplitude") && settings["pulse_amplitude"].is_number())
    { float v = settings["pulse_amplitude"].get<float>(); pulse_amplitude = std::max(0.2f, std::min(2.0f, v)); }
    if(settings.contains("direction_deg") && settings["direction_deg"].is_number())
    { float v = settings["direction_deg"].get<float>(); direction_deg = fmodf(v + 360.0f, 360.0f); }

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "PulseRingEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "styleRow", ring_style);
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "ringThicknessRow", (int)(ring_thickness * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "holeSizeRow", (int)(hole_size * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "pulseAmplitudeRow", (int)(pulse_amplitude * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "directionRow", (int)direction_deg,
                                          [](int v) { return QString::number(v) + QStringLiteral("\u00B0"); });
        }
    }
}
