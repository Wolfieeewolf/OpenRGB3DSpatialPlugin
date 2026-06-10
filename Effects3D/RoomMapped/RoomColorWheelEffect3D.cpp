// SPDX-License-Identifier: GPL-2.0-only

#include "RoomColorWheelEffect3D.h"

#include "EffectStratumBlend.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"

#include <QComboBox>
#include <QLabel>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(RoomColorWheelEffect3D);

namespace
{

Vector3D RoomPivot(const GridContext3D& grid)
{
    return {grid.center_x, grid.center_y, grid.center_z};
}

bool InsideRoomPatternBox(float lx, float ly, float lz, const EffectGridAxisHalfExtents& e)
{
    return std::fabs(lx) <= e.hw && std::fabs(ly) <= e.hh && std::fabs(lz) <= e.hd;
}

float RoomEdgeFadeMul(float x, float y, float z, const GridContext3D& grid, float fade_pct)
{
    const float fade = std::clamp(fade_pct / 100.0f, 0.0f, 1.0f);
    if(fade <= 0.001f)
    {
        return 1.0f;
    }
    const float mx = std::max(grid.max_x - grid.min_x, 1e-4f);
    const float my = std::max(grid.max_y - grid.min_y, 1e-4f);
    const float mz = std::max(grid.max_z - grid.min_z, 1e-4f);
    const float tx = std::fabs((x - grid.min_x) / mx - 0.5f) * 2.0f;
    const float ty = std::fabs((y - grid.min_y) / my - 0.5f) * 2.0f;
    const float tz = std::fabs((z - grid.min_z) / mz - 0.5f) * 2.0f;
    const float u = std::clamp(std::max({tx, ty, tz}), 0.0f, 1.0f);
    const float t = std::clamp(u, 0.0f, 1.0f);
    return 1.0f - fade * (t * t * (3.0f - 2.0f * t));
}

RGBColor ScaleRgb(RGBColor color, float mul)
{
    if(mul <= 0.0f)
    {
        return 0x00000000;
    }
    if(mul >= 1.0f)
    {
        return color;
    }
    const int r = std::min(255, std::max(0, (int)((color & 0xFF) * mul)));
    const int g = std::min(255, std::max(0, (int)(((color >> 8) & 0xFF) * mul)));
    const int b = std::min(255, std::max(0, (int)(((color >> 16) & 0xFF) * mul)));
    return (RGBColor)((b << 16) | (g << 8) | r);
}

} // namespace

RoomColorWheelEffect3D::RoomColorWheelEffect3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
}

EffectInfo3D RoomColorWheelEffect3D::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Color wheel (room)";
    info.effect_description =
        "Rainbow tied to room bounds (not effect origin / reference). "
        "Does not use light occlusion — use Room wash or Campfire for shadows. "
        "Try Hue geometry: Room gradient for a clearly room-fixed look.";
    info.category = SpatialRoom::LibraryGroupForMode(SpatialRoom::SpatialRoomMode::RoomMappedPattern);
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
    info.show_position_offset_control = false;
    info.supports_strip_colormap = true;
    info.supports_height_bands = true;

    return info;
}

void RoomColorWheelEffect3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("RoomColorWheelEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);

    auto* mode_hint = new QLabel(tr(
        "Spatial · Mapped — rainbow tied to room bounds.\n"
        "For keyboard → room glow with occlusion: stack Room emissive relay above this "
        "(keyboard only on this layer). See docs/SPATIAL_MODES_GUIDE.md."));
    mode_hint->setWordWrap(true);
    mode_hint->setObjectName(QStringLiteral("mappedModeHint"));
    layout->addWidget(mode_hint);

    EffectLabeledComboRow* direction_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Direction:"));
    direction_row->setObjectName(QStringLiteral("directionRow"));
    QComboBox* dir_combo = direction_row->combo();
    dir_combo->addItem(QStringLiteral("Clockwise"));
    dir_combo->addItem(QStringLiteral("Counter-clockwise"));
    dir_combo->setCurrentIndex(direction);
    dir_combo->setToolTip(QStringLiteral(
        "Hue progression around the room center in the active plane (see Plane in common controls)."));
    connect(dir_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        direction = std::max(0, std::min(1, idx));
        emit ParametersChanged();
    });

    EffectLabeledComboRow* hue_geometry_row = EffectUiRows::AppendComboRow(layout, tr("Hue geometry:"));
    hue_geometry_row->setObjectName(QStringLiteral("hueGeometryRow"));
    QComboBox* geom_combo = hue_geometry_row->combo();
    geom_combo->addItem(tr("Radial (room center)"), 0);
    geom_combo->addItem(tr("Shear (no focal point)"), 1);
    geom_combo->addItem(tr("Room gradient (volume)"), 2);
    geom_combo->setToolTip(tr(
        "Radial: hue around room center (similar to classic Color Wheel + Room center ref).\n"
        "Shear: rotating bands across the room.\n"
        "Room gradient: hue from position in the room box (corner-to-corner sweep — not a wheel)."));
    for(int i = 0; i < geom_combo->count(); ++i)
    {
        if(geom_combo->itemData(i).toInt() == hue_geometry_mode)
        {
            geom_combo->setCurrentIndex(i);
            break;
        }
    }
    connect(geom_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [geom_combo, this](int) {
        hue_geometry_mode = std::clamp(geom_combo->currentData().toInt(), 0, 2);
        emit ParametersChanged();
    });

    const auto pct_format = [](int v) { return QStringLiteral("%1%").arg(v); };
    EffectSliderRow* edge_fade_row = EffectUiRows::AppendSliderRow(layout,
                                                                   tr("Room edge fade:"),
                                                                   0,
                                                                   100,
                                                                   (int)room_edge_fade_pct,
                                                                   tr("Softens toward room walls (full layout bounds)."));
    edge_fade_row->setObjectName(QStringLiteral("roomEdgeFadeRow"));
    edge_fade_row->bindValueChanged(this,
                                    [this](int v) { room_edge_fade_pct = static_cast<float>(v); },
                                    pct_format,
                                    [this]() { emit ParametersChanged(); });

    AddWidgetToParent(w, parent);
}

void RoomColorWheelEffect3D::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

RGBColor RoomColorWheelEffect3D::CalculateColorGrid(float x,
                                                    float y,
                                                    float z,
                                                    float time,
                                                    const GridContext3D& grid)
{
    const Vector3D pivot = RoomPivot(grid);
    const Vector3D rot = TransformPointByRotation(x, y, z, pivot);
    const float lx = rot.x - pivot.x;
    const float ly = rot.y - pivot.y;
    const float lz = rot.z - pivot.z;

    const float progress = CalculateProgress(time);
    const float detail = std::max(0.05f, GetScaledDetail());

    float stratum_w[3]{};
    const float y_norm = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.09f + 0.08f * (1.0f - detail), 0.05f, 0.20f);
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = std::clamp(0.95f + detail * 0.1f, 0.85f, 2.2f);
    EffectStratumBlend::WeightsForYNorm(y_norm, map, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 = ComputeStratumMotion01(stratum_w, grid, x, y, z, pivot, time);
    const float spd_mul = bb.speed_mul;
    const float sz_mul = bb.tight_mul;
    const float ph_blend = bb.phase_deg;

    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    const float size_tight = 1.0f / std::max(0.2f, GetNormalizedSize());
    e.hw /= sz_mul * size_tight;
    e.hh /= sz_mul * size_tight;
    e.hd /= sz_mul * size_tight;
    if(!InsideRoomPatternBox(lx, ly, lz, e))
    {
        return 0x00000000;
    }

    const int pl = GetPlane();
    const float dir = (direction == 0) ? 1.0f : -1.0f;
    float angle = 0.0f;
    if(hue_geometry_mode == 2)
    {
        const float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
        const float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
        const float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);
        float t = 0.0f;
        if(pl == 0)
        {
            t = 0.55f * nx + 0.45f * nz;
        }
        else if(pl == 1)
        {
            t = 0.55f * nx + 0.45f * ny;
        }
        else
        {
            t = 0.55f * nz + 0.45f * ny;
        }
        t = std::fmod(t + progress * 0.25f * dir + stratum_mot01 * 0.12f + 1.0f, 1.0f);
        angle = t * 6.2831853f;
    }
    else if(hue_geometry_mode == 1)
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
        if(pl == 0)
        {
            angle = atan2f(lz / e.hd, lx / e.hw);
        }
        else if(pl == 1)
        {
            angle = atan2f(lx / e.hw, ly / e.hh);
        }
        else
        {
            angle = atan2f(lz / e.hd, ly / e.hh);
        }
    }

    angle += stratum_mot01 * 6.2831853f * 0.55f;
    angle += EffectStratumBlend::PhaseShiftRad(bb);

    float hue_plane = std::fmod(angle * (180.0f / 3.14159265f) * (0.5f + 0.5f * detail) + progress * 360.0f * dir +
                                time * GetScaledFrequency() * 12.0f * spd_mul + ph_blend,
                            360.0f);
    if(hue_plane < 0.0f)
    {
        hue_plane += 360.0f;
    }

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);

    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = pivot.x;
    sp.origin_y = pivot.y;
    sp.origin_z = pivot.z;
    sp.y_norm = y_norm;

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
        const float ph01 = std::fmod(plane01 + progress * 0.17f + time * GetScaledFrequency() * 0.05f + 1.0f, 1.0f);
        palette01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                               GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               pivot,
                                               rot);
    }
    palette01 = ApplyVoxelDriveToPalette01(palette01, x, y, z, time, grid);

    const float edge_mul = RoomEdgeFadeMul(rot.x, rot.y, rot.z, grid, room_edge_fade_pct);
    if(edge_mul <= 1e-4f)
    {
        return 0x00000000;
    }

    RGBColor out;
    if(UseEffectStripColormap())
    {
        out = ResolveStripKernelFinalColor(*this,
                                           GetEffectStripColormapKernel(),
                                           std::clamp(palette01, 0.0f, 1.0f),
                                           GetEffectStripColormapColorStyle(),
                                           time,
                                           GetScaledFrequency() * 12.0f * spd_mul);
    }
    else
    {
        out = GetRainbowMode() ? GetRainbowColor(palette01 * 360.0f) : GetColorAtPosition(palette01);
    }
    return ScaleRgb(out, edge_mul);
}

nlohmann::json RoomColorWheelEffect3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["direction"] = direction;
    j["hue_geometry_mode"] = hue_geometry_mode;
    j["room_edge_fade_pct"] = room_edge_fade_pct;
    return j;
}

void RoomColorWheelEffect3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("direction") && settings["direction"].is_number_integer())
    {
        direction = std::max(0, std::min(1, settings["direction"].get<int>()));
    }
    if(settings.contains("hue_geometry_mode") && settings["hue_geometry_mode"].is_number_integer())
    {
        hue_geometry_mode = std::clamp(settings["hue_geometry_mode"].get<int>(), 0, 2);
    }
    if(settings.contains("room_edge_fade_pct") && settings["room_edge_fade_pct"].is_number())
    {
        room_edge_fade_pct = std::clamp(settings["room_edge_fade_pct"].get<float>(), 0.0f, 100.0f);
    }

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "RoomColorWheelEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "directionRow", direction);
            EffectUiSync::setComboIndex(fx, "hueGeometryRow", hue_geometry_mode);
            EffectUiSync::setSliderValue(fx, "roomEdgeFadeRow", (int)room_edge_fade_pct);
        }
    }
}
