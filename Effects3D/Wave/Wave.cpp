// SPDX-License-Identifier: GPL-2.0-only

#include "Wave.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_EFFECT_3D(Wave);

const char* Wave::ModeName(int m)
{
    switch(m) {
    case MODE_LINE: return "Wave line";
    case MODE_SURFACE: return "Wave surface";
    default: return "Wave line";
    }
}

const char* Wave::WaveStyleName(int s)
{
    switch(s) {
    case STYLE_SINUS: return "Sinus (Mega-Cube)";
    case STYLE_RADIAL: return "Radial (concentric)";
    case STYLE_LINEAR: return "Linear (flat wave)";
    case STYLE_PACIFICA: return "Pacifica (ocean)";
    case STYLE_GRADIENT: return "Gradient wave";
    default: return "Sinus";
    }
}

Wave::Wave(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(50);
    SetRainbowMode(true);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

Wave::~Wave() = default;

EffectInfo3D Wave::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 3;
    info.effect_name = "Wave";
    info.effect_description =
        "Wave line or 3D surface; optional floor/mid/ceiling band tuning; room mapper and voxel drive on output";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_WAVE;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    info.default_speed_scale = 400.0f;
    info.default_frequency_scale = 10.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;

    return info;
}

void Wave::SetupCustomUI(QWidget* parent)
{
    QWidget* wave_widget = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(wave_widget);
    main_layout->setContentsMargins(0, 0, 0, 0);

    QGridLayout* top = new QGridLayout();
    top->addWidget(new QLabel("Style:"), 0, 0);
    style_combo = new QComboBox();
    for(int m = 0; m < MODE_COUNT; m++) style_combo->addItem(ModeName(m));
    style_combo->setCurrentIndex(std::max(0, std::min(mode, MODE_COUNT - 1)));
    style_combo->setToolTip(
        "Line: traveling fronts using Shape below (good on perimeters). "
        "Surface: a heightfield-style wave across the horizontal span.");
    style_combo->setItemData(0,
        "Traveling fronts—circles, squares, lines, or diagonal bands.",
        Qt::ToolTipRole);
    style_combo->setItemData(1,
        "3D surface waves; tune Wave style, direction, and edge fade for sparse grids.",
        Qt::ToolTipRole);
    top->addWidget(style_combo, 0, 1);
    main_layout->addLayout(top);

    connect(style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave::OnModeChanged);

    line_controls = new QWidget();
    QGridLayout* line_layout = new QGridLayout(line_controls);
    line_layout->setContentsMargins(0, 0, 0, 0);
    line_layout->addWidget(new QLabel("Shape:"), 0, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Circles");
    shape_combo->addItem("Squares");
    shape_combo->addItem("Lines");
    shape_combo->addItem("Diagonal");
    shape_combo->setCurrentIndex(shape_type);
    shape_combo->setToolTip("Wave front geometry in line mode (horizontal plane).");
    shape_combo->setItemData(0, "Circular rings from the effect origin in XZ.", Qt::ToolTipRole);
    shape_combo->setItemData(1, "Square rings—sharper corners than circles.", Qt::ToolTipRole);
    shape_combo->setItemData(2, "Parallel bands; strong on opposing walls.", Qt::ToolTipRole);
    shape_combo->setItemData(3, "Diagonal bands—readable in corners and along two walls.", Qt::ToolTipRole);
    line_layout->addWidget(shape_combo, 0, 1);
    line_layout->addWidget(new QLabel("Edge:"), 1, 0);
    edge_shape_combo = new QComboBox();
    edge_shape_combo->addItem("Round");
    edge_shape_combo->addItem("Sharp");
    edge_shape_combo->addItem("Square");
    edge_shape_combo->setCurrentIndex(std::clamp(edge_shape, 0, 2));
    edge_shape_combo->setToolTip("Cross-section of the traveling band in line mode.");
    edge_shape_combo->setItemData(0, "Soft cosine falloff at the band edges.", Qt::ToolTipRole);
    edge_shape_combo->setItemData(1, "Hard edge—crisp on-off band.", Qt::ToolTipRole);
    edge_shape_combo->setItemData(2, "Flat-top band with steep sides.", Qt::ToolTipRole);
    line_layout->addWidget(edge_shape_combo, 1, 1);
    line_layout->addWidget(new QLabel("Thickness:"), 2, 0);
    thickness_slider = new QSlider(Qt::Horizontal);
    thickness_slider->setRange(5, 100);
    thickness_slider->setValue(wave_thickness);
    thickness_slider->setToolTip("Wave band thickness (higher = wider band).");
    line_layout->addWidget(thickness_slider, 2, 1);
    thickness_label = new QLabel(QString::number(wave_thickness));
    line_layout->addWidget(thickness_label, 2, 2);
    main_layout->addWidget(line_controls);

    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave::OnWaveParameterChanged);
    connect(edge_shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave::OnWaveParameterChanged);
    connect(thickness_slider, &QSlider::valueChanged, this, [this](int v) {
        wave_thickness = v;
        if(thickness_label) thickness_label->setText(QString::number(wave_thickness));
        emit ParametersChanged();
    });

    surface_controls = new QWidget();
    QGridLayout* surf_layout = new QGridLayout(surface_controls);
    surf_layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;
    surf_layout->addWidget(new QLabel("Wave style:"), row, 0);
    surface_style_combo = new QComboBox();
    for(int s = 0; s < STYLE_COUNT; s++) surface_style_combo->addItem(WaveStyleName(s));
    surface_style_combo->setCurrentIndex(std::max(0, std::min(wave_style, STYLE_COUNT - 1)));
    surface_style_combo->setToolTip(
        "How the surface height is synthesized. Radial reads well from above; Linear follows Wave direction.");
    surface_style_combo->setItemData(0, "Classic mega-cube style sinusoid in radius and travel.", Qt::ToolTipRole);
    surface_style_combo->setItemData(1, "Concentric ripples from the horizontal center.", Qt::ToolTipRole);
    surface_style_combo->setItemData(2, "Plane wave along the direction slider.", Qt::ToolTipRole);
    surface_style_combo->setItemData(3, "Layered ocean-like motion with softer peaks.", Qt::ToolTipRole);
    surface_style_combo->setItemData(4, "Smoother gradient roll without sharp crests.", Qt::ToolTipRole);
    surf_layout->addWidget(surface_style_combo, row, 1, 1, 2);
    row++;
    surf_layout->addWidget(new QLabel("Surface thickness:"), row, 0);
    surface_thick_slider = new QSlider(Qt::Horizontal);
    surface_thick_slider->setRange(2, 100);
    surface_thick_slider->setValue((int)(surface_thickness * 100.0f));
    surface_thick_label = new QLabel(QString::number((int)(surface_thickness * 100)) + "%");
    surface_thick_label->setMinimumWidth(36);
    surf_layout->addWidget(surface_thick_slider, row, 1);
    surf_layout->addWidget(surface_thick_label, row, 2);
    row++;
    surf_layout->addWidget(new QLabel("Wave frequency:"), row, 0);
    surface_freq_slider = new QSlider(Qt::Horizontal);
    surface_freq_slider->setRange(3, 30);
    surface_freq_slider->setValue((int)(wave_frequency * 10.0f));
    surface_freq_label = new QLabel(QString::number(wave_frequency, 'f', 1));
    surface_freq_label->setMinimumWidth(36);
    surf_layout->addWidget(surface_freq_slider, row, 1);
    surf_layout->addWidget(surface_freq_label, row, 2);
    row++;
    surf_layout->addWidget(new QLabel("Wave amplitude:"), row, 0);
    surface_amp_slider = new QSlider(Qt::Horizontal);
    surface_amp_slider->setRange(20, 200);
    surface_amp_slider->setValue((int)(wave_amplitude * 100.0f));
    surface_amp_label = new QLabel(QString::number((int)(wave_amplitude * 100)) + "%");
    surface_amp_label->setMinimumWidth(36);
    surf_layout->addWidget(surface_amp_slider, row, 1);
    surf_layout->addWidget(surface_amp_label, row, 2);
    row++;
    surf_layout->addWidget(new QLabel("Wave direction:"), row, 0);
    surface_dir_slider = new QSlider(Qt::Horizontal);
    surface_dir_slider->setRange(0, 360);
    surface_dir_slider->setValue((int)wave_direction_deg);
    surface_dir_label = new QLabel(QString::number((int)wave_direction_deg) + "\u00B0");
    surface_dir_label->setMinimumWidth(36);
    surf_layout->addWidget(surface_dir_slider, row, 1);
    surf_layout->addWidget(surface_dir_label, row, 2);
    row++;
    surf_layout->addWidget(new QLabel("Edge fade:"), row, 0);
    surface_edge_fade_slider = new QSlider(Qt::Horizontal);
    surface_edge_fade_slider->setRange(0, 100);
    surface_edge_fade_slider->setValue((int)surface_edge_fade);
    surface_edge_fade_slider->setToolTip("Softens the wave toward the horizontal room edges (helps wide or large grid layouts).");
    surface_edge_fade_label = new QLabel(QString::number((int)surface_edge_fade) + "%");
    surface_edge_fade_label->setMinimumWidth(36);
    surf_layout->addWidget(surface_edge_fade_slider, row, 1);
    surf_layout->addWidget(surface_edge_fade_label, row, 2);

    connect(surface_style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        wave_style = std::max(0, std::min(idx, STYLE_COUNT - 1));
        emit ParametersChanged();
    });
    connect(surface_thick_slider, &QSlider::valueChanged, this, [this](int v){
        surface_thickness = v / 100.0f;
        if(surface_thick_label) surface_thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    connect(surface_freq_slider, &QSlider::valueChanged, this, [this](int v){
        wave_frequency = v / 10.0f;
        if(surface_freq_label) surface_freq_label->setText(QString::number(wave_frequency, 'f', 1));
        emit ParametersChanged();
    });
    connect(surface_amp_slider, &QSlider::valueChanged, this, [this](int v){
        wave_amplitude = v / 100.0f;
        if(surface_amp_label) surface_amp_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    connect(surface_dir_slider, &QSlider::valueChanged, this, [this](int v){
        wave_direction_deg = (float)v;
        if(surface_dir_label) surface_dir_label->setText(QString::number(v) + "\u00B0");
        emit ParametersChanged();
    });

    main_layout->addWidget(surface_controls);

    if(line_controls) line_controls->setVisible(mode == MODE_LINE);
    if(surface_controls) surface_controls->setVisible(mode == MODE_SURFACE);

    stratum_panel = new StratumBandPanel(wave_widget);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    main_layout->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &Wave::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(wave_widget, parent);
}

void Wave::OnModeChanged()
{
    if(style_combo)
        mode = std::max(0, std::min(style_combo->currentIndex(), MODE_COUNT - 1));
    if(line_controls) line_controls->setVisible(mode == MODE_LINE);
    if(surface_controls) surface_controls->setVisible(mode == MODE_SURFACE);
    emit ParametersChanged();
}

void Wave::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void Wave::OnWaveParameterChanged()
{
    if(shape_combo) shape_type = shape_combo->currentIndex();
    if(edge_shape_combo) edge_shape = std::clamp(edge_shape_combo->currentIndex(), 0, 2);
    if(thickness_slider) wave_thickness = thickness_slider->value();
    emit ParametersChanged();
}

void Wave::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

float Wave::smoothstep(float edge0, float edge1, float x) const
{
    float t = (x - edge0) / (std::max(0.0001f, edge1 - edge0));
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}


RGBColor Wave::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    if(mode == MODE_SURFACE)
    {
        Vector3D rot = TransformPointByRotation(x, y, z, origin);
        float coord_y01 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
        SpatialLayerCore::MapperSettings strat_map_s;
        EffectStratumBlend::InitStratumBreaks(strat_map_s);
        float swt[3];
        EffectStratumBlend::WeightsForYNorm(coord_y01, strat_map_s, swt);
        const EffectStratumBlend::BandBlendScalars bb =
            EffectStratumBlend::BlendBands(stratum_layout_mode, swt, stratum_tuning_);

        float progress_val = CalculateProgress(time) * bb.speed_mul;
        float phase = progress_val * (float)(2.0 * M_PI);
        float phase_rad = bb.phase_deg * (float)(M_PI / 180.0f);
        float travel = phase + phase_rad;
        float scale_eff = std::max(0.05f, GetNormalizedScale());
        float sw = grid.width * 0.5f * scale_eff;
        float sh = grid.height * 0.5f * scale_eff;
        float sd = grid.depth * 0.5f * scale_eff;
        if(sw < 1e-5f) sw = 1.0f;
        if(sh < 1e-5f) sh = 1.0f;
        if(sd < 1e-5f) sd = 1.0f;

        float lx = (rot.x - origin.x) / sw;
        float ly = (rot.y - origin.y) / sh;
        float lz = (rot.z - origin.z) / sd;

        float r = sqrtf(lx*lx + lz*lz);
        float freq = std::max(0.2f, std::min(4.0f, wave_frequency * bb.tight_mul));
        float amp = std::max(0.2f, std::min(2.0f, wave_amplitude));
        float dir_rad = wave_direction_deg * (float)(M_PI / 180.0);
        float wave_pos = (float)(cos(dir_rad) * lx + sin(dir_rad) * lz);

        float surface_y;
        int style = std::max(0, std::min(wave_style, STYLE_COUNT - 1));
        switch(style)
        {
        case STYLE_RADIAL:
            surface_y = amp * sinf(phase + freq * r * 3.0f + travel);
            break;
        case STYLE_LINEAR:
            surface_y = amp * sinf(phase + freq * wave_pos * 4.0f + travel);
            break;
        case STYLE_PACIFICA:
            surface_y =
                amp * (sinf(phase + freq * r + travel) * 0.5f +
                       sinf(phase * 0.7f + freq * r * 1.5f + travel * 1.2f) * 0.3f +
                       sinf(phase * 0.5f + r * 2.0f + travel * 0.8f) * 0.2f);
            break;
        case STYLE_GRADIENT:
            surface_y = amp * (0.5f + 0.5f * sinf(phase + freq * r + wave_pos * 2.0f + travel));
            break;
        case STYLE_SINUS:
        default:
            surface_y = amp * sinf(phase + freq * r + wave_pos * 2.0f + travel);
            break;
        }
        float d = fabsf(ly - surface_y);
        float sigma = std::max(surface_thickness, 0.02f);
        const float d_cutoff = 3.0f * sigma * std::max(1.0f, amp);
        if(d > d_cutoff) return 0x00000000;
        float intensity = expf(-d * d / (sigma * sigma));
        intensity = fminf(1.0f, intensity);

        float fade = std::clamp(surface_edge_fade / 100.0f, 0.0f, 1.0f);
        if(fade > 0.001f)
        {
            float u = std::max(std::fabs(lx), std::fabs(lz));
            float edge_mul = 1.0f - fade * smoothstep(0.0f, 1.0f, u);
            intensity *= std::max(0.0f, std::min(1.0f, edge_mul));
        }

        float hue = fmodf((surface_y / amp + 1.0f) * 90.0f + progress_val * 60.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        float pos_norm = (surface_y / amp + 1.0f) * 0.5f;
        float rate = GetScaledFrequency();
        float pos_color = fmodf(pos_norm + time * rate * 0.02f, 1.0f);
        if(pos_color < 0.0f) pos_color += 1.0f;

        float detail = std::max(0.05f, GetScaledDetail());
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
        sp.y_norm = coord_y01;

        RGBColor c;
        if(GetRainbowMode())
        {
            float hue2 = fmodf(hue + time * rate * 12.0f, 360.0f);
            if(hue2 < 0.0f) hue2 += 360.0f;
            hue2 = ApplySpatialRainbowHue(hue2, pos_norm, basis, sp, map, time, &grid);
            float p01 = std::fmod(hue2 / 360.0f, 1.0f);
            if(p01 < 0.0f) p01 += 1.0f;
            p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
            c = GetRainbowColor(p01 * 360.0f);
        }
        else
        {
            float p = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
            p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
            c = GetColorAtPosition(p);
        }
        int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
        int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
        int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
        return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
    }

    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    progress = CalculateProgress(time);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_map;
    EffectStratumBlend::InitStratumBreaks(strat_map);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_map, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, stratum_w, stratum_tuning_);
    const float prog = progress * bb.speed_mul;
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;
    float wave_value = 0.0f;
    float size_multiplier = GetNormalizedSize();
    float freq_scale_e = detail * 0.1f / size_multiplier * bb.tight_mul;
    float normalized_position = 0.0f;
    if(shape_type == 0)
    {
        float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
        float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
        normalized_position = (max_radius > 0.001f) ? (radial_distance / max_radius) : 0.0f;
    }
    else if(shape_type == 1)
        normalized_position = NormalizeGridAxis01(rotated_pos.x, grid.min_x, grid.max_x);
    else if(shape_type == 2)
        normalized_position = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    else
    {
        float nx = NormalizeGridAxis01(rotated_pos.x, grid.min_x, grid.max_x);
        float nz = NormalizeGridAxis01(rotated_pos.z, grid.min_z, grid.max_z);
        normalized_position = 0.5f * (nx + nz);
    }
    normalized_position = fmaxf(0.0f, fminf(1.0f, normalized_position));
    const float pshift = bb.phase_deg * (1.0f / 360.0f);
    normalized_position = std::fmod(normalized_position + pshift + 1.0f, 1.0f);
    wave_value = sin(normalized_position * freq_scale_e * 10.0f - prog);
    float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float depth_factor = 1.0f;
    if(max_radius > 0.001f)
    {
        float normalized_dist = fmin(1.0f, radial_distance / max_radius);
        depth_factor = 0.4f + 0.6f * (1.0f - normalized_dist * 0.7f);
    }
    float wave_enhanced =
        wave_value * 0.7f + 0.3f * sin(normalized_position * freq_scale_e * 20.0f - prog * 1.5f);
    wave_enhanced = fmax(-1.0f, fmin(1.0f, wave_enhanced));
    float pos_for_spatial = (wave_enhanced + 1.0f) * 0.5f;
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

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = (wave_enhanced + 1.0f) * 180.0f;
        hue = fmodf(hue, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        hue = fmodf(hue + time * rate * 12.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        hue = ApplySpatialRainbowHue(hue, pos_for_spatial, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f) p01 += 1.0f;
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float pos_color = fmodf(pos_for_spatial + time * rate * 0.02f, 1.0f);
        if(pos_color < 0.0f) pos_color += 1.0f;
        float p = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        final_color = GetColorAtPosition(p);
    }
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float edge_distance = 1.0f - wave_enhanced;
    edge_distance = std::max(0.0f, edge_distance);
    float thickness_factor = wave_thickness / 100.0f;
    float intensity = 1.0f;
    switch(edge_shape)
    {
    case 0:
        intensity = 1.0f - smoothstep(0.0f, thickness_factor, edge_distance);
        break;
    case 1:
        intensity = edge_distance < thickness_factor * 0.5f ? 1.0f : 0.0f;
        break;
    case 2:
    default:
        intensity = edge_distance < thickness_factor ? 1.0f : 0.0f;
        break;
    }
    r = (unsigned char)(r * depth_factor * intensity);
    g = (unsigned char)(g * depth_factor * intensity);
    b = (unsigned char)(b * depth_factor * intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Wave::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["mode"] = mode;
    j["shape_type"] = shape_type;
    j["edge_shape"] = edge_shape;
    j["wave_thickness"] = wave_thickness;
    j["wave_style"] = wave_style;
    j["surface_thickness"] = surface_thickness;
    j["wave_frequency"] = wave_frequency;
    j["wave_amplitude"] = wave_amplitude;
    j["wave_direction_deg"] = wave_direction_deg;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "wave_stratum_layout_mode",
                                           sm,
                                           st,
                                           "wave_stratum_band_speed_pct",
                                           "wave_stratum_band_tight_pct",
                                           "wave_stratum_band_phase_deg");
    return j;
}

void Wave::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "wave_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "wave_stratum_band_speed_pct",
                                            "wave_stratum_band_tight_pct",
                                            "wave_stratum_band_phase_deg");
    if(settings.contains("mode") && settings["mode"].is_number_integer())
        mode = std::clamp(settings["mode"].get<int>(), 0, MODE_COUNT - 1);
    if(settings.contains("shape_type") && settings["shape_type"].is_number_integer())
    {
        shape_type = std::max(0, std::min(3, settings["shape_type"].get<int>()));
        if(shape_combo)
            shape_combo->setCurrentIndex(shape_type);
    }
    if(settings.contains("edge_shape") && settings["edge_shape"].is_number_integer())
        edge_shape = std::clamp(settings["edge_shape"].get<int>(), 0, 2);
    if(settings.contains("wave_thickness") && settings["wave_thickness"].is_number_integer())
        wave_thickness = std::clamp(settings["wave_thickness"].get<int>(), 5, 100);
    if(settings.contains("wave_style") && settings["wave_style"].is_number_integer())
        wave_style = std::max(0, std::min(settings["wave_style"].get<int>(), STYLE_COUNT - 1));
    if(settings.contains("surface_thickness") && settings["surface_thickness"].is_number())
        surface_thickness = std::max(0.02f, std::min(1.0f, settings["surface_thickness"].get<float>()));
    if(settings.contains("wave_frequency") && settings["wave_frequency"].is_number())
        wave_frequency = std::max(0.2f, std::min(4.0f, settings["wave_frequency"].get<float>()));
    if(settings.contains("wave_amplitude") && settings["wave_amplitude"].is_number())
        wave_amplitude = std::max(0.2f, std::min(2.0f, settings["wave_amplitude"].get<float>()));
    if(settings.contains("wave_direction_deg") && settings["wave_direction_deg"].is_number())
        wave_direction_deg = fmodf(settings["wave_direction_deg"].get<float>() + 360.0f, 360.0f);
    if(settings.contains("surface_edge_fade") && settings["surface_edge_fade"].is_number())
        surface_edge_fade = std::clamp(settings["surface_edge_fade"].get<float>(), 0.0f, 100.0f);

    if(style_combo)
        style_combo->setCurrentIndex(mode);
    if(line_controls) line_controls->setVisible(mode == MODE_LINE);
    if(surface_controls) surface_controls->setVisible(mode == MODE_SURFACE);
    if(edge_shape_combo)
        edge_shape_combo->setCurrentIndex(edge_shape);
    if(thickness_slider)
    {
        thickness_slider->setValue(wave_thickness);
        if(thickness_label) thickness_label->setText(QString::number(wave_thickness));
    }
    if(surface_style_combo)
        surface_style_combo->setCurrentIndex(wave_style);
    if(surface_thick_slider)
    {
        surface_thick_slider->setValue((int)(surface_thickness * 100.0f));
        if(surface_thick_label) surface_thick_label->setText(QString::number((int)(surface_thickness * 100)) + "%");
    }
    if(surface_freq_slider)
    {
        surface_freq_slider->setValue((int)(wave_frequency * 10.0f));
        if(surface_freq_label) surface_freq_label->setText(QString::number(wave_frequency, 'f', 1));
    }
    if(surface_amp_slider)
    {
        surface_amp_slider->setValue((int)(wave_amplitude * 100.0f));
        if(surface_amp_label) surface_amp_label->setText(QString::number((int)(wave_amplitude * 100)) + "%");
    }
    if(surface_dir_slider)
    {
        surface_dir_slider->setValue((int)wave_direction_deg);
        if(surface_dir_label) surface_dir_label->setText(QString::number((int)wave_direction_deg) + "\u00B0");
    }
    if(surface_edge_fade_slider)
    {
        surface_edge_fade_slider->setValue((int)surface_edge_fade);
        if(surface_edge_fade_label) surface_edge_fade_label->setText(QString::number((int)surface_edge_fade) + "%");
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
