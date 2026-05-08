// SPDX-License-Identifier: GPL-2.0-only

#include "ShellPattern3D.h"
#include "SpatialKernelColormap.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(ShellPattern3D);

const char* ShellPattern3D::UnfoldModeLabel(int m)
{
    switch(m)
    {
    case 0: return "Along X";
    case 1: return "Along Y";
    case 2: return "Along Z";
    case 3: return "Plane XZ (angled)";
    case 4: return "Radial XZ (ring unwrap)";
    case 5: return "Diagonal (x+y+z)";
    case 6: return "Manhattan distance";
    case 7: return "Effect phase (no spatial unfold)";
    default: return "Along X";
    }
}

const char* ShellPattern3D::DisplayModeLabel(int d)
{
    switch(d)
    {
    case DISP_SHELL_Y: return "Shell (wave height)";
    case DISP_FILL_STRIP: return "Extrude (solid by coordinate)";
    default: return "Shell (wave height)";
    }
}

ShellPattern3D::ShellPattern3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetFrequency(50);
    SetRainbowMode(false);
    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

ShellPattern3D::~ShellPattern3D() = default;

EffectInfo3D ShellPattern3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Shell Pattern 3D";
    info.effect_description =
        "Map 3D position to a 1D coordinate, run a pattern kernel, then show it as a shell band or a solid extruded fill.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_SHELL_PATTERN_3D;
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

void ShellPattern3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(w);
    main_layout->setContentsMargins(0, 0, 0, 0);

    QGridLayout* g = new QGridLayout();
    int row = 0;
    g->addWidget(new QLabel("Unfold:"), row, 0);
    unfold_combo = new QComboBox();
    for(int i = 0; i < (int)StripPatternSurface::UnfoldMode::COUNT; i++)
        unfold_combo->addItem(UnfoldModeLabel(i));
    unfold_combo->setCurrentIndex(std::clamp(unfold_mode, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    unfold_combo->setToolTip("How the 1D pattern coordinate is derived from position.");
    g->addWidget(unfold_combo, row, 1, 1, 2);
    row++;

    g->addWidget(new QLabel("Display:"), row, 0);
    display_combo = new QComboBox();
    for(int d = 0; d < DISP_COUNT; d++)
        display_combo->addItem(DisplayModeLabel(d));
    display_combo->setCurrentIndex(std::clamp(display_mode, 0, DISP_COUNT - 1));
    display_combo->setToolTip("Shell: Gaussian band around a height. Extrude: brightness follows only the unfold coordinate.");
    g->addWidget(display_combo, row, 1, 1, 2);
    row++;

    g->addWidget(new QLabel("Pattern:"), row, 0);
    pattern_combo = new QComboBox();
    for(int p = 0; p < SpatialPatternKernelCount(); p++)
        pattern_combo->addItem(SpatialPatternKernelDisplayName(p));
    pattern_combo->setCurrentIndex(std::clamp(pattern_id, 0, SpatialPatternKernelCount() - 1));
    pattern_combo->setToolTip("1D pattern; tune Speed, Frequency, repeats.");
    g->addWidget(pattern_combo, row, 1, 1, 2);
    row++;

    g->addWidget(new QLabel("Pattern colors:"), row, 0);
    color_style_combo = new QComboBox();
    color_style_combo->addItem("Pattern palette");
    color_style_combo->addItem("Effect color stops");
    color_style_combo->addItem("Rainbow");
    color_style_combo->setCurrentIndex(std::clamp(strip_shell_color_style, 0, 2));
    color_style_combo->setToolTip("Pattern palette: built-in look per pattern. Effect stops / Rainbow: map kernel value to your colors or spectrum.");
    g->addWidget(color_style_combo, row, 1, 1, 2);
    row++;

    g->addWidget(new QLabel("Plane angle:"), row, 0);
    dir_slider = new QSlider(Qt::Horizontal);
    dir_slider->setRange(0, 359);
    dir_slider->setValue((int)direction_deg);
    dir_label = new QLabel(QString::number((int)direction_deg) + QChar(0x00B0));
    dir_label->setMinimumWidth(40);
    g->addWidget(dir_slider, row, 1);
    g->addWidget(dir_label, row, 2);
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

    g->addWidget(new QLabel("Pattern repeats:"), row, 0);
    repeats_slider = new QSlider(Qt::Horizontal);
    repeats_slider->setRange(1, 40);
    repeats_slider->setValue((int)strip_repeats);
    repeats_label = new QLabel(QString::number((int)strip_repeats));
    repeats_label->setMinimumWidth(28);
    g->addWidget(repeats_slider, row, 1);
    g->addWidget(repeats_label, row, 2);
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
    edge_label = new QLabel(QString::number((int)edge_fade_pct) + "%");
    edge_label->setMinimumWidth(36);
    g->addWidget(edge_slider, row, 1);
    g->addWidget(edge_label, row, 2);

    main_layout->addLayout(g);

    connect(unfold_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShellPattern3D::OnParameterChanged);
    connect(display_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShellPattern3D::OnParameterChanged);
    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShellPattern3D::OnParameterChanged);
    connect(color_style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShellPattern3D::OnParameterChanged);
    connect(dir_slider, &QSlider::valueChanged, this, [this](int v) {
        direction_deg = (float)v;
        if(dir_label)
            dir_label->setText(QString::number(v) + QChar(0x00B0));
        emit ParametersChanged();
    });
    connect(thick_slider, &QSlider::valueChanged, this, [this](int v) {
        surface_thickness = v / 100.0f;
        if(thick_label)
            thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    connect(repeats_slider, &QSlider::valueChanged, this, [this](int v) {
        strip_repeats = (float)std::max(1, v);
        if(repeats_label)
            repeats_label->setText(QString::number(v));
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

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &ShellPattern3D::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void ShellPattern3D::OnParameterChanged()
{
    if(unfold_combo)
        unfold_mode = std::clamp(unfold_combo->currentIndex(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    if(display_combo)
        display_mode = std::clamp(display_combo->currentIndex(), 0, DISP_COUNT - 1);
    if(pattern_combo)
        pattern_id = std::clamp(pattern_combo->currentIndex(), 0, SpatialPatternKernelCount() - 1);
    if(color_style_combo)
        strip_shell_color_style = std::clamp(color_style_combo->currentIndex(), 0, 2);
    emit ParametersChanged();
}

void ShellPattern3D::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void ShellPattern3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_SHELL_PATTERN_3D;
}

float ShellPattern3D::EvaluateKernel(float s01, float phase01, float time_sec, int pattern) const
{
    return EvalSpatialPatternKernel(pattern, s01, phase01, strip_repeats, time_sec);
}

RGBColor ShellPattern3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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
        EffectStratumBlend::BlendBands(stratum_layout_mode, swt, stratum_tuning_);

    float progress_val = CalculateProgress(time) * bb.speed_mul;
    float phase01 = std::fmod(progress_val + bb.phase_deg * (1.0f / 360.0f) + 1.0f, 1.0f);

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

    auto unfold = static_cast<StripPatternSurface::UnfoldMode>(std::clamp(unfold_mode, 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1));
    float s01 = StripPatternSurface::StripCoord01(lx, ly, lz, unfold, direction_deg);
    int pat = std::clamp(pattern_id, 0, SpatialPatternKernelCount() - 1);
    float k = EvaluateKernel(s01, phase01, time, pat);
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
        float u = std::max(std::fabs(lx), std::fabs(lz));
        float t = (u - 0.0f) / std::max(0.0001f, 1.0f);
        t = std::clamp(t, 0.0f, 1.0f);
        float edge_mul = 1.0f - fade * (t * t * (3.0f - 2.0f * t));
        intensity *= std::max(0.0f, std::min(1.0f, edge_mul));
    }
    if(intensity <= 1e-4f)
        return 0x00000000;

    float pos_norm = (k + 1.0f) * 0.5f;
    pos_norm = std::clamp(pos_norm, 0.0f, 1.0f);
    float rate = GetScaledFrequency();
    float pos_color = std::fmod(pos_norm + time * rate * 0.02f, 1.0f);
    if(pos_color < 0.0f)
        pos_color += 1.0f;

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

    float p_mapped = ApplySpatialPalette01(pos_color, basis, sp, map, time, &grid);
    float p01v = ApplyVoxelDriveToPalette01(p_mapped, x, y, z, time, grid);
    RGBColor c = ResolveStripKernelFinalColor(*this, pat, p01v, strip_shell_color_style, time, rate * 12.0f);
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json ShellPattern3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["shellpattern_unfold_mode"] = unfold_mode;
    j["shellpattern_display_mode"] = display_mode;
    j["shellpattern_pattern_id"] = pattern_id;
    j["shellpattern_color_style"] = strip_shell_color_style;
    j["shellpattern_direction_deg"] = direction_deg;
    j["shellpattern_surface_thickness"] = surface_thickness;
    j["shellpattern_repeats"] = strip_repeats;
    j["shellpattern_wave_amplitude"] = wave_amplitude;
    j["shellpattern_edge_fade_pct"] = edge_fade_pct;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "shellpattern_stratum_layout_mode",
                                           sm,
                                           st,
                                           "shellpattern_stratum_band_speed_pct",
                                           "shellpattern_stratum_band_tight_pct",
                                           "shellpattern_stratum_band_phase_deg");
    return j;
}

void ShellPattern3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                           "shellpattern_stratum_layout_mode",
                                           stratum_layout_mode,
                                           stratum_tuning_,
                                           "shellpattern_stratum_band_speed_pct",
                                           "shellpattern_stratum_band_tight_pct",
                                           "shellpattern_stratum_band_phase_deg");
    if(settings.contains("shellpattern_unfold_mode") && settings["shellpattern_unfold_mode"].is_number_integer())
        unfold_mode = std::clamp(settings["shellpattern_unfold_mode"].get<int>(), 0, (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    if(settings.contains("shellpattern_display_mode") && settings["shellpattern_display_mode"].is_number_integer())
        display_mode = std::clamp(settings["shellpattern_display_mode"].get<int>(), 0, DISP_COUNT - 1);
    if(settings.contains("shellpattern_pattern_id") && settings["shellpattern_pattern_id"].is_number_integer())
        pattern_id = std::clamp(settings["shellpattern_pattern_id"].get<int>(), 0, SpatialPatternKernelCount() - 1);
    if(settings.contains("shellpattern_color_style") && settings["shellpattern_color_style"].is_number_integer())
        strip_shell_color_style = std::clamp(settings["shellpattern_color_style"].get<int>(), 0, 2);
    else
        strip_shell_color_style = GetRainbowMode() ? 2 : 0;
    if(settings.contains("shellpattern_direction_deg") && settings["shellpattern_direction_deg"].is_number())
        direction_deg = std::fmod(settings["shellpattern_direction_deg"].get<float>() + 360.0f, 360.0f);
    if(settings.contains("shellpattern_surface_thickness") && settings["shellpattern_surface_thickness"].is_number())
        surface_thickness = std::clamp(settings["shellpattern_surface_thickness"].get<float>(), 0.0f, 1.0f);
    if(settings.contains("shellpattern_repeats") && settings["shellpattern_repeats"].is_number())
        strip_repeats = std::max(1.0f, std::min(40.0f, settings["shellpattern_repeats"].get<float>()));
    if(settings.contains("shellpattern_wave_amplitude") && settings["shellpattern_wave_amplitude"].is_number())
        wave_amplitude = std::max(0.2f, std::min(2.0f, settings["shellpattern_wave_amplitude"].get<float>()));
    if(settings.contains("shellpattern_edge_fade_pct") && settings["shellpattern_edge_fade_pct"].is_number())
        edge_fade_pct = std::clamp(settings["shellpattern_edge_fade_pct"].get<float>(), 0.0f, 100.0f);

    if(unfold_combo)
        unfold_combo->setCurrentIndex(unfold_mode);
    if(display_combo)
        display_combo->setCurrentIndex(display_mode);
    if(pattern_combo)
        pattern_combo->setCurrentIndex(pattern_id);
    if(color_style_combo)
        color_style_combo->setCurrentIndex(strip_shell_color_style);
    if(dir_slider)
    {
        dir_slider->setValue((int)direction_deg);
        if(dir_label)
            dir_label->setText(QString::number((int)direction_deg) + QChar(0x00B0));
    }
    if(thick_slider)
    {
        thick_slider->setValue((int)std::lround(surface_thickness * 100.0f));
        if(thick_label)
            thick_label->setText(QString::number((int)std::lround(surface_thickness * 100.0f)) + "%");
    }
    if(repeats_slider)
    {
        repeats_slider->setValue((int)strip_repeats);
        if(repeats_label)
            repeats_label->setText(QString::number((int)strip_repeats));
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
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
