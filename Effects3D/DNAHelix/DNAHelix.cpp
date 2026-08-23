// SPDX-License-Identifier: GPL-2.0-only

#include "DNAHelix.h"
#include "DNAHelixVolumeFieldGlsl.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"

#include <QComboBox>
#include "EffectUiRows.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_EFFECT_3D(DNAHelix);

const char* DNAHelix::ShapeName(int s)
{
    switch(s)
    {
    case SHAPE_HELIX: return "Classic helix";
    case SHAPE_ROPE: return "Thick rope";
    case SHAPE_RIBBONS: return "Twisted ribbons";
    case SHAPE_LADDER: return "Ladder (strong rungs)";
    default: return "Classic helix";
    }
}

DNAHelix::DNAHelix(QWidget* parent) : SpatialEffect3D(parent)
{
    std::vector<RGBColor> dna_colors = {
        0x000000FF,
        0x0000FFFF,
        0x0000FF00,
        0x00FF0000
    };
    if(GetColors().empty())
        SetColors(dna_colors);
    SetFrequency(40);
    SetSpeed(35);
    SetRainbowMode(false);
    volume_assist_.setFragmentBody(QString::fromUtf8(DNAHelixVolumeFieldGlsl()));
    volume_assist_.setResolution(20);
}

DNAHelix::~DNAHelix() = default;

EffectInfo3D DNAHelix::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "DNA Helix";
    info.effect_description =
        "Double helix along height through the effect origin: classic tubes, thick rope, twisted ribbons, or ladder with rungs. "
        "Speed spins the twist; Frequency/Detail change how many turns; Size scales radius. GPU assist when available.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_DNA_HELIX;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    info.default_speed_scale = 14.0f;
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

void DNAHelix::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* shape_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Look:"));
    shape_row->setObjectName(QStringLiteral("shapeRow"));
    shape_combo = shape_row->combo();
    for(int s = 0; s < SHAPE_COUNT; s++)
        shape_combo->addItem(QString::fromUtf8(ShapeName(s)));
    shape_combo->setCurrentIndex(std::clamp(helix_shape_mode, 0, SHAPE_COUNT - 1));
    shape_combo->setToolTip(QStringLiteral(
        "Classic: thin double helix. Rope: fatter strands. Ribbons: angular blades (more LEDs catch). "
        "Ladder: stronger base-pair rungs."));
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        helix_shape_mode = std::clamp(idx, 0, SHAPE_COUNT - 1);
        emit ParametersChanged();
    });

    EffectSliderRow* radius_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Helix radius:"),
        15,
        90,
        (int)std::lround(helix_radius_pct),
        QStringLiteral("How far the strands sit from the vertical axis (also scaled by Size)."));
    radius_row->setObjectName(QStringLiteral("helixRadiusRow"));
    radius_slider = radius_row->slider();
    radius_row->bindValueChanged(
        this, [this](int v) { helix_radius_pct = (float)v; }, pct_format, on_changed);

    EffectSliderRow* twist_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Twists:"),
        50,
        600,
        (int)std::lround(twist_amount * 100.0f),
        QStringLiteral("How many turns from floor to ceiling (also boosted by Frequency/Detail)."));
    twist_row->setObjectName(QStringLiteral("twistsRow"));
    twist_slider = twist_row->slider();
    twist_row->bindValueChanged(
        this,
        [this](int v) { twist_amount = std::clamp(v / 100.0f, 0.5f, 6.0f); },
        [this](int) { return QString::number(twist_amount, 'f', 2); },
        on_changed);

    EffectSliderRow* thick_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Strand thickness:"),
        8,
        70,
        (int)std::lround(strand_thickness_pct),
        QStringLiteral("Tube width — raise this if the helix disappears on sparse LED layouts."));
    thick_row->setObjectName(QStringLiteral("thicknessRow"));
    thickness_slider = thick_row->slider();
    thick_row->bindValueChanged(
        this, [this](int v) { strand_thickness_pct = (float)v; }, pct_format, on_changed);

    EffectSliderRow* rung_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Rungs:"),
        0,
        100,
        (int)std::lround(rung_amount_pct),
        QStringLiteral("Base-pair bridges between the two strands."));
    rung_row->setObjectName(QStringLiteral("rungsRow"));
    rung_slider = rung_row->slider();
    rung_row->bindValueChanged(
        this, [this](int v) { rung_amount_pct = (float)v; }, pct_format, on_changed);

    AddWidgetToParent(w, parent);
}

void DNAHelix::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const float spd = std::max(0.05f, GetScaledSpeed());
    const float freq = std::max(0.05f, GetScaledFrequency());
    const float detail = std::max(0.05f, GetNormalizedDetail());
    const float size_m = std::max(0.25f, GetNormalizedSize());
    const float progress = std::fmod(time_sec * spd * 0.11f * bb.speed_mul + 1.0f, 1.0f);
    const int shape = std::clamp(helix_shape_mode, 0, SHAPE_COUNT - 1);
    const float radius01 =
        std::clamp((helix_radius_pct / 100.0f) * (0.55f + 0.55f * size_m) * GetNormalizedScale(), 0.08f, 0.85f);
    const float twists =
        std::clamp(twist_amount * (0.55f + 0.75f * detail) * (0.45f + 0.08f * freq), 0.4f, 8.0f);
    const float thickness =
        std::clamp((strand_thickness_pct / 100.0f) * (0.55f + 0.35f * size_m), 0.04f, 0.45f);
    const float rung_amount = std::clamp(rung_amount_pct / 100.0f, 0.0f, 1.0f);
    const float vp[6] = {
        progress,
        twists,
        radius01,
        thickness,
        rung_amount,
        (float)shape
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 6);
}

RGBColor DNAHelix::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot{x, y, z};
    const float coord2 = SampleStratumYNorm01(rot.y, grid, origin);
    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    SampleGpuVolumeOriginLocal01(rot.x, rot.y, rot.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);

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

    const float spd = std::max(0.05f, GetScaledSpeed());
    const float freq = std::max(0.05f, GetScaledFrequency());
    const float size_m = std::max(0.25f, GetNormalizedSize());
    const float progress =
        std::fmod(time * spd * 0.11f * bb.speed_mul + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f,
                  1.0f);

    float intensity = 0.0f;
    float palette01 = 0.0f;
    float rung_hint = 0.0f;
    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
        intensity = std::clamp(samp.x(), 0.0f, 1.0f);
        if(GetStratumLayoutMode() == 1)
            intensity = EffectStratumBlend::ApplyMotionToUnit01(intensity, stratum_mot01, 0.28f);
        palette01 = std::clamp(samp.y(), 0.0f, 1.0f);
        rung_hint = samp.z();
    }


    if(intensity < 0.01f)
        return 0x00000000;

    const float rate = freq;
    float strip_p01 = 0.0f;
    if(UseEffectStripColormap())
    {
        strip_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                               GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               std::fmod(progress + 1.0f, 1.0f),
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rot);
        strip_p01 = ApplySpatialPalette01(strip_p01, basis, sp, map, time, &grid);
    }

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = palette01 * 360.0f + time * rate * 10.0f * bb.speed_mul;
        if(rung_hint > 0.5f)
            hue += 140.0f;
        if(UseEffectStripColormap())
            hue = strip_p01 * 360.0f + time * rate * 10.0f * bb.speed_mul;
        else
            hue = ApplySpatialRainbowHue(hue, palette01, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f + 1.0f, 1.0f);
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else if(UseEffectStripColormap())
    {
        final_color = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), std::clamp(strip_p01, 0.0f, 1.0f), time);
    }
    else
    {
        float pos = palette01;
        if(rung_hint > 0.5f && GetColors().size() > 1)
            pos = std::fmod(pos + 0.35f, 1.0f);
        pos = ApplySpatialPalette01(pos, basis, sp, map, time, &grid);
        final_color = GetColorAtPosition(pos);
    }

    unsigned char r = (unsigned char)std::min(255.0f, (final_color & 0xFF) * intensity);
    unsigned char g = (unsigned char)std::min(255.0f, ((final_color >> 8) & 0xFF) * intensity);
    unsigned char b = (unsigned char)std::min(255.0f, ((final_color >> 16) & 0xFF) * intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json DNAHelix::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["dna_helix_shape"] = helix_shape_mode;
    j["dna_helix_radius_pct"] = helix_radius_pct;
    j["dna_helix_twists"] = twist_amount;
    j["dna_helix_thickness_pct"] = strand_thickness_pct;
    j["dna_helix_rung_pct"] = rung_amount_pct;
    return j;
}

void DNAHelix::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("dna_helix_shape") && settings["dna_helix_shape"].is_number_integer())
        helix_shape_mode = std::clamp(settings["dna_helix_shape"].get<int>(), 0, SHAPE_COUNT - 1);
    if(settings.contains("dna_helix_radius_pct") && settings["dna_helix_radius_pct"].is_number())
        helix_radius_pct = std::clamp(settings["dna_helix_radius_pct"].get<float>(), 15.0f, 90.0f);
    if(settings.contains("dna_helix_twists") && settings["dna_helix_twists"].is_number())
        twist_amount = std::clamp(settings["dna_helix_twists"].get<float>(), 0.5f, 6.0f);
    if(settings.contains("dna_helix_thickness_pct") && settings["dna_helix_thickness_pct"].is_number())
        strand_thickness_pct = std::clamp(settings["dna_helix_thickness_pct"].get<float>(), 8.0f, 70.0f);
    if(settings.contains("dna_helix_rung_pct") && settings["dna_helix_rung_pct"].is_number())
        rung_amount_pct = std::clamp(settings["dna_helix_rung_pct"].get<float>(), 0.0f, 100.0f);

    if(shape_combo)
        shape_combo->setCurrentIndex(helix_shape_mode);
    if(radius_slider)
        radius_slider->setValue((int)std::lround(helix_radius_pct));
    if(twist_slider)
        twist_slider->setValue((int)std::lround(twist_amount * 100.0f));
    if(thickness_slider)
        thickness_slider->setValue((int)std::lround(strand_thickness_pct));
    if(rung_slider)
        rung_slider->setValue((int)std::lround(rung_amount_pct));
}
