// SPDX-License-Identifier: GPL-2.0-only

#include "Plasma.h"
#include "PlasmaVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"

REGISTER_EFFECT_3D(Plasma);
#include <QComboBox>
#include "EffectUiRows.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int kPlasmaPatternCount = 6;
}

Plasma::Plasma(QWidget* parent) : SpatialEffect3D(parent)
{
    pattern_combo = nullptr;
    pattern_type = 0;
    progress = 0.0f;

    std::vector<RGBColor> plasma_colors = {
        0x0000FF00,
        0x00FF00FF,
        0x00FFFF00
    };
    if(GetColors().empty())
    {
        SetColors(plasma_colors);
    }
    SetFrequency(60);
    SetRainbowMode(false);
    volume_assist_.setFragmentBody(QString::fromUtf8(PlasmaVolumeFieldGlsl()));
    volume_assist_.setResolution(18);
}

Plasma::~Plasma() = default;
EffectInfo3D Plasma::GetEffectInfo() const
{
    EffectInfo3D info;
    info.effect_name = "Plasma";
    info.effect_description = "Plasma field with optional floor/mid/ceiling band tuning; GPU volume assist when available; respects room mapper";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_PLASMA;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 8.0f;
    info.default_frequency_scale = 8.0f;
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

void Plasma::SetupCustomUI(QWidget* parent)
{
    QWidget* plasma_widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(plasma_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    EffectLabeledComboRow* pattern_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Pattern:"));
    pattern_row->setObjectName(QStringLiteral("patternRow"));
    pattern_combo = pattern_row->combo();
    pattern_combo->addItem("Classic");
    pattern_combo->addItem("Swirl");
    pattern_combo->addItem("Ripple");
    pattern_combo->addItem("Organic");
    pattern_combo->addItem("Noise");
    pattern_combo->addItem("CubeFire");
    pattern_combo->setCurrentIndex(std::clamp(pattern_type, 0, kPlasmaPatternCount - 1));
    pattern_combo->setToolTip(
        "How the plasma field is built from normalized room coordinates. "
        "Detail and Size tune spatial frequency; Target zone bounds in the stack helps strips or partial rooms.");
    pattern_combo->setItemData(0,
        "Layered sines in X/Y plus mild radial and Z terms—balanced default.",
        Qt::ToolTipRole);
    pattern_combo->setItemData(1,
        "Polar swirl in the horizontal plane with Z modulation.",
        Qt::ToolTipRole);
    pattern_combo->setItemData(2,
        "Radial rings from center—reads clearly on floors and wide walls.",
        Qt::ToolTipRole);
    pattern_combo->setItemData(3,
        "Coupled flows with nested sines—softer, cloud-like motion.",
        Qt::ToolTipRole);
    pattern_combo->setItemData(4,
        "High-frequency grain—busy texture; works best with lower Detail.",
        Qt::ToolTipRole);
    pattern_combo->setItemData(5,
        "3D radial shells from room center—strong depth cue in volumetric layouts.",
        Qt::ToolTipRole);
    pattern_type = pattern_combo->currentIndex();
    AddWidgetToParent(plasma_widget, parent);

    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Plasma::OnPlasmaParameterChanged);
}

void Plasma::OnPlasmaParameterChanged()
{
    if(pattern_combo)
        pattern_type = std::clamp(pattern_combo->currentIndex(), 0, kPlasmaPatternCount - 1);
    emit ParametersChanged();
}

void Plasma::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    progress = CalculateProgress(time_sec) * bb.speed_mul;
    const float detail = std::max(0.05f, GetScaledDetail()) * bb.tight_mul;
    const float size_multiplier = GetNormalizedSize();
    const float freq_scale = std::min(8.0f, detail * 0.8f / std::fmax(0.1f, size_multiplier));
    float ox = 0.5f, oy = 0.5f, oz = 0.5f;
    PackEffectOrigin01(grid, GetEffectOriginGrid(grid), &ox, &oy, &oz);
    const float vp[6] = {progress, freq_scale, (float)pattern_type, ox, oy, oz};
    volume_assist_.prepare(render_sequence, time_sec, vp, 6);
}

RGBColor Plasma::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    progress = CalculateProgress(time);

    float size_multiplier = GetNormalizedSize();

    Vector3D rotated_pos{x, y, z};
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    float n1 = NormalizeGridAxis01(rotated_pos.x, grid.min_x, grid.max_x);
    float n2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    float n3 = NormalizeGridAxis01(rotated_pos.z, grid.min_z, grid.max_z);
    float oy = 0.5f;
    {
        float ox = 0.5f, oz = 0.5f;
        PackEffectOrigin01(grid, origin, &ox, &oy, &oz);
    }
    float coord2 = std::clamp(n2 - oy + 0.5f, 0.0f, 1.0f);

    SpatialLayerCore::MapperSettings strat_map;
    EffectStratumBlend::InitStratumBreaks(strat_map);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_map, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);
    const float prog = progress * bb.speed_mul;
    const float pshift = EffectStratumBlend::PhaseShift01(bb);

    float plasma_value = 0.0f;
    if(volume_assist_.isAvailable())
    {
        /* GLSL already centers on origin — sample room 01 + stratum phase only. */
        const float g1 = std::fmod(n1 + pshift + 1.0f, 1.0f);
        const float g2 = std::fmod(n2 + pshift + 1.0f, 1.0f);
        const float g3 = std::fmod(n3 + pshift + 1.0f, 1.0f);
        plasma_value = volume_assist_.sampleScalar01(g1, g2, g3);
    }
    plasma_value = EffectStratumBlend::ApplyMotionToUnit01(plasma_value, stratum_mot01, 0.28f);

    float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float depth_factor = 1.0f;
    if(max_radius > 0.001f)
    {
        float normalized_dist = fmin(1.0f, radial_distance / max_radius);
        depth_factor = 0.45f + 0.55f * (1.0f - normalized_dist * 0.6f);
    }

    RGBColor final_color;
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
    sp.y_norm = coord2;

    const float phase01 = std::fmod(prog + pshift + 1.0f, 1.0f);
    float strip_p01 = 0.0f;
    if(UseEffectStripColormap())
    {
        strip_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                 GetEffectStripColormapRepeats(),
                                                 GetEffectStripColormapUnfold(),
                                                 GetEffectStripColormapDirectionDeg(),
                                                 phase01,
                                                 time,
                                                 grid,
                                                 size_multiplier,
                                                 origin,
                                                 rotated_pos);
    }

    if(UseEffectStripColormap())
    {
        float p01v = strip_p01;
        final_color = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), p01v, time);
    }
    else if(GetRainbowMode())
    {
        float hue = plasma_value * 360.0f + time * rate * 12.0f;
        hue = ApplySpatialRainbowHue(hue, plasma_value, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(plasma_value, basis, sp, map, time, &grid);
        final_color = GetColorAtPosition(p);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * depth_factor);
    g = (unsigned char)(g * depth_factor);
    b = (unsigned char)(b * depth_factor);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Plasma::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["pattern_type"] = pattern_type;
    return j;
}

void Plasma::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("pattern_type") && settings["pattern_type"].is_number_integer())
        pattern_type = std::clamp(settings["pattern_type"].get<int>(), 0, kPlasmaPatternCount - 1);
    if(pattern_combo)
        pattern_combo->setCurrentIndex(pattern_type);
}
