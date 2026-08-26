// SPDX-License-Identifier: GPL-2.0-only

#include "RotatingConeSpotlights.h"
#include "RotatingConeVolumeFieldGlsl.h"
#include "PluginLog.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <QByteArray>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QString>
#include <QVector3D>
#include <QVBoxLayout>
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(RotatingConeSpotlights);

namespace
{
float ElevBiasForSurface(int surface)
{
    switch(surface)
    {
    case 2: return -0.55f;
    case 3: return 0.55f;
    default: return 0.0f;
    }
}
} // namespace

const char* RotatingConeSpotlights::SurfaceName(int s)
{
    switch(s)
    {
    case SURF_CENTER: return "Center";
    case SURF_REF: return "Ref point";
    case SURF_CEILING: return "Ceiling";
    case SURF_FLOOR: return "Floor";
    case SURF_WALLS: return "Walls";
    default: return "Center";
    }
}

const char* RotatingConeSpotlights::MotionName(int m)
{
    switch(m)
    {
    case MOTION_INDEPENDENT: return "Independent";
    case MOTION_OPPOSITE: return "Opposite pairs";
    default: return "Independent";
    }
}

const char* RotatingConeSpotlights::LayoutName(int l)
{
    switch(l)
    {
    case LAYOUT_AUTO: return "Auto (by count)";
    case LAYOUT_CENTER: return "All center";
    case LAYOUT_ROW: return "Equal row";
    case LAYOUT_CORNERS: return "Corners";
    case LAYOUT_WALLS: return "One per wall";
    case LAYOUT_CUSTOM: return "Custom";
    default: return "Auto (by count)";
    }
}

RGBColor RotatingConeSpotlights::Hsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
        h += 1.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    float r = 0, g = 0, b = 0;
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - (float)i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch(i % 6)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    int ri = std::clamp((int)std::lround(r * 255.0f), 0, 255);
    int gi = std::clamp((int)std::lround(g * 255.0f), 0, 255);
    int bi = std::clamp((int)std::lround(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}

void RotatingConeSpotlights::ApplyLayoutPreset(int preset)
{
    const int count = std::clamp(cone_count, 1, kMaxCones);
    int use = preset;
    if(use == LAYOUT_AUTO)
    {
        if(count <= 1)
            use = LAYOUT_CENTER;
        else if(count == 4 && surface == SURF_WALLS)
            use = LAYOUT_WALLS;
        else if(count == 4)
            use = LAYOUT_ROW;
        else
            use = LAYOUT_ROW;
    }

    for(int i = 0; i < kMaxCones; i++)
    {
        apex_u[i] = 0.5f;
        apex_v[i] = 0.5f;
    }

    switch(use)
    {
    case LAYOUT_CENTER:
        break;
    case LAYOUT_ROW:
        for(int i = 0; i < count; i++)
        {
            apex_u[i] = (count == 1) ? 0.5f : (0.12f + 0.76f * (float)i / (float)(count - 1));
            apex_v[i] = 0.5f;
        }
        break;
    case LAYOUT_CORNERS:
        apex_u[0] = 0.18f; apex_v[0] = 0.18f;
        apex_u[1] = 0.82f; apex_v[1] = 0.18f;
        apex_u[2] = 0.18f; apex_v[2] = 0.82f;
        apex_u[3] = 0.82f; apex_v[3] = 0.82f;
        break;
    case LAYOUT_WALLS:
        // U = wall angle (0, 0.25, 0.5, 0.75); V = mid height
        for(int i = 0; i < kMaxCones; i++)
        {
            apex_u[i] = (float)i / 4.0f;
            apex_v[i] = 0.5f;
        }
        break;
    default:
        break;
    }

    layout_preset = (preset == LAYOUT_AUTO) ? LAYOUT_AUTO : use;
    SyncUiFromState();
}

void RotatingConeSpotlights::MarkCustomLayout()
{
    layout_preset = LAYOUT_CUSTOM;
    if(layout_combo && layout_combo->currentIndex() != LAYOUT_CUSTOM)
        layout_combo->blockSignals(true);
    if(layout_combo)
    {
        layout_combo->setCurrentIndex(LAYOUT_CUSTOM);
        layout_combo->blockSignals(false);
    }
}

void RotatingConeSpotlights::UpdateConeSliderVisibility()
{
    const int count = std::clamp(cone_count, 1, kMaxCones);
    for(int i = 0; i < kMaxCones; i++)
    {
        if(cone_pos_rows_[i])
            cone_pos_rows_[i]->setVisible(i < count);
    }
}

void RotatingConeSpotlights::UpdateConeSliderLabels()
{
    const char* u_cap = "X";
    const char* v_cap = "Z";
    if(surface == SURF_WALLS)
    {
        u_cap = "Angle";
        v_cap = "Height";
    }
    else if(surface == SURF_REF)
    {
        u_cap = "Offset X";
        v_cap = "Offset Z";
    }

    for(int i = 0; i < kMaxCones; i++)
    {
        if(apex_u_row_[i])
            apex_u_row_[i]->setCaptionText(QStringLiteral("Cone %1 %2:").arg(i + 1).arg(QString::fromUtf8(u_cap)));
        if(apex_v_row_[i])
            apex_v_row_[i]->setCaptionText(QStringLiteral("Cone %1 %2:").arg(i + 1).arg(QString::fromUtf8(v_cap)));
        if(apex_u_slider_[i])
            apex_u_slider_[i]->setToolTip(
                QStringLiteral("Cone %1 %2 (0–100%).").arg(i + 1).arg(QString::fromUtf8(u_cap)));
        if(apex_v_slider_[i])
            apex_v_slider_[i]->setToolTip(
                QStringLiteral("Cone %1 %2 (0–100%).").arg(i + 1).arg(QString::fromUtf8(v_cap)));
    }
}

void RotatingConeSpotlights::SyncUiFromState()
{
    for(int i = 0; i < kMaxCones; i++)
    {
        if(apex_u_slider_[i])
        {
            apex_u_slider_[i]->blockSignals(true);
            apex_u_slider_[i]->setValue((int)std::lround(std::clamp(apex_u[i], 0.0f, 1.0f) * 100.0f));
            apex_u_slider_[i]->blockSignals(false);
        }
        if(apex_v_slider_[i])
        {
            apex_v_slider_[i]->blockSignals(true);
            apex_v_slider_[i]->setValue((int)std::lround(std::clamp(apex_v[i], 0.0f, 1.0f) * 100.0f));
            apex_v_slider_[i]->blockSignals(false);
        }
    }
    if(layout_combo)
    {
        layout_combo->blockSignals(true);
        layout_combo->setCurrentIndex(std::clamp(layout_preset, 0, LAYOUT_COUNT - 1));
        layout_combo->blockSignals(false);
    }
    UpdateConeSliderVisibility();
    UpdateConeSliderLabels();
}

RotatingConeSpotlights::RotatingConeSpotlights(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(40);
    SetSpeed(35);
    SetRainbowMode(false);
    volume_assist_.setFragmentBody(QString::fromUtf8(RotatingConeVolumeFieldGlsl()));
    volume_assist_.setResolution(24);
    ApplyLayoutPreset(LAYOUT_AUTO);
}

RotatingConeSpotlights::~RotatingConeSpotlights() = default;

EffectInfo3D RotatingConeSpotlights::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Rotating Cone Spotlights";
    info.effect_description =
        "One to four single-beam spotlights with static placement (presets + per-cone sliders) and "
        "360° aim wander. Opposite mode locks pairs 180° apart. Speed + Motion drive aim; Frequency scrolls hue.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_ROTATING_CONE_SPOTLIGHTS;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    info.default_speed_scale = 28.0f;
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

void RotatingConeSpotlights::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("RotatingConeSpotlightsEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectSliderRow* count_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Cone count:"), 1, 4, std::clamp(cone_count, 1, 4),
        QStringLiteral("Number of beams. Each has its own apex and aim path."));
    count_row->setObjectName(QStringLiteral("coneCountRow"));
    count_slider = count_row->slider();
    count_row->bindValueChanged(
        this,
        [this](int v) {
            cone_count = std::clamp(v, 1, 4);
            if(layout_preset == LAYOUT_AUTO)
                ApplyLayoutPreset(LAYOUT_AUTO);
            UpdateConeSliderVisibility();
        },
        [](int v) { return QString::number(v); },
        on_changed);

    EffectLabeledComboRow* surf_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Surface:"));
    surf_row->setObjectName(QStringLiteral("surfaceRow"));
    surface_combo = surf_row->combo();
    for(int s = 0; s < SURF_COUNT; s++)
        surface_combo->addItem(QString::fromUtf8(SurfaceName(s)));
    surface_combo->setCurrentIndex(std::clamp(surface, 0, SURF_COUNT - 1));
    surface_combo->setToolTip(QStringLiteral(
        "Where cone apexes sit relative to the stack origin. Floor/Ceiling offset vertically; "
        "Walls use angle + height; Ref uses X/Z offsets around the origin."));
    connect(surface_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, on_changed](int idx) {
        surface = std::clamp(idx, 0, SURF_COUNT - 1);
        UpdateConeSliderLabels();
        if(layout_preset == LAYOUT_AUTO)
            ApplyLayoutPreset(LAYOUT_AUTO);
        on_changed();
    });

    EffectLabeledComboRow* motion_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Motion:"));
    motion_row->setObjectName(QStringLiteral("motionModeRow"));
    motion_combo = motion_row->combo();
    for(int m = 0; m < MOTION_COUNT; m++)
        motion_combo->addItem(QString::fromUtf8(MotionName(m)));
    motion_combo->setCurrentIndex(std::clamp(motion_mode, 0, MOTION_COUNT - 1));
    motion_combo->setToolTip(QStringLiteral(
        "Independent: each cone wanders freely on a sphere. Opposite: pairs lock 180° apart (1 ignored; 3 keeps cone 3 free)."));
    connect(motion_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, on_changed](int idx) {
        motion_mode = std::clamp(idx, 0, MOTION_COUNT - 1);
        on_changed();
    });

    EffectLabeledComboRow* layout_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Layout:"));
    layout_row->setObjectName(QStringLiteral("layoutPresetRow"));
    layout_combo = layout_row->combo();
    for(int l = 0; l < LAYOUT_COUNT; l++)
        layout_combo->addItem(QString::fromUtf8(LayoutName(l)));
    layout_combo->setCurrentIndex(std::clamp(layout_preset, 0, LAYOUT_COUNT - 1));
    layout_combo->setToolTip(QStringLiteral("Presets fill per-cone positions. Editing a slider switches to Custom."));
    connect(layout_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, on_changed](int idx) {
        const int p = std::clamp(idx, 0, LAYOUT_COUNT - 1);
        if(p == LAYOUT_CUSTOM)
        {
            layout_preset = LAYOUT_CUSTOM;
        }
        else
        {
            ApplyLayoutPreset(p);
        }
        on_changed();
    });

    QVBoxLayout* pos_body = EffectUiRows::AppendCollapsibleSectionBody(
        layout, QStringLiteral("Cone positions"),
        QStringLiteral("Per-cone U/V on the chosen surface. Presets fill these; drag to customize."), true);

    for(int i = 0; i < kMaxCones; i++)
    {
        auto* wrap = new QWidget();
        wrap->setObjectName(QStringLiteral("conePos%1").arg(i));
        auto* vl = new QVBoxLayout(wrap);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        EffectSliderRow* u_row = EffectUiRows::AppendSliderRow(
            vl, QStringLiteral("Cone %1 U:").arg(i + 1), 0, 100,
            (int)std::lround(apex_u[i] * 100.0f),
            QStringLiteral("Horizontal / angle placement for this cone."));
        u_row->setObjectName(QStringLiteral("cone%1U").arg(i));
        apex_u_row_[i] = u_row;
        apex_u_slider_[i] = u_row->slider();
        u_row->bindValueChanged(
            this,
            [this, i](int v) {
                apex_u[i] = std::clamp(v / 100.0f, 0.0f, 1.0f);
                MarkCustomLayout();
            },
            pct_format, on_changed);

        EffectSliderRow* v_row = EffectUiRows::AppendSliderRow(
            vl, QStringLiteral("Cone %1 V:").arg(i + 1), 0, 100,
            (int)std::lround(apex_v[i] * 100.0f),
            QStringLiteral("Depth / height placement for this cone."));
        v_row->setObjectName(QStringLiteral("cone%1V").arg(i));
        apex_v_row_[i] = v_row;
        apex_v_slider_[i] = v_row->slider();
        v_row->bindValueChanged(
            this,
            [this, i](int v) {
                apex_v[i] = std::clamp(v / 100.0f, 0.0f, 1.0f);
                MarkCustomLayout();
            },
            pct_format, on_changed);

        cone_pos_rows_[i] = wrap;
        pos_body->addWidget(wrap);
    }

    EffectSliderRow* cone_scale_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Cone scale:"), 5, 500,
        (int)std::lround(cone_scale * 1000.0f),
        QStringLiteral("Beam width. Lower = tighter spotlight, higher = wider wash."));
    cone_scale_row->setObjectName(QStringLiteral("coneScaleRow"));
    cone_slider = cone_scale_row->slider();
    cone_scale_row->bindValueChanged(
        this,
        [this](int v) { cone_scale = std::max(0.02f, v / 1000.0f); },
        [this](int) { return QString::number(cone_scale, 'g', 4); },
        on_changed);

    EffectSliderRow* hue_shift_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Hue shift:"), 0, 1000,
        (int)std::lround(hue01 * 1000.0f),
        QStringLiteral("Static hue offset. Frequency scrolls hue on top of this."));
    hue_shift_row->setObjectName(QStringLiteral("hueShiftRow"));
    hue_slider = hue_shift_row->slider();
    hue_shift_row->bindValueChanged(
        this,
        [this](int v) { hue01 = std::clamp(v / 1000.0f, 0.0f, 1.0f); },
        [this](int) { return QString::number(hue01, 'f', 3); },
        on_changed);

    EffectSliderRow* motion_rate_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Motion rate:"), 20, 300,
        (int)std::lround(motion_rate * 100.0f),
        QStringLiteral("How fast aims wander (multiplies Speed). Does not affect hue."));
    motion_rate_row->setObjectName(QStringLiteral("motionRow"));
    motion_slider = motion_rate_row->slider();
    motion_rate_row->bindValueChanged(
        this,
        [this](int v) { motion_rate = v / 100.0f; },
        [this](int) { return QString::number(motion_rate, 'f', 2); },
        on_changed);

    EffectSliderRow* wander_row = EffectUiRows::AppendSliderRow(
        layout, QStringLiteral("Path wander:"), 15, 200,
        (int)std::lround(wander_amt * 100.0f),
        QStringLiteral("How irregular the spherical aim path is. Higher = more chaotic sweep."));
    wander_row->setObjectName(QStringLiteral("wanderRow"));
    wander_slider = wander_row->slider();
    wander_row->bindValueChanged(
        this,
        [this](int v) { wander_amt = std::clamp(v / 100.0f, 0.15f, 2.0f); },
        [this](int) { return QString::number(wander_amt, 'f', 2); },
        on_changed);

    UpdateConeSliderVisibility();
    UpdateConeSliderLabels();
    AddWidgetToParent(w, parent);
}

void RotatingConeSpotlights::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    const EffectGridAxisHalfExtents he = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    const float gw = std::max(grid.width, 1e-5f);
    const float gh = std::max(grid.height, 1e-5f);
    const float gd = std::max(grid.depth, 1e-5f);
    const float hw01 = std::clamp(he.hw / gw, 0.05f, 0.5f);
    const float hh01 = std::clamp(he.hh / gh, 0.05f, 0.5f);
    const float hd01 = std::clamp(he.hd / gd, 0.05f, 0.5f);

    const float speed_norm = std::clamp(GetNormalizedSpeed(), 0.05f, 1.0f);
    const float wander = std::clamp(wander_amt, 0.15f, 2.0f);
    const float spin_t = time_sec * motion_rate * (0.10f + 0.55f * speed_norm) * (0.55f + 0.45f * wander);
    const float scale = std::max(1e-5f, cone_scale * (0.5f + 0.5f * GetNormalizedSize()));
    const int count = std::clamp(cone_count, 1, kMaxCones);
    const int surf = std::clamp(surface, 0, SURF_COUNT - 1);
    const int mot = std::clamp(motion_mode, 0, MOTION_COUNT - 1);
    const float elev = ElevBiasForSurface(surf);

    float vp[22] = {};
    vp[0] = spin_t;
    vp[1] = scale;
    vp[2] = hue01;
    vp[3] = (float)count;
    vp[4] = (float)mot;
    vp[5] = (float)surf;
    vp[6] = 0.5f;
    vp[7] = 0.5f;
    vp[8] = 0.5f;
    vp[9] = wander;
    vp[10] = elev;
    vp[11] = hw01;
    vp[12] = hh01;
    vp[13] = hd01;
    for(int i = 0; i < kMaxCones; i++)
    {
        vp[14 + i * 2] = std::clamp(apex_u[i], 0.0f, 1.0f);
        vp[15 + i * 2] = std::clamp(apex_v[i], 0.0f, 1.0f);
    }
    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 22))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] RotatingCone volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor RotatingConeSpotlights::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot{x, y, z};
    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    SampleGpuVolumeOriginLocal01(rot.x, rot.y, rot.z, grid, origin, GetNormalizedScale(), &c1, &c2, &c3);

    SpatialLayerCore::MapperSettings strat_map;
    EffectStratumBlend::InitStratumBreaks(strat_map);
    const float ny = SampleStratumYNorm01(rot.y, grid, origin);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(ny, strat_map, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);

    const float freq_norm = std::clamp(GetNormalizedFrequency(), 0.05f, 1.0f);
    const float hue_scroll = time * freq_norm * 0.12f * bb.speed_mul;

    float sat = 0.0f;
    float val = 0.0f;
    float h_base = 0.0f;
    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
        sat = samp.x();
        val = samp.y();
        h_base = samp.z();
    }

    if(sat <= 1e-5f)
        return 0x00000000;

    float h = std::fmod(h_base + hue_scroll + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = ny;

    if(UseEffectStripColormap())
    {
        const float ph01 = std::fmod(h + 1.0f, 1.0f);
        float pal01 = SampleEffectStripColormap01(GetEffectStripColormapRepeats(),
                                                   GetEffectStripColormapUnfold(),
                                                   GetEffectStripColormapDirectionDeg(),
                                                   ph01,
                                                   time,
                                                   grid,
                                                   GetNormalizedSize(),
                                                   origin,
                                                   rot);
        RGBColor c = ResolveStripKernelFinalColor(SpatialPatternKernelClamp(GetEffectStripColormapKernel()),
                                                  std::clamp(pal01, 0.0f, 1.0f), time);
        QColor qc = QColor::fromRgb((int)(c & 0xFF), (int)((c >> 8) & 0xFF), (int)((c >> 16) & 0xFF)).toHsv();
        const float cv = static_cast<float>(qc.valueF());
        h = std::fmod(h + hue01 + 1.0f, 1.0f);
        return Hsv01ToBgr(h, sat, std::clamp(val * cv, 0.0f, 1.0f));
    }

    if(GetRainbowMode())
    {
        float hue_deg = h * 360.0f;
        hue_deg = ApplySpatialRainbowHue(hue_deg, h, basis, sp, map, time, &grid);
        h = std::fmod(hue_deg / 360.0f + 1.0f, 1.0f);
    }

    return Hsv01ToBgr(h, sat, val);
}

nlohmann::json RotatingConeSpotlights::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["cone_spot_scale"] = cone_scale;
    j["cone_spot_hue01"] = hue01;
    j["cone_spot_motion"] = motion_rate;
    j["cone_spot_count"] = cone_count;
    j["cone_spot_surface"] = surface;
    j["cone_spot_motion_mode"] = motion_mode;
    j["cone_spot_layout"] = layout_preset;
    j["cone_spot_wander"] = wander_amt;
    j["cone_spot_apex_u"] = nlohmann::json::array({apex_u[0], apex_u[1], apex_u[2], apex_u[3]});
    j["cone_spot_apex_v"] = nlohmann::json::array({apex_v[0], apex_v[1], apex_v[2], apex_v[3]});
    return j;
}

void RotatingConeSpotlights::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("cone_spot_scale") && settings["cone_spot_scale"].is_number())
        cone_scale = std::max(0.02f, std::min(0.5f, settings["cone_spot_scale"].get<float>()));
    if(settings.contains("cone_spot_hue01") && settings["cone_spot_hue01"].is_number())
        hue01 = std::clamp(settings["cone_spot_hue01"].get<float>(), 0.0f, 1.0f);
    if(settings.contains("cone_spot_motion") && settings["cone_spot_motion"].is_number())
        motion_rate = std::clamp(settings["cone_spot_motion"].get<float>(), 0.2f, 4.0f);
    if(settings.contains("cone_spot_count") && settings["cone_spot_count"].is_number_integer())
        cone_count = std::clamp(settings["cone_spot_count"].get<int>(), 1, kMaxCones);
    if(settings.contains("cone_spot_surface") && settings["cone_spot_surface"].is_number_integer())
        surface = std::clamp(settings["cone_spot_surface"].get<int>(), 0, SURF_COUNT - 1);
    if(settings.contains("cone_spot_motion_mode") && settings["cone_spot_motion_mode"].is_number_integer())
        motion_mode = std::clamp(settings["cone_spot_motion_mode"].get<int>(), 0, MOTION_COUNT - 1);
    if(settings.contains("cone_spot_layout") && settings["cone_spot_layout"].is_number_integer())
        layout_preset = std::clamp(settings["cone_spot_layout"].get<int>(), 0, LAYOUT_COUNT - 1);
    if(settings.contains("cone_spot_wander") && settings["cone_spot_wander"].is_number())
        wander_amt = std::clamp(settings["cone_spot_wander"].get<float>(), 0.15f, 2.0f);
    if(settings.contains("cone_spot_apex_u") && settings["cone_spot_apex_u"].is_array())
    {
        const auto& a = settings["cone_spot_apex_u"];
        for(int i = 0; i < kMaxCones && i < (int)a.size(); i++)
            if(a[i].is_number())
                apex_u[i] = std::clamp(a[i].get<float>(), 0.0f, 1.0f);
    }
    if(settings.contains("cone_spot_apex_v") && settings["cone_spot_apex_v"].is_array())
    {
        const auto& a = settings["cone_spot_apex_v"];
        for(int i = 0; i < kMaxCones && i < (int)a.size(); i++)
            if(a[i].is_number())
                apex_v[i] = std::clamp(a[i].get<float>(), 0.0f, 1.0f);
    }

    if(cone_slider)
        cone_slider->setValue((int)std::lround(cone_scale * 1000.0f));
    if(hue_slider)
        hue_slider->setValue((int)std::lround(hue01 * 1000.0f));
    if(motion_slider)
        motion_slider->setValue((int)std::lround(motion_rate * 100.0f));
    if(wander_slider)
        wander_slider->setValue((int)std::lround(wander_amt * 100.0f));
    if(count_slider)
        count_slider->setValue(std::clamp(cone_count, 1, kMaxCones));
    if(surface_combo)
        surface_combo->setCurrentIndex(std::clamp(surface, 0, SURF_COUNT - 1));
    if(motion_combo)
        motion_combo->setCurrentIndex(std::clamp(motion_mode, 0, MOTION_COUNT - 1));
    SyncUiFromState();
}
