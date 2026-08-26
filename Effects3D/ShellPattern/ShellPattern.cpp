// SPDX-License-Identifier: GPL-2.0-only

#include "ShellPattern.h"
#include "ShellPatternVolumeFieldGlsl.h"
#include "SpatialPatternKernels/SpatialStripKernelEvalGlsl.h"
#include "SpatialPatternKernels/StripUnfoldFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "PluginLog.h"
#include <QByteArray>
#include <QComboBox>
#include <QString>
#include <QVector3D>
#include "EffectUiRows.h"
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(ShellPattern);

const char* ShellPattern::UnfoldModeLabel(int m)
{
    switch(m)
    {
    case 0: return "Along X";
    case 1: return "Along Y";
    case 2: return "Along Z";
    case 3: return "Plane XZ (angled)";
    case 4: return "Radial XZ";
    case 5: return "Diagonal x+y+z";
    case 6: return "Manhattan";
    case 7: return "Effect animation only (no room projection)";
    case 8: return "Static room projection (angle)";
    default: return "Along X";
    }
}

const char* ShellPattern::DisplayModeLabel(int d)
{
    switch(d)
    {
    case DISP_SHELL_Y: return "Shell (wave height)";
    case DISP_FILL_STRIP: return "Extrude (solid by coordinate)";
    case DISP_SHELL_RADIAL_XZ: return "Shell (radial XZ)";
    case DISP_CONTOUR: return "Contour bands";
    case DISP_BARS: return "Bars (rising columns)";
    case DISP_RIPPLES: return "Ripples (water rings)";
    case DISP_DROPLETS: return "Droplets (falling)";
    case DISP_FIREWORKS: return "Fireworks (bursts)";
    case DISP_EXPLOSION: return "Explosion (blast + sparks)";
    case DISP_RAIN: return "Rain (streaks)";
    default: return "Shell (wave height)";
    }
}

namespace
{
QString ShellPatternFullVolumeBody()
{
    return QString::fromUtf8(SpatialStripKernelEvalGlsl())
           + QString::fromUtf8(StripUnfoldFieldGlsl())
           + QString::fromUtf8(ShellPatternVolumeFieldGlsl());
}
} // namespace

ShellPattern::ShellPattern(QWidget* parent) : SpatialEffect3D(parent)
{
    SetSpeed(55);
    SetFrequency(45);
    SetDetail(90);
    effect_size = 130;
    effect_scale = 200;
    SetRainbowMode(false);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
    volume_assist_.setFragmentBody(ShellPatternFullVolumeBody());
    volume_assist_.setResolution(22);
}

ShellPattern::~ShellPattern() = default;

EffectInfo3D ShellPattern::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Shell Pattern";
    info.effect_description =
        "Shell / contour / extrude and LED-cube displays (bars, ripples, droplets, fireworks, explosion, rain) "
        "driven by a 1D pattern on a GPU volume atlas. Spatial Anchor at volume center.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_SHELL_PATTERN;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    info.default_speed_scale = 55.0f;
    info.default_frequency_scale = 14.0f;
    info.default_detail_scale = 10.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void ShellPattern::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* display_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Display:"));
    display_row->setObjectName(QStringLiteral("displayRow"));
    display_combo = display_row->combo();
    for(int d = 0; d < DISP_COUNT; d++)
    {
        display_combo->addItem(DisplayModeLabel(d));
    }
    display_combo->setCurrentIndex(std::clamp(display_mode, 0, DISP_COUNT - 1));
    display_combo->setToolTip(QStringLiteral(
        "How the pattern is drawn in the room.\n"
        "Shell (wave height) = horizontal surface.\n"
        "Shell (radial XZ) = vertical cylinder filling the room (angle pattern; Along Y = lathe).\n"
        "Ripples = flowing water rings across the mid plane.\n"
        "Bars / Droplets / Fireworks / Explosion / Rain = LED-cube volume looks."));
    connect(display_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShellPattern::OnParameterChanged);

    EffectLabeledComboRow* unfold_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Unfold:"));
    unfold_row->setObjectName(QStringLiteral("unfoldRow"));
    unfold_combo = unfold_row->combo();
    for(int m = 0; m < (int)StripPatternSurface::UnfoldMode::COUNT; m++)
    {
        unfold_combo->addItem(QString::fromUtf8(UnfoldModeLabel(m)));
    }
    unfold_combo->setCurrentIndex(std::clamp(unfold_mode, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    unfold_combo->setToolTip(QStringLiteral(
        "How 3D position maps to the 1D pattern coordinate. Used when Strip colormap is off."));
    connect(unfold_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShellPattern::OnParameterChanged);

    EffectLabeledComboRow* pattern_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Pattern:"));
    pattern_row->setObjectName(QStringLiteral("patternRow"));
    pattern_combo = pattern_row->combo();
    for(int p = 0; p < SpatialPatternKernelCount(); p++)
    {
        pattern_combo->addItem(QString::fromUtf8(SpatialPatternKernelDisplayName(p)));
    }
    pattern_combo->setCurrentIndex(std::clamp(pattern_id, 0, SpatialPatternKernelCount() - 1));
    pattern_combo->setToolTip(QStringLiteral(
        "1D kernel shaping the shell height / fill. Used when Strip colormap is off."));
    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShellPattern::OnParameterChanged);

    EffectSliderRow* direction_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Direction:"),
        0,
        359,
        (int)std::lround(direction_deg),
        QStringLiteral("Angle for Plane XZ / angled unfolds (degrees)."));
    direction_row->setObjectName(QStringLiteral("directionRow"));
    direction_slider = direction_row->slider();
    direction_row->bindValueChanged(
        this,
        [this](int v) { direction_deg = (float)v; },
        [](int v) { return QString::number(v) + QStringLiteral("°"); },
        on_changed);

    EffectSliderRow* repeats_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Repeats:"),
        1,
        20,
        (int)std::lround(strip_repeats),
        QStringLiteral("How many pattern cycles along the unfold coordinate."));
    repeats_row->setObjectName(QStringLiteral("repeatsRow"));
    repeats_slider = repeats_row->slider();
    repeats_row->bindValueChanged(
        this,
        [this](int v) { strip_repeats = (float)std::max(1, v); },
        [](int v) { return QString::number(v); },
        on_changed);

    EffectSliderRow* shell_thickness_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Shell thickness:"),
        0,
        100,
        (int)std::lround(surface_thickness * 100.0f),
        QStringLiteral(
            "Shell / contour band width. Global Size also fattens features."));
    shell_thickness_row->setObjectName(QStringLiteral("shellThicknessRow"));
    thick_slider = shell_thickness_row->slider();
    shell_thickness_row->bindValueChanged(
        this, [this](int v) { surface_thickness = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* shell_amplitude_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Shell amplitude:"),
        20,
        200,
        (int)(wave_amplitude * 100.0f),
        QStringLiteral(
            "Wave / fill strength. Global Size also scales amplitude and cube features.\n"
            "Speed / Frequency drive animation rate."));
    shell_amplitude_row->setObjectName(QStringLiteral("shellAmplitudeRow"));
    amp_slider = shell_amplitude_row->slider();
    shell_amplitude_row->bindValueChanged(
        this, [this](int v) { wave_amplitude = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* edge_fade_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Edge fade:"),
        0,
        100,
        (int)edge_fade_pct,
        QStringLiteral(
            "Softens toward the room X/Z walls (full grid bounds). 0% = off. Uses real room edges, not effect scale."));
    edge_fade_row->setObjectName(QStringLiteral("edgeFadeRow"));
    edge_slider = edge_fade_row->slider();
    edge_fade_row->bindValueChanged(
        this, [this](int v) { edge_fade_pct = (float)v; }, pct_format, on_changed);

    AddWidgetToParent(w, parent);
}

void ShellPattern::OnParameterChanged()
{
    if(display_combo)
        display_mode = std::clamp(display_combo->currentIndex(), 0, DISP_COUNT - 1);
    if(unfold_combo)
        unfold_mode = std::clamp(unfold_combo->currentIndex(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    if(pattern_combo)
        pattern_id = std::clamp(pattern_combo->currentIndex(), 0, SpatialPatternKernelCount() - 1);
    emit ParametersChanged();
}

namespace
{
float BoundedProgress01(float time_sec, float speed_mul)
{
    /* Keep phase in 0..1 — raw time*speed loses float precision and stutters. */
    const float spd = std::max(0.15f, speed_mul);
    return std::fmod(time_sec * spd * 0.28f + 1000.0f, 1.0f);
}
} // namespace

void ShellPattern::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    const int pat = std::clamp(UseEffectStripColormap() ? GetEffectStripColormapKernel() : pattern_id, 0,
                               SpatialPatternKernelCount() - 1);
    const int unfold_i = UseEffectStripColormap() ? GetEffectStripColormapUnfold() : unfold_mode;
    const float dir_deg = UseEffectStripColormap() ? GetEffectStripColormapDirectionDeg() : direction_deg;
    const float reps = UseEffectStripColormap() ? GetEffectStripColormapRepeats() : strip_repeats;
    const int disp = std::clamp(display_mode, 0, DISP_COUNT - 1);

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const float size_m = std::clamp(GetNormalizedSize() * 1.15f, 0.35f, 3.0f);
    /* Size also fatten shells/bars so the global Size slider is obvious. */
    const float size_boost = std::clamp(0.75f + 0.45f * size_m, 0.75f, 2.2f);
    const float amp = std::clamp(wave_amplitude * bb.tight_mul * size_boost, 0.25f, 2.5f);
    const float sigma = std::clamp(std::max(surface_thickness, 0.03f) * (0.85f + 0.35f * size_m),
                                   0.03f, 0.85f);
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float freq_n = std::clamp(GetNormalizedFrequency(), 0.05f, 1.0f);
    const float anim_speed = std::max(0.35f, GetScaledSpeed() * bb.speed_mul);
    const float progress_val = BoundedProgress01(time_sec, anim_speed);
    /* Kernel phase: Speed × Frequency so Extrude / Contour actually move. */
    const float phase01 = std::fmod(time_sec * anim_speed * (0.10f + 0.22f * freq_n) + 1000.0f, 1.0f);

    const float vp[12] = {
        (float)disp,
        amp,
        progress_val,
        sigma,
        detail,
        size_m,
        freq_n,
        (float)std::clamp(pat, 0, kSpatialStripGpuKernelMaxId),
        phase01,
        std::max(1.0f, reps),
        (float)std::clamp(unfold_i, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1),
        dir_deg
    };
    /* Keep the full param-driven body. Do not swap in an X-sine fallback — that
       ignored Display/Pattern/Unfold and looked like a fixed RGB bar. */
    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 12))
    {
        const QString err = volume_assist_.lastError();
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] ShellPattern volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor ShellPattern::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    if(!volume_assist_.isAvailable())
        return 0x00000000;

    Vector3D rot{x, y, z};
    float coord_y01 = SampleStratumYNorm01(rot.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_map_s;
    EffectStratumBlend::InitStratumBreaks(strat_map_s);
    float swt[3];
    EffectStratumBlend::WeightsForYNorm(coord_y01, strat_map_s, swt);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), swt, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(swt, grid, x, y, z, origin, time);

    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    SampleGpuVolumeOriginLocal01(rot.x, rot.y, rot.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);
    const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
    float intensity = samp.x();
    float k = samp.y() * 2.0f - 1.0f;

    float fade = std::clamp(edge_fade_pct / 100.0f, 0.0f, 1.0f);
    if(fade > 0.001f)
    {
        const float u = RoomXZEdgeProximity01(rot.x, rot.z, grid);
        const float t = std::clamp(u, 0.0f, 1.0f);
        float edge_mul = 1.0f - fade * (t * t * (3.0f - 2.0f * t));
        intensity *= std::max(0.0f, std::min(1.0f, edge_mul));
    }
    if(intensity <= 1e-4f)
        return 0x00000000;

    int pat = std::clamp(UseEffectStripColormap() ? GetEffectStripColormapKernel() : pattern_id, 0,
                         SpatialPatternKernelCount() - 1);

    float pos_norm = std::clamp((k + 1.0f) * 0.5f, 0.0f, 1.0f);
    float rate = GetScaledFrequency();
    float pos_color = std::fmod(pos_norm + time * rate * 0.018f, 1.0f);
    if(pos_color < 0.0f)
        pos_color += 1.0f;
    pos_color = EffectStratumBlend::ApplyMotionToPhase01(pos_color, stratum_mot01, 0.5f);

    float detail = std::max(0.05f, GetScaledDetail());
    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.09f + 0.08f * (1.0f - detail), 0.05f, 0.20f);
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord_y01;

    RGBColor c = 0x00000000;
    if(UseEffectStripColormap())
    {
        float p_mapped = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
        c = ResolveStripKernelFinalColor(pat, p_mapped, time);
    }
    else if(GetRainbowMode())
    {
        float hue = pos_color * 360.0f;
        hue = ApplySpatialRainbowHue(hue, coord_y01, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
            p01 += 1.0f;
        c = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p_mapped = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
        c = GetColorAtPosition(p_mapped);
    }
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json ShellPattern::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["shellpattern_display_mode"] = display_mode;
    j["shellpattern_unfold_mode"] = unfold_mode;
    j["shellpattern_pattern_id"] = pattern_id;
    j["shellpattern_direction_deg"] = direction_deg;
    j["shellpattern_repeats"] = strip_repeats;
    j["shellpattern_surface_thickness"] = surface_thickness;
    j["shellpattern_wave_amplitude"] = wave_amplitude;
    j["shellpattern_edge_fade_pct"] = edge_fade_pct;
    return j;
}

void ShellPattern::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("shellpattern_display_mode") && settings["shellpattern_display_mode"].is_number_integer())
        display_mode = std::clamp(settings["shellpattern_display_mode"].get<int>(), 0, DISP_COUNT - 1);
    if(settings.contains("shellpattern_unfold_mode") && settings["shellpattern_unfold_mode"].is_number_integer())
        unfold_mode = std::clamp(settings["shellpattern_unfold_mode"].get<int>(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    if(settings.contains("shellpattern_pattern_id") && settings["shellpattern_pattern_id"].is_number_integer())
        pattern_id = std::clamp(settings["shellpattern_pattern_id"].get<int>(), 0, SpatialPatternKernelCount() - 1);
    if(settings.contains("shellpattern_direction_deg") && settings["shellpattern_direction_deg"].is_number())
        direction_deg = std::fmod(settings["shellpattern_direction_deg"].get<float>() + 360.0f, 360.0f);
    if(settings.contains("shellpattern_repeats") && settings["shellpattern_repeats"].is_number())
        strip_repeats = std::max(1.0f, std::min(40.0f, settings["shellpattern_repeats"].get<float>()));
    if(settings.contains("shellpattern_surface_thickness") && settings["shellpattern_surface_thickness"].is_number())
        surface_thickness = std::clamp(settings["shellpattern_surface_thickness"].get<float>(), 0.0f, 1.0f);
    if(settings.contains("shellpattern_wave_amplitude") && settings["shellpattern_wave_amplitude"].is_number())
        wave_amplitude = std::max(0.2f, std::min(2.0f, settings["shellpattern_wave_amplitude"].get<float>()));
    if(settings.contains("shellpattern_edge_fade_pct") && settings["shellpattern_edge_fade_pct"].is_number())
        edge_fade_pct = std::clamp(settings["shellpattern_edge_fade_pct"].get<float>(), 0.0f, 100.0f);

    if(display_combo)
        display_combo->setCurrentIndex(display_mode);
    if(unfold_combo)
        unfold_combo->setCurrentIndex(unfold_mode);
    if(pattern_combo)
        pattern_combo->setCurrentIndex(pattern_id);
    if(direction_slider)
        direction_slider->setValue((int)std::lround(direction_deg));
    if(repeats_slider)
        repeats_slider->setValue((int)std::lround(strip_repeats));
    if(thick_slider)
        thick_slider->setValue((int)std::lround(surface_thickness * 100.0f));
    if(amp_slider)
        amp_slider->setValue((int)(wave_amplitude * 100.0f));
    if(edge_slider)
        edge_slider->setValue((int)edge_fade_pct);
}
