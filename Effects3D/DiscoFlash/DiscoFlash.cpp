// SPDX-License-Identifier: GPL-2.0-only

#include "DiscoFlash.h"
#include "AudioReactiveUi.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include "EffectStratumBlend.h"
#include "SpatialLayerCore.h"
#include "EffectHelpers.h"
#include <cmath>
#include <algorithm>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

DiscoFlash::DiscoFlash(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D DiscoFlash::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Disco Flash";
    info.effect_description =
        "Beat-triggered scattered colour flashes (common Size sets blob radius), or sparkle mode (time-based)";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
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
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void DiscoFlash::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    EffectUiRows::AppendSectionHeading(layout, QStringLiteral("Effect"));

    QWidget* effect_section = EffectUiRows::NewEffectPanel("DiscoFlashEffectSettings");
    QVBoxLayout* effect_layout = EffectUiRows::PanelLayout(effect_section);

    EffectLabeledComboRow* mode_row = EffectUiRows::AppendComboRow(effect_layout, QStringLiteral("Mode:"));
    mode_row->setObjectName(QStringLiteral("modeRow"));
    QComboBox* mode_combo = mode_row->combo();
    mode_combo->addItem(QStringLiteral("Beat (audio)"));
    mode_combo->addItem(QStringLiteral("Sparkle (time)"));
    mode_combo->setCurrentIndex(std::max(0, std::min(this->flash_mode, MODE_COUNT - 1)));
    mode_combo->setToolTip(QStringLiteral("Beat mode needs audio input; Sparkle uses time only."));
    mode_combo->setItemData(0,
                            QStringLiteral("Flash on rhythm peaks—threshold and density matter."),
                            Qt::ToolTipRole);
    mode_combo->setItemData(1, QStringLiteral("Random twinkles over time, no beat required."), Qt::ToolTipRole);

    EffectSliderRow* density_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Density:"),
        5,
        100,
        (int)(flash_density * 100.0f),
        QStringLiteral("Beat: how many flashes per hit. Sparkle: how often voxels twinkle."));
    density_row->setObjectName(QStringLiteral("densityRow"));
    density_row->bindValueChanged(
        this,
        [this](int v) { flash_density = v / 100.0f; },
        [](int v) { return QString::number(v) + QStringLiteral("%"); },
        on_changed);

    EffectSliderRow* flash_decay_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Flash decay:"),
        50,
        1000,
        (int)(flash_decay * 100.0f),
        QStringLiteral("How quickly each beat flash fades out."));
    flash_decay_row->setObjectName(QStringLiteral("flashDecayRow"));
    flash_decay_row->bindValueChanged(
        this,
        [this](int v) { flash_decay = v / 100.0f; },
        [](int v) { return QString::number(v / 100.0f, 'f', 1); },
        on_changed);
    layout->addWidget(effect_section);

    QWidget* beat_audio_panel = new QWidget();
    QVBoxLayout* beat_audio_layout = new QVBoxLayout(beat_audio_panel);
    beat_audio_layout->setContentsMargins(0, 0, 0, 0);
    beat_audio_layout->setSpacing(0);
    AudioReactiveUi::AppendStandardFrequencyBandSection(
        beat_audio_layout, audio_settings, this, on_changed);
    AudioReactiveUi::AppendStandardDriveSection(beat_audio_layout, audio_settings, this, on_changed);
    layout->addWidget(beat_audio_panel);

    QWidget* beat_response_panel = new QWidget();
    QVBoxLayout* beat_response_layout = new QVBoxLayout(beat_response_panel);
    beat_response_layout->setContentsMargins(0, 0, 0, 0);
    beat_response_layout->setSpacing(0);
    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.use_onset_smoothing_label = true;
    AudioReactiveUi::AppendStandardResponseSection(
        beat_response_layout, audio_settings, this, on_changed, response_opts);
    AudioReactiveUi::AppendBeatSensitivityRow(
        beat_response_layout, onset_threshold, this, on_changed);
    layout->addWidget(beat_response_panel);

    const auto update_mode_panels = [beat_audio_panel, beat_response_panel, this]() {
        const bool beat = this->flash_mode == MODE_BEAT;
        beat_audio_panel->setVisible(beat);
        beat_response_panel->setVisible(beat);
    };
    update_mode_panels();
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, update_mode_panels](int idx) {
                this->flash_mode = std::max(0, std::min(idx, MODE_COUNT - 1));
                update_mode_panels();
                emit ParametersChanged();
            });

    AddWidgetToParent(w, parent);
}

float DiscoFlash::EffectiveFlashRadius() const
{
    return 0.25f * std::clamp(GetNormalizedSize(), 0.15f, 2.5f);
}

void DiscoFlash::UpdateParams(SpatialEffectParams& /*params*/)
{
}

void DiscoFlash::SpawnPointFlashes(float time, float strength, uint32_t color_slot)
{
    const int count = std::max(1, static_cast<int>(flash_density * 14.0f * strength));
    std::uniform_real_distribution<float> pos_dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> size_dist(0.5f, 1.5f);

    flashes.reserve(flashes.size() + static_cast<size_t>(count));
    for(int i = 0; i < count; ++i)
    {
        Flash f;
        f.birth_time = time;
        f.nx = pos_dist(rng);
        f.ny = pos_dist(rng);
        f.nz = pos_dist(rng);
        f.size = EffectiveFlashRadius() * size_dist(rng);
        f.color_slot = color_slot;
        flashes.push_back(f);
    }
}

void DiscoFlash::TickFlashes(float time)
{
    if(flash_mode == MODE_SPARKLE)
        return;
    if(std::fabs(time - last_tick_time) < 1e-4f)
    {
        return;
    }
    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
               : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    float strength = 0.0f;
    if(TryTriggerAudioPulse(dt,
                            audio_settings,
                            pulse_trigger,
                            onset_threshold,
                            AudioReactiveOnsetSmoothAlpha(audio_settings),
                            AudioReactiveBeatPulseHoldSec(),
                            strength))
    {
        const uint32_t color_slot = next_pulse_color_slot++;
        SpawnPointFlashes(time, strength, color_slot);
    }

    flashes.erase(std::remove_if(flashes.begin(), flashes.end(),
        [&](const Flash& f) {
            float age = time - f.birth_time;
            return std::exp(-flash_decay * age) < 0.004f;
        }), flashes.end());
}

RGBColor DiscoFlash::SampleFlashField(float x,
                                      float y,
                                      float z,
                                      float nx,
                                      float ny,
                                      float nz,
                                      float time,
                                      const EffectStratumBlend::BandBlendScalars& bb,
                                      float stratum_mot01,
                                      const GridContext3D& grid,
                                      const Vector3D& origin,
                                      const Vector3D& rp)
{
    RGBColor result = ToRGBColor(0, 0, 0);
    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);

    for(unsigned int i = 0; i < flashes.size(); i++)
    {
        const Flash& f = flashes[i];
        float age = time - f.birth_time;
        if(age < 0.0f) continue;

        float dx = nx - f.nx;
        float dy = ny - f.ny;
        float dz = nz - f.nz;
        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        float sz = std::max(f.size / tm, 1e-3f);

        float spatial = std::exp(-(dist * dist) / (sz * sz));
        float fade = std::exp(-flash_decay * age);
        float contribution = spatial * fade;

        if(contribution < 0.004f) continue;

        const float gradient_pos = std::clamp(1.0f - dist / sz, 0.0f, 1.0f);
        AudioReactiveColorParams color_params;
        color_params.gradient_pos01 = gradient_pos;
        color_params.intensity = contribution;
        color_params.beat_color_slot = f.color_slot;
        color_params.time = time;
        color_params.grid_x = x;
        color_params.grid_y = y;
        color_params.grid_z = z;
        color_params.grid = &grid;
        color_params.origin = origin;
        color_params.rotated_pos = rp;
        color_params.y_norm01 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
        color_params.stratum_mot01 = stratum_mot01;
        color_params.band_scalars = &bb;

        RGBColor flash_color = ResolveAudioReactiveColor(audio_settings, color_params);
        flash_color = BrightenAudioEffectColor(flash_color, contribution);

        int rr = std::clamp((int)(result & 0xFF)         + (int)(flash_color & 0xFF),         0, 255);
        int rg = std::clamp((int)((result >> 8) & 0xFF)  + (int)((flash_color >> 8) & 0xFF),  0, 255);
        int rb = std::clamp((int)((result >> 16) & 0xFF) + (int)((flash_color >> 16) & 0xFF), 0, 255);
        result = MakeRGBColor(rr, rg, rb);
    }

    return result;
}

RGBColor DiscoFlash::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
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


    int mode = std::max(0, std::min(flash_mode, MODE_COUNT - 1));
    if(mode == MODE_SPARKLE)
    {
        float nx = (grid.width  > 1e-5f) ? (x - o.x) / (grid.width  * 0.5f) : 0.0f;
        float ny = (grid.height > 1e-5f) ? (y - o.y) / (grid.height * 0.5f) : 0.0f;
        float nz = (grid.depth  > 1e-5f) ? (z - o.z) / (grid.depth  * 0.5f) : 0.0f;
        unsigned int h = (unsigned int)((nx * 1000 + ny * 2000 + nz * 3000) * 1000);
        h = h * 73856093u ^ (unsigned int)(time * 100.f * bb.speed_mul) * 19349663u;
        h = (h << 13) ^ h;
        float sparkle = ((h & 0xFFFFu) / 65535.0f);
        const float density_gate = 1.0f - std::clamp(flash_density, 0.05f, 1.0f) * 0.92f;
        if(sparkle < density_gate)
        {
            return ToRGBColor(0, 0, 0);
        }
        float phase = fmodf(time * bb.speed_mul * (3.0f + sparkle * 5.0f) + sparkle * 6.28f, 6.28f);
        float intensity = (phase < 1.0f) ? (0.3f + 0.7f * phase) : (phase > 5.28f) ? (1.0f - (phase - 5.28f)) : 1.0f;
        intensity = std::max(0.0f, std::min(1.0f, intensity));
        if(intensity < 0.01f) return ToRGBColor(0, 0, 0);

        AudioReactiveColorParams color_params;
        color_params.gradient_pos01 = sparkle;
        color_params.intensity = intensity;
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
        RGBColor c = ResolveAudioReactiveColor(audio_settings, color_params);
        unsigned char r = (unsigned char)((c & 0xFF) * intensity);
        unsigned char g = (unsigned char)(((c >> 8) & 0xFF) * intensity);
        unsigned char b = (unsigned char)(((c >> 16) & 0xFF) * intensity);
        return (RGBColor)((b << 16) | (g << 8) | r);
    }

    TickFlashes(time);

    float nx = (grid.width  > 1e-5f) ? (x - o.x) / (grid.width  * 0.5f) : 0.0f;
    float ny = (grid.height > 1e-5f) ? (y - o.y) / (grid.height * 0.5f) : 0.0f;
    float nz = (grid.depth  > 1e-5f) ? (z - o.z) / (grid.depth  * 0.5f) : 0.0f;

    return SampleFlashField(x, y, z, nx, ny, nz, time, bb, stratum_mot01, grid, o, rot);
}

nlohmann::json DiscoFlash::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["flash_decay"]      = flash_decay;
    j["flash_density"]    = flash_density;
    j["onset_threshold"]  = onset_threshold;
    j["flash_mode"]       = flash_mode;
    return j;
}

void DiscoFlash::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("flash_decay"))      flash_decay      = settings["flash_decay"].get<float>();
    if(settings.contains("flash_density"))    flash_density    = settings["flash_density"].get<float>();
    if(settings.contains("onset_threshold"))  onset_threshold  = settings["onset_threshold"].get<float>();
    if(settings.contains("flash_mode") && settings["flash_mode"].is_number_integer())
        flash_mode = std::max(0, std::min(settings["flash_mode"].get<int>(), MODE_COUNT - 1));
    flashes.clear();
    last_tick_time = std::numeric_limits<float>::lowest();
    pulse_trigger = {};
    next_pulse_color_slot = 0;

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings, this);
    const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "DiscoFlashEffectSettings"))
        {
            EffectUiSync::setComboIndex(fx, "modeRow", flash_mode);
            EffectUiSync::setSliderValue(fx, "densityRow", (int)(flash_density * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "flashDecayRow", (int)(flash_decay * 100.0f),
                                          [](int v) { return QString::number(v / 100.0f, 'f', 1); });
        }
    }
    EffectUiSync::setSliderByCaption(GetCustomSettingsHost(), QStringLiteral("Beat sensitivity:"),
                                     (int)(onset_threshold * 100.0f), pct);
}

REGISTER_EFFECT_3D(DiscoFlash)
