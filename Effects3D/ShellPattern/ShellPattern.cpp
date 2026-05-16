// SPDX-License-Identifier: GPL-2.0-only

#include "ShellPattern.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(ShellPattern);

const char* ShellPattern::UnfoldModeLabel(int m)
{
    switch(m)
    {
    case 0: return "Along X";
    case 1: return "Along Y";
    case 2: return "Along Z";
    case 3: return "Plane XZ (angled)";
    case 4: return "Radial XZ";
    case 5: return "Diagonal x+y+z";
    case 6: return "Manhattan";
    case 7: return "Effect animation only (no room projection)";
    case 8: return "Static room projection (angle)";
    default: return "Along X";
    }
}

const char* ShellPattern::DisplayModeLabel(int d)
{
    switch(d)
    {
    case DISP_SHELL_Y: return "Shell (wave height)";
    case DISP_FILL_STRIP: return "Extrude (solid by coordinate)";
    default: return "Shell (wave height)";
    }
}

ShellPattern::ShellPattern(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(38);
    SetRainbowMode(false);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

ShellPattern::~ShellPattern() = default;

EffectInfo3D ShellPattern::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Shell Pattern";
    info.effect_description =
        "Map 3D position to a 1D coordinate, run a pattern kernel, then show it as a shell band or a solid extruded fill.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_SHELL_PATTERN;
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

void ShellPattern::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(w);
    main_layout->setContentsMargins(0, 0, 0, 0);

    QGridLayout* g = new QGridLayout();
    int row = 0;
    g->addWidget(new QLabel("Display:"), row, 0);
    display_combo = new QComboBox();
    for(int d = 0; d < DISP_COUNT; d++)
        display_combo->addItem(DisplayModeLabel(d));
    display_combo->setCurrentIndex(std::clamp(display_mode, 0, DISP_COUNT - 1));
    display_combo->setToolTip("Shell: Gaussian band around a height. Extrude: brightness follows only the unfold coordinate.");
    g->addWidget(display_combo, row, 1, 1, 2);
    row++;

    g->addWidget(new QLabel("Shell thickness:"), row, 0);
    thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(0, 100);
    thick_slider->setValue((int)std::lround(surface_thickness * 100.0f));
    thick_label = new QLabel(QString::number((int)std::lround(surface_thickness * 100.0f)) + "%");
    thick_label->setMinimumWidth(36);
    g->addWidget(thick_slider, row, 1);
    g->addWidget(thick_label, row, 2);
    thick_slider->setToolTip("Shell band width.");
    row++;

    g->addWidget(new QLabel("Shell amplitude:"), row, 0);
    amp_slider = new QSlider(Qt::Horizontal);
    amp_slider->setRange(20, 200);
    amp_slider->setValue((int)(wave_amplitude * 100.0f));
    amp_label = new QLabel(QString::number((int)(wave_amplitude * 100)) + "%");
    amp_label->setMinimumWidth(36);
    g->addWidget(amp_slider, row, 1);
    g->addWidget(amp_label, row, 2);
    row++;

    g->addWidget(new QLabel("Edge fade:"), row, 0);
    edge_slider = new QSlider(Qt::Horizontal);
    edge_slider->setRange(0, 100);
    edge_slider->setValue((int)edge_fade_pct);
    edge_slider->setToolTip(
        "Softens toward the room X/Z walls (full grid bounds). 0% = off. Uses real room edges, not effect scale.");
    edge_label = new QLabel(QString::number((int)edge_fade_pct) + "%");
    edge_label->setMinimumWidth(36);
    g->addWidget(edge_slider, row, 1);
    g->addWidget(edge_label, row, 2);

    main_layout->addLayout(g);

    connect(display_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShellPattern::OnParameterChanged);
    connect(thick_slider, &QSlider::valueChanged, this, [this](int v) {
        surface_thickness = v / 100.0f;
        if(thick_label)
            thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    connect(amp_slider, &QSlider::valueChanged, this, [this](int v) {
        wave_amplitude = v / 100.0f;
        if(amp_label)
            amp_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    connect(edge_slider, &QSlider::valueChanged, this, [this](int v) {
        edge_fade_pct = (float)v;
        if(edge_label)
            edge_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    AddWidgetToParent(w, parent);
}

void ShellPattern::OnParameterChanged()
{
    if(display_combo)
        display_mode = std::clamp(display_combo->currentIndex(), 0, DISP_COUNT - 1);
    emit ParametersChanged();
}

void ShellPattern::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_SHELL_PATTERN;
}

float ShellPattern::EvaluateKernel(float s01, float phase01, float time_sec, int pattern, float repeats) const
{
    return EvalSpatialPatternKernel(pattern, s01, phase01, repeats, time_sec);
}

RGBColor ShellPattern::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

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
    const float phase_drive = progress_val + EffectStratumBlend::CombinedPhase01(bb, stratum_mot01);

    float scale_eff = std::max(0.05f, GetNormalizedScale());
    float sw = grid.width * 0.5f * scale_eff;
    float sh = grid.height * 0.5f * scale_eff;
    float sd = grid.depth * 0.5f * scale_eff;
    if(sw < 1e-5f)
        sw = 1.0f;
    if(sh < 1e-5f)
        sh = 1.0f;
    if(sd < 1e-5f)
        sd = 1.0f;

    float lx = (rot.x - origin.x) / sw;
    float ly = (rot.y - origin.y) / sh;
    float lz = (rot.z - origin.z) / sd;

    const int unfold_i = UseEffectStripColormap() ? GetEffectStripColormapUnfold() : unfold_mode;
    const float dir_deg = UseEffectStripColormap() ? GetEffectStripColormapDirectionDeg() : direction_deg;
    const float reps = UseEffectStripColormap() ? GetEffectStripColormapRepeats() : strip_repeats;
    auto unfold = static_cast<StripPatternSurface::UnfoldMode>(
        std::clamp(unfold_i, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    float s01 = StripPatternSurface::StripCoord01(lx, ly, lz, unfold, dir_deg);
    int pat = std::clamp(UseEffectStripColormap() ? GetEffectStripColormapKernel() : pattern_id, 0,
                         SpatialPatternKernelCount() - 1);
    float k = EvaluateKernel(s01, phase_drive, time, pat, reps);
    float amp = std::max(0.2f, std::min(2.0f, wave_amplitude * bb.tight_mul));

    float intensity = 1.0f;
    float surface_y = amp * k;
    int disp = std::clamp(display_mode, 0, DISP_COUNT - 1);
    if(disp == DISP_SHELL_Y)
    {
        float sigma = std::max(surface_thickness, 0.005f);
        intensity = StripPatternSurface::ShellIntensityGaussianY(ly, surface_y, sigma, amp);
        if(intensity <= 1e-4f)
            return 0x00000000;
    }

    float fade = std::clamp(edge_fade_pct / 100.0f, 0.0f, 1.0f);
    if(fade > 0.001f)
    {
        const float u = RoomXZEdgeProximity01(rot.x, rot.z, grid);
        const float t = std::clamp(u, 0.0f, 1.0f);
        float edge_mul = 1.0f - fade * (t * t * (3.0f - 2.0f * t));
        intensity *= std::max(0.0f, std::min(1.0f, edge_mul));
    }
    if(intensity <= 1e-4f)
        return 0x00000000;

    float pos_norm = (k + 1.0f) * 0.5f;
    pos_norm = std::clamp(pos_norm, 0.0f, 1.0f);
    float rate = GetScaledFrequency();
    float pos_color = std::fmod(pos_norm + time * rate * 0.009f, 1.0f);
    if(pos_color < 0.0f)
        pos_color += 1.0f;
    pos_color = EffectStratumBlend::ApplyMotionToPhase01(pos_color, stratum_mot01, 0.5f);

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

    RGBColor c = 0x00000000;
    if(UseEffectStripColormap())
    {
        float p_mapped = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
        float p01v = ApplyVoxelDriveToPalette01(p_mapped, x, y, z, time, grid);
        const int color_style =
            UseEffectStripColormap() ? GetEffectStripColormapColorStyle() : strip_shell_color_style;
        c = ResolveStripKernelFinalColor(*this, pat, p01v, color_style, time, rate * 12.0f);
    }
    else if(GetRainbowMode())
    {
        float hue = pos_color * 360.0f;
        hue = ApplySpatialRainbowHue(hue, coord_y01, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        float p01v = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        c = GetRainbowColor(p01v * 360.0f);
    }
    else
    {
        float p_mapped = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
        float p01v = ApplyVoxelDriveToPalette01(p_mapped, x, y, z, time, grid);
        c = GetColorAtPosition(p01v);
    }
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json ShellPattern::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["shellpattern_display_mode"] = display_mode;
    j["shellpattern_surface_thickness"] = surface_thickness;
    j["shellpattern_wave_amplitude"] = wave_amplitude;
    j["shellpattern_edge_fade_pct"] = edge_fade_pct;
    return j;
}

void ShellPattern::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("shellpattern_display_mode") && settings["shellpattern_display_mode"].is_number_integer())
        display_mode = std::clamp(settings["shellpattern_display_mode"].get<int>(), 0, DISP_COUNT - 1);
    if(settings.contains("shellpattern_unfold_mode") && settings["shellpattern_unfold_mode"].is_number_integer())
        unfold_mode = std::clamp(settings["shellpattern_unfold_mode"].get<int>(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    if(settings.contains("shellpattern_pattern_id") && settings["shellpattern_pattern_id"].is_number_integer())
        pattern_id = std::clamp(settings["shellpattern_pattern_id"].get<int>(), 0, SpatialPatternKernelCount() - 1);
    if(settings.contains("shellpattern_color_style") && settings["shellpattern_color_style"].is_number_integer())
        strip_shell_color_style = std::clamp(settings["shellpattern_color_style"].get<int>(), 0, 1);
    if(settings.contains("shellpattern_direction_deg") && settings["shellpattern_direction_deg"].is_number())
        direction_deg = std::fmod(settings["shellpattern_direction_deg"].get<float>() + 360.0f, 360.0f);
    if(settings.contains("shellpattern_repeats") && settings["shellpattern_repeats"].is_number())
        strip_repeats = std::max(1.0f, std::min(40.0f, settings["shellpattern_repeats"].get<float>()));
    if(settings.contains("shellpattern_surface_thickness") && settings["shellpattern_surface_thickness"].is_number())
        surface_thickness = std::clamp(settings["shellpattern_surface_thickness"].get<float>(), 0.0f, 1.0f);
    if(settings.contains("shellpattern_wave_amplitude") && settings["shellpattern_wave_amplitude"].is_number())
        wave_amplitude = std::max(0.2f, std::min(2.0f, settings["shellpattern_wave_amplitude"].get<float>()));
    if(settings.contains("shellpattern_edge_fade_pct") && settings["shellpattern_edge_fade_pct"].is_number())
        edge_fade_pct = std::clamp(settings["shellpattern_edge_fade_pct"].get<float>(), 0.0f, 100.0f);

    if(display_combo)
        display_combo->setCurrentIndex(display_mode);
    if(thick_slider)
    {
        thick_slider->setValue((int)std::lround(surface_thickness * 100.0f));
        if(thick_label)
            thick_label->setText(QString::number((int)std::lround(surface_thickness * 100.0f)) + "%");
    }
    if(amp_slider)
    {
        amp_slider->setValue((int)(wave_amplitude * 100.0f));
        if(amp_label)
            amp_label->setText(QString::number((int)(wave_amplitude * 100)) + "%");
    }
    if(edge_slider)
    {
        edge_slider->setValue((int)edge_fade_pct);
        if(edge_label)
            edge_label->setText(QString::number((int)edge_fade_pct) + "%");
    }
}
