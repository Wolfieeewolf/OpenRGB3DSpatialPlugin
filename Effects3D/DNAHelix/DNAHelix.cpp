// SPDX-License-Identifier: GPL-2.0-only

#include "DNAHelix.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include "../EffectHelpers.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

static inline void dna_strand_point_circle(float a, float r, float& ox, float& oz)
{
    ox = r * cosf(a);
    oz = r * sinf(a);
}

/** Axis-aligned square perimeter (cube silhouette), same period as a circle. */
static inline void dna_strand_point_square(float a, float r, float& ox, float& oz)
{
    float x = cosf(a);
    float z = sinf(a);
    float m = fmaxf(fabsf(x), fabsf(z));
    if(m < 1e-6f)
    {
        ox = oz = 0.0f;
        return;
    }
    ox = r * x / m;
    oz = r * z / m;
}

/** n=4 superellipse (squircle). */
static inline void dna_strand_point_squircle(float a, float r, float& ox, float& oz)
{
    float ca = cosf(a);
    float sa = sinf(a);
    float sx = (ca >= 0.0f) ? 1.0f : -1.0f;
    float sz = (sa >= 0.0f) ? 1.0f : -1.0f;
    ox = r * sx * sqrtf(fabsf(ca));
    oz = r * sz * sqrtf(fabsf(sa));
}

static inline void dna_strand_point(int shape_mode, float a, float r, float& ox, float& oz)
{
    switch(shape_mode)
    {
        case 1:
            dna_strand_point_square(a, r, ox, oz);
            break;
        case 2:
            dna_strand_point_squircle(a, r, ox, oz);
            break;
        case 0:
        case 3:
        case 4:
        default:
            dna_strand_point_circle(a, r, ox, oz);
            break;
    }
}

} // namespace

DNAHelix::DNAHelix(QWidget* parent) : SpatialEffect3D(parent)
{
    radius_slider = nullptr;
    radius_label = nullptr;

    std::vector<RGBColor> dna_colors = {
        0x000000FF,
        0x0000FFFF,
        0x0000FF00,
        0x00FF0000
    };
    if(GetColors().empty())
    {
        SetColors(dna_colors);
    }
    SetFrequency(50);
    SetRainbowMode(false);
}

DNAHelix::~DNAHelix() = default;

EffectInfo3D DNAHelix::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "DNA Helix";
    info.effect_description =
        "Double helix with base pairs; circle, square, or squircle path; optional vertical drift, ring pulse "
        "from the middle outward/inward, flat disc or stacked horizontal bands; sized to fill the room width/depth";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_DNA_HELIX;
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

    info.default_speed_scale = 10.0f;
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

void DNAHelix::SetupCustomUI(QWidget* parent)
{
    QWidget* outer_w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(outer_w);
    vbox->setContentsMargins(0, 0, 0, 0);
    QWidget* dna_widget = new QWidget();
    vbox->addWidget(dna_widget);
    QGridLayout* layout = new QGridLayout(dna_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Helix Radius:"), 0, 0);
    radius_slider = new QSlider(Qt::Horizontal);
    radius_slider->setRange(20, 200);
    radius_slider->setToolTip("How far strands sit from the axis as a fraction of the room width/depth (pairs with Size).");
    radius_slider->setValue((int)helix_radius);
    layout->addWidget(radius_slider, 0, 1);
    radius_label = new QLabel(QString::number((int)helix_radius));
    radius_label->setMinimumWidth(30);
    layout->addWidget(radius_label, 0, 2);

    int row = 1;
    layout->addWidget(new QLabel("Cross-section:"), row, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Circle");
    shape_combo->addItem("Square (cube outline)");
    shape_combo->addItem("Squircle");
    shape_combo->addItem("Flat disc");
    shape_combo->addItem("Stacked planes");
    shape_combo->setCurrentIndex(std::clamp(helix_shape_mode, 0, 4));
    shape_combo->setToolTip("Strand path in the horizontal plane: round, square, squircle, thin horizontal slab, or discrete Y bands.");
    layout->addWidget(shape_combo, row, 1, 1, 2);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        helix_shape_mode = std::clamp(idx, 0, 4);
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Vertical drift:"), row, 0);
    vertical_drift_slider = new QSlider(Qt::Horizontal);
    vertical_drift_slider->setRange(0, 100);
    vertical_drift_slider->setToolTip("Slides the helix along height over time (uses Speed).");
    vertical_drift_slider->setValue(vertical_drift_pct);
    vertical_drift_label = new QLabel(QString::number(vertical_drift_pct) + "%");
    vertical_drift_label->setMinimumWidth(40);
    layout->addWidget(vertical_drift_slider, row, 1);
    layout->addWidget(vertical_drift_label, row, 2);
    connect(vertical_drift_slider, &QSlider::valueChanged, this, [this](int v){
        vertical_drift_pct = v;
        if(vertical_drift_label) vertical_drift_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Ring pulse:"), row, 0);
    ring_pulse_slider = new QSlider(Qt::Horizontal);
    ring_pulse_slider->setRange(0, 100);
    ring_pulse_slider->setToolTip("Breathing of each horizontal ring: radius modulates from the middle of the grid outward or inward.");
    ring_pulse_slider->setValue(ring_pulse_pct);
    ring_pulse_label = new QLabel(QString::number(ring_pulse_pct) + "%");
    ring_pulse_label->setMinimumWidth(40);
    layout->addWidget(ring_pulse_slider, row, 1);
    layout->addWidget(ring_pulse_label, row, 2);
    connect(ring_pulse_slider, &QSlider::valueChanged, this, [this](int v){
        ring_pulse_pct = v;
        if(ring_pulse_label) ring_pulse_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Pulse direction:"), row, 0);
    pulse_dir_combo = new QComboBox();
    pulse_dir_combo->addItem("Off");
    pulse_dir_combo->addItem("Wave outward");
    pulse_dir_combo->addItem("Wave inward");
    pulse_dir_combo->setCurrentIndex(std::clamp(ring_pulse_dir, 0, 2));
    pulse_dir_combo->setToolTip("Traveling pulse along height: from center toward floor/ceiling, or the reverse.");
    layout->addWidget(pulse_dir_combo, row, 1, 1, 2);
    connect(pulse_dir_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        ring_pulse_dir = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });
    row++;

    layout->addWidget(new QLabel("Stacked bands:"), row, 0);
    plane_count_slider = new QSlider(Qt::Horizontal);
    plane_count_slider->setRange(2, 24);
    plane_count_slider->setToolTip("For Stacked planes: how many discrete horizontal DNA layers.");
    plane_count_slider->setValue(std::clamp(plane_layers, 2, 24));
    plane_count_label = new QLabel(QString::number(plane_layers));
    plane_count_label->setMinimumWidth(30);
    layout->addWidget(plane_count_slider, row, 1);
    layout->addWidget(plane_count_label, row, 2);
    connect(plane_count_slider, &QSlider::valueChanged, this, [this](int v){
        plane_layers = v;
        if(plane_count_label) plane_count_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    row++;

    strip_cmap_panel = new StripKernelColormapPanel(outer_w);
    strip_cmap_panel->mirrorStateFromEffect(dnahelix_strip_cmap_on,
                                            dnahelix_strip_cmap_kernel,
                                            dnahelix_strip_cmap_rep,
                                            dnahelix_strip_cmap_unfold,
                                            dnahelix_strip_cmap_dir,
                                            dnahelix_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &DNAHelix::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(outer_w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &DNAHelix::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(outer_w, parent);

    connect(radius_slider, &QSlider::valueChanged, this, &DNAHelix::OnDNAParameterChanged);
}

void DNAHelix::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_DNA_HELIX;
}

void DNAHelix::OnDNAParameterChanged()
{
    if(radius_slider)
    {
        helix_radius = (unsigned int)radius_slider->value();
        if(radius_label) radius_label->setText(QString::number((int)helix_radius));
    }
    emit ParametersChanged();
}

void DNAHelix::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void DNAHelix::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    dnahelix_strip_cmap_on = strip_cmap_panel->useStripColormap();
    dnahelix_strip_cmap_kernel = strip_cmap_panel->kernelId();
    dnahelix_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    dnahelix_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    dnahelix_strip_cmap_dir = strip_cmap_panel->directionDeg();
    dnahelix_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}


RGBColor DNAHelix::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float y_stratum = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(y_stratum, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    const float progress_use = CalculateProgress(time) * bb.speed_mul + bb.phase_deg * (1.0f / 360.0f);

    float size_multiplier = GetNormalizedSize();
    float freq_scale = detail * bb.tight_mul * 4.0f / fmax(0.1f, size_multiplier);

    EffectGridAxisHalfExtents e_room = MakeEffectGridAxisHalfExtents(grid, 1.0f);
    float h_span = std::min(e_room.hw, e_room.hd);
    float radius_frac = (helix_radius / 200.0f);
    radius_frac = std::clamp(radius_frac, 0.08f, 1.25f);
    float radius_scale = h_span * radius_frac * size_multiplier;

    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_z = rotated_pos.z - origin.z;

    float radial_distance = sqrtf(rot_rel_x * rot_rel_x + rot_rel_z * rot_rel_z);
    float angle = atan2f(rot_rel_z, rot_rel_x);

    float y_norm = y_stratum;
    if(vertical_drift_pct > 0)
    {
        float drift = (vertical_drift_pct / 100.0f) * time * GetScaledSpeed() * 0.05f;
        y_norm = y_stratum + drift;
        y_norm = y_norm - floorf(y_norm);
    }

    const int shape_m = std::clamp(helix_shape_mode, 0, 4);
    float coord_along_helix;
    float y_plane_weight = 1.0f;

    switch(shape_m)
    {
        case 3: // Flat disc — twist follows azimuth; brightness in a thin horizontal slab
            coord_along_helix = angle * freq_scale * 1.15f + progress_use + y_norm * freq_scale * 0.06f;
            y_plane_weight = expf(-powf((y_norm - 0.5f) * 11.0f, 2.0f));
            break;
        case 4: // Stacked planes — discrete horizontal bands
        {
            int n = std::clamp(plane_layers, 2, 24);
            float band_idx = floorf(y_norm * (float)n);
            band_idx = fminf(band_idx, (float)(n - 1));
            float band_center = (band_idx + 0.5f) / (float)n;
            float band_half = (0.5f / (float)n) * 0.92f;
            y_plane_weight = 1.0f - smoothstep(band_half * 0.35f, band_half, fabsf(y_norm - band_center));
            coord_along_helix = (band_idx / fmax(1.0f, (float)(n - 1))) * freq_scale + progress_use;
            break;
        }
        default:
            coord_along_helix = y_norm * freq_scale + progress_use;
            break;
    }

    float helix_height = coord_along_helix;
    float coord1 = rot_rel_x;
    float coord2_xz = rot_rel_z;

    if(ring_pulse_pct > 0 && ring_pulse_dir != 0)
    {
        float ring_from_center = fabsf(y_norm - 0.5f) * 2.0f;
        float amp = (ring_pulse_pct / 100.0f) * 0.42f;
        float spd = time * GetScaledFrequency() * 0.35f * bb.speed_mul;
        float dir = (ring_pulse_dir == 1) ? 1.0f : -1.0f;
        float ph = spd - ring_from_center * 7.0f * dir;
        radius_scale *= 1.0f + amp * sinf(ph);
    }

    const int path_shape = (shape_m == 3 || shape_m == 4) ? 0 : shape_m;

    float helix1_angle = angle + helix_height;
    float helix1_c1, helix1_c2;
    dna_strand_point(path_shape, helix1_angle, radius_scale, helix1_c1, helix1_c2);
    float helix1_distance = sqrtf((coord1 - helix1_c1) * (coord1 - helix1_c1) +
                                  (coord2_xz - helix1_c2) * (coord2_xz - helix1_c2));

    float helix2_angle = angle + helix_height + 3.14159f;
    float helix2_c1, helix2_c2;
    dna_strand_point(path_shape, helix2_angle, radius_scale, helix2_c1, helix2_c2);
    float helix2_distance = sqrtf((coord1 - helix2_c1) * (coord1 - helix2_c1) +
                                  (coord2_xz - helix2_c2) * (coord2_xz - helix2_c2));

    float strand_core_thickness = (6.0f + radius_scale * 0.25f) / std::max(0.25f, bb.tight_mul);
    float strand_glow_thickness = (16.0f + radius_scale * 0.5f) / std::max(0.25f, bb.tight_mul);

    float helix1_core = 1.0f - smoothstep(0.0f, strand_core_thickness, helix1_distance);
    float helix2_core = 1.0f - smoothstep(0.0f, strand_core_thickness, helix2_distance);
    float helix1_glow = (1.0f - smoothstep(strand_core_thickness, strand_glow_thickness, helix1_distance)) * 0.5f;
    float helix2_glow = (1.0f - smoothstep(strand_core_thickness, strand_glow_thickness, helix2_distance)) * 0.5f;

    float helix1_intensity = helix1_core + helix1_glow;
    float helix2_intensity = helix2_core + helix2_glow;

    float base_pair_frequency = freq_scale * 1.2f;
    float base_pair_phase = fmod(coord_along_helix * base_pair_frequency + progress_use * 0.5f, 6.28318f);
    float base_pair_active = exp(-fmod(base_pair_phase, 6.28318f / 3.0f) * 8.0f);
    float base_pair_connection = 0.0f;

    if(base_pair_active > 0.1f && radial_distance < radius_scale * 1.8f)
    {
        float rung_distance = fabs(radial_distance - radius_scale);
        float rung_thickness = (1.5f + radius_scale * 0.2f) / std::max(0.25f, bb.tight_mul);
        float rung_intensity = 1.0f - smoothstep(0.0f, rung_thickness, rung_distance);
        float rung_glow = (1.0f - smoothstep(rung_thickness, rung_thickness * 2.0f, rung_distance)) * 0.4f;
        base_pair_connection = (rung_intensity + rung_glow) * base_pair_active;
    }

    float groove_angle = fmod(angle - helix_height * 0.5f, 6.28318f);
    float major_groove = exp(-fabs(groove_angle - 3.14159f) * 2.0f) * 0.15f;
    float minor_groove = exp(-fabs(groove_angle) * 3.0f) * 0.1f;
    float groove_effect = 1.0f - (major_groove + minor_groove);

    float strand_intensity = fmax(helix1_intensity, helix2_intensity);
    
    float ambient_glow = 0.08f * (1.0f - fmin(1.0f, radial_distance / (radius_scale * 4.0f)));
    
    float total_intensity = (strand_intensity + base_pair_connection) * groove_effect;
    float energy_pulse = 0.15f * sinf(helix_height * 4.0f - progress_use * 3.0f) * strand_intensity;
    total_intensity = total_intensity + energy_pulse + ambient_glow;
    total_intensity *= y_plane_weight;
    total_intensity = fmax(0.0f, fmin(1.0f, total_intensity * 1.3f));

    const float dna_phase01 = std::fmod(progress_use + 1.0f, 1.0f);
    float strip_p01 = 0.0f;
    if(dnahelix_strip_cmap_on)
    {
        strip_p01 = SampleStripKernelPalette01(dnahelix_strip_cmap_kernel,
                                               dnahelix_strip_cmap_rep,
                                               dnahelix_strip_cmap_unfold,
                                               dnahelix_strip_cmap_dir,
                                               dna_phase01,
                                               time,
                                               grid,
                                               size_multiplier,
                                               origin,
                                               rotated_pos);
    }

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = helix_height * 50.0f + angle * 20.0f + time * rate * 12.0f * bb.speed_mul + bb.phase_deg;
        if(dnahelix_strip_cmap_on)
            hue = strip_p01 * 360.0f + time * rate * 12.0f * bb.speed_mul;
        if(base_pair_connection > 0.3f)
        {
            hue += 180.0f;
        }
        final_color = GetRainbowColor(hue);
    }
    else
    {
        if(base_pair_connection > strand_intensity * 0.5f)
        {
            float position = (GetColors().size() > 1) ? 0.7f : 0.5f;
            float pos = fmodf(position + time * rate * 0.02f, 1.0f);
            if(dnahelix_strip_cmap_on)
                pos = strip_p01;
            if(pos < 0.0f) pos += 1.0f;
            final_color = GetColorAtPosition(pos);
        }
        else
        {
            float position = fmod(helix_height * 0.3f, 1.0f);
            float pos = fmodf(position + time * rate * 0.02f, 1.0f);
            if(dnahelix_strip_cmap_on)
                pos = strip_p01;
            if(pos < 0.0f) pos += 1.0f;
            final_color = GetColorAtPosition(pos);
        }
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * total_intensity);
    g = (unsigned char)(g * total_intensity);
    b = (unsigned char)(b * total_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json DNAHelix::SaveSettings() const
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
                                           "dna_helix_stratum_layout_mode",
                                           sm,
                                           st,
                                           "dna_helix_stratum_band_speed_pct",
                                           "dna_helix_stratum_band_tight_pct",
                                           "dna_helix_stratum_band_phase_deg");
    j["helix_radius"] = helix_radius;
    j["dna_helix_shape"] = helix_shape_mode;
    j["dna_helix_vertical_drift_pct"] = vertical_drift_pct;
    j["dna_helix_ring_pulse_pct"] = ring_pulse_pct;
    j["dna_helix_ring_pulse_dir"] = ring_pulse_dir;
    j["dna_helix_plane_layers"] = plane_layers;
    StripColormapSaveJson(j, "dnahelix", dnahelix_strip_cmap_on, dnahelix_strip_cmap_kernel, dnahelix_strip_cmap_rep,
                          dnahelix_strip_cmap_unfold, dnahelix_strip_cmap_dir,
                          dnahelix_strip_cmap_color_style);
    return j;
}

void DNAHelix::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "dna_helix_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "dna_helix_stratum_band_speed_pct",
                                            "dna_helix_stratum_band_tight_pct",
                                            "dna_helix_stratum_band_phase_deg");
    StripColormapLoadJson(settings, "dnahelix", dnahelix_strip_cmap_on, dnahelix_strip_cmap_kernel, dnahelix_strip_cmap_rep,
                          dnahelix_strip_cmap_unfold, dnahelix_strip_cmap_dir,
                          dnahelix_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(dnahelix_strip_cmap_on,
                                                dnahelix_strip_cmap_kernel,
                                                dnahelix_strip_cmap_rep,
                                                dnahelix_strip_cmap_unfold,
                                                dnahelix_strip_cmap_dir,
                                                dnahelix_strip_cmap_color_style);
    }
    if(settings.contains("helix_radius") && settings["helix_radius"].is_number())
    {
        int hr = settings["helix_radius"].get<int>();
        helix_radius = (unsigned int)std::clamp(hr, 1, 500);
    }
    if(settings.contains("dna_helix_shape") && settings["dna_helix_shape"].is_number_integer())
        helix_shape_mode = std::clamp(settings["dna_helix_shape"].get<int>(), 0, 4);
    if(settings.contains("dna_helix_vertical_drift_pct") && settings["dna_helix_vertical_drift_pct"].is_number_integer())
        vertical_drift_pct = std::clamp(settings["dna_helix_vertical_drift_pct"].get<int>(), 0, 100);
    if(settings.contains("dna_helix_ring_pulse_pct") && settings["dna_helix_ring_pulse_pct"].is_number_integer())
        ring_pulse_pct = std::clamp(settings["dna_helix_ring_pulse_pct"].get<int>(), 0, 100);
    if(settings.contains("dna_helix_ring_pulse_dir") && settings["dna_helix_ring_pulse_dir"].is_number_integer())
        ring_pulse_dir = std::clamp(settings["dna_helix_ring_pulse_dir"].get<int>(), 0, 2);
    if(settings.contains("dna_helix_plane_layers") && settings["dna_helix_plane_layers"].is_number_integer())
        plane_layers = std::clamp(settings["dna_helix_plane_layers"].get<int>(), 2, 24);

    if(radius_slider) radius_slider->setValue((int)helix_radius);
    if(shape_combo) shape_combo->setCurrentIndex(helix_shape_mode);
    if(vertical_drift_slider)
    {
        vertical_drift_slider->setValue(vertical_drift_pct);
        if(vertical_drift_label) vertical_drift_label->setText(QString::number(vertical_drift_pct) + "%");
    }
    if(ring_pulse_slider)
    {
        ring_pulse_slider->setValue(ring_pulse_pct);
        if(ring_pulse_label) ring_pulse_label->setText(QString::number(ring_pulse_pct) + "%");
    }
    if(pulse_dir_combo) pulse_dir_combo->setCurrentIndex(ring_pulse_dir);
    if(plane_count_slider)
    {
        plane_count_slider->setValue(plane_layers);
        if(plane_count_label) plane_count_label->setText(QString::number(plane_layers));
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(DNAHelix);
