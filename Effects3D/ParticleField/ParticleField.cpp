// SPDX-License-Identifier: GPL-2.0-only

#include "ParticleField.h"
#include "ParticleFieldVolumeFieldGlsl.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

REGISTER_EFFECT_3D(ParticleField);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const char* ParticleField::ModeName(int m)
{
    switch(m)
    {
    case MODE_FLOAT: return "Float / Fuzzy";
    case MODE_SNOW: return "Snow";
    case MODE_EMBERS: return "Embers";
    case MODE_SPARKLE: return "Sparkle";
    case MODE_ATTRACT: return "Attract";
    case MODE_RAIN: return "Rain";
    case MODE_FIREWORKS: return "Fireworks";
    default: return "Float / Fuzzy";
    }
}

void ParticleField::ApplyModeDefaults(int mode_id)
{
    switch(std::clamp(mode_id, 0, MODE_COUNT - 1))
    {
    case MODE_SNOW:
        particle_count = 40;
        particle_size = 0.48f;
        thickness = 0.78f;
        motion_amount = 0.95f;
        noise_amount = 0.40f;
        fill_amount = 1.25f;
        break;
    case MODE_EMBERS:
        particle_count = 32;
        particle_size = 0.58f;
        thickness = 0.88f;
        motion_amount = 1.15f;
        noise_amount = 0.35f;
        fill_amount = 1.05f;
        break;
    case MODE_SPARKLE:
        particle_count = 30;
        particle_size = 0.38f;
        thickness = 0.68f;
        motion_amount = 1.25f;
        noise_amount = 0.20f;
        fill_amount = 1.30f;
        break;
    case MODE_ATTRACT:
        particle_count = 36;
        particle_size = 0.82f;
        thickness = 1.05f;
        motion_amount = 1.00f;
        noise_amount = 0.60f;
        fill_amount = 1.00f;
        break;
    case MODE_RAIN:
        particle_count = 44;
        particle_size = 0.40f;
        thickness = 0.72f;
        motion_amount = 1.35f;
        noise_amount = 0.25f;
        fill_amount = 1.35f;
        break;
    case MODE_FIREWORKS:
        particle_count = 36;
        particle_size = 0.58f;
        thickness = 0.88f;
        motion_amount = 1.20f;
        noise_amount = 0.30f;
        fill_amount = 1.15f;
        break;
    case MODE_FLOAT:
    default:
        particle_count = 36;
        particle_size = 0.72f;
        thickness = 0.95f;
        motion_amount = 1.00f;
        noise_amount = 0.55f;
        fill_amount = 1.15f;
        break;
    }
}

void ParticleField::SyncCustomUiFromModel()
{
    QWidget* panel = CustomSettingsPanelWidget();
    if(!panel)
        return;
    QWidget* fx = EffectUiSync::effectPanel(panel, "ParticleFieldEffectSettings");
    if(!fx)
        return;
    const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
    EffectUiSync::setComboIndex(fx, "modeRow", mode);
    EffectUiSync::setSliderValue(fx, "countRow", particle_count, [](int v) { return QString::number(v); });
    EffectUiSync::setSliderValue(fx, "sizeRow", (int)std::lround(particle_size * 100.0f), pct);
    EffectUiSync::setSliderValue(fx, "thickRow", (int)std::lround(thickness * 100.0f), pct);
    EffectUiSync::setSliderValue(fx, "motionRow", (int)std::lround(motion_amount * 100.0f),
                                  [this](int) { return QString::number(motion_amount, 'f', 2); });
    EffectUiSync::setSliderValue(fx, "noiseRow", (int)std::lround(noise_amount * 100.0f), pct);
    EffectUiSync::setSliderValue(fx, "fillRow", (int)std::lround(fill_amount * 100.0f), pct);
}

ParticleField::ParticleField(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(45);
    SetFrequency(40);
    volume_assist_.setFragmentBody(QString::fromUtf8(ParticleFieldVolumeFieldGlsl()));
    volume_assist_.setResolution(22);
}

EffectInfo3D ParticleField::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Particle Field";
    info.effect_description =
        "Room-fill organic particles (float, snow, embers, sparkle, attract, rain, fireworks). "
        "GPU volume field — Spatial Anchor is the hub for Attract mode.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_PARTICLE_FIELD;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.default_speed_scale = 16.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 14.0f;
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

void ParticleField::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("ParticleFieldEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);

    EffectLabeledComboRow* mode_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Mode:"));
    mode_row->setObjectName(QStringLiteral("modeRow"));
    QComboBox* mode_combo = mode_row->combo();
    for(int m = 0; m < MODE_COUNT; m++)
        mode_combo->addItem(ModeName(m));
    mode_combo->setCurrentIndex(std::clamp(mode, 0, MODE_COUNT - 1));
    mode_combo->setToolTip(QStringLiteral(
        "Float = fuzzy soup; Snow falls; Embers rise; Sparkle flashes; Attract pulls toward the "
        "Spatial Anchor; Rain streaks; Fireworks burst.\n"
        "Changing mode applies Softness / Size / Count presets for that look (you can still tweak after)."));
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        mode = std::clamp(idx, 0, MODE_COUNT - 1);
        ApplyModeDefaults(mode);
        SyncCustomUiFromModel();
        emit ParametersChanged();
    });

    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectSliderRow* count_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Particles:"), 4, kMaxGpuParticles, particle_count,
        QStringLiteral("Particle count (GPU-capped). Higher is denser but heavier."));
    count_row->setObjectName(QStringLiteral("countRow"));
    count_row->bindValueChanged(this, [this](int v) { particle_count = std::clamp(v, 4, kMaxGpuParticles); },
                                [](int v) { return QString::number(v); }, on_changed);

    EffectSliderRow* size_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Particle size:"), 15, 140, (int)std::lround(particle_size * 100.0f),
        QStringLiteral("Soft kernel radius of each particle."));
    size_row->setObjectName(QStringLiteral("sizeRow"));
    size_row->bindValueChanged(this, [this](int v) { particle_size = v / 100.0f; }, pct, on_changed);

    EffectSliderRow* thick_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Softness:"), 20, 140, (int)std::lround(thickness * 100.0f),
        QStringLiteral("Falloff softness — higher = fluffier blobs (fills empty rooms better)."));
    thick_row->setObjectName(QStringLiteral("thickRow"));
    thick_row->bindValueChanged(this, [this](int v) { thickness = v / 100.0f; }, pct, on_changed);

    EffectSliderRow* motion_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Motion:"), 10, 250, (int)std::lround(motion_amount * 100.0f),
        QStringLiteral("How strongly particles move (fall / rise / drift / orbit)."));
    motion_row->setObjectName(QStringLiteral("motionRow"));
    motion_row->bindValueChanged(this, [this](int v) { motion_amount = v / 100.0f; },
                                 [this](int) { return QString::number(motion_amount, 'f', 2); }, on_changed);

    EffectSliderRow* noise_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Noise:"), 0, 150, (int)std::lround(noise_amount * 100.0f),
        QStringLiteral("Noise advection / sway / orbit wobble."));
    noise_row->setObjectName(QStringLiteral("noiseRow"));
    noise_row->bindValueChanged(this, [this](int v) { noise_amount = v / 100.0f; }, pct, on_changed);

    EffectSliderRow* fill_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Fill:"), 35, 160, (int)std::lround(fill_amount * 100.0f),
        QStringLiteral("How widely particles spread across the room."));
    fill_row->setObjectName(QStringLiteral("fillRow"));
    fill_row->bindValueChanged(this, [this](int v) { fill_amount = v / 100.0f; }, pct, on_changed);

    AddWidgetToParent(w, parent);
}

void ParticleField::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    const float size_m = GetNormalizedSize();
    const float speed_scale = 0.35f + GetScaledSpeed() * 0.08f;

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const float size01 =
        std::clamp(std::max(0.15f, particle_size) * size_m * 0.26f, 0.04f, 0.34f);
    const float thick01 =
        std::clamp(std::max(0.20f, thickness) * 0.125f, 0.022f, 0.20f);
    const float hue_scroll =
        std::fmod(time_sec * GetScaledFrequency() * 0.020f * bb.speed_mul + 1000.0f, 1.0f);

    const float vp[10] = {
        time_sec,
        (float)std::clamp(mode, 0, MODE_COUNT - 1),
        (float)std::clamp(particle_count, 4, kMaxGpuParticles),
        size01,
        thick01,
        std::clamp(motion_amount, 0.10f, 2.5f) * bb.speed_mul,
        std::clamp(noise_amount, 0.0f, 1.5f),
        std::clamp(fill_amount, 0.35f, 1.6f),
        hue_scroll,
        speed_scale * bb.speed_mul
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 10);
}

RGBColor ParticleField::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    if(!volume_assist_.isAvailable())
        return 0x00000000;

    Vector3D rp{x, y, z};
    float coord2 = SampleStratumYNorm01(rp.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 = ComputeStratumMotion01(sw, grid, x, y, z, origin, time);
    const bool strat_on = (GetStratumLayoutMode() == 1);

    float color_cycle = time * GetScaledFrequency() * 8.0f;
    if(strat_on)
    {
        color_cycle = color_cycle * bb.speed_mul + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f;
    }

    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    SampleGpuVolumeOriginLocal01(rp.x, rp.y, rp.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);
    const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
    float max_intensity = samp.x();
    float best_hue = std::fmod(samp.y() * 360.0f + color_cycle * 0.15f + 720.0f, 360.0f);
    if(strat_on)
        max_intensity = EffectStratumBlend::ApplyMotionToUnit01(max_intensity, stratum_mot01, 0.18f);

    if(max_intensity < 0.005f)
        return 0x00000000;

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
    sp.y_norm = coord2;

    const float size_m = GetNormalizedSize();
    RGBColor final_color;
    if(UseEffectStripColormap())
    {
        const float ph01 = std::fmod(color_cycle * (1.f / 360.f) + best_hue * (1.f / 360.f) + 1.f, 1.f);
        float pal01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                 GetEffectStripColormapUnfold(),
                                                 GetEffectStripColormapDirectionDeg(),
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rp);
        pal01 = ApplySpatialPalette01(pal01, basis, sp, map, time, &grid);
        final_color = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), std::clamp(pal01, 0.0f, 1.0f), time);
    }
    else if(GetRainbowMode())
    {
        float hue = ApplySpatialRainbowHue(best_hue,
                                           std::fmod(best_hue * (1.0f / 360.0f) + 1.0f, 1.0f),
                                           basis,
                                           sp,
                                           map,
                                           time,
                                           &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
            p01 += 1.0f;
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(0.5f, basis, sp, map, time, &grid);
        final_color = GetColorAtPosition(p);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json ParticleField::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["pf_mode"] = mode;
    j["pf_count"] = particle_count;
    j["pf_size"] = particle_size;
    j["pf_thickness"] = thickness;
    j["pf_motion"] = motion_amount;
    j["pf_noise"] = noise_amount;
    j["pf_fill"] = fill_amount;
    return j;
}

void ParticleField::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("pf_mode") && settings["pf_mode"].is_number_integer())
        mode = std::clamp(settings["pf_mode"].get<int>(), 0, MODE_COUNT - 1);
    if(settings.contains("pf_count") && settings["pf_count"].is_number_integer())
        particle_count = std::clamp(settings["pf_count"].get<int>(), 4, kMaxGpuParticles);
    if(settings.contains("pf_size") && settings["pf_size"].is_number())
        particle_size = std::clamp(settings["pf_size"].get<float>(), 0.15f, 1.4f);
    if(settings.contains("pf_thickness") && settings["pf_thickness"].is_number())
        thickness = std::clamp(settings["pf_thickness"].get<float>(), 0.20f, 1.4f);
    if(settings.contains("pf_motion") && settings["pf_motion"].is_number())
        motion_amount = std::clamp(settings["pf_motion"].get<float>(), 0.10f, 2.5f);
    if(settings.contains("pf_noise") && settings["pf_noise"].is_number())
        noise_amount = std::clamp(settings["pf_noise"].get<float>(), 0.0f, 1.5f);
    if(settings.contains("pf_fill") && settings["pf_fill"].is_number())
        fill_amount = std::clamp(settings["pf_fill"].get<float>(), 0.35f, 1.6f);

    SyncCustomUiFromModel();
}
