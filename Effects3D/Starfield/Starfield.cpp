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
constexpr float kTau = 6.28318530718f;

float hash_float(unsigned int seed, unsigned int salt)
{
    unsigned int v = seed * 73856093u ^ salt * 19349663u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
}

float hash01(unsigned int seed, unsigned int salt)
{
    return hash_float(seed, salt) * 0.5f + 0.5f;
}

float frac01(float v)
{
    return v - std::floor(v);
}

float saturate(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

float sparkle_flash(float phase, float sharpness)
{
    const float s = std::max(0.0f, std::sin(phase));
    return std::pow(s, std::max(4.0f, sharpness));
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
    // Moderate atlas: particle loop is inside the shader, keep voxels lean.
    volume_assist_.setResolution(16);
}

EffectInfo3D Starfield::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Space";
    info.effect_description =
        "Cockpit-window space field with GPU assist: stars, warp streaks, blackhole, and wormhole. "
        "Place the effect origin at the viewpoint; rotate to aim into the room.";
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
        "Looking out a starship window. Place the effect origin at the viewpoint; rotate to aim into the room."));
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

Starfield::ViewSample Starfield::MakeViewSample(const Vector3D& rp,
                                               const Vector3D& origin,
                                               const EffectGridAxisHalfExtents& e,
                                               float fill) const
{
    ViewSample v;
    const float fov = std::max(0.4f, fill);
    v.vx = (rp.x - origin.x) / std::max(1e-4f, e.hw * fov);
    v.vy = (rp.y - origin.y) / std::max(1e-4f, e.hh * fov);
    v.vz = (rp.z - origin.z) / std::max(1e-4f, e.hd * fov);
    v.radial = std::sqrt(v.vx * v.vx + v.vy * v.vy);
    v.ang = std::atan2(v.vy, v.vx);
    return v;
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
            sp.y_norm = NormalizeGridAxis01(ctx.rp.y, ctx.grid->min_y, ctx.grid->max_y);
            hue = ApplySpatialRainbowHue(hue, pos01, basis, sp, map, ctx.time, ctx.grid);
        }
        return GetRainbowColor(hue);
    }
    return GetColorAtPosition(std::clamp(pos01, 0.0f, 1.0f));
}

RGBColor Starfield::FinishSample(const EvalContext& ctx, float intensity, float palette01, float hotness,
                                 int mode_i) const
{
    if(intensity < 0.015f)
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

void Starfield::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float ox = 0.5f, oy = 0.5f, oz = 0.5f;
    PackEffectOrigin01(grid, origin, &ox, &oy, &oz);
    const float progress = CalculateProgress(time_sec);
    const int mode_i = std::clamp(mode, 0, MODE_COUNT - 1);
    const int count = std::clamp(num_stars, 12, kMaxGpuParticles);
    const float thickness = std::max(0.02f, star_size);
    const float size_m = std::max(0.25f, GetNormalizedSize());
    const float fill = std::clamp(fill_amount, 0.4f, 1.0f);
    const float hue_scroll = std::fmod(time_sec * GetScaledFrequency() * 0.035f + 1.0f, 1.0f);
    const float vp[13] = {
        progress,
        time_sec,
        (float)mode_i,
        (float)count,
        thickness,
        size_m,
        fill,
        std::clamp(drift_amount, 0.0f, 1.0f),
        std::clamp(twinkle_speed, 0.0f, 1.0f),
        hue_scroll,
        ox,
        oy,
        oz
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 13);
}

RGBColor Starfield::AccumParticles(const EvalContext& ctx, int mode_i) const
{
    // Slim CPU fallback — fewer particles, one color resolve after accumulate.
    const int n = std::max(8, std::min(kMaxCpuParticles, ctx.particle_count));
    const ViewSample& led = ctx.view;

    const float sway = ctx.drift * 0.18f;
    const float sway_c = std::cos(ctx.time * 0.35f * sway * 6.0f + 0.4f);
    const float sway_s = std::sin(ctx.time * 0.28f * sway * 6.0f);
    const float led_vx = led.vx * (1.0f + sway * 0.08f * sway_c) - led.vy * sway * 0.12f * sway_s;
    const float led_vy = led.vy * (1.0f + sway * 0.08f * sway_c) + led.vx * sway * 0.12f * sway_s;
    const float led_vz = led.vz;

    const float base_thick = std::max(0.012f, ctx.thickness * 0.40f * ctx.size_m);
    float sum_i = 0.0f, sum_p = 0.0f, sum_h = 0.0f;

    for(int i = 0; i < n; i++)
    {
        const float dir_x = hash_float((unsigned int)i, 31u);
        const float dir_y = hash_float((unsigned int)i, 32u);
        const float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y) + 1e-4f;
        const float ux = dir_x / dir_len;
        const float uy = dir_y / dir_len;
        const float aim = 0.15f + 0.95f * hash01((unsigned int)i, 33u);
        const float seed_d = hash01((unsigned int)i, 34u);

        float depth, stretch = 1.0f, bright = 1.0f;
        if(mode_i == MODE_TWINKLE)
        {
            depth = 0.35f + 0.60f * seed_d;
            const float rate = 1.0f + ctx.twinkle * 5.0f + hash01((unsigned int)i, 35u) * 3.0f;
            const float ph = ctx.time * rate * 0.8f + seed_d * kTau;
            bright = 0.14f + 0.86f * sparkle_flash(ph, 12.0f);
        }
        else
        {
            float travel = ctx.progress * 0.22f;
            if(mode_i == MODE_WARP)
                travel *= 1.35f;
            if(mode_i == MODE_HYPERDRIVE)
                travel *= 2.05f;
            depth = frac01(seed_d - travel);
            const float nearness = 1.0f - depth;
            if(mode_i == MODE_STARS)
            {
                stretch = 1.0f + nearness * nearness * (2.2f + 2.0f * ctx.size_m);
                bright = 0.35f + 0.65f * nearness;
            }
            else if(mode_i == MODE_WARP)
            {
                stretch = 1.0f + nearness * (8.0f + 10.0f * ctx.size_m);
                bright = 0.25f + 0.90f * nearness;
            }
            else
            {
                stretch = 1.0f + nearness * (14.0f + 16.0f * ctx.size_m);
                bright = 0.20f + 1.10f * nearness;
            }
        }

        const float persp = 1.0f / std::max(0.06f, 0.08f + depth * 0.92f);
        const float px = ux * aim * persp * 0.95f;
        const float py = uy * aim * persp * 0.95f;
        const float pz = depth * 2.0f - 1.0f;

        float dx = led_vx - px, dy = led_vy - py, dz = led_vz - pz;
        float mx = ux, my = uy, mz = -1.0f;
        if(mode_i == MODE_WARP || mode_i == MODE_HYPERDRIVE)
            mz = -0.35f - 0.65f * (1.0f - depth);
        const float mlen = std::sqrt(mx * mx + my * my + mz * mz) + 1e-4f;
        mx /= mlen;
        my /= mlen;
        mz /= mlen;

        const float along = dx * mx + dy * my + dz * mz;
        const float ax = dx - along * mx, ay = dy - along * my, az = dz - along * mz;
        const float across2 = ax * ax + ay * ay + az * az;
        float thick = base_thick * ((mode_i == MODE_TWINKLE) ? 0.70f : (mode_i == MODE_HYPERDRIVE ? 0.85f : 1.0f));
        const float along_sig = thick * stretch;
        const float across_sig = thick;
        const float d2 = (along * along) / (along_sig * along_sig) + across2 / (across_sig * across_sig);
        if(d2 > 10.0f)
            continue;

        float contrib = std::exp(-d2) * bright;
        if(contrib < 0.02f)
            continue;

        float hot = 0.0f;
        if(mode_i == MODE_WARP || mode_i == MODE_HYPERDRIVE)
            hot = saturate((1.0f - depth) * (mode_i == MODE_HYPERDRIVE ? 0.75f : 0.45f));
        else if(mode_i == MODE_TWINKLE)
            hot = saturate((bright - 0.5f) * 1.2f);

        sum_i += contrib;
        sum_p += (1.0f - depth) * contrib;
        sum_h += hot * contrib;
    }

    if(sum_i < 1e-5f)
        return 0x00000000;
    const float intensity = std::clamp(sum_i * (mode_i == MODE_HYPERDRIVE ? 1.15f : 0.95f), 0.0f, 1.0f);
    const float palette01 = frac01(sum_p / sum_i);
    const float hotness = saturate(sum_h / sum_i);
    return FinishSample(ctx, intensity, palette01, hotness, mode_i);
}

RGBColor Starfield::EvalBlackhole(const EvalContext& ctx) const
{
    const ViewSample& v = ctx.view;
    const float r_xy = std::max(1e-4f, v.radial);
    const float ang = v.ang + ctx.drift * 0.25f * std::sin(ctx.time * 0.4f);
    const float horizon = 0.14f + 0.05f * ctx.thickness;
    if(r_xy < horizon && v.vz > -0.2f)
        return 0x00000000;

    const float disk_thick = std::max(0.04f, ctx.thickness * 0.50f * ctx.size_m);
    const float plane = std::exp(-(v.vz * v.vz) / (disk_thick * disk_thick * 4.0f))
                        * std::exp(-(v.vy * v.vy) / (disk_thick * disk_thick));
    const float disk = plane * saturate((r_xy - horizon * 1.1f) / std::max(1e-4f, horizon * 0.5f))
                       * saturate((1.15f - r_xy) / 0.60f);
    const float spiral = 0.5f + 0.5f * std::sin(4.0f * ang - std::log(r_xy) * 3.8f - ctx.progress * 3.5f);
    const float photon = std::exp(-std::pow((r_xy - horizon * 1.35f) / (0.04f + 0.03f * ctx.thickness), 2.0f));
    const float lens = std::exp(-std::pow((r_xy - horizon) / 0.08f, 2.0f)) * 0.45f;
    float intensity = disk * (0.30f + 0.70f * spiral) + photon * 1.4f + lens;
    if(intensity < 0.02f)
        return 0x00000000;
    const float hotness = saturate(1.0f - (r_xy - horizon) / 0.5f) * 0.55f + photon * 0.4f;
    return FinishSample(ctx, intensity, saturate(1.0f - r_xy), hotness, MODE_BLACKHOLE);
}

RGBColor Starfield::EvalWormhole(const EvalContext& ctx) const
{
    const ViewSample& v = ctx.view;
    const float rad = v.radial;
    const float ang = v.ang;
    const float depth = saturate(v.vz * 0.5f + 0.5f);
    const float tunnel_r = 0.38f + 0.20f * ctx.size_m;
    const float wall_w = std::max(0.03f, ctx.thickness * 0.50f * ctx.size_m);
    const float wall = std::exp(-std::pow((rad - tunnel_r) / wall_w, 2.0f));
    const float rings = 0.5f + 0.5f * std::cos((depth * 5.5f + ctx.progress * 0.85f) * kTau);
    const float helix = 0.5f + 0.5f * std::cos(6.0f * ang + depth * (5.0f + ctx.drift * 4.0f) - ctx.progress * 6.6f);
    const float perspective = 0.30f + 0.70f * (1.0f - depth);
    float intensity = wall * (0.28f + 0.40f * rings + 0.48f * helix) * perspective;
    if(rad > tunnel_r + wall_w * 3.0f)
        intensity = 0.0f;
    intensity *= 1.25f;
    if(intensity < 0.02f)
        return 0x00000000;
    return FinishSample(ctx, intensity, depth, depth * 0.35f, MODE_WORMHOLE);
}

RGBColor Starfield::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rp{x, y, z};
    const int mode_i = std::clamp(this->mode, 0, MODE_COUNT - 1);

    EvalContext ctx;
    ctx.origin = origin;
    ctx.rp = rp;
    ctx.e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    ctx.fill = std::clamp(fill_amount, 0.4f, 1.0f);
    ctx.view = MakeViewSample(rp, origin, ctx.e, ctx.fill);
    ctx.progress = CalculateProgress(time);
    ctx.size_m = GetNormalizedSize();
    ctx.thickness = std::max(0.02f, star_size);
    ctx.drift = std::clamp(drift_amount, 0.0f, 1.0f);
    ctx.twinkle = std::clamp(twinkle_speed, 0.0f, 1.0f);
    ctx.color_cycle = time * GetScaledFrequency() * 12.0f;
    ctx.time = time;
    ctx.grid = &grid;
    ctx.particle_count = std::clamp(num_stars, 12, kMaxGpuParticles);

    float oy = 0.5f;
    PackEffectOrigin01(grid, origin, nullptr, &oy, nullptr);
    const float norm_y = std::clamp(NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y) - oy + 0.5f, 0.0f, 1.0f);
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
                                                   ctx.size_m,
                                                   origin,
                                                   rp);
    }

    if(volume_assist_.isAvailable())
    {
        const float nx = NormalizeGridAxis01(rp.x, grid.min_x, grid.max_x);
        const float ny = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
        const float nz = NormalizeGridAxis01(rp.z, grid.min_z, grid.max_z);
        const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
        return FinishSample(ctx, samp.x(), samp.y(), samp.z(), mode_i);
    }

    if(mode_i == MODE_BLACKHOLE)
        return EvalBlackhole(ctx);
    if(mode_i == MODE_WORMHOLE)
        return EvalWormhole(ctx);
    return AccumParticles(ctx, mode_i);
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
