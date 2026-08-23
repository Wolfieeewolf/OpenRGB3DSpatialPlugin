// SPDX-License-Identifier: GPL-2.0-only

#include "ColorWheel.h"
#include "ColorWheelVolumeFieldGlsl.h"
#include "EffectStratumBlend.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

REGISTER_EFFECT_3D(ColorWheel);

ColorWheel::ColorWheel(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    volume_assist_.setFragmentBody(QString::fromUtf8(ColorWheelVolumeFieldGlsl()));
    volume_assist_.setResolution(18);
}

EffectInfo3D ColorWheel::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Color Wheel";
    info.effect_description =
        "Rotating hue layouts (radial wheel, shear bands, concentric rings, pie slices) with GPU assist; "
        "optional independent floor / mid / ceiling wheels";
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
    QWidget* w = EffectUiRows::NewEffectPanel("ColorWheelEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);

    EffectLabeledComboRow* direction_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Direction:"));
    direction_row->setObjectName(QStringLiteral("directionRow"));
    QComboBox* dir_combo = direction_row->combo();
    dir_combo->addItem(QStringLiteral("Clockwise"));
    dir_combo->addItem(QStringLiteral("Counter-clockwise"));
    dir_combo->setCurrentIndex(direction);
    dir_combo->setToolTip(QStringLiteral(
        "Hue progression around the effect origin in the active plane (see Plane in common controls)."));
    dir_combo->setItemData(0,
                            QStringLiteral("Increasing angle follows clock motion when viewed from the plane normal."),
                            Qt::ToolTipRole);
    dir_combo->setItemData(1, QStringLiteral("Reverses hue sweep—useful to match other rotating effects."),
                            Qt::ToolTipRole);
    connect(dir_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        direction = std::max(0, std::min(1, idx));
        emit ParametersChanged();
    });

    EffectLabeledComboRow* hue_geometry_row = EffectUiRows::AppendComboRow(layout, tr("Hue geometry:"));
    hue_geometry_row->setObjectName(QStringLiteral("hueGeometryRow"));
    QComboBox* geom_combo = hue_geometry_row->combo();
    geom_combo->addItem(tr("Radial (classic)"), 0);
    geom_combo->addItem(tr("Shear (bands)"), 1);
    geom_combo->addItem(tr("Rings (concentric)"), 2);
    geom_combo->addItem(tr("Pie slices"), 3);
    geom_combo->setToolTip(tr(
        "How hue is laid out in the active plane.\n"
        "Radial: classic wheel around the origin.\n"
        "Shear: rotating planar bands (no single center).\n"
        "Rings: concentric hue by distance from origin.\n"
        "Pie: hard color wedges; Hue repeats sets slice count."));
    for(int i = 0; i < geom_combo->count(); i++)
    {
        if(geom_combo->itemData(i).toInt() == hue_geometry_mode)
        {
            geom_combo->setCurrentIndex(i);
            break;
        }
    }
    connect(geom_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [geom_combo, this](int) {
        hue_geometry_mode = std::clamp(geom_combo->currentData().toInt(), 0, 3);
        emit ParametersChanged();
    });

    const auto on_changed = [this]() { emit ParametersChanged(); };
    EffectSliderRow* hue_repeats_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Hue repeats:"),
        10,
        300,
        (int)std::lround(hue_repeats * 100.0f),
        QStringLiteral("How many times the rainbow wraps around one turn. 100% = one smooth wheel."));
    hue_repeats_row->setObjectName(QStringLiteral("hueRepeatsRow"));
    hue_repeats_row->bindValueChanged(
        this,
        [this](int v) { hue_repeats = std::clamp(v / 100.0f, 0.1f, 3.0f); },
        [](int v) { return QString::number(v) + QStringLiteral("%"); },
        on_changed);

    AddWidgetToParent(w, parent);
}

void ColorWheel::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    (void)grid;
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const float progress = CalculateProgress(time_sec) * bb.speed_mul;
    const float dir = (direction == 0) ? 1.0f : -1.0f;
    const float wrap = std::clamp(hue_repeats, 0.1f, 3.0f);
    const float freq_spin = time_sec * GetScaledFrequency() * 0.12f * bb.speed_mul;
    const float vp[6] = {
        progress,
        dir,
        wrap,
        (float)GetPlane(),
        (float)hue_geometry_mode,
        freq_spin
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 6);
}

RGBColor ColorWheel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    const bool room_mapped = UsesRoomMappedCoordinates();
    Vector3D origin = room_mapped ? Vector3D{grid.center_x, grid.center_y, grid.center_z}
                                  : GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;

    float progress = CalculateProgress(time);
    float detail = std::max(0.05f, GetScaledDetail());
    Vector3D rot{x, y, z};
    float lx = rot.x - origin.x, ly = rot.y - origin.y, lz = rot.z - origin.z;

    float oy = 0.5f;
    PackEffectOrigin01(grid, origin, nullptr, &oy, nullptr);
    const float y_norm = std::clamp(NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y) - oy + 0.5f, 0.0f, 1.0f);
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

    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    if(room_mapped)
    {
        const float size_tight = 1.0f / std::max(0.2f, GetNormalizedSize());
        e.hw /= sz_mul * size_tight;
        e.hh /= sz_mul * size_tight;
        e.hd /= sz_mul * size_tight;
        if(std::fabs(lx) > e.hw || std::fabs(ly) > e.hh || std::fabs(lz) > e.hd)
        {
            return 0x00000000;
        }
    }
    else
    {
        if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        {
            return 0x00000000;
        }
        e.hw /= sz_mul;
        e.hh /= sz_mul;
        e.hd /= sz_mul;
    }

    float gpu_plane01 = 0.0f;
    bool have_gpu_plane = false;
    if(volume_assist_.isAvailable())
    {
        float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
        SampleCoordsOriginLocal01(rot.x, rot.y, rot.z, origin, e, &c1, &c2, &c3);
        const QVector3D cs = volume_assist_.sample01(c1, c2, c3);
        // Cos/sin atlas encoding (range-packed to 0..1 for the RGBA8 atlas) — avoids
        // the rotating false seam from filtering wrapped hue.
        float hue_rad = std::atan2(cs.y() * 2.0f - 1.0f, cs.x() * 2.0f - 1.0f);
        gpu_plane01 = std::fmod(hue_rad / TWO_PI + 1.0f, 1.0f);
        gpu_plane01 = std::fmod(gpu_plane01 + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01)
                                    + time * GetScaledFrequency() * 0.02f * (spd_mul - 1.0f) + 1.0f,
                                1.0f);
        have_gpu_plane = true;
    }

    float hue_plane = 0.0f;
    if(!have_gpu_plane)
        return 0x00000000;
    hue_plane = gpu_plane01 * 360.0f;


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
    if(UseEffectStripColormap())
    {
        return ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), std::clamp(palette01, 0.0f, 1.0f), time);
    }
    return GetRainbowMode() ? GetRainbowColor(palette01 * 360.0f) : GetColorAtPosition(palette01);
}

nlohmann::json ColorWheel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["direction"] = direction;
    j["hue_geometry_mode"] = hue_geometry_mode;
    j["hue_repeats"] = hue_repeats;
    return j;
}

void ColorWheel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("direction") && settings["direction"].is_number_integer())
        direction = std::max(0, std::min(1, settings["direction"].get<int>()));
    if(settings.contains("hue_geometry_mode") && settings["hue_geometry_mode"].is_number_integer())
        hue_geometry_mode = std::clamp(settings["hue_geometry_mode"].get<int>(), 0, 3);
    if(settings.contains("hue_repeats") && settings["hue_repeats"].is_number())
        hue_repeats = std::clamp(settings["hue_repeats"].get<float>(), 0.1f, 3.0f);

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "ColorWheelEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "directionRow", direction);
            EffectUiSync::setComboIndex(fx, "hueGeometryRow", hue_geometry_mode);
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "hueRepeatsRow", (int)std::lround(hue_repeats * 100.0f), pct);
        }
    }
}
