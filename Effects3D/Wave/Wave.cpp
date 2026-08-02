// SPDX-License-Identifier: GPL-2.0-only

#include "Wave.h"
#include "WaveSurfaceVolumeFieldGlsl.h"
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
    surface_volume_assist_.setFragmentBody(QString::fromUtf8(WaveSurfaceVolumeFieldGlsl()));
    surface_volume_assist_.setResolution(18); // was 28 — wave surface is soft; lower atlas = less lag
}

Wave::~Wave() = default;

EffectInfo3D Wave::GetEffectInfo() const
{
    EffectInfo3D info;
    info.effect_name = "Wave";
    info.effect_description =
        "3D surface height-field wave (amplitude, frequency, direction); GPU assist when available";
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

    EffectLabeledComboRow* wave_style_row = EffectUiRows::AppendComboRow(main_layout, QStringLiteral("Wave style:"));
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
        main_layout, QStringLiteral("Surface thickness:"), 2, 100, (int)(surface_thickness * 100.0f));
    surface_thickness_row->setObjectName(QStringLiteral("surfaceThicknessRow"));
    surface_thick_slider = surface_thickness_row->slider();
    surface_thickness_row->bindValueChanged(
        this, [this](int v) { surface_thickness = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* wave_frequency_row = EffectUiRows::AppendSliderRow(
        main_layout, QStringLiteral("Wave frequency:"), 3, 30, (int)(wave_frequency * 10.0f));
    wave_frequency_row->setObjectName(QStringLiteral("waveFrequencyRow"));
    surface_freq_slider = wave_frequency_row->slider();
    wave_frequency_row->bindValueChanged(
        this,
        [this](int v) { wave_frequency = v / 10.0f; },
        [this](int) { return QString::number(wave_frequency, 'f', 1); },
        on_changed);

    EffectSliderRow* wave_amplitude_row = EffectUiRows::AppendSliderRow(
        main_layout, QStringLiteral("Wave amplitude:"), 20, 200, (int)(wave_amplitude * 100.0f));
    wave_amplitude_row->setObjectName(QStringLiteral("waveAmplitudeRow"));
    surface_amp_slider = wave_amplitude_row->slider();
    wave_amplitude_row->bindValueChanged(
        this, [this](int v) { wave_amplitude = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* wave_direction_row = EffectUiRows::AppendSliderRow(
        main_layout, QStringLiteral("Wave direction:"), 0, 360, (int)wave_direction_deg);
    wave_direction_row->setObjectName(QStringLiteral("waveDirectionRow"));
    surface_dir_slider = wave_direction_row->slider();
    wave_direction_row->bindValueChanged(
        this,
        [this](int v) { wave_direction_deg = (float)v; },
        [](int v) { return QString::number(v) + QStringLiteral("\u00B0"); },
        on_changed);

    EffectSliderRow* edge_fade_row = EffectUiRows::AppendSliderRow(
        main_layout,
        QStringLiteral("Edge fade:"),
        0,
        100,
        (int)surface_edge_fade,
        QStringLiteral("Softens the wave toward the horizontal room edges (helps wide or large grid layouts)."));
    edge_fade_row->setObjectName(QStringLiteral("edgeFadeRow"));
    surface_edge_fade_slider = edge_fade_row->slider();
    edge_fade_row->bindValueChanged(
        this, [this](int v) { surface_edge_fade = (float)v; }, pct_format, on_changed);

    AddWidgetToParent(wave_widget, parent);
}

float Wave::smoothstep(float edge0, float edge1, float x) const
{
    float t = (x - edge0) / (std::max(0.0001f, edge1 - edge0));
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void Wave::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    const float freq = std::max(0.2f, std::min(4.0f, wave_frequency));
    const float amp = std::max(0.2f, std::min(2.0f, wave_amplitude));
    const float dir_rad = wave_direction_deg * (float)(M_PI / 180.0);
    const int style = std::max(0, std::min(wave_style, STYLE_COUNT - 1));
    const float sigma = std::max(surface_thickness, 0.02f);
    const float travel_base = CalculateProgress(time_sec) * (float)(2.0 * M_PI);
    const float vp[6] = {travel_base, freq, amp, (float)style, dir_rad, sigma};
    surface_volume_assist_.prepare(render_sequence, time_sec, vp, 6);
}

RGBColor Wave::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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

    float intensity = 0.0f;
    float pos_norm = 0.5f;
    if(surface_volume_assist_.isAvailable())
    {
        EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
        e.hw = sw;
        e.hh = sh;
        e.hd = sd;
        float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
        SampleCoordsOriginLocal01(rot.x, rot.y, rot.z, origin, e, &c1, &c2, &c3);
        const QVector3D samp = surface_volume_assist_.sample01(c1, c2, c3);
        intensity = samp.x();
        pos_norm = EffectStratumBlend::ApplyMotionToUnit01(samp.y(), stratum_mot01, 0.28f);
        if(intensity <= 1e-5f)
        {
            return 0x00000000;
        }
    }
    else
    {
        const float amp = std::max(0.2f, std::min(2.0f, wave_amplitude));
        const float dir_rad = wave_direction_deg * (float)(M_PI / 180.0);
        const int style = std::max(0, std::min(wave_style, STYLE_COUNT - 1));
        const float sigma = std::max(surface_thickness, 0.02f);
        float r = sqrtf(lx * lx + lz * lz);
        float wave_pos = (float)(cos(dir_rad) * lx + sin(dir_rad) * lz);
        float phase = progress_val * (float)(2.0 * M_PI);
        float travel = phase + EffectStratumBlend::ApplyMotionToAngleRad(EffectStratumBlend::PhaseShiftRad(bb),
                                                                         stratum_mot01, 0.45f);
        float freq_e = std::max(0.2f, std::min(4.0f, wave_frequency * bb.tight_mul));
        float surface_y;
        switch(style)
        {
        case STYLE_RADIAL:
            surface_y = amp * sinf(freq_e * r * 3.0f + travel);
            break;
        case STYLE_LINEAR:
            surface_y = amp * sinf(freq_e * wave_pos * 4.0f + travel);
            break;
        case STYLE_OCEAN_DRIFT:
            surface_y =
                amp * (sinf(freq_e * r + travel) * 0.5f +
                       sinf(phase * 0.7f + freq_e * r * 1.5f + travel * 1.2f) * 0.3f +
                       sinf(phase * 0.5f + r * 2.0f + travel * 0.8f) * 0.2f);
            break;
        case STYLE_GRADIENT:
            surface_y = amp * (0.5f + 0.5f * sinf(freq_e * r + wave_pos * 2.0f + travel));
            break;
        case STYLE_SINUS:
        default:
            surface_y = amp * sinf(freq_e * r + wave_pos * 2.0f + travel);
            break;
        }
        float d = fabsf(ly - surface_y);
        const float d_cutoff = 3.0f * sigma * std::max(1.0f, amp);
        if(d > d_cutoff) return 0x00000000;
        intensity = expf(-d * d / (sigma * sigma));
        intensity = fminf(1.0f, intensity);
        pos_norm = (surface_y / amp + 1.0f) * 0.5f;
    }

    float fade = std::clamp(surface_edge_fade / 100.0f, 0.0f, 1.0f);
    if(fade > 0.001f)
    {
        const float u = RoomXZEdgeProximity01(rot.x, rot.z, grid);
        float edge_mul = 1.0f - fade * smoothstep(0.0f, 1.0f, u);
        intensity *= std::max(0.0f, std::min(1.0f, edge_mul));
    }

    float hue = fmodf(pos_norm * 180.0f + progress_val * 60.0f, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
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
        c = ResolveStripKernelFinalColor(GetEffectStripColormapKernel(), p01v, time);
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

nlohmann::json Wave::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
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
