// SPDX-License-Identifier: GPL-2.0-only

#include "CubeLayer.h"
#include "AudioReactiveUi.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <QSignalBlocker>
#include "SpatialLayerCore.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>

float CubeLayer::EvaluateIntensity(float amplitude, float time)
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
    return ApplyAudioIntensity(std::clamp(smoothed, 0.0f, 1.0f), audio_settings);
}

float CubeLayer::AxisPosition(int axis, float x, float y, float z,
                                float min_x, float max_x, float min_y, float max_y, float min_z, float max_z)
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

CubeLayer::CubeLayer(QWidget* parent) : SpatialEffect3D(parent)
{
}

EffectInfo3D CubeLayer::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Cube Layer";
    info.effect_description =
        "One lit layer at a time (LED cube style); layer position follows audio level; optional floor/mid/ceiling band tuning";
    info.category = "Audio";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 0;
    info.min_speed = 0;
    info.user_colors = 1;
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

    return info;
}

void CubeLayer::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);
    AudioReactiveUi::AppendStandardDriveSection(layout, audio_settings, this, on_changed);

    EffectUiRows::AppendSectionHeading(layout, QStringLiteral("Effect"));

    QWidget* effect_section = EffectUiRows::NewEffectPanel("CubeLayerEffectSettings");
    QVBoxLayout* effect_layout = EffectUiRows::PanelLayout(effect_section);

    EffectSliderRow* layer_thickness_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Layer thickness:"),
        5,
        100,
        (int)(layer_thickness * 100.0f),
        QStringLiteral("Thickness of the lit audio layer along the Path axis."));
    layer_thickness_row->setObjectName(QStringLiteral("layerThicknessRow"));
    layer_thickness_row->bindValueChanged(
        this,
        [this](int v) { layer_thickness = v / 100.0f; },
        [](int v) { return QString::number(v) + QStringLiteral("%"); },
        on_changed);

    EffectLabeledComboRow* layer_edge_row = EffectUiRows::AppendComboRow(effect_layout, QStringLiteral("Layer edge:"));
    layer_edge_row->setObjectName(QStringLiteral("layerEdgeRow"));
    layer_edge_combo = layer_edge_row->combo();
    layout->addWidget(effect_section);
    layer_edge_combo->addItem(QStringLiteral("Round"));
    layer_edge_combo->addItem(QStringLiteral("Sharp"));
    layer_edge_combo->addItem(QStringLiteral("Square"));
    layer_edge_combo->setCurrentIndex(std::clamp(layer_edge_shape, 0, 2));
    layer_edge_combo->setToolTip(
        QStringLiteral("How sharply the active audio layer transitions to dark above/below it."));
    layer_edge_combo->setItemData(0, QStringLiteral("Smooth blend across the layer thickness."), Qt::ToolTipRole);
    layer_edge_combo->setItemData(1, QStringLiteral("Hard cut at layer boundaries."), Qt::ToolTipRole);
    layer_edge_combo->setItemData(2, QStringLiteral("Lit slab with steep sides."), Qt::ToolTipRole);
    connect(layer_edge_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        layer_edge_shape = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.peak_boost_tooltip =
        QStringLiteral("Gain so quiet passages still move the layer.");
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);

    AddWidgetToParent(w, parent);
}

void CubeLayer::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor CubeLayer::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float amplitude = SampleAudioVisualLevel(audio_settings);
    float layer_pos = EvaluateIntensity(amplitude, time);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), stratum_w, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(stratum_w, grid, x, y, z, origin, time);


    float axis_pos = AxisPosition(GetPathAxis(), rotated_pos.x, rotated_pos.y, rotated_pos.z,
                                   grid.min_x, grid.max_x,
                                   grid.min_y, grid.max_y,
                                   grid.min_z, grid.max_z);
    float edge_distance = std::abs(axis_pos - layer_pos);
    float size_m = GetNormalizedSize();
    float thickness = std::max(layer_thickness, 0.02f) * size_m / std::max(0.25f, bb.tight_mul);
    float intensity = 0.0f;
    switch(layer_edge_shape)
    {
    case 0:
        intensity = 1.0f - smoothstep(0.0f, thickness, edge_distance);
        break;
    case 1:
        intensity = edge_distance < thickness * 0.5f ? 1.0f : 0.0f;
        break;
    case 2:
    default:
        intensity = edge_distance < thickness ? 1.0f : 0.0f;
        break;
    }
    intensity = std::clamp(intensity, 0.0f, 1.0f);

    float detail = std::max(0.05f, GetScaledDetail()) * bb.tight_mul;
    float gradient_pos = fmodf(layer_pos * (0.6f + 0.4f * detail) + time * GetScaledFrequency() * 0.02f * bb.speed_mul +
                                    EffectStratumBlend::CombinedPhase01(bb, stratum_mot01),
                                1.0f);
    if(gradient_pos < 0.0f) gradient_pos += 1.0f;

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

float CubeLayer::smoothstep(float edge0, float edge1, float x) const
{
    float t = (x - edge0) / (std::max(0.0001f, edge1 - edge0));
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

nlohmann::json CubeLayer::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["layer_thickness"] = layer_thickness;
    j["layer_edge_shape"] = layer_edge_shape;
return j;
}

void CubeLayer::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("layer_thickness")) layer_thickness = std::clamp(settings["layer_thickness"].get<float>(), 0.05f, 1.0f);
    if(settings.contains("layer_edge_shape")) layer_edge_shape = std::clamp(settings["layer_edge_shape"].get<int>(), 0, 2);
    if(layer_edge_combo)
    {
        QSignalBlocker blocker(layer_edge_combo);
        layer_edge_combo->setCurrentIndex(layer_edge_shape);
    }
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings, this);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "CubeLayerEffectSettings"))
        {
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "layerThicknessRow", (int)(layer_thickness * 100.0f), pct);
            EffectUiSync::setComboIndex(fx, "layerEdgeRow", layer_edge_shape);
        }
    }
}

REGISTER_EFFECT_3D(CubeLayer)
