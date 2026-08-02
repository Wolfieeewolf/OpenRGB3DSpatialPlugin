// SPDX-License-Identifier: GPL-2.0-only

#include "BreathingSphere.h"
#include "BreathingSphereVolumeFieldGlsl.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"

REGISTER_EFFECT_3D(BreathingSphere);

#include <QComboBox>
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <algorithm>
#include "../EffectHelpers.h"

namespace
{

float polyRadialXZ(float px, float pz, int n)
{
    if(n < 3) n = 3;
    const float an = TWO_PI / float(n);
    const float a = atan2f(pz, px);
    const float r = hypotf(px, pz);
    return cosf(floorf(0.5f + a / an) * an - a) * r / cosf(3.14159265f / float(n));
}

/** Iso-metric: sphere / cube / box / polygonal prism (not revolved → sphere-like). */
float shapeMetric(float lx, float ly, float lz, int shape, float ax, float az)
{
    switch(shape)
    {
    default:
    case 0:
        return hypotf(lx, hypotf(ly, lz));
    case 1:
        return fmaxf(fmaxf(fabsf(lx), fabsf(ly)), fabsf(lz));
    case 2:
        return fmaxf(fmaxf(fabsf(lx) / fmaxf(ax, 1e-4f), fabsf(ly)), fabsf(lz) / fmaxf(az, 1e-4f));
    case 3:
    case 4:
    {
        const int n = (shape == 3) ? 3 : 5;
        return fmaxf(polyRadialXZ(lx, lz, n), fabsf(ly));
    }
    }
}

float smstep(float e0, float e1, float x)
{
    float t = (x - e0) / fmaxf(e1 - e0, 1e-5f);
    t = fmaxf(0.0f, fminf(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

} // namespace

const char* BreathingSphere::ShapeName(int s)
{
    switch(s)
    {
    case SHAPE_SPHERE: return "Sphere";
    case SHAPE_SQUARE: return "Square";
    case SHAPE_RECTANGLE: return "Rectangle";
    case SHAPE_TRIANGLE: return "Triangle";
    case SHAPE_PENTAGON: return "Pentagon";
    case SHAPE_WHOLE_ROOM: return "Whole room (inhale wave)";
    default: return "Sphere";
    }
}

const char* BreathingSphere::EdgeName(int e)
{
    switch(NormalizeEdgeProfile(e))
    {
    case EDGE_CRISP: return "Crisp";
    case EDGE_SOFT:
    default: return "Soft";
    }
}

int BreathingSphere::NormalizeEdgeProfile(int e)
{
    if(e == EDGE_LEGACY_FEATHERED || e == EDGE_SOFT)
        return EDGE_SOFT;
    if(e == EDGE_LEGACY_RING || e == EDGE_CRISP || e == 1)
        return EDGE_CRISP;
    return EDGE_SOFT;
}

BreathingSphere::BreathingSphere(QWidget* parent) : SpatialEffect3D(parent)
{
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
    volume_assist_.setFragmentBody(QString::fromUtf8(BreathingSphereVolumeFieldGlsl()));
    volume_assist_.setResolution(28);
}

BreathingSphere::~BreathingSphere() = default;

EffectInfo3D BreathingSphere::GetEffectInfo() const
{
    EffectInfo3D info;
    info.effect_name = "Breathing Sphere";
    info.effect_description =
        "Breathing shell (sphere / cube / box / triangle / pentagon prism) or whole-room wave; "
        "soft or crisp silhouette, optional center hole; GPU assist";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_BREATHING_SPHERE;
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

    info.default_speed_scale = 20.0f;
    info.default_frequency_scale = 100.0f;
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

void BreathingSphere::SetupCustomUI(QWidget* parent)
{
    QWidget* w = EffectUiRows::NewEffectPanel("BreathingSphereEffectSettings");
    QVBoxLayout* layout = EffectUiRows::PanelLayout(w);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* shape_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Shape:"));
    shape_row->setObjectName(QStringLiteral("shapeRow"));
    QComboBox* shape_combo = shape_row->combo();
    for(int s = 0; s < SHAPE_COUNT; s++)
        shape_combo->addItem(ShapeName(s));
    shape_combo->setCurrentIndex(std::max(0, std::min(breathing_shape, SHAPE_COUNT - 1)));
    shape_combo->setToolTip(QStringLiteral(
        "Shell footprint. Square/rectangle/triangle/pentagon keep that silhouette (prism/box) — "
        "they do not bloom into a sphere. Whole room is a distance wave from the effect origin."));
    shape_combo->setItemData(SHAPE_SPHERE, QStringLiteral("Round 3D ball shell."), Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_SQUARE, QStringLiteral("Axis-aligned cube silhouette."), Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_RECTANGLE,
                             QStringLiteral("Rectangular box matching room width vs depth."),
                             Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_TRIANGLE, QStringLiteral("Triangular prism (flat sides)."), Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_PENTAGON, QStringLiteral("Pentagonal prism (flat sides)."), Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_WHOLE_ROOM,
                             QStringLiteral("No shell — inhale/exhale color wave from the effect origin."),
                             Qt::ToolTipRole);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        breathing_shape = std::max(0, std::min(idx, SHAPE_COUNT - 1));
        emit ParametersChanged();
    });

    EffectLabeledComboRow* edge_row = EffectUiRows::AppendComboRow(layout, QStringLiteral("Edge:"));
    edge_row->setObjectName(QStringLiteral("edgeRow"));
    QComboBox* edge_combo = edge_row->combo();
    for(int e = 0; e < EDGE_COUNT; e++)
        edge_combo->addItem(EdgeName(e));
    edge_combo->setCurrentIndex(NormalizeEdgeProfile(edge_profile));
    edge_combo->setToolTip(QStringLiteral(
        "Soft = wider falloff. Crisp = razor edges — square pulse expands as a square, not a blob."));
    edge_combo->setItemData(EDGE_SOFT, QStringLiteral("Gentle boundary falloff."), Qt::ToolTipRole);
    edge_combo->setItemData(EDGE_CRISP,
                            QStringLiteral("Hard silhouette. Breath pulse keeps the chosen shape."),
                            Qt::ToolTipRole);
    connect(edge_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        edge_profile = NormalizeEdgeProfile(idx);
        emit ParametersChanged();
    });

    EffectSliderRow* breath_pulse_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Breath pulse:"),
        0,
        100,
        breath_pulse_pct,
        QStringLiteral(
            "How far the shell grows/shrinks each breath (shape-metric size). "
            "0 = static. High values = dramatic square/triangle expansion."));
    breath_pulse_row->setObjectName(QStringLiteral("breathPulseRow"));
    breath_pulse_row->bindValueChanged(
        this, [this](int v) { breath_pulse_pct = v; }, pct_format, on_changed);

    EffectSliderRow* center_hole_row = EffectUiRows::AppendSliderRow(
        layout,
        QStringLiteral("Center hole:"),
        0,
        100,
        center_hole_pct,
        QStringLiteral(
            "0 = solid through the middle; higher values carve an empty core (shell). "
            "Ignored in whole-room mode."));
    center_hole_row->setObjectName(QStringLiteral("centerHoleRow"));
    center_hole_row->bindValueChanged(
        this, [this](int v) { center_hole_pct = v; }, pct_format, on_changed);

    AddWidgetToParent(w, parent);
}

void BreathingSphere::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float ox = 0.5f, oy = 0.5f, oz = 0.5f;
    PackEffectOrigin01(grid, origin, &ox, &oy, &oz);

    const float progress_v = CalculateProgress(time_sec);
    const float detail = std::max(0.05f, GetScaledDetail());
    const float size_multiplier = GetNormalizedSize();
    const float base_scale = 0.45f;
    const float breath_t = breath_pulse_pct / 100.0f;
    const float breath_amp = breath_t * 0.92f; // strong grow/shrink; 0 when slider is 0
    const float rate = GetScaledFrequency();
    const float breath_phase = progress_v * rate * 0.2f;
    /* R expands in shape-metric space — square breath stays a square pulse. */
    const float R_l = base_scale * size_multiplier * (1.0f + breath_amp * sinf(breath_phase));
    const int edge = NormalizeEdgeProfile(edge_profile);
    const int shape = std::max(0, std::min(breathing_shape, SHAPE_COUNT - 1));

    float med = EffectGridMedianHalfExtent(grid, GetNormalizedScale());
    if(med < 1e-4f)
        med = 1.0f;
    const float sx = std::max(0.25f, grid.width / med);
    const float sy = std::max(0.25f, grid.height / med);
    const float sz = std::max(0.25f, grid.depth / med);

    float aspect_med = std::max(grid.width, grid.depth);
    if(aspect_med < 1e-4f)
        aspect_med = 1.0f;
    const float ax = std::clamp(grid.width / aspect_med, 0.15f, 1.0f);
    const float az = std::clamp(grid.depth / aspect_med, 0.15f, 1.0f);
    const float pulse_strength = breath_t; // 0 when slider is 0

    const float vp[16] = {
        R_l,
        breath_phase,
        progress_v,
        detail,
        (float)edge,
        center_hole_pct / 100.0f,
        ox,
        oy,
        oz,
        (float)shape,
        ax,
        az,
        pulse_strength,
        sx,
        sy,
        sz
    };
    volume_assist_.prepare(render_sequence, time_sec, vp, 16);
}

RGBColor BreathingSphere::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float raw_rx = x - origin.x;
    float raw_ry = y - origin.y;
    float raw_rz = z - origin.z;

    if(!IsWithinEffectBoundary(raw_rx, raw_ry, raw_rz, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);

    float rel_x = rot.x - origin.x;
    float rel_y = rot.y - origin.y;
    float rel_z = rot.z - origin.z;

    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    progress = CalculateProgress(time * bb.speed_mul);
    const float cmap_phase01 = std::fmod(progress + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);
    float strip_p01 = 0.0f;
    if(UseEffectStripColormap())
    {
        strip_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                               GetEffectStripColormapRepeats(),
                                               GetEffectStripColormapUnfold(),
                                               GetEffectStripColormapDirectionDeg(),
                                               cmap_phase01,
                                               time,
                                               grid,
                                               GetNormalizedSize(),
                                               origin,
                                               rot);
    }
    int shape = std::max(0, std::min(breathing_shape, SHAPE_COUNT - 1));
    int edge = NormalizeEdgeProfile(edge_profile);
    float breath_phase = progress * rate * 0.2f;
    const float breath_t = breath_pulse_pct / 100.0f;
    float pulse_strength = breath_t;

    float ox = 0.5f, oy = 0.5f, oz = 0.5f;
    PackEffectOrigin01(grid, origin, &ox, &oy, &oz);
    float c1 = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    float c2 = coord2;
    float c3 = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    if(shape == SHAPE_WHOLE_ROOM)
    {
        if(volume_assist_.isAvailable())
        {
            const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
            float air = samp.x();
            float pos = samp.y();
            RGBColor c;
            if(UseEffectStripColormap())
                c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), strip_p01, time);
            else if(GetRainbowMode())
            {
                float hue = pos * 360.0f + time * rate * 12.0f * bb.speed_mul
                            + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f;
                c = GetRainbowColor(hue);
            }
            else
                c = GetColorAtPosition(std::fmod(pos + progress * 0.04f + 1.0f, 1.0f));
            unsigned char r = (unsigned char)fminf(255.0f, fmaxf(0.0f, (c & 0xFF) * air));
            unsigned char g = (unsigned char)fminf(255.0f, fmaxf(0.0f, ((c >> 8) & 0xFF) * air));
            unsigned char b = (unsigned char)fminf(255.0f, fmaxf(0.0f, ((c >> 16) & 0xFF) * air));
            return (RGBColor)((b << 16) | (g << 8) | r);
        }
        float med = EffectGridMedianHalfExtent(grid, GetNormalizedScale());
        float dist_norm = hypotf(rel_x, hypotf(rel_y, rel_z)) / fmaxf(med * 2.0f, 1e-4f);
        float inhale = sinf(breath_phase) * pulse_strength;
        float exhale = sinf(breath_phase + 1.2f) * pulse_strength;
        const float tm = std::max(0.25f, bb.tight_mul);
        float wave = sinf(inhale * 3.14159265f * 1.15f - dist_norm * (9.0f + 5.0f * detail) / tm) * pulse_strength;
        float ripple = sinf(breath_phase * 2.1f * bb.speed_mul - dist_norm * TWO_PI * 2.2f / tm + rel_y * 0.02f * detail / tm) * pulse_strength;
        float rush = sinf(exhale * 1.7f * bb.speed_mul + (rel_x + rel_z) * 0.015f * detail / tm) * 0.4f * pulse_strength;

        RGBColor c;
        if(UseEffectStripColormap())
            c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), strip_p01, time);
        else if(GetRainbowMode())
        {
            float hue = time * rate * 12.0f * bb.speed_mul + inhale * 110.0f + wave * 175.0f + ripple * 70.0f + rush * 60.0f + progress * 35.0f + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f;
            c = GetRainbowColor(hue);
        }
        else
        {
            float pos = fmodf(0.38f + 0.32f * inhale + 0.24f * wave + 0.12f * ripple + rush * 0.1f + progress * 0.04f, 1.0f);
            if(pos < 0.0f) pos += 1.0f;
            c = GetColorAtPosition(pos);
        }
        float air = 0.78f + 0.22f * (0.5f + 0.5f * sinf(breath_phase * 1.05f)) * (0.55f + 0.45f * pulse_strength);
        if(pulse_strength < 0.001f)
            air = 0.85f;
        unsigned char r = (unsigned char)fminf(255.0f, fmaxf(0.0f, (c & 0xFF) * air));
        unsigned char g = (unsigned char)fminf(255.0f, fmaxf(0.0f, ((c >> 8) & 0xFF) * air));
        unsigned char b = (unsigned char)fminf(255.0f, fmaxf(0.0f, ((c >> 16) & 0xFF) * air));
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    float size_multiplier = GetNormalizedSize();
    float med = EffectGridMedianHalfExtent(grid, GetNormalizedScale());
    if(med < 1e-4f)
        med = 1.0f;
    float base_scale = 0.45f;
    float breath_amp = breath_t * 0.92f;
    float sphere_radius = med * base_scale * size_multiplier * (1.0f + breath_amp * sinf(breath_phase));

    float aspect_med = std::max(grid.width, grid.depth);
    if(aspect_med < 1e-4f)
        aspect_med = 1.0f;
    const float ax = std::clamp(grid.width / aspect_med, 0.15f, 1.0f);
    const float az = std::clamp(grid.depth / aspect_med, 0.15f, 1.0f);

    float sphere_intensity = 0.0f;
    float norm_in_shell = 0.0f;
    bool used_gpu = false;

    if(volume_assist_.isAvailable())
    {
        const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
        sphere_intensity = samp.x();
        norm_in_shell = samp.y();
        used_gpu = true;
    }

    if(!used_gpu)
    {
        const float lx = rel_x / med;
        const float ly = rel_y / med;
        const float lz = rel_z / med;
        const float R = sphere_radius / med;
        const float distance = shapeMetric(lx, ly, lz, shape, ax, az);

        float band = (edge == EDGE_CRISP) ? 0.018f : 0.16f;
        band = fmaxf(band * (0.7f + 0.3f / fmaxf(detail, 0.2f)), (edge == EDGE_CRISP) ? 0.012f : 0.02f);

        const bool use_donut = (center_hole_pct > 0);
        const float hole_frac = center_hole_pct / 100.0f;

        if(!use_donut)
        {
            if(edge == EDGE_CRISP)
            {
                float inside = 1.0f - smstep(R - band * 0.12f, R + band * 0.55f, distance);
                float surface = 1.0f - smstep(0.0f, band * 0.55f, fabsf(distance - R));
                inside = fmaxf(inside, surface * 0.4f * ((distance <= R + band) ? 1.0f : 0.0f));
                if(distance > R + band)
                    inside = 0.0f;
                sphere_intensity = fmaxf(0.0f, fminf(1.0f, inside));
            }
            else
            {
                float inside = 1.0f - smstep(R - band * 0.35f, R + band, distance);
                float soft_out = 1.0f - smstep(R, R + band * 2.2f, distance);
                inside = fmaxf(inside, soft_out * 0.35f);
                sphere_intensity = fmaxf(0.0f, fminf(1.0f, inside));
            }
            norm_in_shell = fmaxf(0.0f, fminf(1.2f, distance / (R + 1e-5f)));
        }
        else
        {
            const float r_in = hole_frac * R * 0.9f;
            const float iw = (edge == EDGE_CRISP) ? band * 0.4f : band * 1.1f;
            const float ow = (edge == EDGE_CRISP) ? band * 0.5f : band * 1.6f;
            const float inner_open = smstep(r_in - iw, r_in + iw, distance);
            float outer_open = 1.0f - smstep(R - ow * 0.2f, R + ow, distance);
            const float span_eff = fmaxf(R - r_in, R * 0.08f);
            float u = (distance - r_in) / span_eff;
            u = fmaxf(0.0f, fminf(1.0f, u));
            float bell = sinf(u * 3.14159265f);
            if(edge == EDGE_CRISP)
            {
                float b2 = bell * bell;
                bell = b2 * b2;
                if(distance > R + ow)
                    outer_open = 0.0f;
            }
            sphere_intensity = fmaxf(0.0f, fminf(1.0f, inner_open * outer_open * (0.15f + 0.85f * bell)));
            norm_in_shell = fmaxf(0.0f, fminf(1.2f, (distance - r_in) / fmaxf(R - r_in, 1e-4f)));
        }
    }

    RGBColor final_color;
    if(UseEffectStripColormap())
        final_color = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), strip_p01, time);
    else if(GetRainbowMode())
    {
        float hue = norm_in_shell * 290.0f * (0.6f + 0.4f * detail) + breath_phase * 72.0f + time * rate * 12.0f * bb.speed_mul + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) * 360.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        float pos = fmodf(fmin(1.0f, norm_in_shell) * (0.6f + 0.4f * detail) + breath_phase * 0.1f, 1.0f);
        if(pos < 0.0f) pos += 1.0f;
        final_color = GetColorAtPosition(pos);
    }
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * sphere_intensity);
    g = (unsigned char)(g * sphere_intensity);
    b = (unsigned char)(b * sphere_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json BreathingSphere::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["breathing_shape"] = breathing_shape;
    j["edge_profile"] = NormalizeEdgeProfile(edge_profile);
    j["breath_pulse_pct"] = breath_pulse_pct;
    j["center_hole_pct"] = center_hole_pct;
    return j;
}

void BreathingSphere::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("breathing_shape") && settings["breathing_shape"].is_number_integer())
        breathing_shape = std::max(0, std::min(settings["breathing_shape"].get<int>(), SHAPE_COUNT - 1));
    if(settings.contains("edge_profile") && settings["edge_profile"].is_number_integer())
        edge_profile = NormalizeEdgeProfile(settings["edge_profile"].get<int>());
    if(settings.contains("breath_pulse_pct") && settings["breath_pulse_pct"].is_number_integer())
        breath_pulse_pct = std::max(0, std::min(settings["breath_pulse_pct"].get<int>(), 100));
    if(settings.contains("center_hole_pct") && settings["center_hole_pct"].is_number_integer())
        center_hole_pct = std::max(0, std::min(settings["center_hole_pct"].get<int>(), 100));

    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "BreathingSphereEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "shapeRow", breathing_shape);
            EffectUiSync::setComboIndex(fx, "edgeRow", NormalizeEdgeProfile(edge_profile));
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "breathPulseRow", breath_pulse_pct, pct);
            EffectUiSync::setSliderValue(fx, "centerHoleRow", center_hole_pct, pct);
        }
    }
}
