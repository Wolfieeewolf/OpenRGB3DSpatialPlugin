// SPDX-License-Identifier: GPL-2.0-only

#include "BassPunch.h"
#include "AudioReactiveUi.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <QVBoxLayout>

BassPunch::BassPunch(QWidget* parent)
    : AudioPulse(parent)
{
    audio_settings = MakeDefaultLowPunchAudioReactiveSettings3D();
    onset_threshold = 0.24f;
    particle_amount = 18;
}

EffectInfo3D BassPunch::GetEffectInfo() const
{
    EffectInfo3D info = AudioPulse::GetEffectInfo();
    info.effect_name = "Bass Punch";
    info.effect_type = SPATIAL_EFFECT_BASS_PUNCH;
    info.effect_description =
        "Kick / bass shockwaves. Defaults to Low listen role — change Role/Hz on the effect, "
        "and assign controllers/zones on the stack. Pair under Note Sparkle for melody.";
    return info;
}

void BassPunch::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);

    AudioReactiveUi::AudioBeatUiOptions beat_opts;
    beat_opts.include_pulse_color = true;
    beat_opts.include_shell_falloff = true;
    beat_opts.include_spread_fade = true;
    AudioReactiveUi::AppendStandardBeatWaveSection(layout, audio_settings, this, on_changed, beat_opts);

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.use_onset_smoothing_label = true;
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);
    AudioReactiveUi::AppendBeatSensitivityRow(layout, onset_threshold, this, on_changed);

    QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Effect"));
    QWidget* effect_section = EffectUiRows::NewEffectPanel("BassPunchEffectSettings");
    EffectSliderRow* surface_sparks_row = EffectUiRows::AppendSliderRow(
        EffectUiRows::PanelLayout(effect_section),
        QStringLiteral("Surface sparks:"),
        0,
        100,
        (int)particle_amount,
        QStringLiteral("Sparse spark particles on the shell (0 = smooth ring only)."));
    surface_sparks_row->setObjectName(QStringLiteral("surfaceSparksRow"));
    surface_sparks_row->bindValueChanged(
        this,
        [this](int v) { particle_amount = v; },
        [](int v) { return QString::number(v) + QStringLiteral("%"); },
        on_changed);
    if(effect_body)
        effect_body->addWidget(effect_section);
    else
        layout->addWidget(effect_section);

    AddWidgetToParent(w, parent);
}

void BassPunch::LoadSettings(const nlohmann::json& settings)
{
    AudioPulse::LoadSettings(settings);
    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
    const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "BassPunchEffectSettings"))
            EffectUiSync::setSliderValue(fx, "surfaceSparksRow", particle_amount, pct);
    }
}

REGISTER_EFFECT_3D(BassPunch)
