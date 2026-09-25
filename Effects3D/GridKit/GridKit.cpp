// SPDX-License-Identifier: GPL-2.0-only

#include "GridKit.h"
#include "Shaders/SpatialShaderCatalog.h"
#include "SpatialKernelColormap.h"
#include "EffectHelpers.h"
#include "PluginLog.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QByteArray>
#include <QComboBox>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

REGISTER_EFFECT_3D(GridKit);

namespace
{
float HueScroll01(float time_sec, float cycles_per_sec)
{
    return std::fmod(time_sec * std::max(0.0f, cycles_per_sec) + 1000.0f, 1.0f);
}
} // namespace

const char* GridKit::ModeName(int m)
{
    switch(m)
    {
    case MODE_PLANE_SWEEP: return "Plane Sweep";
    case MODE_WIREFRAME: return "Wireframe Box";
    case MODE_MOVING_BOXES: return "Moving Boxes";
    case MODE_RUBIK: return "Rubik Cube";
    case MODE_SPHERE_ROAM: return "Sphere Roam";
    case MODE_AXIS_SEND: return "Axis Send";
    case MODE_LIGHTNING: return "Lightning";
    default: return "Plane Sweep";
    }
}

GridKit::GridKit(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    volume_assist_.setFragmentBody(SpatialShaderCatalog::LoadEffectShader("grid-kit"));
    volume_assist_.setResolution(26);
}

EffectInfo3D GridKit::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Grid Kit";
    info.effect_description =
        "LED-cube style room volumes: sweeping planes, wireframe box, moving boxes, "
        "Rubik cube (scramble↔solve with classic face colors), roaming sphere, "
        "axis send, and lightning. Speed drives motion; Size scales; Detail sharpens.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_GRID_KIT;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
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

void GridKit::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("GridKitEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* mode_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Mode:"));
    mode_row->setObjectName(QStringLiteral("modeRow"));
    QComboBox* mode_combo = mode_row->combo();
    for(int m = 0; m < MODE_COUNT; m++)
        mode_combo->addItem(QString::fromUtf8(ModeName(m)));
    mode_combo->setCurrentIndex(std::clamp(mode, 0, MODE_COUNT - 1));
    mode_combo->setToolTip(QStringLiteral(
        "Plane Sweep = slab across an axis.\n"
        "Wireframe Box = glowing room-sized cube edges.\n"
        "Moving Boxes = soft cubes in motion.\n"
        "Rubik Cube = 3×3 stickers, classic colors, forever scrambles then solves.\n"
        "Sphere Roam = solid ball on a Lissajous path.\n"
        "Axis Send = sparks traveling along an axis.\n"
        "Lightning = jagged bolts through the volume."));
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        mode = std::clamp(idx, 0, MODE_COUNT - 1);
        emit ParametersChanged();
    });

    EffectLabeledComboRow* axis_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Travel axis:"));
    axis_row->setObjectName(QStringLiteral("axisRow"));
    QComboBox* axis_combo = axis_row->combo();
    axis_combo->addItem(QStringLiteral("X (left/right)"));
    axis_combo->addItem(QStringLiteral("Y (up/down)"));
    axis_combo->addItem(QStringLiteral("Z (depth)"));
    axis_combo->setCurrentIndex(std::clamp(travel_axis, 0, 2));
    axis_combo->setToolTip(QStringLiteral("Used by Plane Sweep and Axis Send."));
    connect(axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        travel_axis = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    EffectSliderRow* thick_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Band thickness:"),
        2,
        100,
        (int)std::lround(thickness * 100.0f),
        QStringLiteral("Width of glowing bands / edges / sparks."));
    thick_row->setObjectName(QStringLiteral("thicknessRow"));
    thick_row->bindValueChanged(
        this, [this](int v) { thickness = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* density_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Density:"),
        15,
        100,
        (int)std::lround(density * 100.0f),
        QStringLiteral("How many boxes / sparks / bolts (mode-dependent)."));
    density_row->setObjectName(QStringLiteral("densityRow"));
    density_row->bindValueChanged(
        this, [this](int v) { density = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* sway_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Sway:"),
        0,
        150,
        (int)std::lround(sway * 100.0f),
        QStringLiteral("Orbit / jog / trail amount."));
    sway_row->setObjectName(QStringLiteral("swayRow"));
    sway_row->bindValueChanged(
        this, [this](int v) { sway = v / 100.0f; }, pct_format, on_changed);

    AddWidgetToParent(w, parent);
}

void GridKit::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    (void)grid;
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const float progress = std::fmod(CalculateProgress(time_sec) * bb.speed_mul + 1000.0f, 1.0f);
    const float detail = std::clamp(GetNormalizedDetail(), 0.05f, 1.0f);
    const float size_scale = std::clamp(GetNormalizedSize(), 0.35f, 2.5f);
    const float hue_scroll01 = HueScroll01(time_sec, GetColorCycleHz() * bb.speed_mul);
    const float phase2 = std::fmod(time_sec * 0.37f * bb.speed_mul + 1000.0f, 1.0f);
    const float vp[10] = {
        (float)std::clamp(mode, 0, MODE_COUNT - 1),
        progress,
        std::max(thickness, 0.015f),
        size_scale,
        (float)std::clamp(travel_axis, 0, 2),
        detail,
        hue_scroll01,
        std::clamp(sway, 0.0f, 1.5f),
        std::clamp(density, 0.15f, 1.0f),
        phase2
    };
    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 10))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] GridKit volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor GridKit::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot{x, y, z};
    const float coord2 = SampleStratumYNorm01(rot.y, grid, origin);
    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    if(!SampleGpuVolumeOriginLocal01(rot.x, rot.y, rot.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3))
        return 0x00000000;

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);
    const float progress =
        std::fmod(CalculateProgress(time) * bb.speed_mul + 1000.0f, 1.0f);
    const float phase01 =
        std::fmod(progress + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);

    float intensity = 0.0f;
    float color01 = 0.0f;
    if(!volume_assist_.isAvailable())
        return 0x00000000;
    {
        const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
        intensity = samp.x();
        color01 = samp.y();
        if(GetStratumLayoutMode() == 1)
            intensity = EffectStratumBlend::ApplyMotionToUnit01(intensity, stratum_mot01, 0.22f);
    }
    if(intensity <= 1e-5f)
        return 0x00000000;

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = 0.12f;
    map.center_size = std::clamp(0.10f + 0.22f * GetNormalizedScale(), 0.06f, 0.50f);
    map.directional_sharpness = 1.1f;

    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;

    RGBColor c;
    if(mode == MODE_RUBIK && !UseEffectStripColormap())
    {
        /* Classic Rubik face colors (BGR pack). G encodes (face+0.5)/6. */
        static const RGBColor kRubikFaces[6] = {
            0x000000FFu, /* +X red */
            0x000080FFu, /* -X orange */
            0x00FFFFFFu, /* +Y white */
            0x0000FFFFu, /* -Y yellow */
            0x00FF0000u, /* +Z blue */
            0x0000FF00u  /* -Z green */
        };
        const int face = std::clamp((int)std::floor(color01 * 6.0f), 0, 5);
        c = kRubikFaces[face];
    }
    else if(UseEffectStripColormap())
    {
        const float strip_p01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                           GetEffectStripColormapUnfold(),
                                                           GetEffectStripColormapDirectionDeg(),
                                                           phase01,
                                                           time,
                                                           grid,
                                                           GetNormalizedSize(),
                                                           origin,
                                                           rot);
        c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), strip_p01, time);
    }
    else if(GetRainbowMode())
    {
        float hue = std::fmod(color01 * 360.0f + 720.0f, 360.0f);
        hue = ApplySpatialRainbowHue(hue, color01, basis, sp, map, time, &grid);
        c = GetRainbowColor(hue);
    }
    else if(!colors.empty())
    {
        const int n = (int)colors.size();
        if(n == 1)
        {
            c = colors[0];
        }
        else
        {
            const float scaled = std::fmod(color01 + 2.0f, 1.0f) * (float)n;
            const int i0 = ((int)std::floor(scaled)) % n;
            const int i1 = (i0 + 1) % n;
            const float frac = scaled - std::floor(scaled);
            const RGBColor a = colors[i0];
            const RGBColor b = colors[i1];
            const int r = (int)((a & 0xFF) * (1.0f - frac) + (b & 0xFF) * frac);
            const int g = (int)(((a >> 8) & 0xFF) * (1.0f - frac) + ((b >> 8) & 0xFF) * frac);
            const int bl = (int)(((a >> 16) & 0xFF) * (1.0f - frac) + ((b >> 16) & 0xFF) * frac);
            c = (RGBColor)((bl << 16) | (g << 8) | r);
        }
    }
    else
    {
        c = GetColorAtPosition(std::clamp(color01, 0.0f, 1.0f));
    }

    const int r_ = std::clamp((int)((c & 0xFF) * intensity), 0, 255);
    const int g_ = std::clamp((int)(((c >> 8) & 0xFF) * intensity), 0, 255);
    const int b_ = std::clamp((int)(((c >> 16) & 0xFF) * intensity), 0, 255);
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json GridKit::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["mode"] = mode;
    j["travel_axis"] = travel_axis;
    j["thickness"] = thickness;
    j["density"] = density;
    j["sway"] = sway;
    return j;
}

void GridKit::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("mode") && settings["mode"].is_number_integer())
        mode = std::clamp(settings["mode"].get<int>(), 0, MODE_COUNT - 1);
    if(settings.contains("travel_axis") && settings["travel_axis"].is_number_integer())
        travel_axis = std::clamp(settings["travel_axis"].get<int>(), 0, 2);
    if(settings.contains("thickness") && settings["thickness"].is_number())
        thickness = std::clamp(settings["thickness"].get<float>(), 0.02f, 1.0f);
    if(settings.contains("density") && settings["density"].is_number())
        density = std::clamp(settings["density"].get<float>(), 0.15f, 1.0f);
    if(settings.contains("sway") && settings["sway"].is_number())
        sway = std::clamp(settings["sway"].get<float>(), 0.0f, 1.5f);

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "GridKitEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "modeRow", mode);
            EffectUiSync::setComboIndex(fx, "axisRow", travel_axis);
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "thicknessRow", (int)std::lround(thickness * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "densityRow", (int)std::lround(density * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "swayRow", (int)std::lround(sway * 100.0f), pct);
        }
    }
}
