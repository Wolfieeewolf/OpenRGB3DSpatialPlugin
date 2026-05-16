// SPDX-License-Identifier: GPL-2.0-only

#include "ColorWheel.h"
#include "EffectStratumBlend.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(ColorWheel);

ColorWheel::ColorWheel(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
}

EffectInfo3D ColorWheel::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Color Wheel";
    info.effect_description = "Rotating rainbow from center; optional independent floor / mid / ceiling wheels";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.show_plane_control = true;
    info.supports_strip_colormap = true;
    info.supports_height_bands = true;

    return info;
}

void ColorWheel::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    outer->addLayout(layout);

    int row = 0;
    layout->addWidget(new QLabel("Direction:"), row, 0);
    QComboBox* dir_combo = new QComboBox();
    dir_combo->addItem("Clockwise");
    dir_combo->addItem("Counter-clockwise");
    dir_combo->setCurrentIndex(direction);
    dir_combo->setToolTip("Hue progression around the effect origin in the active plane (see Plane in common controls).");
    dir_combo->setItemData(0, "Increasing angle follows clock motion when viewed from the plane normal.", Qt::ToolTipRole);
    dir_combo->setItemData(1, "Reverses hue sweep—useful to match other rotating effects.", Qt::ToolTipRole);
    layout->addWidget(dir_combo, row, 1, 1, 2);
    connect(dir_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        direction = std::max(0, std::min(1, idx));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel(tr("Hue geometry:")), row, 0);
    QComboBox* geom_combo = new QComboBox();
    geom_combo->addItem(tr("Radial (classic)"), 0);
    geom_combo->addItem(tr("Shear (no focal point)"), 1);
    geom_combo->setToolTip(
        tr("Radial: hue wraps around the effect origin (see Effect origin in the main tab).\n"
           "Shear: moving rainbow bands from a rotating planar gradient—no single center, but still \"wheels\" in time."));
    for(int i = 0; i < geom_combo->count(); i++)
    {
        if(geom_combo->itemData(i).toInt() == hue_geometry_mode)
        {
            geom_combo->setCurrentIndex(i);
            break;
        }
    }
    layout->addWidget(geom_combo, row, 1, 1, 2);
    connect(geom_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, geom_combo](int) {
        hue_geometry_mode = std::clamp(geom_combo->currentData().toInt(), 0, 1);
        emit ParametersChanged();
    });

    AddWidgetToParent(w, parent);
}

void ColorWheel::UpdateParams(SpatialEffectParams& params) { (void)params; }

RGBColor ColorWheel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress = CalculateProgress(time);
    float detail = std::max(0.05f, GetScaledDetail());
    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float lx = rot.x - origin.x, ly = rot.y - origin.y, lz = rot.z - origin.z;

    const float y_norm = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.09f + 0.08f * (1.0f - detail), 0.05f, 0.20f);
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);

    float stratum_w[3]{};
    EffectStratumBlend::WeightsForYNorm(y_norm, map, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);
    float spd_mul = bb.speed_mul;
    float sz_mul = bb.tight_mul;
    float ph_blend = bb.phase_deg;

    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    e.hw /= sz_mul;
    e.hh /= sz_mul;
    e.hd /= sz_mul;

    int pl = GetPlane();
    const float dir = (direction == 0) ? 1.0f : -1.0f;
    float angle = 0.0f;
    if(hue_geometry_mode == 1)
    {
        float u = 0.0f;
        float v = 0.0f;
        if(pl == 0)
        {
            u = lx / e.hw;
            v = lz / e.hd;
        }
        else if(pl == 1)
        {
            u = lx / e.hw;
            v = ly / e.hh;
        }
        else
        {
            u = lz / e.hd;
            v = ly / e.hh;
        }
        const float spin = progress * 6.2831855f * dir + time * GetScaledFrequency() * 0.12f * spd_mul;
        const float cu = std::cos(spin);
        const float su = std::sin(spin);
        angle = (u * cu + v * su) * 3.14159265f * (0.5f + 0.5f * detail);
    }
    else
    {
        if(pl == 0) angle = atan2f(lz / e.hd, lx / e.hw);
        else if(pl == 1) angle = atan2f(lx / e.hw, ly / e.hh);
        else angle = atan2f(lz / e.hd, ly / e.hh);
    }

    angle += stratum_mot01 * 6.2831853f * 0.55f;
    angle += EffectStratumBlend::PhaseShiftRad(bb);

    float hue_plane = fmodf(angle * (180.0f / 3.14159265f) * (0.5f + 0.5f * detail) + progress * 360.0f * dir +
                            time * GetScaledFrequency() * 12.0f * spd_mul + ph_blend,
                        360.0f);

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);

    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = y_norm;

    if(hue_plane < 0.0f) hue_plane += 360.0f;
    const float plane01 = hue_plane / 360.0f;
    float mapped_hue = ApplySpatialRainbowHue(hue_plane, plane01, basis, sp, map, time, &grid);
    float palette01 = std::fmod(mapped_hue / 360.0f, 1.0f);
    if(palette01 < 0.0f)
    {
        palette01 += 1.0f;
    }
    if(UseEffectStripColormap())
    {
        const float size_m = GetNormalizedSize();
        const float ph01 = std::fmod(plane01 + progress * 0.17f + time * GetScaledFrequency() * 0.05f + 1.f, 1.f);
        palette01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                               GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rot);
    }
    palette01 = ApplyVoxelDriveToPalette01(palette01, x, y, z, time, grid);

    if(UseEffectStripColormap())
    {
        return ResolveStripKernelFinalColor(*this,
                                            GetEffectStripColormapKernel(),
                                            std::clamp(palette01, 0.0f, 1.0f),
                                            GetEffectStripColormapColorStyle(),
                                            time,
                                            GetScaledFrequency() * 12.0f * spd_mul);
    }
    return GetRainbowMode() ? GetRainbowColor(palette01 * 360.0f) : GetColorAtPosition(palette01);
}

nlohmann::json ColorWheel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["direction"] = direction;
    j["hue_geometry_mode"] = hue_geometry_mode;
    return j;
}

void ColorWheel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("direction") && settings["direction"].is_number_integer())
        direction = std::max(0, std::min(1, settings["direction"].get<int>()));
    if(settings.contains("hue_geometry_mode") && settings["hue_geometry_mode"].is_number_integer())
        hue_geometry_mode = std::clamp(settings["hue_geometry_mode"].get<int>(), 0, 1);
}
