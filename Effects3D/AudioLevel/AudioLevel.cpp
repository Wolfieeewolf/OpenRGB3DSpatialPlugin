// SPDX-License-Identifier: GPL-2.0-only

#include "AudioLevel.h"
#include "AudioReactiveUi.h"
#include "SpatialKernelColormap.h"
#include "SpatialPatternKernels/SpatialPatternKernels.h"
#include "Colors.h"
#include "SpatialLayerCore.h"
#include <cmath>
#include <algorithm>
#include <QVBoxLayout>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

float AudioLevel::EvaluateIntensity(float amplitude, float time)
{
    float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    if(std::fabs(time - last_intensity_time) > 1e-4f)
    {
        smoothed = alpha * smoothed + (1.0f - alpha) * amplitude;
        last_intensity_time = time;
    }
    else if(alpha <= 0.0f)
    {
        smoothed = amplitude;
    }
    return ApplyAudioIntensity(smoothed, audio_settings);
}

AudioLevel::AudioLevel(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D AudioLevel::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Audio Level";
    info.effect_description =
        "Level drives a 3D fill surface (like a water level) with optional wave on the boundary; optional stratum band tuning";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.show_path_axis_control = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void AudioLevel::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);
    AudioReactiveUi::AppendStandardDriveSection(layout, audio_settings, this, on_changed);

    EffectUiRows::AppendSectionHeading(layout, QStringLiteral("Effect"));

    QWidget* effect_section = EffectUiRows::NewEffectPanel("AudioLevelEffectSettings");
    QVBoxLayout* effect_layout = EffectUiRows::PanelLayout(effect_section);
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectSliderRow* boundary_wave_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Boundary wave:"),
        0,
        100,
        (int)wave_amount,
        QStringLiteral("Wobble on the lit/dark boundary (Path axis in common controls)."));
    boundary_wave_row->setObjectName(QStringLiteral("boundaryWaveRow"));
    boundary_wave_row->bindValueChanged(
        this, [this](int v) { wave_amount = (float)v; }, pct_format, on_changed);

    EffectSliderRow* edge_softness_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Edge softness:"),
        2,
        100,
        (int)edge_soft,
        QStringLiteral("Thickness of the transition at the fill surface (higher = softer)."));
    edge_softness_row->setObjectName(QStringLiteral("edgeSoftnessRow"));
    edge_softness_row->bindValueChanged(
        this, [this](int v) { edge_soft = (float)v; }, pct_format, on_changed);
    layout->addWidget(effect_section);

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.include_falloff = true;
    response_opts.falloff_slider_max = 500;
    response_opts.falloff_tooltip =
        QStringLiteral("Steepness of the lit region versus dark below the fill boundary.");
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);

    AddWidgetToParent(w, parent);
}

void AudioLevel::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor AudioLevel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);


    float amplitude = SampleAudioVisualLevel(audio_settings);
    float fill_level = EvaluateIntensity(amplitude, time);

    float ax = NormalizeGridAxis01(rotated_pos.x, grid.min_x, grid.max_x);
    float ay = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    float az = NormalizeGridAxis01(rotated_pos.z, grid.min_z, grid.max_z);
    int fax = GetPathAxis();
    float axis_pos = (fax == 0) ? ax : ((fax == 1) ? ay : az);
    float axis_other = (fax == 0) ? ay : ((fax == 1) ? ax : ay);
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float wave =
        wave_amount * (1.0f + 0.45f * fill_level) *
        std::sin(time * 4.0f * bb.speed_mul * std::max(0.2f, GetScaledFrequency() * 0.15f * bb.tight_mul) +
                 axis_other * 6.283185f * (0.6f + 0.4f * detail * bb.tight_mul));
    float fill_boundary = std::clamp(fill_level + wave, 0.0f, 1.0f);
    float edge = std::max(edge_soft, 0.01f) / std::max(0.35f, size_m * bb.tight_mul);
    float blend = std::clamp((fill_boundary - axis_pos) / edge + 0.5f, 0.0f, 1.0f);
    float intensity = blend;

    float dx = rotated_pos.x - origin.x, dy = rotated_pos.y - origin.y, dz = rotated_pos.z - origin.z;
    float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float radial_norm = ComputeRadialNormalized(dx, dy, dz, max_radius);
    float gradient_pos = std::clamp(0.65f * axis_pos + 0.35f * (1.0f - radial_norm), 0.0f, 1.0f);

    AudioReactiveColorParams color_params;
    color_params.gradient_pos01 = gradient_pos;
    color_params.intensity = intensity;
    color_params.beat_color_slot = (uint32_t)std::floor(time * 2.5f);
    color_params.time = time;
    color_params.grid_x = x;
    color_params.grid_y = y;
    color_params.grid_z = z;
    color_params.grid = &grid;
    color_params.origin = origin;
    color_params.rotated_pos = rotated_pos;
    color_params.y_norm01 = coord2;
    color_params.stratum_mot01 = stratum_mot01;
    color_params.band_scalars = &bb;

    RGBColor color = ResolveAudioReactiveColor(audio_settings, color_params);
    return BrightenAudioEffectColor(color, intensity);
}

nlohmann::json AudioLevel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["wave_amount"] = wave_amount;
    j["edge_soft"] = edge_soft;
return j;
}

void AudioLevel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("wave_amount")) wave_amount = std::clamp(settings["wave_amount"].get<float>(), 0.0f, 0.5f);
    if(settings.contains("edge_soft")) edge_soft = std::clamp(settings["edge_soft"].get<float>(), 0.02f, 0.5f);
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings, this);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "AudioLevelEffectSettings"))
        {
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "boundaryWaveRow", (int)wave_amount, pct);
            EffectUiSync::setSliderValue(fx, "edgeSoftnessRow", (int)edge_soft, pct);
        }
    }
}

REGISTER_EFFECT_3D(AudioLevel)
