// SPDX-License-Identifier: GPL-2.0-only

#include "ShellPattern.h"
#include "ShellPatternCubeVolumeFieldGlsl.h"
#include "SpatialPatternKernels/SpatialStripKernelFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "EffectHelpers.h"
#include <QComboBox>
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

ShellPattern::ShellPattern(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(38);
    SetRainbowMode(false);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
    strip_assist_.setFragmentBody(QString::fromUtf8(SpatialStripKernelFieldGlsl()));
    strip_assist_.setWidth(256);
    volume_assist_.setFragmentBody(QString::fromUtf8(ShellPatternCubeVolumeFieldGlsl()));
    volume_assist_.setResolution(28);
}

ShellPattern::~ShellPattern() = default;

EffectInfo3D ShellPattern::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Shell Pattern";
    info.effect_description =
        "Shell / contour / extrude surfaces driven by a 1D pattern, plus LED-cube style displays "
        "(bars, ripples, droplets, fireworks, explosion, rain) with GPU atlas for cube modes. "
        "Pattern still colors the look.";
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
    info.default_speed_scale = 35.0f;
    info.default_frequency_scale = 12.0f;
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
        QStringLiteral("Shell / contour band width."));
    shell_thickness_row->setObjectName(QStringLiteral("shellThicknessRow"));
    thick_slider = shell_thickness_row->slider();
    shell_thickness_row->bindValueChanged(
        this, [this](int v) { surface_thickness = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* shell_amplitude_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Shell amplitude:"), 20, 200, (int)(wave_amplitude * 100.0f));
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

float ShellPattern::EvaluateKernel(float s01, float phase01, float time_sec, int pattern, float repeats) const
{
    return EvalSpatialPatternKernel(pattern, s01, phase01, repeats, time_sec);
}

namespace
{
float Hash11(float n)
{
    const float x = std::sin(n * 127.1f) * 43758.5453f;
    return x - std::floor(x);
}

float SoftBand(float d, float sigma)
{
    const float s = std::max(sigma, 0.02f);
    return std::exp(-(d * d) / (s * s));
}

float WrapFade(float life)
{
    const float a = std::clamp(life / 0.06f, 0.0f, 1.0f);
    const float b = std::clamp((1.0f - life) / 0.10f, 0.0f, 1.0f);
    const float sa = a * a * (3.0f - 2.0f * a);
    const float sb = b * b * (3.0f - 2.0f * b);
    return sa * sb;
}

float BoundedProgress01(float time_sec, float speed_mul)
{
    /* Keep phase in 0..1 — raw time*speed loses float precision and stutters. */
    const float spd = std::max(0.05f, speed_mul);
    return std::fmod(time_sec * spd * 0.085f + 1000.0f, 1.0f);
}
} // namespace

float ShellPattern::EvaluateCubeDisplay(int disp, float lx, float ly, float lz, float k, float amp,
                                        float progress, float time_sec, float sigma) const
{
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float size_m = std::clamp(GetNormalizedSize(), 0.2f, 2.5f);
    const float freq_n = std::clamp(GetNormalizedFrequency(), 0.05f, 1.0f);

    if(disp == DISP_BARS)
    {
        const int n = 3 + (int)std::lround(3.0f * detail);
        const float cell = 2.0f / (float)n;
        const float cx = (std::floor((lx + 1.0f) / cell) + 0.5f) * cell - 1.0f;
        const float cz = (std::floor((lz + 1.0f) / cell) + 0.5f) * cell - 1.0f;
        const float dx = lx - cx;
        const float dz = lz - cz;
        const float half = cell * (0.28f + 0.08f * amp);
        if(std::fabs(dx) > half || std::fabs(dz) > half)
            return 0.0f;
        const float id = Hash11(cx * 12.7f + cz * 91.3f);
        const float breathe = 0.5f + 0.5f * std::sin(progress * TWO_PI + id * TWO_PI);
        const float h = std::clamp(0.18f + 0.50f * (0.5f + 0.5f * k) * amp + 0.28f * breathe, 0.10f, 1.10f);
        const float y01 = (ly + 1.0f) * 0.5f;
        if(y01 < 0.0f || y01 > h + 0.08f)
            return 0.0f;
        const float edge = 1.0f - std::max(std::fabs(dx), std::fabs(dz)) / half;
        const float body = edge * (0.55f + 0.45f * std::clamp(y01 / std::max(h, 1e-3f), 0.0f, 1.0f));
        const float tip = SoftBand(y01 - h, 0.10f + sigma * 0.45f);
        return std::clamp(body + tip * 0.55f, 0.0f, 1.0f);
    }

    if(disp == DISP_RIPPLES)
    {
        const float r = std::sqrt(lx * lx + lz * lz);
        const float freq = 2.4f + 3.6f * detail;
        const float travel = progress * (2.0f + 2.2f * amp) * TWO_PI;

        const float wave1 = std::sin(r * freq - travel);
        const float crest1 = std::pow(0.5f + 0.5f * wave1, 2.2f + 1.6f * (1.0f - std::clamp(sigma * 2.0f, 0.0f, 1.0f)));

        const float wave2 = std::sin(r * freq * 0.62f - travel * 0.55f + 1.3f);
        const float crest2 = std::pow(0.5f + 0.5f * wave2, 2.8f) * 0.42f;

        const float age = std::fmod(progress * (0.85f + 0.35f * amp) + 0.17f + 10.0f, 1.0f);
        const float R = age * (0.95f + 0.35f * size_m);
        const float drop_ring = SoftBand(r - R, 0.045f + sigma * 0.25f) * (1.0f - age) * WrapFade(age);

        const float fall = std::exp(-r * (0.22f + 0.18f * (1.0f - std::clamp(size_m * 0.5f, 0.0f, 1.0f))));
        const float y_plane = SoftBand(ly, 0.24f + 0.30f * size_m);
        const float k_mod = 0.88f + 0.12f * (0.5f + 0.5f * k);
        const float v = (crest1 + crest2) * fall + drop_ring * 0.85f;
        return std::clamp(v * y_plane * k_mod * (0.8f + 0.2f * amp), 0.0f, 1.0f);
    }

    if(disp == DISP_DROPLETS)
    {
        float best = 0.0f;
        const int drops = std::min(8, 4 + (int)std::lround(4.0f * detail));
        const float fall = 0.55f + 1.1f * amp;
        for(int i = 0; i < drops; i++)
        {
            const float seed = (float)i * 19.17f + 3.1f;
            const float px = Hash11(seed) * 2.0f - 1.0f;
            const float pz = Hash11(seed + 7.3f) * 2.0f - 1.0f;
            const float life = std::fmod(progress * fall + Hash11(seed + 1.7f) + 10.0f, 1.0f);
            const float py = 1.0f - life * 2.2f;
            const float rad = 0.06f + 0.05f * size_m + 0.03f * Hash11(seed + 4.2f);
            const float d = std::sqrt((lx - px) * (lx - px) + (ly - py) * (ly - py) + (lz - pz) * (lz - pz));
            float blob = SoftBand(d, rad + sigma * 0.25f) * WrapFade(life);
            if(life > 0.82f)
            {
                const float splash_r = (life - 0.82f) * 4.0f;
                const float sr = std::sqrt((lx - px) * (lx - px) + (lz - pz) * (lz - pz));
                blob = std::max(blob, SoftBand(sr - splash_r, 0.06f) * SoftBand(ly + 0.85f, 0.12f) * WrapFade(life));
            }
            best = std::max(best, blob);
        }
        return std::clamp(best, 0.0f, 1.0f);
    }

    if(disp == DISP_FIREWORKS)
    {
        float best = 0.0f;
        const int bursts = std::min(4, 2 + (int)std::lround(2.0f * detail));
        for(int i = 0; i < bursts; i++)
        {
            const float seed = (float)i * 31.7f + 11.0f;
            const float life = std::fmod(progress * (0.55f + 0.35f * amp) + Hash11(seed) + 10.0f, 1.0f);
            const float fade = WrapFade(life);
            const float bx = (Hash11(seed + 1.0f) * 2.0f - 1.0f) * 0.55f;
            const float bz = (Hash11(seed + 2.0f) * 2.0f - 1.0f) * 0.55f;
            float by = -0.85f + life * 1.5f;
            float v = 0.0f;
            if(life < 0.45f)
            {
                const float d = std::sqrt((lx - bx) * (lx - bx) + (ly - by) * (ly - by) + (lz - bz) * (lz - bz));
                v = SoftBand(d, 0.05f + sigma * 0.2f);
            }
            else
            {
                by = -0.85f + 0.45f * 1.5f;
                const float expand = (life - 0.45f) / 0.55f;
                const float R = expand * (0.35f + 0.45f * size_m);
                const float d = std::sqrt((lx - bx) * (lx - bx) + (ly - by) * (ly - by) + (lz - bz) * (lz - bz));
                v = SoftBand(d - R, 0.055f + sigma * 0.35f) * (1.0f - expand);
                const float ang = std::atan2(lz - bz, lx - bx);
                const float spark = 0.55f + 0.45f * std::sin(ang * (5.0f + 3.0f * detail) + time_sec * 2.8f);
                v *= 0.5f + 0.5f * spark;
            }
            best = std::max(best, v * fade);
        }
        return std::clamp(best * (0.85f + 0.2f * amp), 0.0f, 1.0f);
    }

    if(disp == DISP_EXPLOSION)
    {
        const float expand = std::fmod(progress * (0.7f + 0.5f * amp) + 10.0f, 1.0f);
        const float fade = WrapFade(expand);
        const float R = expand * (0.55f + 0.65f * size_m);
        const float d = std::sqrt(lx * lx + ly * ly + lz * lz);
        float shell = SoftBand(d - R, 0.07f + sigma * 0.4f) * (1.0f - expand * 0.85f);
        const float ang = std::atan2(lz, lx);
        const float rays = 0.5f + 0.5f * std::sin(ang * (6.0f + 6.0f * detail) + progress * TWO_PI);
        const float streak = SoftBand(d - R * 0.7f, 0.18f) * rays * (1.0f - expand);
        const float core = (expand < 0.18f) ? SoftBand(d, 0.12f) * (1.0f - expand / 0.18f) : 0.0f;
        return std::clamp(std::max(shell, std::max(streak * 0.75f, core)) * fade, 0.0f, 1.0f);
    }

    if(disp == DISP_RAIN)
    {
        float best = 0.0f;
        const int streaks = std::min(12, 6 + (int)std::lround(6.0f * detail));
        const float fall = 0.8f + 1.4f * amp;
        const float slant = 0.35f + 0.25f * freq_n;
        for(int i = 0; i < streaks; i++)
        {
            const float seed = (float)i * 13.91f + 2.4f;
            const float px = Hash11(seed) * 2.0f - 1.0f;
            const float pz = Hash11(seed + 5.5f) * 2.0f - 1.0f;
            const float life = std::fmod(progress * fall + Hash11(seed + 0.7f) + 10.0f, 1.0f);
            const float py = 1.15f - life * 2.4f;
            const float dx = lx - (px + slant * (ly - py) * 0.15f);
            const float dz = lz - pz;
            const float dy = ly - py;
            const float radial = std::sqrt(dx * dx + dz * dz);
            const float along = SoftBand(dy, 0.24f + 0.14f * size_m);
            const float thin = SoftBand(radial, 0.04f + sigma * 0.22f);
            best = std::max(best, thin * along * WrapFade(life));
        }
        return std::clamp(best, 0.0f, 1.0f);
    }

    return 0.0f;
}

void ShellPattern::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    const int pat = std::clamp(UseEffectStripColormap() ? GetEffectStripColormapKernel() : pattern_id, 0,
                               SpatialPatternKernelCount() - 1);
    if(pat <= kSpatialStripGpuKernelMaxId)
    {
        const float reps = UseEffectStripColormap() ? GetEffectStripColormapRepeats() : strip_repeats;
        const float phase01 = CalculateProgress(time_sec);
        const float sp[4] = {(float)pat, phase01, reps, time_sec};
        strip_assist_.prepare(render_sequence, time_sec, sp, 4);
    }

    const int disp = std::clamp(display_mode, 0, DISP_COUNT - 1);
    if(disp < DISP_BARS)
    {
        return;
    }

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float progress_val = BoundedProgress01(time_sec, GetScaledSpeed() * bb.speed_mul);
    const float amp = std::max(0.2f, std::min(2.0f, wave_amplitude * bb.tight_mul));
    const float sigma = std::max(surface_thickness, 0.02f);
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float size_m = std::clamp(GetNormalizedSize(), 0.2f, 2.5f);
    const float freq_n = std::clamp(GetNormalizedFrequency(), 0.05f, 1.0f);

    float vp[8] = {
        (float)disp,
        amp,
        progress_val,
        sigma,
        detail,
        size_m,
        freq_n,
        0.0f // k_hint — per-LED k still drives color via strip; geometry uses mid bias
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 8);
}

RGBColor ShellPattern::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot{x, y, z};
    float coord_y01 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_map_s;
    EffectStratumBlend::InitStratumBreaks(strat_map_s);
    float swt[3];
    EffectStratumBlend::WeightsForYNorm(coord_y01, strat_map_s, swt);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), swt, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(swt, grid, x, y, z, origin, time);

    float progress_val = BoundedProgress01(time, GetScaledSpeed() * bb.speed_mul);
    const float phase_drive = progress_val + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);

    float scale_eff = std::max(0.05f, GetNormalizedScale());
    float sw = grid.width * 0.5f * scale_eff;
    float sh = grid.height * 0.5f * scale_eff;
    float sd = grid.depth * 0.5f * scale_eff;
    if(sw < 1e-5f)
        sw = 1.0f;
    if(sh < 1e-5f)
        sh = 1.0f;
    if(sd < 1e-5f)
        sd = 1.0f;

    float lx = (rot.x - origin.x) / sw;
    float ly = (rot.y - origin.y) / sh;
    float lz = (rot.z - origin.z) / sd;

    /* Room-centered unit coords for radial shell + cube displays (fills whole room). */
    const float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);
    float ox01 = 0.5f, oy01 = 0.5f, oz01 = 0.5f;
    PackEffectOrigin01(grid, origin, &ox01, &oy01, &oz01);
    const float rlx = (nx - ox01) * 2.0f;
    const float rly = (ny - oy01) * 2.0f;
    const float rlz = (nz - oz01) * 2.0f;

    const int unfold_i = UseEffectStripColormap() ? GetEffectStripColormapUnfold() : unfold_mode;
    const float dir_deg = UseEffectStripColormap() ? GetEffectStripColormapDirectionDeg() : direction_deg;
    const float reps = UseEffectStripColormap() ? GetEffectStripColormapRepeats() : strip_repeats;
    auto unfold = static_cast<StripPatternSurface::UnfoldMode>(
        std::clamp(unfold_i, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    float s01 = StripPatternSurface::StripCoord01(lx, ly, lz, unfold, dir_deg);
    int pat = std::clamp(UseEffectStripColormap() ? GetEffectStripColormapKernel() : pattern_id, 0,
                         SpatialPatternKernelCount() - 1);

    float k = 0.0f;
    if(pat <= kSpatialStripGpuKernelMaxId)
    {
        if(strip_assist_.isAvailable())
            k = strip_assist_.sampleKernelSigned(s01);
    }
    else
    {
        k = EvaluateKernel(s01, phase_drive, time, pat, reps);
    }
    float amp = std::max(0.2f, std::min(2.0f, wave_amplitude * bb.tight_mul));

    float intensity = 1.0f;
    float surface_y = amp * k;
    int disp = std::clamp(display_mode, 0, DISP_COUNT - 1);
    if(disp == DISP_SHELL_Y)
    {
        float sigma = std::max(surface_thickness, 0.005f);
        intensity = StripPatternSurface::ShellIntensityGaussianY(ly, surface_y, sigma, amp);
        if(intensity <= 1e-4f)
            return 0x00000000;
    }
    else if(disp == DISP_SHELL_RADIAL_XZ)
    {
        /* Vertical cylinder spanning the room. Pattern profiles radius by angle
         * (or height when Unfold = Along Y for a lathe/vase). */
        auto shell_unfold = unfold;
        if(shell_unfold != StripPatternSurface::UnfoldMode::AlongY)
            shell_unfold = StripPatternSurface::UnfoldMode::RadialXZ;
        const float s_shell = StripPatternSurface::StripCoord01(rlx, rly, rlz, shell_unfold, dir_deg);
        float k_shell = 0.0f;
        if(pat <= kSpatialStripGpuKernelMaxId)
        {
            if(strip_assist_.isAvailable())
                k_shell = strip_assist_.sampleKernelSigned(s_shell);
        }
        else
            k_shell = EvaluateKernel(s_shell, phase_drive, time, pat, reps);
        k = k_shell;

        const float k01 = std::clamp((k_shell + 1.0f) * 0.5f, 0.0f, 1.0f);
        const float size_m = std::clamp(GetNormalizedSize(), 0.35f, 1.6f);
        const float r_span = (0.55f + 0.45f * std::clamp(amp, 0.2f, 2.0f) / 2.0f) * size_m;
        const float surface_r = (0.18f + 0.82f * k01) * r_span;
        const float lr = std::sqrt(rlx * rlx + rlz * rlz);
        const float sigma = std::max(surface_thickness, 0.02f);
        intensity = StripPatternSurface::ShellIntensityGaussianY(lr, surface_r, sigma, std::max(1.0f, amp));
        if(intensity <= 1e-4f)
            return 0x00000000;
    }
    else if(disp == DISP_CONTOUR)
    {
        float sigma = std::max(0.02f, 0.04f + surface_thickness * 0.35f);
        intensity = std::exp(-(k * k) / (sigma * sigma));
        if(intensity <= 1e-4f)
            return 0x00000000;
    }
    else if(disp == DISP_FILL_STRIP)
    {
        intensity = std::clamp((k + 1.0f) * 0.5f, 0.0f, 1.0f);
        if(intensity <= 1e-4f)
            return 0x00000000;
    }
    else
    {
        // LED-cube style volume displays — sample room unit cube (matches GPU atlas).
        if(volume_assist_.isAvailable())
        {
            intensity = volume_assist_.sampleScalar01(nx, ny, nz);
            if(disp == DISP_BARS || disp == DISP_RIPPLES)
            {
                intensity = std::clamp(intensity * (0.82f + 0.18f * (0.5f + 0.5f * k)), 0.0f, 1.0f);
            }
        }
        else
        {
            intensity = 0.0f;
        }
        if(intensity <= 1e-4f)
            return 0x00000000;
    }

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

    float pos_norm = (k + 1.0f) * 0.5f;
    pos_norm = std::clamp(pos_norm, 0.0f, 1.0f);
    float rate = GetScaledFrequency();
    float pos_color = std::fmod(pos_norm + time * rate * 0.009f, 1.0f);
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
        {
            p01 += 1.0f;
        }
        float p01v = p01;
        c = GetRainbowColor(p01v * 360.0f);
    }
    else
    {
        float p_mapped = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
        float p01v = p_mapped;
        c = GetColorAtPosition(p01v);
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
