// SPDX-License-Identifier: GPL-2.0-only

#include "AudioStripVisualizer.h"
#include "AudioStripVisualizerVolumeFieldGlsl.h"
#include "AudioReactiveUi.h"
#include "PluginLog.h"
#include "SpatialLayerCore.h"
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QByteArray>
#include <QImage>
#include <QVector3D>
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
    volume_assist_.setFragmentBody(QString::fromUtf8(AudioStripVisualizerVolumeFieldGlsl()));
    volume_assist_.setResolution(22);
    RefreshSpectrumColumns();
}

EffectInfo3D AudioStripVisualizer::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Audio Strip Visualizer";
    info.effect_description =
        "Strip-first spectrum bars or scrolling spectrogram (GPU). Best on a single strip or narrow zone "
        "via stack targeting; Role/Hz + Rainbow for musical color.";
    info.category = "Audio";
    info.effect_type = SPATIAL_EFFECT_AUDIO_STRIP_VISUALIZER;
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
    AudioReactiveUi::AppendAudioSectionBody(layout, QStringLiteral("Color"));
    AudioReactiveUi::AppendAudioPulseColorModeRow(layout, audio_settings, this, on_changed);

    AddWidgetToParent(w, parent);
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

void AudioStripVisualizer::UploadMediaTexture()
{
    if(display_mode == MODE_SPECTROGRAM)
    {
        QImage img(kSpectrogramCols, kSpectrogramRows, QImage::Format_RGBA8888);
        const int rows = kSpectrogramRows;
        const int newest = (spectrogram_write_index > 0) ? ((spectrogram_write_index - 1) % rows) : 0;
        for(int age = 0; age < rows; ++age)
        {
            const int src_row = (newest - age + rows) % rows;
            const std::vector<float>& row =
                (src_row >= 0 && src_row < (int)spectrogram_history.size())
                    ? spectrogram_history[src_row]
                    : column_smoothed;
            for(int c = 0; c < kSpectrogramCols; ++c)
            {
                float v = 0.0f;
                if(c < (int)row.size())
                    v = std::clamp(row[c], 0.0f, 1.0f);
                const int g = (int)std::lround(v * 255.0f);
                img.setPixel(c, age, qRgba(g, g, g, 255));
            }
        }
        volume_assist_.setMediaTexture(img, false);
        return;
    }

    QImage img(kSpectrogramCols, 1, QImage::Format_RGBA8888);
    for(int c = 0; c < kSpectrogramCols; ++c)
    {
        const float v = (c < (int)column_smoothed.size()) ? std::clamp(column_smoothed[c], 0.0f, 1.0f) : 0.0f;
        const int g = (int)std::lround(v * 255.0f);
        img.setPixel(c, 0, qRgba(g, g, g, 255));
    }
    volume_assist_.setMediaTexture(img, false);
}

void AudioStripVisualizer::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    const float epsilon = 1.0f / 60.0f;
    if(last_push_time == std::numeric_limits<float>::lowest() || (time_sec - last_push_time) >= epsilon)
    {
        PushSpectrogramHistory();
        last_push_time = time_sec;
    }

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    UploadMediaTexture();

    float scroll_offset = std::fmod(time_sec * scroll_speed, 1.0f);
    if(scroll_offset < 0.0f)
        scroll_offset += 1.0f;

    const float size_m = std::max(0.2f, GetNormalizedSize());
    const float bar_edge = std::max(0.02f, audio_settings.falloff * 0.0015f);
    const float peak_boost = std::clamp(audio_settings.peak_boost, 0.0f, 4.0f);

    float vp[10] = {
        (float)std::clamp(display_mode, 0, 1),
        (float)std::clamp(GetPathAxis(), 0, 2),
        mirror_bars ? 1.0f : 0.0f,
        size_m,
        bar_edge,
        scroll_offset,
        peak_boost,
        (float)kSpectrogramCols,
        (float)kSpectrogramRows,
        std::max(0.15f, bb.speed_mul)
    };
    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 10))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] AudioStripVisualizer volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor AudioStripVisualizer::ComposeStripColor(float path01, float energy, float time, const GridContext3D& grid,
                                                 float x, float y, float z, const Vector3D& origin,
                                                 const Vector3D& rotated_pos, float stratum_phase01,
                                                 const EffectStratumBlend::BandBlendScalars& bb)
{
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
    color_params.y_norm01 = SampleStratumYNorm01(rotated_pos.y, grid, origin);
    color_params.stratum_mot01 = stratum_phase01;
    color_params.band_scalars = &bb;
    RGBColor color = ResolveAudioReactiveColor(audio_settings, color_params);
    return BrightenAudioEffectColor(color, energy);
}

RGBColor AudioStripVisualizer::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
        return 0x00000000;

    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    if(!volume_assist_.isAvailable())
        return 0x00000000;

    Vector3D rotated_pos{x, y, z};
    float stratum_mot01 = 0.0f;
    EffectStratumBlend::BandBlendScalars bb{1.0f, 1.0f};
    if(UseSpatialRoomTint())
    {
        float coord2 = SampleStratumYNorm01(rotated_pos.y, grid, origin);
        SpatialLayerCore::MapperSettings strat_st;
        EffectStratumBlend::InitStratumBreaks(strat_st);
        float sw[3];
        EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
        bb = EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
        stratum_mot01 = ComputeStratumMotion01(sw, grid, x, y, z, origin, time);
    }

    const float nx = NormalizeGridAxis01(rotated_pos.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rotated_pos.z, grid.min_z, grid.max_z);
    const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
    float energy = samp.x();
    float path01 = samp.y();
    energy = ApplyAudioVisualIntensity(std::clamp(energy, 0.0f, 1.0f), audio_settings);
    if(energy <= 0.001f)
        return 0x00000000;

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
        display_mode = std::clamp(settings["display_mode"].get<int>(), 0, 1);
    if(settings.contains("scroll_speed"))
        scroll_speed = settings["scroll_speed"].get<float>();
    if(settings.contains("mirror_bars"))
        mirror_bars = settings["mirror_bars"].get<bool>();
    RefreshSpectrumColumns();
    last_push_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
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
