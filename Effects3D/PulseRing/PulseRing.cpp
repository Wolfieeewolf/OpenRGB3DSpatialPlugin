// SPDX-License-Identifier: GPL-2.0-only

#include "PulseRing.h"
#include "PulseRingVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "EffectHelpers.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

REGISTER_EFFECT_3D(PulseRing);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
float HexMetric(float x, float z)
{
    const float ax = std::fabs(x);
    const float az = std::fabs(z);
    return std::max(ax * 0.5f + az * 0.8660254f, ax);
}

float PolyMetric(float x, float z, float n)
{
    const float an = (float)(2.0 * M_PI) / n;
    const float a = std::atan2(z, x);
    const float r = std::hypot(x, z);
    return std::cos(std::floor(0.5f + a / an) * an - a) * r;
}

float FootprintXZ(float lx, float lz, int shape)
{
    // shape: 0 Circle, 2 Hexagon, 3 Triangle, 4 Square (1 = Sphere uses 3D)
    if(shape == 2)
        return HexMetric(lx, lz) / 0.8660254f;
    if(shape == 3)
        return std::max(PolyMetric(lx, lz, 3.0f) / 0.5f, 0.0f);
    if(shape == 4)
        return std::max(std::fabs(lx), std::fabs(lz));
    return std::hypot(lx, lz);
}

float ShapeDist01(float lx, float ly, float lz, int shape, float detail, float* height_mul)
{
    // Sphere only uses true 3D distance. Everything else is a FLAT extruded XZ
    // prism (hard Y slab) so hex / triangle / square keep hard edges.
    if(shape == 1)
    {
        *height_mul = 1.0f;
        return std::sqrt(lx * lx + ly * ly + lz * lz);
    }
    const float d = std::clamp(detail, 0.05f, 1.0f);
    const float y_half = 0.28f + (0.07f - 0.28f) * d;
    const float ay = std::fabs(ly);
    if(ay <= y_half)
        *height_mul = 1.0f;
    else if(ay >= y_half + 0.035f)
        *height_mul = 0.0f;
    else
    {
        const float t = (ay - y_half) / 0.035f;
        *height_mul = 1.0f - t;
    }
    return FootprintXZ(lx, lz, shape);
}

float SharpBand(float dist_to_edge, float half_w)
{
    const float w = std::max(half_w, 0.008f);
    const float a = std::fabs(dist_to_edge);
    if(a >= w)
        return 0.0f;
    const float t = 1.0f - a / w;
    return t * t;
}

// Keep hue drivers bounded — fmod(huge_time * rate, 360) loses precision and flashes.
float HueScroll01(float time_sec, float scaled_freq)
{
    const float rate = std::max(0.02f, scaled_freq) * 0.12f; // cycles/sec
    return std::fmod(time_sec * rate + 1000.0f, 1.0f);
}
} // namespace

const char* PulseRing::StyleName(int s)
{
    switch(s)
    {
    case STYLE_PULSE: return "Pulse";
    case STYLE_RADIAL_RAINBOW: return "Radial Rainbow";
    default: return "Pulse";
    }
}

const char* PulseRing::ShapeName(int s)
{
    switch(s)
    {
    case SHAPE_CIRCLE: return "Circle (flat ring)";
    case SHAPE_SPHERE: return "Sphere";
    case SHAPE_HEXAGON: return "Hexagon";
    case SHAPE_TRIANGLE: return "Triangle";
    case SHAPE_SQUARE: return "Square";
    default: return "Sphere";
    }
}

PulseRing::PulseRing(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(55);
    SetFrequency(35);
    volume_assist_.setFragmentBody(QString::fromUtf8(PulseRingVolumeFieldGlsl()));
    // Hex/square corners need enough voxels, but 28×28×28 was a major frame cost.
    volume_assist_.setResolution(20);
}

EffectInfo3D PulseRing::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Pulse Ring";
    info.effect_description =
        "Expanding pulse shells (sphere / hex / triangle / square / flat ring) with GPU assist. "
        "Speed drives expansion, Frequency scrolls rainbow hue, Size scales the shell, "
        "Detail sharpens the band and flattens non-sphere shapes.";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 22.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 18.0f;
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
        style_combo->addItem(QString::fromUtf8(StyleName(s)));
    style_combo->setCurrentIndex(std::clamp(ring_style, 0, STYLE_COUNT - 1));
    style_combo->setToolTip(QStringLiteral("Pulse = expanding shell. Radial Rainbow = filled shape with hue by angle."));
    connect(style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        ring_style = std::clamp(idx, 0, STYLE_COUNT - 1);
        emit ParametersChanged();
    });

    EffectLabeledComboRow* shape_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Pulse shape:"));
    shape_row->setObjectName(QStringLiteral("shapeRow"));
    QComboBox* shape_combo = shape_row->combo();
    for(int s = 0; s < SHAPE_COUNT; s++)
        shape_combo->addItem(QString::fromUtf8(ShapeName(s)));
    shape_combo->setCurrentIndex(std::clamp(pulse_shape, 0, SHAPE_COUNT - 1));
    shape_combo->setToolTip(QStringLiteral(
        "Footprint of the expanding wavefront.\n"
        "Sphere = true 3D shell.\n"
        "Circle / Hexagon / Triangle / Square = extruded XZ shapes "
        "(pulse outward keeping hard edges — not spherized blobs)."));
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        pulse_shape = std::clamp(idx, 0, SHAPE_COUNT - 1);
        emit ParametersChanged();
    });

    EffectSliderRow* ring_thickness_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Shell thickness:"),
        2,
        100,
        (int)std::lround(ring_thickness * 100.0f),
        QStringLiteral("Width of the glowing band (Pulse style)."));
    ring_thickness_row->setObjectName(QStringLiteral("ringThicknessRow"));
    ring_thickness_row->bindValueChanged(
        this, [this](int v) { ring_thickness = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* hole_size_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Hole size:"),
        0,
        75,
        (int)std::lround(hole_size * 100.0f),
        QStringLiteral("Dark core before the shell starts expanding."));
    hole_size_row->setObjectName(QStringLiteral("holeSizeRow"));
    hole_size_row->bindValueChanged(
        this, [this](int v) { hole_size = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* pulse_amplitude_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Pulse amplitude:"),
        20,
        200,
        (int)std::lround(pulse_amplitude * 100.0f),
        QStringLiteral("Brightness of the shell front."));
    pulse_amplitude_row->setObjectName(QStringLiteral("pulseAmplitudeRow"));
    pulse_amplitude_row->bindValueChanged(
        this, [this](int v) { pulse_amplitude = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* direction_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Phase:"),
        0,
        360,
        (int)direction_deg,
        QStringLiteral("Offsets where in the expand cycle the shell starts."));
    direction_row->setObjectName(QStringLiteral("directionRow"));
    direction_row->bindValueChanged(
        this,
        [this](int v) { direction_deg = (float)v; },
        [](int v) { return QString::number(v) + QStringLiteral("\u00B0"); },
        on_changed);

    AddWidgetToParent(w, parent);
}

float PulseRing::EvaluatePulseCpu(float nx, float ny, float nz, float progress, float time_sec, float* out_color01) const
{
    const float hole_r = std::clamp(hole_size, 0.0f, 0.75f);
    const float sigma = std::max(ring_thickness, 0.012f);
    const float amp = std::clamp(pulse_amplitude, 0.2f, 2.0f);
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float size_scale = std::clamp(GetNormalizedSize() * (0.65f + 0.55f * GetNormalizedScale()), 0.25f, 2.5f);
    const int style = std::clamp(ring_style, 0, STYLE_COUNT - 1);
    const int shape = std::clamp(pulse_shape, 0, SHAPE_COUNT - 1);
    const float phase_offset = direction_deg / 360.0f;
    const float hue_scroll = HueScroll01(time_sec, GetScaledFrequency());

    const float lx = nx * 2.0f - 1.0f;
    const float ly = ny * 2.0f - 1.0f;
    const float lz = nz * 2.0f - 1.0f;
    float height_mul = 1.0f;
    const float d = ShapeDist01(lx, ly, lz, shape, detail, &height_mul) / std::max(size_scale, 0.25f);
    const float usable = std::max(0.12f, 1.0f - hole_r);
    const float az01 = std::fmod(std::atan2(lz, lx) / (float)(2.0 * M_PI) + 0.5f + hue_scroll + 2.0f, 1.0f);

    if(style == STYLE_RADIAL_RAINBOW)
    {
        *out_color01 = az01;
        const float fill = std::clamp((d - (hole_r - 0.02f)) / 0.03f, 0.0f, 1.0f);
        const float outer = 1.0f - std::clamp((d - 1.02f) / 0.18f, 0.0f, 1.0f);
        return std::clamp(fill * outer * height_mul, 0.0f, 1.0f);
    }

    *out_color01 = std::fmod(std::clamp((d - hole_r) / usable, 0.0f, 1.0f) * 0.85f + hue_scroll + 2.0f, 1.0f);

    const float expand = std::fmod(progress + phase_offset + 10.0f, 1.0f);
    const float center = hole_r + expand * usable;
    const float half_w = std::max(0.010f, sigma * (0.55f + (0.18f - 0.55f) * detail));
    float intensity = SharpBand(d - center, half_w) * height_mul;
    intensity *= std::clamp((d - (hole_r - 0.03f)) / 0.04f, 0.0f, 1.0f);
    intensity *= amp;
    return std::clamp(intensity, 0.0f, 1.0f);
}

void PulseRing::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    // Faster expand: progress is fractional cycles (not raw CalculateProgress which grows forever).
    const float spd = std::max(0.05f, GetScaledSpeed());
    const float progress = std::fmod(time_sec * spd * 0.085f + 1000.0f, 1.0f);
    const float hole_r = std::clamp(hole_size, 0.0f, 0.75f);
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float amp = std::clamp(pulse_amplitude, 0.2f, 2.0f);
    const float sigma = std::max(ring_thickness, 0.015f);
    const float phase_offset = direction_deg / 360.0f;
    const float size_scale = std::clamp(GetNormalizedSize() * (0.65f + 0.55f * GetNormalizedScale()), 0.25f, 2.5f);
    // Bounded 0..1 scroll — never pass raw time*freq (float fmod flash).
    const float hue_scroll01 = HueScroll01(time_sec, GetScaledFrequency());
    float ox = 0.5f, oy = 0.5f, oz = 0.5f;
    PackEffectOrigin01(grid, GetEffectOriginGrid(grid), &ox, &oy, &oz);
    const float vp[13] = {
        progress,
        hole_r,
        sigma,
        amp,
        detail,
        (float)std::clamp(ring_style, 0, STYLE_COUNT - 1),
        phase_offset,
        (float)std::clamp(pulse_shape, 0, SHAPE_COUNT - 1),
        size_scale,
        hue_scroll01,
        ox,
        oy,
        oz
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 13);
}

RGBColor PulseRing::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    const float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    const float spd = std::max(0.05f, GetScaledSpeed());
    const float progress = std::fmod(time * spd * 0.085f + 1000.0f, 1.0f);

    float intensity = 0.0f;
    float color01 = 0.0f;
    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
        intensity = samp.x();
        color01 = samp.y();
    }
    else
    {
        float ox = 0.5f, oy = 0.5f, oz = 0.5f;
        PackEffectOrigin01(grid, origin, &ox, &oy, &oz);
        const float lnx = std::clamp(nx - ox + 0.5f, 0.0f, 1.0f);
        const float lny = std::clamp(ny - oy + 0.5f, 0.0f, 1.0f);
        const float lnz = std::clamp(nz - oz + 0.5f, 0.0f, 1.0f);
        intensity = EvaluatePulseCpu(lnx, lny, lnz, progress, time, &color01);
    }
    if(intensity <= 1e-5f)
        return 0x00000000;

    // Light stratum phase only — avoid heavy SpatialLayerCore work on the hot path.
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(ny, strat_st, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);
    const float phase01 =
        std::fmod(progress + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);

    RGBColor c;
    if(UseEffectStripColormap())
    {
        const float strip_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                           GetEffectStripColormapRepeats(),
                                                           GetEffectStripColormapUnfold(),
                                                           GetEffectStripColormapDirectionDeg(),
                                                           phase01,
                                                           time,
                                                           grid,
                                                           GetNormalizedSize(),
                                                           origin,
                                                           rot);
        c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), strip_p01, time);
    }
    else if(GetRainbowMode())
    {
        // color01 already holds bounded azimuth+scroll (Radial) or radius+scroll (Pulse).
        // Never use wrapping expand progress — that caused the slow-then-flash reset.
        // Never accumulate unbounded time*freq into fmod (precision flash).
        const float hue = std::fmod(color01 * 360.0f + direction_deg + 720.0f, 360.0f);
        c = GetRainbowColor(hue);
    }
    else if(!colors.empty())
    {
        // Circular palette so scroll wrap (0.99→0.00) does not flash end→start.
        const int n = (int)colors.size();
        if(n == 1)
        {
            c = colors[0];
        }
        else
        {
            const float scaled = std::fmod(color01 + direction_deg / 360.0f + 2.0f, 1.0f) * (float)n;
            const int i0 = ((int)std::floor(scaled)) % n;
            const int i1 = (i0 + 1) % n;
            const float frac = scaled - std::floor(scaled);
            const RGBColor a = colors[i0];
            const RGBColor b = colors[i1];
            const int r = (int)((a & 0xFF) * (1.0f - frac) + (b & 0xFF) * frac);
            const int g = (int)(((a >> 8) & 0xFF) * (1.0f - frac) + ((b >> 8) & 0xFF) * frac);
            const int bl = (int)(((a >> 16) & 0xFF) * (1.0f - frac) + ((b >> 16) & 0xFF) * frac);
            c = (RGBColor)((bl << 16) | (g << 8) | r);
        }
    }
    else
    {
        c = GetColorAtPosition(std::clamp(color01, 0.0f, 1.0f));
    }

    const int r_ = std::clamp((int)((c & 0xFF) * intensity), 0, 255);
    const int g_ = std::clamp((int)(((c >> 8) & 0xFF) * intensity), 0, 255);
    const int b_ = std::clamp((int)(((c >> 16) & 0xFF) * intensity), 0, 255);
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json PulseRing::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["ring_style"] = ring_style;
    j["pulse_shape"] = pulse_shape;
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
        ring_style = std::clamp(settings["ring_style"].get<int>(), 0, STYLE_COUNT - 1);
    if(settings.contains("pulse_shape") && settings["pulse_shape"].is_number_integer())
        pulse_shape = std::clamp(settings["pulse_shape"].get<int>(), 0, SHAPE_COUNT - 1);
    if(settings.contains("ring_thickness") && settings["ring_thickness"].is_number())
        ring_thickness = std::clamp(settings["ring_thickness"].get<float>(), 0.02f, 1.0f);
    if(settings.contains("hole_size") && settings["hole_size"].is_number())
        hole_size = std::clamp(settings["hole_size"].get<float>(), 0.0f, 0.75f);
    if(settings.contains("pulse_amplitude") && settings["pulse_amplitude"].is_number())
        pulse_amplitude = std::clamp(settings["pulse_amplitude"].get<float>(), 0.2f, 2.0f);
    if(settings.contains("direction_deg") && settings["direction_deg"].is_number())
        direction_deg = std::fmod(settings["direction_deg"].get<float>() + 360.0f, 360.0f);

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "PulseRingEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "styleRow", ring_style);
            EffectUiSync::setComboIndex(fx, "shapeRow", pulse_shape);
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "ringThicknessRow", (int)std::lround(ring_thickness * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "holeSizeRow", (int)std::lround(hole_size * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "pulseAmplitudeRow", (int)std::lround(pulse_amplitude * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "directionRow", (int)direction_deg,
                                         [](int v) { return QString::number(v) + QStringLiteral("\u00B0"); });
        }
    }
}
