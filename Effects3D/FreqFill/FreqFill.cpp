// SPDX-License-Identifier: GPL-2.0-only

#include "FreqFill.h"
#include "AudioReactiveUi.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <cmath>
#include <algorithm>
#include <QVBoxLayout>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

float FreqFill::EvaluateIntensity(float amplitude, float time)
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

FreqFill::FreqFill(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D FreqFill::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Frequency Fill";
    info.effect_description = "Fills room along an axis like a VU meter; optional stratum band tuning";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 0;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 1.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = false;
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

void FreqFill::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);
    AudioReactiveUi::AppendStandardDriveSection(layout, audio_settings, this, on_changed);

    QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Effect"));

    QWidget* effect_section = EffectUiRows::NewEffectPanel("FreqFillEffectSettings");
    EffectSliderRow* edge_width_row = EffectUiRows::AppendSliderRow(
        EffectUiRows::PanelLayout(effect_section),
        QStringLiteral("Edge width:"),
        0,
        100,
        (int)edge_width,
        QStringLiteral(
            "Width of the lit-to-dark transition along the fill axis (Path axis in common controls)."));
    edge_width_row->setObjectName(QStringLiteral("edgeWidthRow"));
    edge_width_row->bindValueChanged(
        this,
        [this](int v) { edge_width = (float)v; },
        [](int v) { return QString::number(v) + QStringLiteral("%"); },
        on_changed);
    if(effect_body)
    {
        effect_body->addWidget(effect_section);
    }
    else
    {
        layout->addWidget(effect_section);
    }

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.peak_boost_tooltip =
        QStringLiteral("Raises quiet passages so the fill reaches farther along the axis.");
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);

    AddWidgetToParent(w, parent);
}

void FreqFill::UpdateParams(SpatialEffectParams& /*params*/)
{
}

static float AxisPosition(int axis, float x, float y, float z,
                          float min_x, float max_x,
                          float min_y, float max_y,
                          float min_z, float max_z)
{
    float val = 0.0f, lo = 0.0f, hi = 1.0f;
    switch(axis)
    {
        case 0: val = x; lo = min_x; hi = max_x; break;
        case 2: val = z; lo = min_z; hi = max_z; break;
        default: val = y; lo = min_y; hi = max_y; break;
    }
    float range = hi - lo;
    if(range < 1e-5f) return 0.5f;
    return std::clamp((val - lo) / range, 0.0f, 1.0f);
}

RGBColor FreqFill::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    float amplitude = SampleAudioVisualLevel(audio_settings);
    float fill_level = EvaluateIntensity(amplitude, time);

    Vector3D o = GetEffectOriginGrid(grid);
    Vector3D rot = TransformPointByRotation(x, y, z, o);
    float coord2 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, o, time);


    float pos = AxisPosition(GetPathAxis(), rot.x, rot.y, rot.z,
                             grid.min_x, grid.max_x,
                             grid.min_y, grid.max_y,
                             grid.min_z, grid.max_z);

    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    float edge = std::max(edge_width, 1e-3f) / std::max(0.35f, size_m * tm);
    float blend = std::clamp((fill_level - pos) / edge + 0.5f, 0.0f, 1.0f);

    float pos_color =
        fmodf(pos * (0.6f + 0.4f * detail * tm) + time * GetScaledFrequency() * 0.02f * bb.speed_mul +
                  EffectStratumBlend::CombinedPhase01(bb, stratum_mot01),
              1.0f);
    if(pos_color < 0.0f) pos_color += 1.0f;

    AudioReactiveColorParams color_params;
    color_params.gradient_pos01 = pos_color;
    color_params.intensity = blend;
    color_params.beat_color_slot = (uint32_t)std::floor(time * 2.5f);
    color_params.time = time;
    color_params.grid_x = x;
    color_params.grid_y = y;
    color_params.grid_z = z;
    color_params.grid = &grid;
    color_params.origin = o;
    color_params.rotated_pos = rot;
    color_params.y_norm01 = coord2;
    color_params.stratum_mot01 = stratum_mot01;
    color_params.band_scalars = &bb;

    RGBColor lit_color = ResolveAudioReactiveColor(audio_settings, color_params);
    RGBColor dark_color = GetColorAtPosition(1.0f);

    RGBColor color = BlendRGBColors(dark_color, lit_color, blend);
    return BrightenAudioEffectColor(color, blend);
}

nlohmann::json FreqFill::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["edge_width"] = edge_width;
    return j;
}

void FreqFill::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("edge_width")) edge_width = settings["edge_width"].get<float>();
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings, this);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "FreqFillEffectSettings"))
        {
            EffectUiSync::setSliderValue(fx, "edgeWidthRow", (int)edge_width,
                                          [](int v) { return QString::number(v) + QStringLiteral("%"); });
        }
    }
}

REGISTER_EFFECT_3D(FreqFill)
