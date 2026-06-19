// SPDX-License-Identifier: GPL-2.0-only

#include "AudioStripVisualizer.h"
#include "AudioReactiveUi.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr int kSpectrogramRows = 72;
constexpr int kSpectrogramCols = 64;

inline int MapHzToColumn(float hz, int columns, float f_min, float f_max)
{
    float clamped = std::clamp(hz, f_min, f_max);
    float denom = std::log(f_max / f_min);
    if(std::abs(denom) < 1e-6f)
    {
        return 0;
    }
    float t = std::log(clamped / f_min) / denom;
    int idx = static_cast<int>(std::floor(t * columns));
    return std::clamp(idx, 0, columns - 1);
}
}

AudioStripVisualizer::AudioStripVisualizer(QWidget* parent)
    : SpatialEffect3D(parent)
{
    RefreshSpectrumColumns();
}

EffectInfo3D AudioStripVisualizer::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Audio Strip Visualizer";
    info.effect_description =
        "Strip-first spectrum bars or scrolling spectrogram; use Path axis and zone targeting for floor, wall, or ceiling strips";
    info.category = "Audio";
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_speed_scale = 8.0f;
    info.default_frequency_scale = 12.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.show_path_axis_control = true;
    info.supports_height_bands = false;
    info.supports_strip_colormap = true;
    return info;
}

void AudioStripVisualizer::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);

    QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Effect"));

    QWidget* effect_section = EffectUiRows::NewEffectPanel("AudioStripVisualizerEffectSettings");
    QVBoxLayout* effect_layout = EffectUiRows::PanelLayout(effect_section);

    EffectLabeledComboRow* display_row = EffectUiRows::AppendComboRow(effect_layout, QStringLiteral("Display:"));
    display_row->setObjectName(QStringLiteral("displayRow"));
    QComboBox* mode_combo = display_row->combo();
    mode_combo->addItem(QStringLiteral("Spectrum bars"));
    mode_combo->addItem(QStringLiteral("Spectrogram scroll"));
    mode_combo->setCurrentIndex(std::clamp(display_mode, 0, 1));
    mode_combo->setToolTip(QStringLiteral(
        "Bars: level along the strip (Path axis). Spectrogram: frequency along Path axis, scroll over time."));
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, on_changed](int idx) {
        display_mode = std::clamp(idx, 0, 1);
        on_changed();
    });

    auto* mirror_bars_check = new QCheckBox(QStringLiteral("Mirror bars"), effect_section);
    mirror_bars_check->setObjectName(QStringLiteral("mirrorBarsCheck"));
    mirror_bars_check->setChecked(mirror_bars);
    mirror_bars_check->setToolTip(
        QStringLiteral("In bar mode, fold the strip so bass and treble meet in the middle."));
    effect_layout->addWidget(mirror_bars_check);
    connect(mirror_bars_check, &QCheckBox::toggled, this, [this, on_changed](bool checked) {
        mirror_bars = checked;
        on_changed();
    });

    EffectSliderRow* scroll_speed_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Scroll speed:"),
        0,
        200,
        (int)(scroll_speed * 100.0f),
        QStringLiteral("Spectrogram scroll rate (bar mode ignores this)."));
    scroll_speed_row->setObjectName(QStringLiteral("scrollSpeedRow"));
    scroll_speed_row->bindValueChanged(
        this,
        [this](int v) { scroll_speed = v / 100.0f; },
        [](int v) { return QString::number(v / 100.0f, 'f', 2); },
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
    response_opts.include_falloff = true;
    response_opts.falloff_label = QStringLiteral("Bar edge:");
    response_opts.falloff_slider_min = 20;
    response_opts.falloff_slider_max = 800;
    response_opts.falloff_tooltip =
        QStringLiteral("Sharpness of each bar's top edge in spectrum bar mode (spectrogram ignores this).");
    response_opts.peak_boost_tooltip =
        QStringLiteral("Boosts quiet input so bars and spectrogram read clearly on strips.");
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);

    AddWidgetToParent(w, parent);
}

void AudioStripVisualizer::UpdateParams(SpatialEffectParams& /*params*/)
{
    RefreshSpectrumColumns();
}

void AudioStripVisualizer::RefreshSpectrumColumns()
{
    column_levels.assign(kSpectrogramCols, 0.0f);
    column_smoothed.assign(kSpectrogramCols, 0.0f);
    if(spectrogram_history.size() != static_cast<size_t>(kSpectrogramRows))
    {
        spectrogram_history.assign(kSpectrogramRows, std::vector<float>(kSpectrogramCols, 0.0f));
        spectrogram_write_index = 0;
    }
}

void AudioStripVisualizer::PushSpectrogramHistory()
{
    AudioInputManager* audio = AudioInputManager::instance();
    if(!audio || !audio->isRunning())
    {
        return;
    }

    AudioInputManager::SpectrumSnapshot snap = audio->getSpectrumSnapshot(kSpectrogramCols);
    if(snap.bins.empty())
    {
        return;
    }

    if(spectrogram_history.size() != static_cast<size_t>(kSpectrogramRows))
    {
        spectrogram_history.assign(kSpectrogramRows, std::vector<float>(kSpectrogramCols, 0.0f));
    }

    const int row = spectrogram_write_index % kSpectrogramRows;
    spectrogram_history[row] = snap.bins;
    if(static_cast<int>(spectrogram_history[row].size()) < kSpectrogramCols)
    {
        spectrogram_history[row].resize(kSpectrogramCols, 0.0f);
    }

    float low = (float)audio_settings.low_hz;
    float high = (float)audio_settings.high_hz;
    float f_min = snap.min_frequency_hz > 0.0f ? snap.min_frequency_hz : 20.0f;
    float f_max = snap.max_frequency_hz > f_min ? snap.max_frequency_hz : 20000.0f;
    int i0 = MapHzToColumn(low, kSpectrogramCols, f_min, f_max);
    int i1 = MapHzToColumn(high, kSpectrogramCols, f_min, f_max);
    if(i1 < i0)
    {
        std::swap(i0, i1);
    }

    float smooth = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    for(int c = 0; c < kSpectrogramCols; ++c)
    {
        float v = 0.0f;
        if(c >= i0 && c <= i1 && c < static_cast<int>(snap.bins.size()))
        {
            const int eq_bands = std::max(1, audio->getEqBandCount());
            const int eq_band = std::min((c * eq_bands) / kSpectrogramCols, eq_bands - 1);
            v = std::clamp(snap.bins[c] * audio->getEqGain(eq_band), 0.0f, 1.0f);
        }
        column_smoothed[c] = smooth * column_smoothed[c] + (1.0f - smooth) * v;
    }

    spectrogram_write_index++;
}

void AudioStripVisualizer::PathAndDisplayNorm(float rx, float ry, float rz, const GridContext3D& grid, float& path01,
                                            float& disp01) const
{
    float ax = NormalizeGridAxis01(rx, grid.min_x, grid.max_x);
    float ay = NormalizeGridAxis01(ry, grid.min_y, grid.max_y);
    float az = NormalizeGridAxis01(rz, grid.min_z, grid.max_z);
    const int path_axis = GetPathAxis();
    if(path_axis == 1)
    {
        path01 = ay;
        disp01 = ax;
    }
    else if(path_axis == 2)
    {
        path01 = az;
        disp01 = ay;
    }
    else
    {
        path01 = ax;
        disp01 = ay;
    }
    if(mirror_bars && display_mode == MODE_BARS)
    {
        path01 = std::fabs(path01 * 2.0f - 1.0f);
    }
    path01 = std::clamp(path01, 0.0f, 1.0f);
    disp01 = std::clamp(disp01, 0.0f, 1.0f);
}

float AudioStripVisualizer::SampleSpectrumEnergy(float path01) const
{
    if(column_smoothed.empty())
    {
        return 0.0f;
    }
    const int count = static_cast<int>(column_smoothed.size());
    float scaled = path01 * (float)(count - 1);
    int i0 = std::clamp(static_cast<int>(std::floor(scaled)), 0, count - 1);
    int i1 = std::min(i0 + 1, count - 1);
    float frac = scaled - std::floor(scaled);
    float v = column_smoothed[i0] + (column_smoothed[i1] - column_smoothed[i0]) * frac;
    return ApplyAudioPulseIntensity(std::clamp(v, 0.0f, 1.0f), audio_settings);
}

float AudioStripVisualizer::SampleSpectrogramEnergy(float path01, float disp01, float time) const
{
    if(spectrogram_history.empty())
    {
        return 0.0f;
    }

    const int rows = static_cast<int>(spectrogram_history.size());
    const int cols = kSpectrogramCols;
    const int newest = (spectrogram_write_index > 0) ? ((spectrogram_write_index - 1) % rows) : 0;
    float scroll_offset = std::fmod(time * scroll_speed, 1.0f);
    if(scroll_offset < 0.0f)
    {
        scroll_offset += 1.0f;
    }
    float age01 = std::clamp(1.0f - disp01 + scroll_offset * 0.15f, 0.0f, 1.0f);
    int age_rows = static_cast<int>(age01 * (float)(rows - 1));
    int row_idx = (newest - age_rows + rows) % rows;

    const std::vector<float>& row = spectrogram_history[row_idx];
    if(row.empty())
    {
        return 0.0f;
    }

    float scaled = path01 * (float)(cols - 1);
    int c0 = std::clamp(static_cast<int>(std::floor(scaled)), 0, cols - 1);
    int c1 = std::min(c0 + 1, cols - 1);
    float frac = scaled - std::floor(scaled);
    float v = row[c0] + (row[c1] - row[c0]) * frac;
    return ApplyAudioPulseIntensity(std::clamp(v, 0.0f, 1.0f), audio_settings);
}

RGBColor AudioStripVisualizer::ComposeStripColor(float path01, float energy, float time, const GridContext3D& grid, float x,
                                                 float y, float z, const Vector3D& origin, const Vector3D& rotated_pos,
                                                 float stratum_phase01,
                                                 const EffectStratumBlend::BandBlendScalars& bb)
{
  RGBColor base = ComposeAudioGradientColor(audio_settings, path01, energy);
  base = BrightenAudioEffectColor(base, energy);

  SpatialLayerCore::Basis basis;
  SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
  SpatialLayerCore::MapperSettings map;
  SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), std::max(0.05f, GetScaledDetail()));
  SpatialLayerCore::SamplePoint sp{};
  sp.grid_x = x;
  sp.grid_y = y;
  sp.grid_z = z;
  sp.origin_x = origin.x;
  sp.origin_y = origin.y;
  sp.origin_z = origin.z;
  sp.y_norm = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);

  AudioReactiveColorParams color_params;
  color_params.gradient_pos01 = path01;
  color_params.intensity = energy;
  color_params.beat_color_slot = (uint32_t)std::floor(time * 2.5f);
  color_params.time = time;
  color_params.grid_x = x;
  color_params.grid_y = y;
  color_params.grid_z = z;
  color_params.grid = &grid;
  color_params.origin = origin;
  color_params.rotated_pos = rotated_pos;
  color_params.y_norm01 = sp.y_norm;
  color_params.stratum_mot01 = stratum_phase01;
  color_params.band_scalars = &bb;
  RGBColor user_color = ResolveAudioReactiveColor(audio_settings, color_params);
  return ModulateRGBColors(base, user_color);
}

RGBColor AudioStripVisualizer::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    const float epsilon = 1.0f / 60.0f;
    if(last_push_time == std::numeric_limits<float>::lowest() || (time - last_push_time) >= epsilon)
    {
        PushSpectrogramHistory();
        last_push_time = time;
    }

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float path01 = 0.0f;
    float disp01 = 0.0f;
    PathAndDisplayNorm(rotated_pos.x, rotated_pos.y, rotated_pos.z, grid, path01, disp01);

    float stratum_mot01 = 0.0f;
    EffectStratumBlend::BandBlendScalars bb{1.0f, 1.0f};
    if(UseSpatialRoomTint())
    {
        float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
        SpatialLayerCore::MapperSettings strat_st;
        EffectStratumBlend::InitStratumBreaks(strat_st);
        float sw[3];
        EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
        bb = EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
        stratum_mot01 = ComputeStratumMotion01(sw, grid, x, y, z, origin, time);
    }

    float energy = 0.0f;
    if(display_mode == MODE_SPECTROGRAM)
    {
        energy = SampleSpectrogramEnergy(path01, disp01, time);
    }
    else
    {
        float level = SampleSpectrumEnergy(path01);
        float size_m = std::max(0.2f, GetNormalizedSize());
        float bar_edge = std::max(0.02f, audio_settings.falloff * 0.0015f) / size_m;
        float bar = std::clamp((level - disp01) / bar_edge + 0.5f, 0.0f, 1.0f);
        energy = level * bar;
    }

    if(energy <= 0.001f)
    {
        return 0x00000000;
    }

    return ComposeStripColor(path01, energy, time, grid, x, y, z, origin, rotated_pos, stratum_mot01, bb);
}

nlohmann::json AudioStripVisualizer::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["display_mode"] = display_mode;
    j["scroll_speed"] = scroll_speed;
    j["mirror_bars"] = mirror_bars;
    return j;
}

void AudioStripVisualizer::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("display_mode"))
    {
        display_mode = std::clamp(settings["display_mode"].get<int>(), 0, 1);
    }
    if(settings.contains("scroll_speed"))
    {
        scroll_speed = settings["scroll_speed"].get<float>();
    }
    if(settings.contains("mirror_bars"))
    {
        mirror_bars = settings["mirror_bars"].get<bool>();
    }
    RefreshSpectrumColumns();
    last_push_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings, this);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "AudioStripVisualizerEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "displayRow", display_mode);
            EffectUiSync::setCheckBox(fx, "mirrorBarsCheck", mirror_bars);
            EffectUiSync::setSliderValue(fx, "scrollSpeedRow", (int)(scroll_speed * 100.0f),
                                          [](int v) { return QString::number(v / 100.0f, 'f', 2); });
        }
    }
}

REGISTER_EFFECT_3D(AudioStripVisualizer)
