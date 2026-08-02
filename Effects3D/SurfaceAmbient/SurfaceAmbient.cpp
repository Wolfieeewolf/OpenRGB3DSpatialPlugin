// SPDX-License-Identifier: GPL-2.0-only

#include "SurfaceAmbient.h"
#include "SurfaceAmbientVolumeFieldGlsl.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include <cmath>
#include <QComboBox>
#include <QCheckBox>
#include <QVector3D>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const char* SurfaceAmbient::StyleName(int s)
{
    switch(s) {
    case STYLE_FIRE: return "Fire";
    case STYLE_WATER: return "Water";
    case STYLE_SLIME: return "Slime";
    case STYLE_LAVA: return "Lava";
    case STYLE_EMBER: return "Embers";
    case STYLE_OCEAN: return "Ocean";
    case STYLE_STEAM: return "Steam";
    default: return "Fire";
    }
}

const char* SurfaceAmbient::MotionName(int m)
{
    switch(m) {
    case MOTION_FIELD: return "Preset (auto)";
    case MOTION_WATERFALL: return "Waterfall";
    case MOTION_RAIN: return "Rain";
    case MOTION_DRIP: return "Drip";
    case MOTION_FIRE_RISE: return "Fire rise";
    case MOTION_WAVES: return "Waves";
    case MOTION_PULSE: return "Pulse";
    default: return "Preset (auto)";
    }
}

namespace {

float saHash(float x, float y)
{
    float h = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
    return h - std::floor(h);
}

float saNoise(float x, float y)
{
    const float ix = std::floor(x);
    const float iy = std::floor(y);
    const float fx = x - ix;
    const float fy = y - iy;
    const float a = saHash(ix, iy);
    const float b = saHash(ix + 1.0f, iy);
    const float c = saHash(ix, iy + 1.0f);
    const float d = saHash(ix + 1.0f, iy + 1.0f);
    const float ux = fx * fx * (3.0f - 2.0f * fx);
    const float uy = fy * fy * (3.0f - 2.0f * fy);
    return a + (b - a) * ux + (c - a) * uy * (1.0f - ux) + (d - b) * ux * uy;
}

float saFbm(float x, float y)
{
    float v = 0.0f;
    float a = 0.5f;
    float px = x;
    float py = y;
    for(int i = 0; i < 4; ++i)
    {
        v += a * saNoise(px, py);
        px *= 2.03f;
        py *= 2.03f;
        a *= 0.5f;
    }
    return v;
}

} // namespace

float SurfaceAmbient::ApplySpatialMotion(int motion, int role, float alongA, float alongB, float up01,
                                         float time, float speed, float base)
{
    if(motion <= MOTION_FIELD)
        return base;
    const float t = time * std::max(0.05f, speed);
    float m = base;

    if(motion == MOTION_WATERFALL || motion == MOTION_RAIN || motion == MOTION_DRIP)
    {
        if(role == 0)
        {
            const float r = std::sqrt((alongA - 0.5f) * (alongA - 0.5f) + (alongB - 0.5f) * (alongB - 0.5f));
            const float splash = std::fabs(std::sin(r * 20.0f - t * (motion == MOTION_RAIN ? 5.0f : 3.5f)));
            m = std::clamp(base * 0.5f + (1.0f - splash) * 0.55f, 0.0f, 1.0f) * 0.65f + base * 0.35f;
        }
        else if(role == 1)
        {
            const float cell = saNoise(std::floor(alongA * 8.0f), std::floor(alongB * 8.0f));
            const float drip = std::fmod(cell * 4.0f + t * (motion == MOTION_DRIP ? 0.55f : 1.2f), 1.0f);
            const float hit = std::pow(1.0f - std::fabs(drip * 2.0f - 1.0f), 2.2f) * (cell > 0.3f ? 1.0f : 0.0f);
            m = std::clamp(base * 0.35f + hit, 0.0f, 1.0f) * 0.7f + base * 0.3f;
        }
        else
        {
            const float flow = (1.0f - up01) - t * (motion == MOTION_WATERFALL ? 1.4f : (motion == MOTION_RAIN ? 2.0f : 0.7f));
            const float sheet = saFbm(alongA * 3.0f, flow * 4.0f);
            m = std::clamp(0.25f + 0.75f * sheet, 0.0f, 1.0f) * 0.7f + base * 0.3f;
        }
    }
    else if(motion == MOTION_FIRE_RISE)
    {
        if(role == 1)
        {
            const float spark = saNoise(alongA * 9.0f + t * 1.5f, alongB * 9.0f + t);
            m = std::clamp(std::pow(std::max(0.0f, spark - 0.6f), 1.5f) * 2.5f, 0.0f, 1.0f) * 0.6f + base * 0.4f;
        }
        else if(role == 0)
        {
            const float rise = saFbm(alongA * 3.0f, alongB * 3.0f - t * 0.8f);
            m = std::clamp(0.3f + 0.7f * rise, 0.0f, 1.0f) * 0.55f + base * 0.45f;
        }
        else
        {
            const float rise = up01 - t * 0.9f;
            const float turb = saFbm(alongA * 3.5f, rise * 5.0f);
            m = std::clamp(0.2f + 0.8f * turb * (1.0f - up01 * 0.5f), 0.0f, 1.0f) * 0.7f + base * 0.3f;
        }
    }
    else if(motion == MOTION_WAVES)
    {
        const float crest = std::sin((alongA + alongB * 0.25f) * 6.2831853f * 1.5f - t * 2.0f);
        if(role == 2)
            m = std::clamp(0.45f + 0.45f * crest + 0.15f * up01, 0.0f, 1.0f) * 0.65f + base * 0.35f;
        else
            m = std::clamp(0.4f + 0.5f * (0.5f + 0.5f * crest), 0.0f, 1.0f) * 0.6f + base * 0.4f;
    }
    else if(motion == MOTION_PULSE)
    {
        const float breathe = 0.55f + 0.45f * std::sin(t * 2.2f);
        m = std::clamp(base * breathe, 0.0f, 1.0f);
    }
    return std::clamp(m, 0.0f, 1.0f);
}

float SurfaceAmbient::EvalPresetField(int style, int role, float alongA, float alongB, float up01,
                                      float time, float freq, float speed, float* sparse_mul)
{
    const float t = time * std::max(0.05f, speed);
    const float f = std::max(0.2f, freq);
    float plasma = 0.5f;
    float sparse = 1.0f;

    if(style == STYLE_FIRE)
    {
        if(role == 0)
        {
            const float bed = saFbm(alongA * (2.2f * f) + t * 0.35f, alongB * (2.2f * f) - t * 0.2f);
            const float edge = std::max(std::fabs(alongA - 0.5f), std::fabs(alongB - 0.5f)) * 2.0f;
            plasma = std::clamp(0.35f + 0.55f * bed + 0.25f * (1.0f - edge), 0.0f, 1.0f);
        }
        else if(role == 1)
        {
            const float spark = saNoise(alongA * (8.0f * f) + t * 1.4f, alongB * (8.0f * f) + t * 0.9f);
            const float flick = 0.5f + 0.5f * std::sin(t * 9.0f + spark * 12.0f);
            plasma = std::clamp(std::pow(std::max(0.0f, spark - 0.62f), 1.4f) * 2.8f * flick, 0.0f, 1.0f);
            sparse = 0.45f + 0.55f * plasma;
        }
        else
        {
            const float rise = up01 - t * 0.55f;
            const float turb = saFbm(alongA * (3.0f * f), rise * (4.5f * f));
            const float tongue = saNoise(alongA * (5.5f * f) + turb * 0.4f, rise * (6.0f * f));
            const float base_boost = 1.0f - up01;
            plasma = std::clamp((0.25f + 0.75f * tongue) * (0.35f + 0.65f * base_boost) * (0.55f + 0.45f * turb), 0.0f, 1.0f);
            plasma *= (1.0f - up01) * 0.72f + 0.28f;
        }
    }
    else if(style == STYLE_WATER)
    {
        if(role == 0)
        {
            const float r = std::sqrt((alongA - 0.5f) * (alongA - 0.5f) + (alongB - 0.5f) * (alongB - 0.5f));
            const float ring = std::fabs(std::sin(r * 18.0f * f - t * 4.0f));
            const float slosh = saFbm(alongA * (3.0f * f) + t * 0.5f, alongB * (3.0f * f) - t * 0.35f);
            plasma = std::clamp(0.3f + 0.45f * slosh + 0.35f * (1.0f - ring) * (1.0f - std::clamp(r * 1.6f, 0.0f, 1.0f)), 0.0f, 1.0f);
        }
        else if(role == 1)
        {
            const float r = std::sqrt((alongA - 0.5f) * (alongA - 0.5f) + (alongB - 0.5f) * (alongB - 0.5f));
            const float pour = std::exp(-r * r * 14.0f) *
                               (0.55f + 0.45f * saNoise(alongA * (6.0f * f) + t * 2.0f, alongB * (6.0f * f)));
            const float outward = std::fabs(std::sin(r * 22.0f * f - t * 3.5f));
            plasma = std::clamp(pour * 1.4f + (1.0f - outward) * 0.25f * (1.0f - std::clamp(r, 0.0f, 1.0f)), 0.0f, 1.0f);
        }
        else
        {
            const float sheet = saFbm(alongA * (2.2f * f), (1.0f - up01) * (5.0f * f) - t * 2.2f);
            const float streaks = std::fabs(std::sin(alongA * 14.0f * f + sheet * 2.0f - t * 3.0f));
            plasma = std::clamp((0.35f + 0.55f * sheet) * (0.4f + 0.6f * up01) * (0.45f + 0.55f * (1.0f - streaks)), 0.0f, 1.0f);
        }
    }
    else if(style == STYLE_SLIME)
    {
        if(role == 0)
        {
            const float pool = saFbm(alongA * (2.4f * f) + t * 0.12f, alongB * (2.4f * f) + t * 0.08f);
            const float settle = 0.55f + 0.45f * std::sin(pool * 6.28318f + t * 0.4f);
            plasma = std::clamp(0.4f + 0.5f * pool * settle, 0.0f, 1.0f);
        }
        else if(role == 1)
        {
            const float cell = saNoise(std::floor(alongA * 7.0f * f), std::floor(alongB * 7.0f * f));
            const float drip = std::fmod(cell * 5.0f + t * (0.35f + 0.4f * cell), 1.0f);
            const float blob = 1.0f - std::fabs(drip * 2.0f - 1.0f);
            const float near = std::min(alongA * 7.0f * f - std::floor(alongA * 7.0f * f),
                                        alongB * 7.0f * f - std::floor(alongB * 7.0f * f));
            plasma = std::clamp(std::pow(blob, 1.6f) * (cell > 0.35f ? 1.0f : 0.0f) * (0.5f + near), 0.0f, 1.0f);
            sparse = 0.5f + 0.5f * plasma;
        }
        else
        {
            const float slide = (1.0f - up01) - t * 0.45f;
            const float stream = saFbm(alongA * (2.8f * f), slide * (3.5f * f));
            const float thick = std::fabs(std::sin(alongA * 9.0f * f + stream * 3.0f));
            plasma = std::clamp((0.3f + 0.7f * stream) * (0.45f + 0.55f * (1.0f - thick * 0.7f)), 0.0f, 1.0f);
        }
    }
    else if(style == STYLE_LAVA)
    {
        if(role == 0)
        {
            const float churn = saFbm(alongA * (2.0f * f) + t * 0.25f, alongB * (2.0f * f) - t * 0.18f);
            const float hot = saNoise(alongA * (5.0f * f) + t * 0.7f, alongB * (5.0f * f) + t * 0.5f);
            plasma = std::clamp(0.35f + 0.4f * churn + 0.35f * hot, 0.0f, 1.0f);
        }
        else if(role == 1)
        {
            const float cell = saNoise(std::floor(alongA * 5.0f * f), std::floor(alongB * 5.0f * f));
            const float drip = std::fmod(cell * 3.0f + t * 0.5f, 1.0f);
            plasma = std::clamp(std::pow(1.0f - std::fabs(drip * 2.0f - 1.0f), 2.0f) *
                                (cell > 0.4f ? 1.0f : 0.0f) * (0.6f + 0.4f * cell), 0.0f, 1.0f);
            sparse = 0.55f + 0.45f * plasma;
        }
        else
        {
            const float flow = (1.0f - up01) - t * 0.35f;
            const float heavy = saFbm(alongA * (2.0f * f), flow * (2.8f * f));
            const float flicker = 0.5f + 0.5f * std::sin(t * 7.0f + heavy * 10.0f);
            plasma = std::clamp((0.3f + 0.7f * heavy) * (0.55f + 0.45f * flicker), 0.0f, 1.0f);
        }
    }
    else if(style == STYLE_EMBER)
    {
        if(role == 0)
        {
            const float edge = std::max(std::fabs(alongA - 0.5f), std::fabs(alongB - 0.5f)) * 2.0f;
            const float src = saNoise(alongA * (10.0f * f) + t * 0.8f, alongB * (10.0f * f) + t * 0.6f);
            plasma = std::clamp(std::pow(std::max(0.0f, src - 0.55f), 1.5f) * (0.4f + 0.9f * edge), 0.0f, 1.0f);
            sparse = 0.25f + 0.75f * plasma;
        }
        else if(role == 1)
        {
            const float spark = saNoise(alongA * (11.0f * f) + t * 1.6f, alongB * (11.0f * f) - t * 1.1f);
            plasma = std::clamp(std::pow(std::max(0.0f, spark - 0.68f), 1.8f) * 3.0f, 0.0f, 1.0f);
            sparse = 0.2f + 0.8f * plasma;
        }
        else
        {
            const float rise = up01 - t * 0.7f;
            const float spark = saNoise(alongA * (9.0f * f), rise * (10.0f * f));
            plasma = std::clamp(std::pow(std::max(0.0f, spark - 0.58f), 1.7f) * (0.5f + 0.5f * (1.0f - up01)), 0.0f, 1.0f);
            sparse = 0.2f + 0.8f * plasma;
        }
    }
    else if(style == STYLE_OCEAN)
    {
        const float current = alongA + alongB * 0.35f;
        if(role == 0)
        {
            const float deep = saFbm(alongA * (1.6f * f) + t * 0.22f, alongB * (1.6f * f) + t * 0.18f);
            const float slow = std::sin(current * 6.28318f * f - t * 0.8f);
            plasma = std::clamp(0.35f + 0.4f * deep + 0.25f * (0.5f + 0.5f * slow), 0.0f, 1.0f);
        }
        else if(role == 1)
        {
            const float caus = saFbm(alongA * (3.5f * f) + t * 0.55f, alongB * (3.5f * f) - t * 0.4f);
            const float rip = std::fabs(std::sin((alongA + alongB) * 12.0f * f - t * 2.2f));
            plasma = std::clamp(0.4f + 0.45f * caus + 0.25f * (1.0f - rip), 0.0f, 1.0f);
        }
        else
        {
            const float caus = saFbm(current * (2.8f * f) - t * 0.45f, up01 * (2.0f * f));
            const float band = 0.5f + 0.5f * std::sin(current * 8.0f * f - t * 1.4f + up01 * 2.0f);
            plasma = std::clamp(0.35f + 0.4f * caus + 0.3f * band, 0.0f, 1.0f);
        }
    }
    else /* STEAM */
    {
        if(role == 0)
        {
            const float edge = std::max(std::fabs(alongA - 0.5f), std::fabs(alongB - 0.5f)) * 2.0f;
            const float vent = saFbm(alongA * (3.5f * f), alongB * (3.5f * f) - t * 0.9f);
            plasma = std::clamp(std::pow(edge, 1.2f) * (0.35f + 0.65f * vent), 0.0f, 1.0f);
            sparse = 0.35f + 0.65f * plasma;
        }
        else if(role == 1)
        {
            const float fog = saFbm(alongA * (2.2f * f) + t * 0.3f, alongB * (2.2f * f) + t * 0.25f);
            const float blob = saNoise(alongA * (4.0f * f) - t * 0.2f, alongB * (4.0f * f));
            plasma = std::clamp(0.3f + 0.45f * fog + 0.3f * blob, 0.0f, 1.0f);
            sparse = 0.55f + 0.35f * plasma;
        }
        else
        {
            const float rise = up01 - t * 0.5f;
            const float haze = saFbm(alongA * (2.5f * f), rise * (3.2f * f));
            const float streak = std::fabs(std::sin(alongA * 7.0f * f + haze * 2.0f - t * 1.5f));
            const float from_bot = 1.0f - up01;
            plasma = std::clamp((0.25f + 0.65f * haze) * (0.35f + 0.65f * from_bot) *
                                (0.5f + 0.5f * (1.0f - streak * 0.6f)), 0.0f, 1.0f);
            sparse = 0.4f + 0.6f * plasma;
        }
    }

    if(sparse_mul)
        *sparse_mul = sparse;
    return std::clamp(plasma, 0.0f, 1.0f);
}

SurfaceAmbient::SurfaceAmbient(QWidget* parent) : SpatialEffect3D(parent)
{
    volume_assist_.setFragmentBody(QString::fromUtf8(SurfaceAmbientVolumeFieldGlsl()));
    volume_assist_.setResolution(20);
}

void SurfaceAmbient::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    // Kernel-on-wall stays CPU (needs per-LED EvalSpatialPatternKernel).
    if(kernel_on_wall)
    {
        return;
    }

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float tm = std::max(0.25f, bb.tight_mul);

    float h_pct = std::max(0.05f, std::min(1.0f, height_pct));
    float sigma = std::max(thickness * 0.5f, 0.02f) / tm;
    float detail = std::max(0.05f, GetScaledDetail()) * tm;
    float freq = std::max(0.3f, std::min(3.0f, 0.3f + detail * 0.27f));
    float speed = std::max(0.0f, std::min(2.0f, GetScaledSpeed() / 4.0f));
    const float time_e = time_sec * bb.speed_mul;
    int mask = GetSurfaceMask();
    if(mask == 0)
        mask = 1;

    float vp[8] = {
        (float)mask,
        (float)std::clamp(style, 0, STYLE_COUNT - 1),
        (float)std::clamp(motion, 0, MOTION_COUNT - 1),
        h_pct,
        sigma,
        freq,
        speed,
        time_e
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 8);
}

EffectInfo3D SurfaceAmbient::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Surface Ambient";
    info.effect_description =
        "Spatial presets (fire, water, slime, lava, embers, ocean, steam) with distinct floor/ceiling/wall behavior; "
        "optional 3D-aware motion override and pattern-on-surface; optional floor/mid/ceiling band tuning";
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
    info.default_frequency_scale = 1.0f;
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

void SurfaceAmbient::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("SurfaceAmbientEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* style_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Preset:"));
    style_row->setObjectName(QStringLiteral("styleRow"));
    QComboBox* style_combo = style_row->combo();
    for(int s = 0; s < STYLE_COUNT; s++)
    {
        style_combo->addItem(StyleName(s));
    }
    style_combo->setCurrentIndex(std::max(0, std::min(style, STYLE_COUNT - 1)));
    style_combo->setToolTip(QStringLiteral(
        "Spatial look with distinct floor / ceiling / wall behavior. Surface mask is in common controls."));
    style_combo->setItemData(0, QStringLiteral("Fire: flames rise on walls; ember bed on floor; sparse sparks on ceiling."), Qt::ToolTipRole);
    style_combo->setItemData(1, QStringLiteral("Water: ceiling pours outward; walls fall as sheets; floor splash/slosh."), Qt::ToolTipRole);
    style_combo->setItemData(2, QStringLiteral("Slime: ceiling drips; walls slide down; floor pools and settles."), Qt::ToolTipRole);
    style_combo->setItemData(3, QStringLiteral("Lava: heavy downward wall flow + hot flicker; ceiling drips; floor churn."), Qt::ToolTipRole);
    style_combo->setItemData(4, QStringLiteral("Embers: thin rising sparks; floor edge sources; ceiling flicker hits."), Qt::ToolTipRole);
    style_combo->setItemData(5, QStringLiteral("Ocean: shared underwater caustic current on walls; surface caustics on ceiling; deep floor."), Qt::ToolTipRole);
    style_combo->setItemData(6, QStringLiteral("Steam: floor-edge vents rise on walls; ceiling fog haze."), Qt::ToolTipRole);
    connect(style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        style = std::max(0, std::min(idx, STYLE_COUNT - 1));
        emit ParametersChanged();
    });

    EffectLabeledComboRow* motion_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Motion:"));
    motion_row->setObjectName(QStringLiteral("motionRow"));
    QComboBox* motion_combo = motion_row->combo();
    for(int m = 0; m < MOTION_COUNT; m++)
        motion_combo->addItem(QString::fromUtf8(MotionName(m)));
    motion_combo->setCurrentIndex(std::clamp(motion, 0, MOTION_COUNT - 1));
    motion_combo->setToolTip(QStringLiteral(
        "Preset (auto) uses the preset’s built-in spatial motion. Other options override with role-aware flow "
        "(walls/floor/ceiling differ) without changing the preset palette."));
    connect(motion_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        motion = std::clamp(idx, 0, MOTION_COUNT - 1);
        emit ParametersChanged();
    });

    QCheckBox* kernel_check = new QCheckBox(QStringLiteral("Pattern on surface"));
    kernel_check->setObjectName(QStringLiteral("kernelOnWallCheck"));
    kernel_check->setChecked(kernel_on_wall);
    kernel_check->setToolTip(QStringLiteral(
        "Optional texture layer: drive brightness from a 1D pattern kernel along the surface. "
        "When off, uses the Preset field. Preset still supplies hue/feel where applicable."));
    layout->addWidget(kernel_check);
    connect(kernel_check, &QCheckBox::toggled, this, [this](bool on) {
        kernel_on_wall = on;
        emit ParametersChanged();
    });

    EffectLabeledComboRow* kernel_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Surface pattern:"));
    kernel_row->setObjectName(QStringLiteral("surfacePatternRow"));
    QComboBox* kernel_combo = kernel_row->combo();
    for(int p = 0; p < SpatialPatternKernelCount(); p++)
        kernel_combo->addItem(QString::fromUtf8(SpatialPatternKernelDisplayName(p)));
    kernel_combo->setCurrentIndex(std::clamp(wall_kernel_id, 0, SpatialPatternKernelCount() - 1));
    kernel_combo->setToolTip(QStringLiteral("Pattern kernel mapped along the surface UV when Pattern on surface is on."));
    connect(kernel_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        wall_kernel_id = std::clamp(idx, 0, SpatialPatternKernelCount() - 1);
        emit ParametersChanged();
    });

    EffectSliderRow* repeats_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Pattern repeats:"),
        1,
        16,
        (int)std::lround(wall_kernel_repeats),
        QStringLiteral("How many pattern cycles across the surface UV."));
    repeats_row->setObjectName(QStringLiteral("patternRepeatsRow"));
    repeats_row->bindValueChanged(
        this,
        [this](int v) { wall_kernel_repeats = (float)std::max(1, v); },
        [](int v) { return QString::number(v); },
        on_changed);

    EffectSliderRow* height_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Height:"),
        5,
        100,
        (int)(height_pct * 100.0f),
        QStringLiteral(
            "How far the effect extends from each enabled shell into the room. "
            "Raise this for taller Fire/Water wall coverage (surface mask in common controls)."));
    height_row->setObjectName(QStringLiteral("heightRow"));
    height_row->bindValueChanged(
        this, [this](int v) { height_pct = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* thickness_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Thickness:"),
        2,
        50,
        (int)(thickness * 100.0f),
        QStringLiteral("Falloff thickness from the shell—higher = softer edge into the volume."));
    thickness_row->setObjectName(QStringLiteral("thicknessRow"));
    thickness_row->bindValueChanged(
        this, [this](int v) { thickness = v / 100.0f; }, pct_format, on_changed);

    AddWidgetToParent(w, parent);
}

static void eval_surface_role(int surf, const GridContext3D& grid, float x, float y, float z,
    float& dist, float& alongA, float& alongB, float& up01, float& extent, int& role)
{
    dist = 0.0f; alongA = 0.0f; alongB = 0.0f; up01 = 0.0f; extent = 0.0f; role = 0;
    const float nx = NormalizeGridAxis01(x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(z, grid.min_z, grid.max_z);
    switch(surf)
    {
    case 1:
        extent = std::max(0.001f, grid.height);
        dist = y - grid.min_y;
        alongA = nx; alongB = nz; up01 = 0.0f; role = 0;
        break;
    case 2:
        extent = std::max(0.001f, grid.height);
        dist = grid.max_y - y;
        alongA = nx; alongB = nz; up01 = 1.0f; role = 1;
        break;
    case 4:
        extent = std::max(0.001f, grid.width);
        dist = x - grid.min_x;
        alongA = nz; alongB = ny; up01 = ny; role = 2;
        break;
    case 8:
        extent = std::max(0.001f, grid.width);
        dist = grid.max_x - x;
        alongA = nz; alongB = ny; up01 = ny; role = 2;
        break;
    case 16:
        extent = std::max(0.001f, grid.depth);
        dist = z - grid.min_z;
        alongA = nx; alongB = ny; up01 = ny; role = 2;
        break;
    case 32:
        extent = std::max(0.001f, grid.depth);
        dist = grid.max_z - z;
        alongA = nx; alongB = ny; up01 = ny; role = 2;
        break;
    default:
        break;
    }
}

RGBColor SurfaceAmbient::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rp{x, y, z};
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);

    const float tm = std::max(0.25f, bb.tight_mul);

    float h_pct = std::max(0.05f, std::min(1.0f, height_pct));
    float sigma = std::max(thickness * 0.5f, 0.02f) / tm;
    float speed = std::max(0.0f, std::min(2.0f, GetScaledSpeed() / 4.0f));
    const float time_e = time * bb.speed_mul;
    int mask = GetSurfaceMask();
    if(mask == 0) mask = 1;

    float best_intensity = 0.0f;
    float best_plasma = 0.0f;

    if(volume_assist_.isAvailable() && !kernel_on_wall)
    {
        const float nx = NormalizeGridAxis01(x, grid.min_x, grid.max_x);
        const float ny = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
        const float nz = NormalizeGridAxis01(z, grid.min_z, grid.max_z);
        const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
        best_intensity = samp.x();
        best_plasma = samp.y();
        if(GetStratumLayoutMode() == 1)
            best_intensity = EffectStratumBlend::ApplyMotionToUnit01(best_intensity, stratum_mot01, 0.18f);
    }
    else if(kernel_on_wall)
    {
        for(int bit = 1; bit <= 32; bit <<= 1)
        {
            if(!(mask & bit)) continue;
            float dist, alongA, alongB, up01, extent;
            int role = 0;
            eval_surface_role(bit, grid, x, y, z, dist, alongA, alongB, up01, extent, role);
            float height_ext = h_pct * extent;
            if(dist < 0.0f || dist > height_ext) continue;
            float d_sigma = sigma * extent;
            float intensity = expf(-dist * dist / (d_sigma * d_sigma));
            float s01 = std::fmod(alongA * 0.72f + alongB * 0.28f + 2.0f, 1.0f);
            float k = EvalSpatialPatternKernel(wall_kernel_id, s01, CalculateProgress(time_e),
                                               wall_kernel_repeats, time_e);
            float plasma = std::clamp((k + 1.0f) * 0.5f, 0.0f, 1.0f);
            plasma = ApplySpatialMotion(motion, role, alongA, alongB, up01, time_e, speed, plasma);
            if(intensity > best_intensity) { best_intensity = intensity; best_plasma = plasma; }
        }
    }

    if(best_intensity < 0.01f) return 0x00000000;

    float hue;
    if(GetRainbowMode() && style != STYLE_STEAM)
    {
        hue = fmodf(best_plasma * 360.0f + time * GetScaledFrequency() * 12.0f * bb.speed_mul  + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
    }
    else if(style != STYLE_STEAM)
    {
        switch(style)
        {
        case STYLE_FIRE: hue = 20.0f + best_plasma * 40.0f; break;
        case STYLE_WATER: hue = 190.0f + best_plasma * 40.0f; break;
        case STYLE_SLIME: hue = 100.0f + best_plasma * 30.0f; break;
        case STYLE_LAVA: hue = 25.0f + best_plasma * 35.0f; break;
        case STYLE_EMBER: hue = 12.0f + best_plasma * 22.0f; break;
        case STYLE_OCEAN: hue = 200.0f + best_plasma * 30.0f; break;
        default: hue = best_plasma * 360.0f;
        }
        hue = fmodf(hue + time * GetScaledFrequency() * 12.0f * bb.speed_mul  + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
    }

    float palette_driver = best_plasma;
    if(UseEffectStripColormap() && style != STYLE_STEAM)
    {
        const float size_m = GetNormalizedSize();
        const float ph01 = std::fmod(time * GetScaledFrequency() * 12.0f * bb.speed_mul * (1.f / 360.f) +
                                         EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + best_plasma * 0.08f + 1.f,
                                     1.f);
        palette_driver = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                    GetEffectStripColormapRepeats(),
                                                    GetEffectStripColormapUnfold(),
                                                    GetEffectStripColormapDirectionDeg(),
                                                    ph01,
                                                    time,
                                                    grid,
                                                    size_m,
                                                    origin,
                                                    rp);
    }

    RGBColor c;
    if(style == STYLE_STEAM)
    {
        unsigned char gv = (unsigned char)(180 + (int)(best_plasma * 75));
        c = (RGBColor)((gv << 16) | (gv << 8) | gv);
    }
    else if(UseEffectStripColormap())
    {
        float sp = palette_driver;
        c      = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), std::clamp(sp, 0.0f, 1.0f), time);
    }
    else if(GetRainbowMode())
        c = GetRainbowColor(hue);
    else
        c = GetColorAtPosition(palette_driver);
    float mult = best_intensity;
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * mult)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * mult)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * mult)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json SurfaceAmbient::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["style"] = style;
    j["motion"] = motion;
    j["kernel_on_wall"] = kernel_on_wall;
    j["wall_kernel_id"] = wall_kernel_id;
    j["wall_kernel_repeats"] = wall_kernel_repeats;
    j["height_pct"] = height_pct;
    j["thickness"] = thickness;
    return j;
}

void SurfaceAmbient::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("style") && settings["style"].is_number_integer())
        style = std::max(0, std::min(settings["style"].get<int>(), (int)STYLE_COUNT - 1));
    if(settings.contains("motion") && settings["motion"].is_number_integer())
        motion = std::clamp(settings["motion"].get<int>(), 0, MOTION_COUNT - 1);
    if(settings.contains("kernel_on_wall") && settings["kernel_on_wall"].is_boolean())
        kernel_on_wall = settings["kernel_on_wall"].get<bool>();
    if(settings.contains("wall_kernel_id") && settings["wall_kernel_id"].is_number_integer())
        wall_kernel_id = std::clamp(settings["wall_kernel_id"].get<int>(), 0, SpatialPatternKernelCount() - 1);
    if(settings.contains("wall_kernel_repeats") && settings["wall_kernel_repeats"].is_number())
        wall_kernel_repeats = std::max(1.0f, std::min(16.0f, settings["wall_kernel_repeats"].get<float>()));
    if(settings.contains("height_pct") && settings["height_pct"].is_number())
        height_pct = std::max(0.05f, std::min(1.0f, settings["height_pct"].get<float>()));
    if(settings.contains("thickness") && settings["thickness"].is_number())
        thickness = std::max(0.02f, std::min(0.5f, settings["thickness"].get<float>()));

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "SurfaceAmbientEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "styleRow", style);
            EffectUiSync::setComboIndex(fx, "motionRow", motion);
            EffectUiSync::setComboIndex(fx, "surfacePatternRow", wall_kernel_id);
            if(QCheckBox* kc = fx->findChild<QCheckBox*>(QStringLiteral("kernelOnWallCheck")))
                kc->setChecked(kernel_on_wall);
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "heightRow", (int)(height_pct * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "thicknessRow", (int)(thickness * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "patternRepeatsRow", (int)std::lround(wall_kernel_repeats),
                                         [](int v) { return QString::number(v); });
        }
    }
}

REGISTER_EFFECT_3D(SurfaceAmbient);
