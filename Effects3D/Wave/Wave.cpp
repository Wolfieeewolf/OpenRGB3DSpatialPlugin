// SPDX-License-Identifier: GPL-2.0-only

#include "Wave.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <QComboBox>
#include <QVBoxLayout>
#include "EffectUiRows.h"
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
    case STYLE_OCEAN_DRIFT: return "Ocean drift (waves)";
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

EffectInfo3D Wave::GetEffectInfo() const
{
    EffectInfo3D info;
    info.info_version = 3;
    info.effect_name = "Wave";
    info.effect_description =
        "Wave line or 3D surface; optional floor/mid/ceiling band tuning";
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

    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void Wave::SetupCustomUI(QWidget* parent)
{
    QWidget* wave_widget = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(wave_widget);
    main_layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectLabeledComboRow* style_row = EffectUiRows::AppendComboRow(main_layout, QStringLiteral("Style:"));
    style_row->setObjectName(QStringLiteral("styleRow"));
    style_combo = style_row->combo();
    for(int m = 0; m < MODE_COUNT; m++)
    {
        style_combo->addItem(ModeName(m));
    }
    style_combo->setCurrentIndex(std::max(0, std::min(this->mode, MODE_COUNT - 1)));
    style_combo->setToolTip(QStringLiteral(
        "Line: traveling fronts using Shape below (good on perimeters). "
        "Surface: a heightfield-style wave across the horizontal span."));
    style_combo->setItemData(0,
                             QStringLiteral("Traveling fronts—circles, squares, lines, or diagonal bands."),
                             Qt::ToolTipRole);
    style_combo->setItemData(1,
                             QStringLiteral("3D surface waves; tune Wave style, direction, and edge fade for sparse grids."),
                             Qt::ToolTipRole);
    connect(style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave::OnModeChanged);

    line_controls = new QWidget();
    line_controls->setObjectName(QStringLiteral("lineControlsPanel"));
    QVBoxLayout* line_layout = new QVBoxLayout(line_controls);
    line_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->addWidget(line_controls);

    surface_controls = new QWidget();
    surface_controls->setObjectName(QStringLiteral("surfaceControlsPanel"));
    QVBoxLayout* surface_layout = new QVBoxLayout(surface_controls);
    surface_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->addWidget(surface_controls);

    EffectLabeledComboRow* shape_row = EffectUiRows::AppendComboRow(line_layout, QStringLiteral("Shape:"));
    shape_row->setObjectName(QStringLiteral("shapeRow"));
    shape_combo = shape_row->combo();
    shape_combo->addItem(QStringLiteral("Circles"));
    shape_combo->addItem(QStringLiteral("Squares"));
    shape_combo->addItem(QStringLiteral("Lines"));
    shape_combo->addItem(QStringLiteral("Diagonal"));
    shape_combo->setCurrentIndex(shape_type);
    shape_combo->setToolTip(QStringLiteral("Wave front geometry in line mode (horizontal plane)."));
    shape_combo->setItemData(0, QStringLiteral("Circular rings from the effect origin in XZ."), Qt::ToolTipRole);
    shape_combo->setItemData(1, QStringLiteral("Square rings—sharper corners than circles."), Qt::ToolTipRole);
    shape_combo->setItemData(2, QStringLiteral("Parallel bands; strong on opposing walls."), Qt::ToolTipRole);
    shape_combo->setItemData(3, QStringLiteral("Diagonal bands—readable in corners and along two walls."), Qt::ToolTipRole);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave::OnWaveParameterChanged);

    EffectLabeledComboRow* edge_row = EffectUiRows::AppendComboRow(line_layout, QStringLiteral("Edge:"));
    edge_row->setObjectName(QStringLiteral("edgeRow"));
    edge_shape_combo = edge_row->combo();
    edge_shape_combo->addItem(QStringLiteral("Round"));
    edge_shape_combo->addItem(QStringLiteral("Sharp"));
    edge_shape_combo->addItem(QStringLiteral("Square"));
    edge_shape_combo->setCurrentIndex(std::clamp(edge_shape, 0, 2));
    edge_shape_combo->setToolTip(QStringLiteral("Cross-section of the traveling band in line mode."));
    edge_shape_combo->setItemData(0, QStringLiteral("Soft cosine falloff at the band edges."), Qt::ToolTipRole);
    edge_shape_combo->setItemData(1, QStringLiteral("Hard edge—crisp on-off band."), Qt::ToolTipRole);
    edge_shape_combo->setItemData(2, QStringLiteral("Flat-top band with steep sides."), Qt::ToolTipRole);
    connect(edge_shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave::OnWaveParameterChanged);

    EffectSliderRow* thickness_row = EffectUiRows::AppendSliderRow(
        line_layout,
        QStringLiteral("Thickness:"),
        5,
        100,
        wave_thickness,
        QStringLiteral("Wave band thickness (higher = wider band)."));
    thickness_row->setObjectName(QStringLiteral("thicknessRow"));
    thickness_slider = thickness_row->slider();
    thickness_row->bindValueChanged(
        this, [this](int v) { wave_thickness = v; }, [](int v) { return QString::number(v); }, on_changed);

    EffectLabeledComboRow* wave_style_row = EffectUiRows::AppendComboRow(surface_layout, QStringLiteral("Wave style:"));
    wave_style_row->setObjectName(QStringLiteral("waveStyleRow"));
    surface_style_combo = wave_style_row->combo();
    for(int s = 0; s < STYLE_COUNT; s++)
    {
        surface_style_combo->addItem(WaveStyleName(s));
    }
    surface_style_combo->setCurrentIndex(std::max(0, std::min(wave_style, STYLE_COUNT - 1)));
    surface_style_combo->setToolTip(QStringLiteral(
        "How the surface height is synthesized. Radial reads well from above; Linear follows Wave direction."));
    surface_style_combo->setItemData(0, QStringLiteral("Classic mega-cube style sinusoid in radius and travel."), Qt::ToolTipRole);
    surface_style_combo->setItemData(1, QStringLiteral("Concentric ripples from the horizontal center."), Qt::ToolTipRole);
    surface_style_combo->setItemData(2, QStringLiteral("Plane wave along the direction slider."), Qt::ToolTipRole);
    surface_style_combo->setItemData(3, QStringLiteral("Layered ocean-like motion with softer peaks."), Qt::ToolTipRole);
    surface_style_combo->setItemData(4, QStringLiteral("Smoother gradient roll without sharp crests."), Qt::ToolTipRole);
    connect(surface_style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        wave_style = std::max(0, std::min(idx, STYLE_COUNT - 1));
        emit ParametersChanged();
    });

    EffectSliderRow* surface_thickness_row = EffectUiRows::AppendSliderRow(
        surface_layout, QStringLiteral("Surface thickness:"), 2, 100, (int)(surface_thickness * 100.0f));
    surface_thickness_row->setObjectName(QStringLiteral("surfaceThicknessRow"));
    surface_thick_slider = surface_thickness_row->slider();
    surface_thickness_row->bindValueChanged(
        this, [this](int v) { surface_thickness = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* wave_frequency_row = EffectUiRows::AppendSliderRow(
        surface_layout, QStringLiteral("Wave frequency:"), 3, 30, (int)(wave_frequency * 10.0f));
    wave_frequency_row->setObjectName(QStringLiteral("waveFrequencyRow"));
    surface_freq_slider = wave_frequency_row->slider();
    wave_frequency_row->bindValueChanged(
        this,
        [this](int v) { wave_frequency = v / 10.0f; },
        [this](int) { return QString::number(wave_frequency, 'f', 1); },
        on_changed);

    EffectSliderRow* wave_amplitude_row = EffectUiRows::AppendSliderRow(
        surface_layout, QStringLiteral("Wave amplitude:"), 20, 200, (int)(wave_amplitude * 100.0f));
    wave_amplitude_row->setObjectName(QStringLiteral("waveAmplitudeRow"));
    surface_amp_slider = wave_amplitude_row->slider();
    wave_amplitude_row->bindValueChanged(
        this, [this](int v) { wave_amplitude = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* wave_direction_row = EffectUiRows::AppendSliderRow(
        surface_layout, QStringLiteral("Wave direction:"), 0, 360, (int)wave_direction_deg);
    wave_direction_row->setObjectName(QStringLiteral("waveDirectionRow"));
    surface_dir_slider = wave_direction_row->slider();
    wave_direction_row->bindValueChanged(
        this,
        [this](int v) { wave_direction_deg = (float)v; },
        [](int v) { return QString::number(v) + QStringLiteral("\u00B0"); },
        on_changed);

    EffectSliderRow* edge_fade_row = EffectUiRows::AppendSliderRow(
        surface_layout,
        QStringLiteral("Edge fade:"),
        0,
        100,
        (int)surface_edge_fade,
        QStringLiteral("Softens the wave toward the horizontal room edges (helps wide or large grid layouts)."));
    edge_fade_row->setObjectName(QStringLiteral("edgeFadeRow"));
    surface_edge_fade_slider = edge_fade_row->slider();
    edge_fade_row->bindValueChanged(
        this, [this](int v) { surface_edge_fade = (float)v; }, pct_format, on_changed);

    if(line_controls)
        line_controls->setVisible(this->mode == MODE_LINE);
    if(surface_controls)
        surface_controls->setVisible(this->mode == MODE_SURFACE);

    AddWidgetToParent(wave_widget, parent);
}

void Wave::OnModeChanged()
{
    if(style_combo)
        this->mode = std::max(0, std::min(style_combo->currentIndex(), MODE_COUNT - 1));
    if(line_controls) line_controls->setVisible(this->mode == MODE_LINE);
    if(surface_controls) surface_controls->setVisible(this->mode == MODE_SURFACE);
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

    if(this->mode == MODE_SURFACE)
    {
        Vector3D rot = TransformPointByRotation(x, y, z, origin);
        float coord_y01 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
        SpatialLayerCore::MapperSettings strat_map_s;
        EffectStratumBlend::InitStratumBreaks(strat_map_s);
        float swt[3];
        EffectStratumBlend::WeightsForYNorm(coord_y01, strat_map_s, swt);
        const EffectStratumBlend::BandBlendScalars bb =
            EffectStratumBlend::BlendBands(GetStratumLayoutMode(), swt, GetStratumTuning());
        const float stratum_mot01 =
            ComputeStratumMotion01(swt, grid, x, y, z, origin, time);

        float progress_val = CalculateProgress(time) * bb.speed_mul;
        const float surf_phase01 =
            std::fmod(progress_val + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);
        float strip_surf_p01 = 0.0f;
        if(UseEffectStripColormap())
        {
            strip_surf_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                        GetEffectStripColormapRepeats(),
                                                        GetEffectStripColormapUnfold(),
                                                        GetEffectStripColormapDirectionDeg(),
                                                        surf_phase01,
                                                        time,
                                                        grid,
                                                        GetNormalizedScale(),
                                                        origin,
                                                        rot);
        }
        float phase = progress_val * (float)(2.0 * M_PI);
        float travel = phase + EffectStratumBlend::ApplyMotionToAngleRad(EffectStratumBlend::PhaseShiftRad(bb),
                                                                         stratum_mot01, 0.45f);
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
            surface_y = amp * sinf(freq * r * 3.0f + travel);
            break;
        case STYLE_LINEAR:
            surface_y = amp * sinf(freq * wave_pos * 4.0f + travel);
            break;
        case STYLE_OCEAN_DRIFT:
            surface_y =
                amp * (sinf(freq * r + travel) * 0.5f +
                       sinf(phase * 0.7f + freq * r * 1.5f + travel * 1.2f) * 0.3f +
                       sinf(phase * 0.5f + r * 2.0f + travel * 0.8f) * 0.2f);
            break;
        case STYLE_GRADIENT:
            surface_y = amp * (0.5f + 0.5f * sinf(freq * r + wave_pos * 2.0f + travel));
            break;
        case STYLE_SINUS:
        default:
            surface_y = amp * sinf(freq * r + wave_pos * 2.0f + travel);
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
            const float u = RoomXZEdgeProximity01(rot.x, rot.z, grid);
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
        if(UseEffectStripColormap())
        {
        float p01v = strip_surf_p01;
            c = ResolveStripKernelFinalColor(*this, GetEffectStripColormapKernel(), p01v, GetEffectStripColormapColorStyle(), time,
                                             rate * 12.0f);
        }
        else if(GetRainbowMode())
        {
            float hue2 = fmodf(hue + time * rate * 12.0f, 360.0f);
            if(hue2 < 0.0f) hue2 += 360.0f;
            hue2 = ApplySpatialRainbowHue(hue2, pos_norm, basis, sp, map, time, &grid);
            float p01 = std::fmod(hue2 / 360.0f, 1.0f);
            if(p01 < 0.0f) p01 += 1.0f;
            c = GetRainbowColor(p01 * 360.0f);
        }
        else
        {
            float p = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
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
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);
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
    normalized_position =
        std::fmod(normalized_position + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);
    wave_value = sin(normalized_position * freq_scale_e * 10.0f - prog +
                     stratum_mot01 * (float)(2.0 * M_PI) * 0.55f);
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
    const float line_phase01 = std::fmod(prog + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01) + 1.0f, 1.0f);
    float strip_line_p01 = 0.0f;
    if(UseEffectStripColormap())
    {
        strip_line_p01 = SampleStripKernelPalette01(GetEffectStripColormapKernel(),
                                                    GetEffectStripColormapRepeats(),
                                                    GetEffectStripColormapUnfold(),
                                                    GetEffectStripColormapDirectionDeg(),
                                                    line_phase01,
                                                    time,
                                                    grid,
                                                    size_multiplier,
                                                    origin,
                                                    rotated_pos);
    }
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
    if(UseEffectStripColormap())
    {
        float p01v = strip_line_p01;
        final_color = ResolveStripKernelFinalColor(*this, GetEffectStripColormapKernel(), p01v, GetEffectStripColormapColorStyle(), time,
                                                    rate * 12.0f);
    }
    else if(GetRainbowMode())
    {
        float hue = (wave_enhanced + 1.0f) * 180.0f;
        hue = fmodf(hue, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        hue = fmodf(hue + time * rate * 12.0f, 360.0f);
        if(hue < 0.0f) hue += 360.0f;
        hue = ApplySpatialRainbowHue(hue, pos_for_spatial, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f) p01 += 1.0f;
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float pos_color = fmodf(pos_for_spatial + time * rate * 0.02f, 1.0f);
        if(pos_color < 0.0f) pos_color += 1.0f;
        float p = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
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
    j["surface_edge_fade"] = surface_edge_fade;
    return j;
}

void Wave::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
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
        thickness_slider->setValue(wave_thickness);
    if(surface_style_combo)
        surface_style_combo->setCurrentIndex(wave_style);
    if(surface_thick_slider)
        surface_thick_slider->setValue((int)(surface_thickness * 100.0f));
    if(surface_freq_slider)
        surface_freq_slider->setValue((int)(wave_frequency * 10.0f));
    if(surface_amp_slider)
        surface_amp_slider->setValue((int)(wave_amplitude * 100.0f));
    if(surface_dir_slider)
        surface_dir_slider->setValue((int)wave_direction_deg);
    if(surface_edge_fade_slider)
        surface_edge_fade_slider->setValue((int)surface_edge_fade);
}
