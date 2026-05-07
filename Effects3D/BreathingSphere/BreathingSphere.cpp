// SPDX-License-Identifier: GPL-2.0-only

#include "BreathingSphere.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"

REGISTER_EFFECT_3D(BreathingSphere);

#include <QGridLayout>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>
#include "../EffectHelpers.h"

namespace
{

float sdRegularPolygonXZ(float px, float pz, float circum_r, int n)
{
    if(n < 3) n = 3;
    float an = TWO_PI / float(n);
    float a = atan2f(pz, px);
    return cosf(floorf(0.5f + a / an) * an - a) * hypotf(px, pz) - circum_r * cosf(0.5f * an);
}

float shapeDistance(float rx, float ry, float rz, int shape, float R, const GridContext3D& grid)
{
    switch(shape)
    {
    default:
    case 0:
        return hypotf(rx, hypotf(ry, rz));
    case 1:
    {
        float gx = fmaxf(fabsf(rx), fabsf(rz));
        return sqrtf(ry * ry + gx * gx);
    }
    case 2:
    {
        float med = fmaxf(grid.width, grid.depth);
        if(med < 1e-4f) med = 1.0f;
        float ax = grid.width / med;
        float az = grid.depth / med;
        float hx = R * ax;
        float hz = R * az;
        float mx = fabsf(rx) / fmaxf(1e-5f, hx);
        float mz = fabsf(rz) / fmaxf(1e-5f, hz);
        float g = fmaxf(mx, mz) * R;
        return sqrtf(ry * ry + g * g);
    }
    case 3:
    case 4:
    {
        int n = (shape == 3) ? 3 : 5;
        float sd = sdRegularPolygonXZ(rx, rz, R, n);
        float an = TWO_PI / float(n);
        float dist_xz = sd + R * cosf(0.5f * an);
        if(dist_xz < 0.0f) dist_xz = 0.0f;
        return sqrtf(ry * ry + dist_xz * dist_xz);
    }
    }
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
    switch(e)
    {
    case EDGE_SMOOTH: return "Smooth";
    case EDGE_SHARP: return "Sharp";
    case EDGE_FEATHERED: return "Feathered";
    case EDGE_RING: return "Crisp ring";
    default: return "Smooth";
    }
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
}


BreathingSphere::~BreathingSphere() = default;


EffectInfo3D BreathingSphere::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Breathing Shape";
    info.effect_description = "Breathing shell or whole-room wave; edge softness, pulse depth, and optional center hole (donut)";
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

    return info;
}


void BreathingSphere::SetupCustomUI(QWidget* parent)
{
    QWidget* outer_w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(outer_w);
    vbox->setContentsMargins(0, 0, 0, 0);
    QWidget* breathing_widget = new QWidget();
    vbox->addWidget(breathing_widget);
    QGridLayout* layout = new QGridLayout(breathing_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    layout->addWidget(new QLabel("Shape:"), row, 0);
    QComboBox* shape_combo = new QComboBox();
    for(int s = 0; s < SHAPE_COUNT; s++) shape_combo->addItem(ShapeName(s));
    shape_combo->setCurrentIndex(std::max(0, std::min(breathing_shape, SHAPE_COUNT - 1)));
    shape_combo->setToolTip("Geometry used for the breathing shell (horizontal footprint + height). Whole room fills the volume with a hue wave that moves with distance from the effect origin.");
    shape_combo->setItemData(SHAPE_SPHERE, "Round shell; distance from origin in 3D.", Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_SQUARE, "Axis-aligned square footprint in XZ with vertical extent.", Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_RECTANGLE, "Rectangle in XZ sized from your grid width vs depth.", Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_TRIANGLE, "Equilateral triangle footprint in XZ.", Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_PENTAGON, "Regular pentagon footprint in XZ.", Qt::ToolTipRole);
    shape_combo->setItemData(SHAPE_WHOLE_ROOM, "No shell: colors sweep through the room as a slow inhale/exhale wave tied to distance from the effect origin.", Qt::ToolTipRole);
    layout->addWidget(shape_combo, row, 1, 1, 2);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        breathing_shape = std::max(0, std::min(idx, SHAPE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Edge:"), row, 0);
    QComboBox* edge_combo = new QComboBox();
    for(int e = 0; e < EDGE_COUNT; e++) edge_combo->addItem(EdgeName(e));
    edge_combo->setCurrentIndex(std::max(0, std::min(edge_profile, EDGE_COUNT - 1)));
    edge_combo->setToolTip("How soft or hard the shell boundary looks. Crisp ring favors a thin bright band.");
    edge_combo->setItemData(EDGE_SMOOTH, "Balanced falloff (default).", Qt::ToolTipRole);
    edge_combo->setItemData(EDGE_SHARP, "Narrow transitions; harder silhouette.", Qt::ToolTipRole);
    edge_combo->setItemData(EDGE_FEATHERED, "Wide, soft glow and gentle edges.", Qt::ToolTipRole);
    edge_combo->setItemData(EDGE_RING, "Thinner bright band toward the outside.", Qt::ToolTipRole);
    layout->addWidget(edge_combo, row, 1, 1, 2);
    connect(edge_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        edge_profile = std::max(0, std::min(idx, EDGE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Breath pulse:"), row, 0);
    QSlider* breath_slider = new QSlider(Qt::Horizontal);
    breath_slider->setRange(0, 100);
    breath_slider->setToolTip("How much the shell grows and shrinks each cycle (0 = almost static, 100 = strong pulse).");
    breath_slider->setValue(breath_pulse_pct);
    QLabel* breath_label = new QLabel(QString::number(breath_pulse_pct) + "%");
    breath_label->setMinimumWidth(40);
    layout->addWidget(breath_slider, row, 1);
    layout->addWidget(breath_label, row, 2);
    connect(breath_slider, &QSlider::valueChanged, this, [this, breath_label](int v){
        breath_pulse_pct = v;
        if(breath_label) breath_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Center hole:"), row, 0);
    QSlider* hole_slider = new QSlider(Qt::Horizontal);
    hole_slider->setRange(0, 100);
    hole_slider->setToolTip("0 = solid through the middle; higher values carve a larger empty core (donut). Ignored in whole-room mode.");
    hole_slider->setValue(center_hole_pct);
    QLabel* hole_label = new QLabel(QString::number(center_hole_pct) + "%");
    hole_label->setMinimumWidth(40);
    layout->addWidget(hole_slider, row, 1);
    layout->addWidget(hole_label, row, 2);
    connect(hole_slider, &QSlider::valueChanged, this, [this, hole_label](int v){
        center_hole_pct = v;
        if(hole_label) hole_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(outer_w);
    strip_cmap_panel->mirrorStateFromEffect(breathing_strip_cmap_on,
                                            breathing_strip_cmap_kernel,
                                            breathing_strip_cmap_rep,
                                            breathing_strip_cmap_unfold,
                                            breathing_strip_cmap_dir,
                                            breathing_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &BreathingSphere::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(outer_w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &BreathingSphere::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(outer_w, parent);
}


void BreathingSphere::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void BreathingSphere::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    breathing_strip_cmap_on = strip_cmap_panel->useStripColormap();
    breathing_strip_cmap_kernel = strip_cmap_panel->kernelId();
    breathing_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    breathing_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    breathing_strip_cmap_dir = strip_cmap_panel->directionDeg();
    breathing_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}


void BreathingSphere::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_BREATHING_SPHERE;
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
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float rel_x = rot.x - origin.x;
    float rel_y = rot.y - origin.y;
    float rel_z = rot.z - origin.z;

    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    progress = CalculateProgress(time * bb.speed_mul);
    const float cmap_phase01 = std::fmod(progress + bb.phase_deg * (1.0f / 360.0f) + 1.0f, 1.0f);
    float strip_p01 = 0.0f;
    if(breathing_strip_cmap_on)
    {
        strip_p01 = SampleStripKernelPalette01(breathing_strip_cmap_kernel,
                                               breathing_strip_cmap_rep,
                                               breathing_strip_cmap_unfold,
                                               breathing_strip_cmap_dir,
                                               cmap_phase01,
                                               time,
                                               grid,
                                               GetNormalizedSize(),
                                               origin,
                                               rot);
    }
    int shape = std::max(0, std::min(breathing_shape, SHAPE_COUNT - 1));
    int edge = std::max(0, std::min(edge_profile, EDGE_COUNT - 1));
    float breath_phase = progress * rate * 0.2f;
    float pulse_strength = 0.45f + 0.55f * (breath_pulse_pct / 100.0f);

    if(shape == SHAPE_WHOLE_ROOM)
    {
        float bounds_r = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
        float diag = fmaxf(1e-4f, bounds_r);
        float dist_norm = hypotf(rel_x, hypotf(rel_y, rel_z)) / diag;
        float inhale = sinf(breath_phase) * pulse_strength;
        float exhale = sinf(breath_phase + 1.2f) * pulse_strength;
        const float tm = std::max(0.25f, bb.tight_mul);
        float wave = sinf(inhale * 3.14159265f * 1.15f - dist_norm * (9.0f + 5.0f * detail) / tm) * pulse_strength;
        float ripple = sinf(breath_phase * 2.1f * bb.speed_mul - dist_norm * TWO_PI * 2.2f / tm + rel_y * 0.02f * detail / tm) * pulse_strength;
        float rush = sinf(exhale * 1.7f * bb.speed_mul + (rel_x + rel_z) * 0.015f * detail / tm) * 0.4f * pulse_strength;

        RGBColor c;
        if(breathing_strip_cmap_on)
        {
            float p01v = ApplyVoxelDriveToPalette01(strip_p01, x, y, z, time, grid);
            c = ResolveStripKernelFinalColor(*this, breathing_strip_cmap_kernel, p01v, breathing_strip_cmap_color_style, time,
                                              rate * 12.0f * bb.speed_mul);
        }
        else if(GetRainbowMode())
        {
            float hue = time * rate * 12.0f * bb.speed_mul + inhale * 110.0f + wave * 175.0f + ripple * 70.0f + rush * 60.0f + progress * 35.0f + bb.phase_deg;
            c = GetRainbowColor(hue);
        }
        else
        {
            float pos = fmodf(0.38f + 0.32f * inhale + 0.24f * wave + 0.12f * ripple + rush * 0.1f + progress * 0.04f, 1.0f);
            if(pos < 0.0f) pos += 1.0f;
            c = GetColorAtPosition(pos);
        }
        float air = 0.78f + 0.22f * (0.5f + 0.5f * sinf(breath_phase * 1.05f)) * (0.7f + 0.3f * pulse_strength);
        unsigned char r = (unsigned char)fminf(255.0f, fmaxf(0.0f, (c & 0xFF) * air));
        unsigned char g = (unsigned char)fminf(255.0f, fmaxf(0.0f, ((c >> 8) & 0xFF) * air));
        unsigned char b = (unsigned char)fminf(255.0f, fmaxf(0.0f, ((c >> 16) & 0xFF) * air));
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    float size_multiplier = GetNormalizedSize();
    float bounds_r = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float base_scale = 0.45f;
    float breath_amp = 0.02f + (breath_pulse_pct / 100.0f) * 0.58f;
    float sphere_radius = bounds_r * base_scale * size_multiplier * (1.0f + breath_amp * sinf(breath_phase));

    float distance = shapeDistance(rel_x, rel_y, rel_z, shape, sphere_radius, grid);
    float R = sphere_radius;

    float ec0 = 0.7f, g_lo = 0.7f, g_hi = 1.3f, gk = 0.5f, rk = 0.22f, ak = 0.12f, fa = 0.42f, fb = 0.58f;
    if(edge == EDGE_SHARP)
    {
        ec0 = 0.92f;
        g_lo = 0.94f;
        g_hi = 1.08f;
        gk = 0.3f;
        rk = 0.12f;
        ak = 0.05f;
        fa = 0.52f;
        fb = 0.48f;
    }
    else if(edge == EDGE_FEATHERED)
    {
        ec0 = 0.52f;
        g_lo = 0.48f;
        g_hi = 1.58f;
        gk = 0.7f;
        rk = 0.26f;
        ak = 0.18f;
        fa = 0.28f;
        fb = 0.72f;
    }
    else if(edge == EDGE_RING)
    {
        ec0 = 0.82f;
        g_lo = 0.8f;
        g_hi = 1.22f;
        gk = 0.88f;
        rk = 0.18f;
        ak = 0.08f;
        fa = 0.38f;
        fb = 0.62f;
    }

    float sphere_intensity;
    const bool use_donut = (center_hole_pct > 0);
    const float hole_frac = center_hole_pct / 100.0f;
    const float r_in = hole_frac * R * 0.9f;

    if(!use_donut)
    {
        float core_intensity = 1.0f - smoothstep(0.0f, R * ec0, distance);
        if(edge == EDGE_RING) core_intensity *= 0.45f;
        float glow_intensity = gk * (1.0f - smoothstep(R * g_lo, R * g_hi, distance));
        float ripple = rk * sinf(distance * (detail / (bounds_r + 0.001f)) * 1.5f / std::max(0.25f, bb.tight_mul) - progress * 2.0f * bb.speed_mul);
        ripple = (ripple + 1.0f) * 0.5f;
        float ambient = ak * (1.0f - smoothstep(0.0f, R * 2.0f, distance));
        sphere_intensity = core_intensity + glow_intensity + ripple * 0.35f + ambient;
        sphere_intensity = fmax(0.0f, fmin(1.0f, sphere_intensity));
        sphere_intensity = fa + fb * sphere_intensity;
    }
    else
    {
        float iw = R * 0.07f;
        if(edge == EDGE_SHARP) iw = R * 0.028f;
        else if(edge == EDGE_FEATHERED) iw = R * 0.18f;
        else if(edge == EDGE_RING) iw = R * 0.036f;

        float o_lo = R * ((edge == EDGE_SHARP) ? 0.96f : (edge == EDGE_FEATHERED) ? 0.52f : (edge == EDGE_RING) ? 0.84f : 0.82f);
        float o_hi = R * ((edge == EDGE_SHARP) ? 1.06f : (edge == EDGE_FEATHERED) ? 1.62f : (edge == EDGE_RING) ? 1.16f : 1.38f);

        float inner_open = smoothstep(r_in - iw, r_in + iw, distance);
        float outer_open = 1.0f - smoothstep(o_lo, o_hi, distance);
        float span_eff = fmaxf(R * 0.92f - r_in, R * 0.08f);
        float u = (distance - r_in) / span_eff;
        u = fmaxf(0.0f, fminf(1.0f, u));
        float bell = sinf(u * 3.14159265f);
        if(edge == EDGE_RING)
        {
            float b2 = bell * bell;
            bell = b2 * (0.35f + 0.65f * bell);
        }
        float shell = inner_open * outer_open * (0.1f + 0.9f * bell);

        float ripple = rk * sinf(distance * (detail / (bounds_r + 0.001f)) * 1.5f / std::max(0.25f, bb.tight_mul) - progress * 2.0f * bb.speed_mul);
    ripple = (ripple + 1.0f) * 0.5f;
        float rip_mix = ripple * 0.38f * inner_open * outer_open;

        float ring_glow = 0.48f * expf(-((distance - R * 0.93f) / (R * 0.13f + 1e-4f)) * ((distance - R * 0.93f) / (R * 0.13f + 1e-4f)));
        if(edge == EDGE_FEATHERED) ring_glow *= 1.25f;
        if(edge == EDGE_SHARP) ring_glow *= 0.65f;
    
        float ambient = ak * (1.0f - smoothstep(0.0f, R * 2.0f, distance)) * inner_open;
    
        sphere_intensity = shell + rip_mix + ring_glow * inner_open + ambient;
    sphere_intensity = fmax(0.0f, fmin(1.0f, sphere_intensity));
        sphere_intensity = fa + fb * sphere_intensity;
    }

    float norm_in_shell;
    if(!use_donut)
        norm_in_shell = distance / (R * 1.12f + 1e-5f);
    else
    {
        float span = fmaxf(R * 0.92f - r_in, 1e-4f);
        norm_in_shell = (distance - r_in) / span;
        norm_in_shell = fmaxf(0.0f, fminf(1.2f, norm_in_shell));
    }

    RGBColor final_color;
    if(breathing_strip_cmap_on)
    {
        float p01v = ApplyVoxelDriveToPalette01(strip_p01, x, y, z, time, grid);
        final_color = ResolveStripKernelFinalColor(*this, breathing_strip_cmap_kernel, p01v, breathing_strip_cmap_color_style, time,
                                                   rate * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        float hue = norm_in_shell * 290.0f * (0.6f + 0.4f * detail) + breath_phase * 72.0f + time * rate * 12.0f * bb.speed_mul + bb.phase_deg;
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
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "breathing_sphere_stratum_layout_mode",
                                           sm,
                                           st,
                                           "breathing_sphere_stratum_band_speed_pct",
                                           "breathing_sphere_stratum_band_tight_pct",
                                           "breathing_sphere_stratum_band_phase_deg");
    j["breathing_shape"] = breathing_shape;
    j["edge_profile"] = edge_profile;
    j["breath_pulse_pct"] = breath_pulse_pct;
    j["center_hole_pct"] = center_hole_pct;
    j["breathing_strip_cmap_on"] = breathing_strip_cmap_on;
    j["breathing_strip_cmap_kernel"] = breathing_strip_cmap_kernel;
    j["breathing_strip_cmap_rep"] = breathing_strip_cmap_rep;
    j["breathing_strip_cmap_unfold"] = breathing_strip_cmap_unfold;
    j["breathing_strip_cmap_dir"] = breathing_strip_cmap_dir;
    j["breathing_strip_cmap_color_style"] = breathing_strip_cmap_color_style;
    return j;
}

void BreathingSphere::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "breathing_sphere_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "breathing_sphere_stratum_band_speed_pct",
                                            "breathing_sphere_stratum_band_tight_pct",
                                            "breathing_sphere_stratum_band_phase_deg");
    if(settings.contains("breathing_shape") && settings["breathing_shape"].is_number_integer())
        breathing_shape = std::max(0, std::min(settings["breathing_shape"].get<int>(), SHAPE_COUNT - 1));
    else if(settings.contains("breathing_mode") && settings["breathing_mode"].is_number_integer())
    {
        int old = settings["breathing_mode"].get<int>();
        breathing_shape = (old == 1) ? SHAPE_WHOLE_ROOM : SHAPE_SPHERE;
    }
    if(settings.contains("edge_profile") && settings["edge_profile"].is_number_integer())
        edge_profile = std::max(0, std::min(settings["edge_profile"].get<int>(), EDGE_COUNT - 1));
    if(settings.contains("breath_pulse_pct") && settings["breath_pulse_pct"].is_number_integer())
        breath_pulse_pct = std::max(0, std::min(settings["breath_pulse_pct"].get<int>(), 100));
    if(settings.contains("center_hole_pct") && settings["center_hole_pct"].is_number_integer())
        center_hole_pct = std::max(0, std::min(settings["center_hole_pct"].get<int>(), 100));
    if(settings.contains("breathing_strip_cmap_on") && settings["breathing_strip_cmap_on"].is_boolean())
        breathing_strip_cmap_on = settings["breathing_strip_cmap_on"].get<bool>();
    else if(settings.contains("breathing_strip_cmap_on") && settings["breathing_strip_cmap_on"].is_number_integer())
        breathing_strip_cmap_on = settings["breathing_strip_cmap_on"].get<int>() != 0;
    if(settings.contains("breathing_strip_cmap_kernel") && settings["breathing_strip_cmap_kernel"].is_number_integer())
        breathing_strip_cmap_kernel = std::clamp(settings["breathing_strip_cmap_kernel"].get<int>(), 0, StripShellKernelCount() - 1);
    if(settings.contains("breathing_strip_cmap_rep") && settings["breathing_strip_cmap_rep"].is_number())
        breathing_strip_cmap_rep = std::max(1.0f, std::min(40.0f, settings["breathing_strip_cmap_rep"].get<float>()));
    if(settings.contains("breathing_strip_cmap_unfold") && settings["breathing_strip_cmap_unfold"].is_number_integer())
        breathing_strip_cmap_unfold = std::clamp(settings["breathing_strip_cmap_unfold"].get<int>(), 0,
                                               (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    if(settings.contains("breathing_strip_cmap_dir") && settings["breathing_strip_cmap_dir"].is_number())
        breathing_strip_cmap_dir = std::fmod(settings["breathing_strip_cmap_dir"].get<float>() + 360.0f, 360.0f);
    if(settings.contains("breathing_strip_cmap_color_style") && settings["breathing_strip_cmap_color_style"].is_number_integer())
        breathing_strip_cmap_color_style = std::clamp(settings["breathing_strip_cmap_color_style"].get<int>(), 0, 2);
    else
        breathing_strip_cmap_color_style = GetRainbowMode() ? 2 : 1;

    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(breathing_strip_cmap_on,
                                                breathing_strip_cmap_kernel,
                                                breathing_strip_cmap_rep,
                                                breathing_strip_cmap_unfold,
                                                breathing_strip_cmap_dir,
                                                breathing_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
