// SPDX-License-Identifier: GPL-2.0-only

#include "SurfaceAmbient.h"
#include "SurfaceAmbientVolumeFieldGlsl.h"
#include "PluginLog.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QByteArray>
#include <QComboBox>
#include <QVector3D>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

const char* SurfaceAmbient::StyleName(int s)
{
    switch(s) {
    case STYLE_NONE: return "None (use Motion + colors)";
    case STYLE_FIRE: return "Fire";
    case STYLE_WATER: return "Water";
    case STYLE_SLIME: return "Slime";
    case STYLE_LAVA: return "Lava";
    case STYLE_EMBER: return "Embers";
    case STYLE_OCEAN: return "Ocean";
    case STYLE_STEAM: return "Steam";
    default: return "None (use Motion + colors)";
    }
}

const char* SurfaceAmbient::MotionName(int m)
{
    switch(m) {
    case MOTION_SOFT: return "Soft field";
    case MOTION_WATERFALL: return "Waterfall";
    case MOTION_RAIN: return "Rain";
    case MOTION_DRIP: return "Drip";
    case MOTION_FIRE_RISE: return "Fire rise";
    case MOTION_WAVES: return "Waves";
    case MOTION_PULSE: return "Pulse";
    default: return "Soft field";
    }
}

SurfaceAmbient::SurfaceAmbient(QWidget* parent) : SpatialEffect3D(parent)
{
    /* Sensible mid defaults so Speed / Frequency / Size actually move the look. */
    SetSpeed(55);
    SetFrequency(50);
    SetDetail(90);
    effect_size = 120;
    effect_scale = 200;
    volume_assist_.setFragmentBody(QString::fromUtf8(SurfaceAmbientVolumeFieldGlsl()));
    volume_assist_.setResolution(28);
}

void SurfaceAmbient::UpdateMotionUiEnabled()
{
    const bool motion_ok = !HasLockedPreset();
    if(motion_combo_)
        motion_combo_->setEnabled(motion_ok);
    if(motion_row_)
        motion_row_->setEnabled(motion_ok);
}

void SurfaceAmbient::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float tm = std::max(0.25f, bb.tight_mul);

    /* Scale = how far the shell reaches into the room (global coverage). */
    const float scale_n = std::max(0.2f, GetNormalizedScale());
    const float h_pct = std::clamp(0.12f + 0.40f * std::min(scale_n, 1.75f), 0.08f, 0.98f);
    float sigma = std::max(thickness * 0.5f, 0.02f) / tm;

    /* Detail = pattern density; Size = feature scale (bigger Size → larger tongues/pools). */
    const float detail = std::max(0.05f, GetScaledDetail()) * tm;
    const float freq = std::clamp(0.28f + detail * 0.22f, 0.22f, 3.0f);
    const float feature = std::clamp(GetNormalizedSize(), 0.45f, 3.0f);
    const float speed = std::clamp(GetScaledSpeed() / 2.0f, 0.35f, 4.0f);
    const float band_mul = std::max(0.15f, bb.speed_mul);

    int mask = GetSurfaceMask();
    if(mask == 0)
        mask = 1;

    const float style_gpu = (float)std::clamp(style, 0, STYLE_COUNT - 1);
    const float motion_gpu = HasLockedPreset() ? 0.0f : (float)std::clamp(motion, 0, MOTION_COUNT - 1);

    float vp[9] = {
        (float)mask,
        style_gpu,
        motion_gpu,
        h_pct,
        sigma,
        freq,
        speed,
        band_mul,
        feature
    };
    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 9))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] SurfaceAmbient volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

EffectInfo3D SurfaceAmbient::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Surface Ambient";
    info.effect_description =
        "Room-shell ambience: locked presets (fire, water, …) with their own look, or Preset None "
        "to drive Motion with your Colors / Patterns. Use Speed / Size / Scale / Detail / Frequency "
        "in Motion controls — only Thickness is effect-specific.";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 8.0f;
    info.default_detail_scale = 10.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_detail_control = true;
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
    style_combo_ = style_row->combo();
    for(int s = 0; s < STYLE_COUNT; s++)
        style_combo_->addItem(StyleName(s));
    style_combo_->setCurrentIndex(std::clamp(style, 0, STYLE_COUNT - 1));
    style_combo_->setToolTip(QStringLiteral(
        "Locked presets use their own motion and colors (ignores Motion dropdown + Colors/Patterns).\n"
        "Tune with global Speed / Size / Scale / Detail / Frequency.\n"
        "None: Motion dropdown + your Colors / Patterns / rainbow."));
    style_combo_->setItemData(STYLE_NONE, QStringLiteral("Custom: pick Motion and use Colors & Patterns."), Qt::ToolTipRole);
    style_combo_->setItemData(STYLE_FIRE, QStringLiteral("Fire: flames rise on walls; ember bed on floor; sparks on ceiling."), Qt::ToolTipRole);
    style_combo_->setItemData(STYLE_WATER, QStringLiteral("Water: ceiling pours; walls fall as sheets; floor splash."), Qt::ToolTipRole);
    style_combo_->setItemData(STYLE_SLIME, QStringLiteral("Slime: ceiling drips; walls slide; floor pools."), Qt::ToolTipRole);
    style_combo_->setItemData(STYLE_LAVA, QStringLiteral("Lava: heavy downward flow + hot flicker."), Qt::ToolTipRole);
    style_combo_->setItemData(STYLE_EMBER, QStringLiteral("Embers: low coal bed, thin flame wisps, rising sparks."), Qt::ToolTipRole);
    style_combo_->setItemData(STYLE_OCEAN, QStringLiteral("Ocean: caustic currents."), Qt::ToolTipRole);
    style_combo_->setItemData(STYLE_STEAM, QStringLiteral("Steam: grey vents and haze."), Qt::ToolTipRole);
    connect(style_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        style = std::clamp(idx, 0, STYLE_COUNT - 1);
        UpdateMotionUiEnabled();
        emit ParametersChanged();
    });

    EffectLabeledComboRow* motion_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Motion:"));
    motion_row->setObjectName(QStringLiteral("motionRow"));
    motion_row_ = motion_row;
    motion_combo_ = motion_row->combo();
    for(int m = 0; m < MOTION_COUNT; m++)
        motion_combo_->addItem(QString::fromUtf8(MotionName(m)));
    motion_combo_->setCurrentIndex(std::clamp(motion, 0, MOTION_COUNT - 1));
    motion_combo_->setToolTip(QStringLiteral(
        "Only when Preset is None. Pick a flow (waterfall, drip, fire rise, …) and color it "
        "with Colors & Patterns — e.g. green “fire rise”."));
    connect(motion_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        motion = std::clamp(idx, 0, MOTION_COUNT - 1);
        emit ParametersChanged();
    });
    UpdateMotionUiEnabled();

    EffectSliderRow* thickness_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Thickness:"),
        2,
        50,
        (int)(thickness * 100.0f),
        QStringLiteral(
            "Soft falloff from the shell into the volume (no global equivalent).\n"
            "Shell depth → Scale. Feature size → Size. Animation → Speed. "
            "Pattern density → Detail. Color range → Frequency."));
    thickness_row->setObjectName(QStringLiteral("thicknessRow"));
    thickness_row->bindValueChanged(
        this, [this](int v) { thickness = v / 100.0f; }, pct_format, on_changed);

    AddWidgetToParent(w, parent);
}

RGBColor SurfaceAmbient::PresetColor(float plasma01, float time, float speed_mul, float stratum_phase01) const
{
    const float p = std::clamp(plasma01, 0.0f, 1.0f);
    /* Frequency widens the palette; slight shimmer keeps color alive without a full rainbow wash. */
    const float spread = std::clamp(0.9f + GetScaledFrequency() * 0.14f, 0.9f, 2.6f);
    const float shimmer =
        std::sin(time * std::max(0.25f, GetScaledFrequency()) * 1.15f * speed_mul + p * 6.28318f) *
        (5.0f + 9.0f * p);

    if(style == STYLE_STEAM)
    {
        /* Cool mist → warm haze instead of flat grey. */
        const float cool = 150.0f + p * 70.0f;
        const float warm = 190.0f + p * 50.0f;
        const float mix = std::clamp(0.35f + 0.5f * p, 0.0f, 1.0f);
        const unsigned char r = (unsigned char)std::clamp((int)(cool * (1.0f - mix) + warm * mix), 0, 255);
        const unsigned char g = (unsigned char)std::clamp((int)(cool * 0.95f + p * 40.0f), 0, 255);
        const unsigned char b = (unsigned char)std::clamp((int)(cool + (1.0f - p) * 35.0f), 0, 255);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    float hue0 = 0.0f;
    float span = 60.0f;
    switch(style)
    {
    case STYLE_FIRE:  hue0 = 8.0f;   span = 52.0f; break;
    case STYLE_WATER: hue0 = 165.0f; span = 75.0f; break;  /* teal → blue → cyan */
    case STYLE_SLIME: hue0 = 70.0f;  span = 70.0f; break;  /* yellow-green → lime */
    case STYLE_LAVA:  hue0 = 0.0f;   span = 62.0f; break;  /* red → orange → yellow */
    case STYLE_EMBER: hue0 = 4.0f;   span = 50.0f; break;  /* deep red coals → orange wisps */
    case STYLE_OCEAN: hue0 = 175.0f; span = 70.0f; break;  /* deep blue → aqua */
    default: hue0 = p * 360.0f; span = 0.0f; break;
    }
    float hue = hue0 + p * span * spread + shimmer + stratum_phase01 * 14.0f;
    hue = std::fmod(hue, 360.0f);
    if(hue < 0.0f)
        hue += 360.0f;
    return GetRainbowColor(hue);
}

RGBColor SurfaceAmbient::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rp{x, y, z};
    float coord2 = SampleStratumYNorm01(rp.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);

    if(!volume_assist_.isAvailable())
        return 0x00000000;

    const float nx = NormalizeGridAxis01(x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(z, grid.min_z, grid.max_z);
    const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
    float best_intensity = samp.x();
    float best_plasma = samp.y();
    if(GetStratumLayoutMode() == 1)
        best_intensity = EffectStratumBlend::ApplyMotionToUnit01(best_intensity, stratum_mot01, 0.18f);

    if(best_intensity < 0.01f)
        return 0x00000000;

    const float phase01 = EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);
    RGBColor c;

    if(HasLockedPreset())
    {
        c = PresetColor(best_plasma, time, bb.speed_mul, phase01);
    }
    else
    {
        float palette_driver = best_plasma;
        if(UseEffectStripColormap())
        {
            const float size_m = GetNormalizedSize();
            const float ph01 = std::fmod(time * GetScaledFrequency() * 12.0f * bb.speed_mul * (1.f / 360.f) +
                                             phase01 + best_plasma * 0.08f + 1.f,
                                         1.f);
            palette_driver = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                         GetEffectStripColormapUnfold(),
                                                         GetEffectStripColormapDirectionDeg(),
                                                         ph01,
                                                         time,
                                                         grid,
                                                         size_m,
                                                         origin,
                                                         rp);
            c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(),
                                             std::clamp(palette_driver, 0.0f, 1.0f),
                                             time);
        }
        else if(GetRainbowMode())
        {
            float hue = std::fmod(best_plasma * 360.0f + time * GetScaledFrequency() * 12.0f * bb.speed_mul
                                      + phase01 * 360.0f,
                                  360.0f);
            if(hue < 0.0f)
                hue += 360.0f;
            c = GetRainbowColor(hue);
        }
        else
        {
            c = GetColorAtPosition(palette_driver);
        }
    }

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
    j["thickness"] = thickness;
    return j;
}

void SurfaceAmbient::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);

    if(settings.contains("style") && settings["style"].is_number_integer())
        style = std::clamp(settings["style"].get<int>(), 0, STYLE_COUNT - 1);
    if(settings.contains("motion") && settings["motion"].is_number_integer())
        motion = std::clamp(settings["motion"].get<int>(), 0, MOTION_COUNT - 1);
    if(settings.contains("thickness") && settings["thickness"].is_number())
        thickness = std::max(0.02f, std::min(0.5f, settings["thickness"].get<float>()));

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "SurfaceAmbientEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "styleRow", style);
            EffectUiSync::setComboIndex(fx, "motionRow", motion);
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "thicknessRow", (int)(thickness * 100.0f), pct);
        }
    }
    UpdateMotionUiEnabled();
}

REGISTER_EFFECT_3D(SurfaceAmbient);
