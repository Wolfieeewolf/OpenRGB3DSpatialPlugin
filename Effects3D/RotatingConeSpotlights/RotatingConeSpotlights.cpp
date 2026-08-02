// SPDX-License-Identifier: GPL-2.0-only

#include "RotatingConeSpotlights.h"
#include "RotatingConeVolumeFieldGlsl.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QVector3D>
#include <QVBoxLayout>
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_EFFECT_3D(RotatingConeSpotlights);

namespace
{
struct Vec3
{
    float x, y, z;
};

Vec3 Normalize(Vec3 v)
{
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if(len < 1e-5f)
        return {0.0f, 0.0f, 1.0f};
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 Cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 ConeLocal(Vec3 p, Vec3 aim)
{
    Vec3 z = Normalize(aim);
    Vec3 up = (std::fabs(z.y) < 0.92f) ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
    Vec3 x = Normalize(Cross(up, z));
    Vec3 y = Cross(z, x);
    return {p.x * x.x + p.y * x.y + p.z * x.z,
            p.x * y.x + p.y * y.y + p.z * y.z,
            p.x * z.x + p.y * z.y + p.z * z.z};
}

Vec3 ResolveApex(int surface, float u, float v, float ox, float oy, float oz)
{
    const float uu = std::clamp(u, 0.0f, 1.0f);
    const float vv = std::clamp(v, 0.0f, 1.0f);
    switch(surface)
    {
    case 1: // Ref
    {
        const float dx = (uu - 0.5f) * 0.70f;
        const float dz = (vv - 0.5f) * 0.70f;
        return {std::clamp(ox + dx, 0.05f, 0.95f),
                std::clamp(oy, 0.05f, 0.95f),
                std::clamp(oz + dz, 0.05f, 0.95f)};
    }
    case 2: // Ceiling
        return {0.10f + 0.80f * uu, 0.92f, 0.10f + 0.80f * vv};
    case 3: // Floor
        return {0.10f + 0.80f * uu, 0.08f, 0.10f + 0.80f * vv};
    case 4: // Walls
    {
        const float ang = uu * TWO_PI;
        return {0.5f + 0.46f * std::cos(ang), 0.12f + 0.76f * vv, 0.5f + 0.46f * std::sin(ang)};
    }
    default: // Center
        return {0.12f + 0.76f * uu, 0.5f, 0.12f + 0.76f * vv};
    }
}

Vec3 AimWander(int i, int count, int motion_mode, float spin_t, float wander, float elev_bias)
{
    const int pair = (i / 2) * 2;
    const bool is_follower = (motion_mode > 0 && i != pair && !(count == 3 && i == 2));
    const int src = is_follower ? pair : i;

    const float seed = (float)src * 1.6180339f;
    const float rate = 0.55f + 0.22f * (float)src;
    const float t = spin_t * rate + seed * 2.3999632f;

    float yaw = t
              + wander * 0.85f * std::sin(t * 0.73f + seed * 4.1f)
              + wander * 0.45f * std::sin(t * 1.37f + seed * 2.7f)
              + wander * 0.25f * std::sin(t * 2.11f + seed);
    float pitch = elev_bias
                + wander * 0.55f * std::sin(t * 0.91f + seed * 3.3f)
                + wander * 0.35f * std::sin(t * 1.67f + seed * 1.9f);

    if(is_follower)
    {
        yaw += (float)M_PI;
        pitch = elev_bias - (pitch - elev_bias);
    }

    pitch = std::clamp(pitch, -1.35f, 1.35f);
    const float cp = std::cos(pitch);
    return Normalize({std::cos(yaw) * cp, std::sin(pitch), std::sin(yaw) * cp});
}

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
    volume_assist_.setResolution(18);
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
        "Where cone apexes sit. Floor/Ceiling use X/Z; Walls use angle + height; Ref offsets around the reference point."));
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
    float ox, oy, oz;
    PackEffectOrigin01(grid, origin, &ox, &oy, &oz);

    const float speed_norm = std::clamp(GetNormalizedSpeed(), 0.05f, 1.0f);
    const float wander = std::clamp(wander_amt, 0.15f, 2.0f);
    const float spin_t = time_sec * motion_rate * (0.10f + 0.55f * speed_norm) * (0.55f + 0.45f * wander);
    const float scale = std::max(1e-5f, cone_scale * (0.5f + 0.5f * GetNormalizedSize()));
    const int count = std::clamp(cone_count, 1, kMaxCones);
    const int surf = std::clamp(surface, 0, SURF_COUNT - 1);
    const int mot = std::clamp(motion_mode, 0, MOTION_COUNT - 1);
    const float elev = ElevBiasForSurface(surf);

    float vp[16] = {};
    vp[0] = spin_t;
    vp[1] = scale;
    vp[2] = hue01;
    vp[3] = (float)count;
    vp[4] = (float)mot;
    vp[5] = (float)surf;
    for(int i = 0; i < kMaxCones; i++)
    {
        vp[6 + i * 2] = std::clamp(apex_u[i], 0.0f, 1.0f);
        vp[7 + i * 2] = std::clamp(apex_v[i], 0.0f, 1.0f);
    }
    if(surf == SURF_REF)
    {
        /* Pack wander into integer band, ox in fraction; oy/oz in [15]. */
        vp[14] = std::floor(wander * 100.0f + 0.5f) + std::clamp(ox, 0.0f, 0.999f);
        vp[15] = std::floor(std::clamp(oy, 0.0f, 1.0f) * 4095.0f + 0.5f) + std::clamp(oz, 0.0f, 0.999f);
    }
    else
    {
        vp[14] = wander;
        vp[15] = elev;
    }
    volume_assist_.prepare(render_sequence, time_sec, vp, 16);
}

RGBColor RotatingConeSpotlights::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    const float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    const float speed_norm = std::clamp(GetNormalizedSpeed(), 0.05f, 1.0f);
    const float freq_norm = std::clamp(GetNormalizedFrequency(), 0.05f, 1.0f);
    const float wander = std::clamp(wander_amt, 0.15f, 2.0f);
    const float spin_t = time * motion_rate * (0.10f + 0.55f * speed_norm) * (0.55f + 0.45f * wander);
    const float hue_scroll = time * freq_norm * 0.12f;
    const float scale = std::max(1e-5f, cone_scale * (0.5f + 0.5f * GetNormalizedSize()));
    const int count = std::clamp(cone_count, 1, kMaxCones);
    const int surf = std::clamp(surface, 0, SURF_COUNT - 1);
    const int mot = std::clamp(motion_mode, 0, MOTION_COUNT - 1);
    float ox, oy, oz;
    PackEffectOrigin01(grid, origin, &ox, &oy, &oz);

    float sat = 0.0f;
    float val = 0.0f;
    float h_base = 0.0f;
    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
        sat = samp.x();
        val = samp.y();
        h_base = samp.z();
    }
    else
    {
        const float elev = ElevBiasForSurface(surf);
        float best_sat = 0.0f, best_val = 0.0f, best_h = 0.0f;
        for(int i = 0; i < count; i++)
        {
            Vec3 apex = ResolveApex(surf, apex_u[i], apex_v[i], ox, oy, oz);
            Vec3 aim = AimWander(i, count, mot, spin_t, wander, elev);
            if(surf == SURF_WALLS)
            {
                Vec3 inward = Normalize({0.5f - apex.x, 0.5f - apex.y, 0.5f - apex.z});
                aim = Normalize({inward.x * 0.35f + aim.x * 0.65f,
                                 inward.y * 0.35f + aim.y * 0.65f,
                                 inward.z * 0.35f + aim.z * 0.65f});
            }
            else if(surf == SURF_CEILING)
            {
                aim = Normalize({aim.x * 0.70f, aim.y * 0.70f - 0.30f, aim.z * 0.70f});
            }
            else if(surf == SURF_FLOOR)
            {
                aim = Normalize({aim.x * 0.70f, aim.y * 0.70f + 0.30f, aim.z * 0.70f});
            }

            Vec3 local = ConeLocal({nx - apex.x, ny - apex.y, nz - apex.z}, aim);
            if(local.z <= 0.0f)
                continue;
            const float radial = std::sqrt((local.x * local.x + local.y * local.y) / scale);
            float dist = std::clamp(local.z - radial, -1.0f, 1.0f);
            const float si = std::clamp(1.0f - dist, 0.0f, 1.0f);
            const float vi = std::clamp(std::pow(std::max(0.0f, 1.0f + dist), 4.0f), 0.0f, 1.0f);
            const float hi = std::fmod(hue01 + (float)i / (float)count + 0.07f * apex.x + 1.0f, 1.0f);
            if(si > best_sat)
            {
                best_sat = si;
                best_val = vi;
                best_h = hi;
            }
        }
        sat = best_sat;
        val = best_val;
        h_base = best_h;
    }

    if(sat <= 1e-5f)
        return 0x00000000;

    float h = std::fmod(h_base + hue_scroll + 1.0f, 1.0f);

    if(UseEffectStripColormap())
    {
        const float ph01 = std::fmod(h + 1.0f, 1.0f);
        float pal01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                 GetEffectStripColormapRepeats(),
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
        const float ch = static_cast<float>(qc.hueF());
        const float cv = static_cast<float>(qc.valueF());
        h = (ch >= 0.0f) ? std::fmod(ch + hue01 + hue_scroll + 1.0f, 1.0f) : h;
        return Hsv01ToBgr(h, sat, std::clamp(val * cv, 0.0f, 1.0f));
    }

    if(GetRainbowMode())
        h = std::fmod(h_base + hue_scroll + (nx - 0.5f) * 0.08f + 1.0f, 1.0f);

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
    else if(!settings.contains("cone_spot_apex_u"))
    {
        ApplyLayoutPreset(layout_preset == LAYOUT_CUSTOM ? LAYOUT_AUTO : layout_preset);
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
