// SPDX-License-Identifier: GPL-2.0-only

#include "Plasma.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"

REGISTER_EFFECT_3D(Plasma);
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
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
}

Plasma::~Plasma() = default;

EffectInfo3D Plasma::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 3;
    info.effect_name = "Plasma";
    info.effect_description = "Plasma field with optional floor/mid/ceiling band tuning; respects room mapper";
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

    return info;
}

void Plasma::SetupCustomUI(QWidget* parent)
{
    QWidget* plasma_widget = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(plasma_widget);
    vbox->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    vbox->addLayout(layout);

    layout->addWidget(new QLabel("Pattern:"), 0, 0);
    pattern_combo = new QComboBox();
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
    layout->addWidget(pattern_combo, 0, 1);
    pattern_type = pattern_combo->currentIndex();

    strip_cmap_panel = new StripKernelColormapPanel(plasma_widget);
    strip_cmap_panel->mirrorStateFromEffect(plasma_strip_cmap_on,
                                            plasma_strip_cmap_kernel,
                                            plasma_strip_cmap_rep,
                                            plasma_strip_cmap_unfold,
                                            plasma_strip_cmap_dir,
                                            plasma_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &Plasma::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(plasma_widget);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &Plasma::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(plasma_widget, parent);

    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Plasma::OnPlasmaParameterChanged);
}

void Plasma::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_PLASMA;
}

void Plasma::OnPlasmaParameterChanged()
{
    if(pattern_combo)
        pattern_type = std::clamp(pattern_combo->currentIndex(), 0, kPlasmaPatternCount - 1);
    emit ParametersChanged();
}

void Plasma::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void Plasma::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    plasma_strip_cmap_on = strip_cmap_panel->useStripColormap();
    plasma_strip_cmap_kernel = strip_cmap_panel->kernelId();
    plasma_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    plasma_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    plasma_strip_cmap_dir = strip_cmap_panel->directionDeg();
    plasma_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
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
    float freq_scale = detail * 0.8f / fmax(0.1f, size_multiplier);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    float coord1 = NormalizeGridAxis01(rotated_pos.x, grid.min_x, grid.max_x);
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    float coord3 = NormalizeGridAxis01(rotated_pos.z, grid.min_z, grid.max_z);

    SpatialLayerCore::MapperSettings strat_map;
    EffectStratumBlend::InitStratumBreaks(strat_map);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_map, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, stratum_w, stratum_tuning_);
    const float prog = progress * bb.speed_mul;
    const float freq_scale_e = freq_scale * bb.tight_mul;
    const float pshift = bb.phase_deg * (1.0f / 360.0f);
    coord1 = std::fmod(coord1 + pshift + 1.0f, 1.0f);
    coord2 = std::fmod(coord2 + pshift + 1.0f, 1.0f);
    coord3 = std::fmod(coord3 + pshift + 1.0f, 1.0f);

    float plasma_value;
    switch(pattern_type)
    {
        case 0:
            plasma_value =
                sin((coord1 + prog * 2.0f) * freq_scale_e * 10.0f) +
                sin((coord2 + prog * 1.7f) * freq_scale_e * 8.0f) +
                sin((coord1 + coord2 + prog * 1.3f) * freq_scale_e * 6.0f) +
                cos((coord1 - coord2 + prog * 2.2f) * freq_scale_e * 7.0f) +
                sin(sqrtf(coord1*coord1 + coord2*coord2) * freq_scale_e * 5.0f + prog * 1.5f) +
                cos(coord3 * freq_scale_e * 4.0f + prog * 0.9f);
            break;
        case 1:
            {
                float angle = atan2(coord2 - 0.5f, coord1 - 0.5f);
                float radius = sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f));
                plasma_value =
                    sin(angle * 4.0f + radius * freq_scale_e * 8.0f + prog * 2.0f) +
                    sin(angle * 3.0f - radius * freq_scale_e * 6.0f + prog * 1.5f) +
                    cos(angle * 5.0f + radius * freq_scale_e * 4.0f - prog * 1.8f) +
                    sin(coord3 * freq_scale_e * 5.0f + prog) +
                    cos((angle * 2.0f + coord3 * freq_scale_e * 3.0f) + prog * 1.2f);
            }
            break;
        case 2:
            {
                float dist_from_center = sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f));
                plasma_value =
                    sin(dist_from_center * freq_scale_e * 10.0f - prog * 3.0f) +
                    sin(dist_from_center * freq_scale_e * 15.0f - prog * 2.3f) +
                    cos(dist_from_center * freq_scale_e * 8.0f + prog * 1.8f) +
                    sin((coord1 + coord2) * freq_scale_e * 6.0f + prog * 1.2f) +
                    cos(coord3 * freq_scale_e * 5.0f - prog * 0.7f);
            }
            break;
        case 3:
            {
                float flow1 = sin(coord1 * freq_scale_e * 8.0f + sin(coord2 * freq_scale_e * 12.0f + prog) + prog * 0.5f);
                float flow2 = cos(coord2 * freq_scale_e * 9.0f + cos(coord3 * freq_scale_e * 11.0f + prog * 1.3f));
                float flow3 = sin(coord3 * freq_scale_e * 7.0f + sin(coord1 * freq_scale_e * 13.0f + prog * 0.7f));
                float flow4 = cos((coord1 + coord2) * freq_scale_e * 6.0f + sin(prog * 1.5f));
                float flow5 = sin((coord2 + coord3) * freq_scale_e * 5.0f + cos(prog * 1.8f));
                plasma_value = flow1 + flow2 + flow3 + flow4 + flow5;
            }
            break;
        case 4:
            {
                float n1 = sin((coord1 + prog * 0.5f) * freq_scale_e * 40.0f) * sin((coord2 + prog * 0.3f) * freq_scale_e * 52.0f) * sin((coord3 + prog * 0.7f) * freq_scale_e * 31.0f);
                float n2 = sin((coord1 * 2.3f + coord2 + prog) * freq_scale_e * 20.0f) * cos((coord2 * 1.7f + coord3 + prog * 1.2f) * freq_scale_e * 25.0f);
                float n3 = cos((coord1 + coord2 * 2.1f + coord3) * freq_scale_e * 15.0f + prog * 2.0f);
                plasma_value = n1 * 0.5f + n2 * 0.35f + n3 * 0.15f;
            }
            break;
        case 5:
            {
                float r = sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f) + (coord3 - 0.5f)*(coord3 - 0.5f));
                plasma_value =
                    sin(r * freq_scale_e * 30.0f - prog * 2.0f) +
                    sin((coord1 + coord2) * freq_scale_e * 20.0f + prog * 1.5f) * 0.6f +
                    cos((coord2 + coord3) * freq_scale_e * 18.0f - prog * 1.2f) * 0.5f +
                    sin(coord3 * freq_scale_e * 25.0f + prog * 0.8f) * 0.4f;
            }
            break;
        default:
            plasma_value = 0.5f;
            break;
    }

    plasma_value = (plasma_value + 6.0f) / 12.0f;
    plasma_value = fmax(0.0f, fmin(1.0f, plasma_value));

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
    if(plasma_strip_cmap_on)
    {
        strip_p01 = SampleStripKernelPalette01(plasma_strip_cmap_kernel,
                                                 plasma_strip_cmap_rep,
                                                 plasma_strip_cmap_unfold,
                                                 plasma_strip_cmap_dir,
                                                 phase01,
                                                 time,
                                                 grid,
                                                 size_multiplier,
                                                 origin,
                                                 rotated_pos);
    }

    if(plasma_strip_cmap_on)
    {
        float p01v = ApplyVoxelDriveToPalette01(strip_p01, x, y, z, time, grid);
        final_color = ResolveStripKernelFinalColor(*this, plasma_strip_cmap_kernel, p01v, plasma_strip_cmap_color_style, time,
                                                   rate * 12.0f);
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
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(plasma_value, basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
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
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "plasma_stratum_layout_mode",
                                           sm,
                                           st,
                                           "plasma_stratum_band_speed_pct",
                                           "plasma_stratum_band_tight_pct",
                                           "plasma_stratum_band_phase_deg");
    j["plasma_strip_cmap_on"] = plasma_strip_cmap_on;
    j["plasma_strip_cmap_kernel"] = plasma_strip_cmap_kernel;
    j["plasma_strip_cmap_rep"] = plasma_strip_cmap_rep;
    j["plasma_strip_cmap_unfold"] = plasma_strip_cmap_unfold;
    j["plasma_strip_cmap_dir"] = plasma_strip_cmap_dir;
    j["plasma_strip_cmap_color_style"] = plasma_strip_cmap_color_style;
    return j;
}

void Plasma::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("pattern_type") && settings["pattern_type"].is_number_integer())
        pattern_type = std::clamp(settings["pattern_type"].get<int>(), 0, kPlasmaPatternCount - 1);

    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "plasma_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "plasma_stratum_band_speed_pct",
                                            "plasma_stratum_band_tight_pct",
                                            "plasma_stratum_band_phase_deg");
    if(settings.contains("plasma_strip_cmap_on") && settings["plasma_strip_cmap_on"].is_boolean())
        plasma_strip_cmap_on = settings["plasma_strip_cmap_on"].get<bool>();
    else if(settings.contains("plasma_strip_cmap_on") && settings["plasma_strip_cmap_on"].is_number_integer())
        plasma_strip_cmap_on = settings["plasma_strip_cmap_on"].get<int>() != 0;
    if(settings.contains("plasma_strip_cmap_kernel") && settings["plasma_strip_cmap_kernel"].is_number_integer())
        plasma_strip_cmap_kernel = std::clamp(settings["plasma_strip_cmap_kernel"].get<int>(), 0, StripShellKernelCount() - 1);
    if(settings.contains("plasma_strip_cmap_rep") && settings["plasma_strip_cmap_rep"].is_number())
        plasma_strip_cmap_rep = std::max(1.0f, std::min(40.0f, settings["plasma_strip_cmap_rep"].get<float>()));
    if(settings.contains("plasma_strip_cmap_unfold") && settings["plasma_strip_cmap_unfold"].is_number_integer())
        plasma_strip_cmap_unfold = std::clamp(settings["plasma_strip_cmap_unfold"].get<int>(), 0, 6);
    if(settings.contains("plasma_strip_cmap_dir") && settings["plasma_strip_cmap_dir"].is_number())
        plasma_strip_cmap_dir = std::fmod(settings["plasma_strip_cmap_dir"].get<float>() + 360.0f, 360.0f);
    if(settings.contains("plasma_strip_cmap_color_style") && settings["plasma_strip_cmap_color_style"].is_number_integer())
        plasma_strip_cmap_color_style = std::clamp(settings["plasma_strip_cmap_color_style"].get<int>(), 0, 2);
    else
        plasma_strip_cmap_color_style = GetRainbowMode() ? 2 : 1;

    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(plasma_strip_cmap_on,
                                                plasma_strip_cmap_kernel,
                                                plasma_strip_cmap_rep,
                                                plasma_strip_cmap_unfold,
                                                plasma_strip_cmap_dir,
                                                plasma_strip_cmap_color_style);
    }
    if(pattern_combo)
        pattern_combo->setCurrentIndex(pattern_type);
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
