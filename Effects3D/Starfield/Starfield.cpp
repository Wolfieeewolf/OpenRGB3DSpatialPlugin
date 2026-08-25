// SPDX-License-Identifier: GPL-2.0-only

#include "Starfield.h"
#include "StarfieldVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "EffectHelpers.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

REGISTER_EFFECT_3D(Starfield);

namespace
{
float saturate(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

RGBColor PackRGB(float r, float g, float b, float intensity)
{
    intensity = saturate(intensity);
    if(intensity < 1e-4f)
        return 0x00000000;
    const int ri = std::min(255, std::max(0, (int)(r * intensity * 255.0f)));
    const int gi = std::min(255, std::max(0, (int)(g * intensity * 255.0f)));
    const int bi = std::min(255, std::max(0, (int)(b * intensity * 255.0f)));
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}

void ColorToRGB(RGBColor c, float& r, float& g, float& b)
{
    r = (c & 0xFF) / 255.0f;
    g = ((c >> 8) & 0xFF) / 255.0f;
    b = ((c >> 16) & 0xFF) / 255.0f;
}
} // namespace

const char* Starfield::ModeName(int m)
{
    switch(m)
    {
    case MODE_STARS: return "Stars";
    case MODE_TWINKLE: return "Twinkle";
    case MODE_WARP: return "Warp";
    case MODE_HYPERDRIVE: return "Hyperdrive";
    case MODE_BLACKHOLE: return "Blackhole";
    case MODE_WORMHOLE: return "Wormhole";
    default: return "Stars";
    }
}

Starfield::Starfield(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(45);
    volume_assist_.setFragmentBody(QString::fromUtf8(StarfieldVolumeFieldGlsl()));
    // Sparse particles need more atlas cells than soft fills (Plasma) or they vanish on LEDs/viewport.
    volume_assist_.setResolution(22);
}

EffectInfo3D Starfield::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Space";
    info.effect_description =
        "Cockpit-window space field (GPU volume): stars, warp streaks, blackhole, and wormhole. "
        "Spatial Anchor is the viewpoint; rotate to aim into the room.";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.default_speed_scale = 20.0f;
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

void Starfield::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("StarfieldEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);

    EffectLabeledComboRow* mode_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Mode:"));
    mode_row->setObjectName(QStringLiteral("modeRow"));
    QComboBox* mode_combo = mode_row->combo();
    for(int m = 0; m < MODE_COUNT; m++)
        mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::clamp(this->mode, 0, MODE_COUNT - 1));
    mode_combo->setToolTip(QStringLiteral(
        "Looking out a starship window. Spatial Anchor is the viewpoint; rotate to aim into the room."));
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        this->mode = std::clamp(idx, 0, MODE_COUNT - 1);
        emit ParametersChanged();
    });

    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectSliderRow* star_count_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Particles:"), 12, kMaxGpuParticles, num_stars,
        QStringLiteral("Star / streak count (GPU-capped). Higher is denser but heavier."));
    star_count_row->setObjectName(QStringLiteral("starCountRow"));
    star_count_row->bindValueChanged(
        this, [this](int v) { num_stars = std::clamp(v, 12, kMaxGpuParticles); },
        [](int v) { return QString::number(v); }, on_changed);

    EffectSliderRow* star_size_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Thickness:"), 2, 100, (int)(star_size * 100.0f),
        QStringLiteral("Particle size / streak width / tunnel wall thickness."));
    star_size_row->setObjectName(QStringLiteral("starSizeRow"));
    star_size_row->bindValueChanged(
        this, [this](int v) { star_size = v / 100.0f; }, pct, on_changed);

    EffectSliderRow* fill_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Field of view:"), 40, 100, (int)(fill_amount * 100.0f),
        QStringLiteral("How wide the cockpit view spreads through the room."));
    fill_row->setObjectName(QStringLiteral("fillRow"));
    fill_row->bindValueChanged(
        this, [this](int v) { fill_amount = v / 100.0f; }, pct, on_changed);

    EffectSliderRow* drift_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Sway:"), 0, 100, (int)(drift_amount * 100.0f),
        QStringLiteral("Subtle ship sway / roll on the view."));
    drift_row->setObjectName(QStringLiteral("driftRow"));
    drift_row->bindValueChanged(
        this, [this](int v) { drift_amount = v / 100.0f; }, pct, on_changed);

    EffectSliderRow* twinkle_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Sparkle:"), 0, 100, (int)(twinkle_speed * 100.0f),
        QStringLiteral("Twinkle flash strength (also mild shimmer while cruising)."));
    twinkle_row->setObjectName(QStringLiteral("twinkleRow"));
    twinkle_row->bindValueChanged(
        this, [this](int v) { twinkle_speed = v / 100.0f; }, pct, on_changed);

    AddWidgetToParent(w, parent);
}

RGBColor Starfield::ResolveSpaceColor(const EvalContext& ctx, float pos01, float hue_shift) const
{
    if(UseEffectStripColormap())
    {
        return ResolveStripKernelFinalColor(GetEffectStripColormapKernel(),
                                            std::clamp(ctx.strip_p01, 0.0f, 1.0f), ctx.time);
    }
    if(GetRainbowMode())
    {
        float hue = std::fmod(hue_shift + ctx.color_cycle, 360.0f);
        if(hue < 0.0f)
            hue += 360.0f;
        if(ctx.grid)
        {
            SpatialLayerCore::Basis basis;
            SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
            SpatialLayerCore::MapperSettings map;
            EffectStratumBlend::InitStratumBreaks(map);
            SpatialLayerCore::SamplePoint sp{};
            sp.grid_x = ctx.rp.x;
            sp.grid_y = ctx.rp.y;
            sp.grid_z = ctx.rp.z;
            sp.origin_x = ctx.origin.x;
            sp.origin_y = ctx.origin.y;
            sp.origin_z = ctx.origin.z;
            sp.y_norm = SampleStratumYNorm01(ctx.rp.y, *ctx.grid, ctx.origin);
            hue = ApplySpatialRainbowHue(hue, pos01, basis, sp, map, ctx.time, ctx.grid);
        }
        return GetRainbowColor(hue);
    }
    return GetColorAtPosition(std::clamp(pos01, 0.0f, 1.0f));
}

RGBColor Starfield::FinishSample(const EvalContext& ctx, float intensity, float palette01, float hotness,
                                 int mode_i) const
{
    if(intensity < 0.008f)
        return 0x00000000;

    float cr, cg, cb;
    ColorToRGB(ResolveSpaceColor(ctx, palette01, palette01 * 360.0f), cr, cg, cb);

    if(mode_i == MODE_WARP || mode_i == MODE_HYPERDRIVE || mode_i == MODE_BLACKHOLE)
    {
        const float hot = saturate(hotness);
        cr = cr * (1.0f - hot) + hot;
        cg = cg * (1.0f - hot) + hot * (mode_i == MODE_HYPERDRIVE ? 0.85f : 0.55f);
        cb = cb * (1.0f - hot) + hot * (mode_i == MODE_BLACKHOLE ? 0.12f : 0.70f);
    }
    else if(mode_i == MODE_TWINKLE)
    {
        const float whiten = saturate(hotness);
        cr = cr * (1.0f - whiten) + whiten;
        cg = cg * (1.0f - whiten) + whiten;
        cb = cb * (1.0f - whiten) + whiten;
    }
    else if(mode_i == MODE_WORMHOLE)
    {
        const float cool = hotness;
        cr *= 1.0f - cool * 0.45f;
        cg *= 1.0f - cool * 0.15f;
        cb = std::min(1.0f, cb + cool * 0.35f);
    }

    return PackRGB(cr, cg, cb, intensity);
}

void Starfield::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    const float progress = CalculateProgress(time_sec);
    const int mode_i = std::clamp(mode, 0, MODE_COUNT - 1);
    const int count = std::clamp(num_stars, 12, kMaxGpuParticles);
    const float thickness = std::max(0.02f, star_size);
    const float size_m = std::max(0.25f, GetNormalizedSize());
    const float fill = std::clamp(fill_amount, 0.4f, 1.0f);
    const float hue_scroll = std::fmod(time_sec * GetScaledFrequency() * 0.035f + 1.0f, 1.0f);
    const float vp[10] = {
        progress,
        time_sec,
        (float)mode_i,
        (float)count,
        thickness,
        size_m,
        fill,
        std::clamp(drift_amount, 0.0f, 1.0f),
        std::clamp(twinkle_speed, 0.0f, 1.0f),
        hue_scroll
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 10);
}

RGBColor Starfield::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    if(!volume_assist_.isAvailable())
        return 0x00000000;

    Vector3D rp{x, y, z};
    const int mode_i = std::clamp(this->mode, 0, MODE_COUNT - 1);

    EvalContext ctx;
    ctx.origin = origin;
    ctx.rp = rp;
    ctx.color_cycle = time * GetScaledFrequency() * 12.0f;
    ctx.time = time;
    ctx.grid = &grid;

    const float norm_y = SampleStratumYNorm01(rp.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(norm_y, strat_st, sw);
    ctx.strat_on = (GetStratumLayoutMode() == 1);
    ctx.bb = EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    ctx.stratum_mot01 = ComputeStratumMotion01(sw, grid, x, y, z, origin, time);
    if(ctx.strat_on)
        ctx.color_cycle = ctx.color_cycle * ctx.bb.speed_mul
                          + EffectStratumBlend::CombinedPhase01(ctx.bb, ctx.stratum_mot01) * 360.0f;

    if(UseEffectStripColormap())
    {
        const float sf_phase01 = std::fmod(ctx.color_cycle * (1.0f / 360.0f) + 1.0f, 1.0f);
        ctx.strip_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                   GetEffectStripColormapRepeats(),
                                                   GetEffectStripColormapUnfold(),
                                                   GetEffectStripColormapDirectionDeg(),
                                                   sf_phase01,
                                                   time,
                                                   grid,
                                                   GetNormalizedSize(),
                                                   origin,
                                                   rp);
    }

    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    SampleGpuVolumeOriginLocal01(rp.x, rp.y, rp.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);
    const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
    return FinishSample(ctx, samp.x(), samp.y(), samp.z(), mode_i);
}

nlohmann::json Starfield::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["mode"] = this->mode;
    j["star_size"] = star_size;
    j["num_stars"] = num_stars;
    j["drift_amount"] = drift_amount;
    j["twinkle_speed"] = twinkle_speed;
    j["fill_amount"] = fill_amount;
    return j;
}

void Starfield::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("mode") && settings["mode"].is_number_integer())
        this->mode = std::clamp(settings["mode"].get<int>(), 0, MODE_COUNT - 1);
    if(settings.contains("star_size") && settings["star_size"].is_number())
        star_size = std::clamp(settings["star_size"].get<float>(), 0.02f, 1.0f);
    if(settings.contains("num_stars") && settings["num_stars"].is_number())
        num_stars = std::clamp(settings["num_stars"].get<int>(), 12, kMaxGpuParticles);
    if(settings.contains("drift_amount") && settings["drift_amount"].is_number())
        drift_amount = std::clamp(settings["drift_amount"].get<float>(), 0.0f, 1.0f);
    if(settings.contains("twinkle_speed") && settings["twinkle_speed"].is_number())
        twinkle_speed = std::clamp(settings["twinkle_speed"].get<float>(), 0.0f, 1.0f);
    if(settings.contains("fill_amount") && settings["fill_amount"].is_number())
        fill_amount = std::clamp(settings["fill_amount"].get<float>(), 0.4f, 1.0f);

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "StarfieldEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "modeRow", mode);
            EffectUiSync::setSliderValue(fx, "starCountRow", num_stars, [](int v) { return QString::number(v); });
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "starSizeRow", (int)(star_size * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "fillRow", (int)(fill_amount * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "driftRow", (int)(drift_amount * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "twinkleRow", (int)(twinkle_speed * 100.0f), pct);
        }
    }
}
