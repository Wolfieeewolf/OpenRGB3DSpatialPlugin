// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOREACTIVEUI_H
#define AUDIOREACTIVEUI_H

#include "AudioReactiveCommon.h"
#include "EffectUiSync.h"
#include "EffectSliderRow.h"
#include "EffectLabeledComboRow.h"
#include "EffectUiRows.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <QVariant>
#include <functional>

namespace AudioReactiveUi
{

inline EffectLabeledComboRow* AppendLabeledComboHBoxRow(QVBoxLayout* layout, const QString& caption)
{
    return EffectUiRows::AppendComboRow(layout, caption);
}

inline QVBoxLayout* AppendAudioSectionBody(QVBoxLayout* layout, const QString& title)
{
    return EffectUiRows::AppendCollapsibleSectionBody(layout, title);
}

inline void AppendSliderHBoxRow(QVBoxLayout* layout,
                                const QString& label_text,
                                int slider_min,
                                int slider_max,
                                int slider_value,
                                const QString& tooltip,
                                int value_label_min_width,
                                QObject* owner,
                                const std::function<QString(int)>& format_value,
                                const std::function<void(int)>& apply_value,
                                const std::function<void()>& on_changed)
{
    if(!layout || !owner)
    {
        return;
    }

    auto* row = new EffectSliderRow();
    row->setCaptionText(label_text);
    row->setValueLabelMinimumWidth(value_label_min_width);
    row->configure(slider_min, slider_max, slider_value, tooltip);
    row->bindValueChanged(owner, apply_value, format_value, on_changed);
    layout->addWidget(row);
}

inline void AppendAudioSmoothingRow(QVBoxLayout* layout,
                                    AudioReactiveSettings3D& cfg,
                                    QObject* owner,
                                    const std::function<void()>& on_changed,
                                    const QString& label = QStringLiteral("Smoothing:"),
                                    const QString& tooltip = QString(),
                                    int slider_max = 99)
{
    AppendSliderHBoxRow(layout,
                        label,
                        0,
                        slider_max,
                        (int)(cfg.smoothing * 100.0f),
                        tooltip,
                        36,
                        owner,
                        [](int v) { return QString::number(v / 100.0f, 'f', 2); },
                        [&cfg](int v) { cfg.smoothing = v / 100.0f; },
                        on_changed);
}

inline void AppendAudioFalloffRow(QVBoxLayout* layout,
                                  AudioReactiveSettings3D& cfg,
                                  QObject* owner,
                                  const std::function<void()>& on_changed,
                                  const QString& label,
                                  int slider_min,
                                  int slider_max,
                                  const QString& tooltip)
{
    AppendSliderHBoxRow(layout,
                        label,
                        slider_min,
                        slider_max,
                        (int)(cfg.falloff * 100.0f),
                        tooltip,
                        36,
                        owner,
                        [](int v) { return QString::number(v / 100.0f, 'f', 1); },
                        [&cfg](int v) { cfg.falloff = v / 100.0f; },
                        on_changed);
}

inline void AppendAudioPeakBoostRow(QVBoxLayout* layout,
                                    AudioReactiveSettings3D& cfg,
                                    QObject* owner,
                                    const std::function<void()>& on_changed,
                                    const QString& tooltip = QString())
{
    const QString tip = tooltip.isEmpty()
                            ? QStringLiteral(
                                  "How strongly this effect reacts to its band. "
                                  "Does not change microphone/loopback level — use Input gain on the Audio panel.")
                            : tooltip;
    AppendSliderHBoxRow(layout,
                        QStringLiteral("Effect sensitivity:"),
                        50,
                        500,
                        (int)(cfg.peak_boost * 100.0f),
                        tip,
                        44,
                        owner,
                        [](int v) {
                            return QString::number(v / 100.0f, 'f', 2) + QStringLiteral("x");
                        },
                        [&cfg](int v) { cfg.peak_boost = v / 100.0f; },
                        on_changed);
}

inline void AppendOnsetThresholdRow(QVBoxLayout* layout,
                                    float& threshold,
                                    QObject* owner,
                                    const std::function<void()>& on_changed,
                                    const QString& label,
                                    const QString& tooltip,
                                    int slider_min = 0,
                                    int slider_max = 95)
{
    AppendSliderHBoxRow(layout,
                        label,
                        slider_min,
                        slider_max,
                        (int)(threshold * 100.0f),
                        tooltip,
                        40,
                        owner,
                        [](int v) { return QString::number(v) + QStringLiteral("%"); },
                        [&threshold](int v) { threshold = v / 100.0f; },
                        on_changed);
}

inline void AppendPercentScaledRow(QVBoxLayout* layout,
                                   float& value_0_to_1,
                                   QObject* owner,
                                   const std::function<void()>& on_changed,
                                   const QString& label,
                                   int slider_min,
                                   int slider_max,
                                   const QString& tooltip)
{
    AppendSliderHBoxRow(layout,
                        label,
                        slider_min,
                        slider_max,
                        (int)(value_0_to_1 * 100.0f),
                        tooltip,
                        40,
                        owner,
                        [](int v) { return QString::number(v) + QStringLiteral("%"); },
                        [&value_0_to_1](int v) { value_0_to_1 = v / 100.0f; },
                        on_changed);
}

inline void AppendFrequencyBandRows(QVBoxLayout* layout,
                                    AudioReactiveSettings3D& cfg,
                                    QObject* owner,
                                    const std::function<void()>& on_changed,
                                    QSlider** out_low_slider = nullptr,
                                    QSlider** out_high_slider = nullptr)
{
    if(!layout || !owner)
    {
        return;
    }

    auto* low_row = new EffectSliderRow();
    low_row->setCaptionText(QStringLiteral("Low Hz:"));
    low_row->setValueLabelMinimumWidth(56);
    low_row->configure(20, 20000, std::clamp(cfg.low_hz, 20, 20000),
                       QStringLiteral("Lower edge of the frequency band that drives this effect."));
    QSlider* low_slider = low_row->slider();
    layout->addWidget(low_row);
    low_row->bindValueChanged(
        owner,
        [&cfg](int v) { cfg.low_hz = v; },
        [](int v) { return QStringLiteral("%1 Hz").arg(v); },
        on_changed);

    auto* high_row = new EffectSliderRow();
    high_row->setCaptionText(QStringLiteral("High Hz:"));
    high_row->setValueLabelMinimumWidth(56);
    high_row->configure(20, 20000, std::clamp(cfg.high_hz, 20, 20000),
                        QStringLiteral("Upper edge of the frequency band that drives this effect."));
    QSlider* high_slider = high_row->slider();
    layout->addWidget(high_row);
    high_row->bindValueChanged(
        owner,
        [&cfg](int v) { cfg.high_hz = v; },
        [](int v) { return QStringLiteral("%1 Hz").arg(v); },
        on_changed);

    QObject::connect(low_slider, &QSlider::valueChanged, owner, [high_slider, low_slider](int low_v) {
        if(high_slider && low_v > high_slider->value())
        {
            high_slider->setValue(low_v);
        }
    });
    QObject::connect(high_slider, &QSlider::valueChanged, owner, [low_slider, high_slider](int high_v) {
        if(low_slider && high_v < low_slider->value())
        {
            low_slider->setValue(high_v);
        }
    });

    if(out_low_slider)
    {
        *out_low_slider = low_slider;
    }
    if(out_high_slider)
    {
        *out_high_slider = high_slider;
    }
}

inline void AppendAudioDriveModeRow(QVBoxLayout* layout,
                                    AudioReactiveSettings3D& cfg,
                                    QObject* owner,
                                    const std::function<void()>& on_changed,
                                    QComboBox** out_drive_combo = nullptr)
{
    if(!layout || !owner)
    {
        return;
    }

    EffectLabeledComboRow* labeled_row = AppendLabeledComboHBoxRow(layout, QStringLiteral("Drive:"));
    if(!labeled_row)
    {
        return;
    }
    QComboBox* combo = labeled_row->combo();
    combo->addItem(QStringLiteral("Sustained (level)"),
                   static_cast<int>(AudioDriveMode::Sustained));
    combo->addItem(QStringLiteral("Transient (attacks)"),
                   static_cast<int>(AudioDriveMode::Transient));
    combo->addItem(QStringLiteral("Beat (kick vs bass)"),
                   static_cast<int>(AudioDriveMode::Beat));
    combo->addItem(QStringLiteral("Band onset (flux)"),
                   static_cast<int>(AudioDriveMode::BandOnset));
    int idx = combo->findData(cfg.drive_mode);
    if(idx < 0) idx = 0;
    combo->setCurrentIndex(idx);

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), owner,
                     [&cfg, combo, on_changed](int) {
                         cfg.drive_mode = combo->currentData().toInt();
                         NormalizeAudioReactiveSettings(cfg);
                         if(on_changed)
                         {
                             on_changed();
                         }
                     });

    if(out_drive_combo)
    {
        *out_drive_combo = combo;
    }
}

inline void AppendSustainRejectRow(QVBoxLayout* layout,
                                   AudioReactiveSettings3D& cfg,
                                   QObject* owner,
                                   const std::function<void()>& on_changed)
{
    AppendSliderHBoxRow(layout,
                        QStringLiteral("Sustain reject:"),
                        0,
                        100,
                        (int)(cfg.sustain_reject * 100.0f),
                        QStringLiteral(
                            "Beat mode only: subtract this much sustained energy in the band "
                            "(higher = ignore bass guitar / synth drones)."),
                        40,
                        owner,
                        [](int v) { return QString::number(v) + QStringLiteral("%"); },
                        [&cfg](int v) { cfg.sustain_reject = v / 100.0f; },
                        on_changed);
}

inline void AppendBandPresetRow(QVBoxLayout* layout,
                                AudioReactiveSettings3D& cfg,
                                QObject* owner,
                                QSlider* low_slider,
                                QSlider* high_slider,
                                const std::function<void()>& on_changed)
{
    if(!layout || !owner)
    {
        return;
    }

    EffectLabeledComboRow* labeled_row = AppendLabeledComboHBoxRow(layout, QStringLiteral("Preset:"));
    if(!labeled_row)
    {
        return;
    }
    QComboBox* combo = labeled_row->combo();
    combo->addItem(QStringLiteral("Custom"), 0);
    combo->addItem(QStringLiteral("Kick drum (55–110 Hz)"), 1);
    combo->addItem(QStringLiteral("Tight kick (45–95 Hz)"), 2);
    combo->addItem(QStringLiteral("Bass / synth (90–250 Hz)"), 3);
    combo->addItem(QStringLiteral("Low beat mix (40–180 Hz)"), 4);
    combo->addItem(QStringLiteral("Snare body (150–400 Hz)"), 5);
    combo->addItem(QStringLiteral("Snare crack (180–900 Hz)"), 6);
    combo->addItem(QStringLiteral("Hi-hat / cymbals (3–14 kHz)"), 7);
    combo->addItem(QStringLiteral("Tom / floor (80–220 Hz)"), 8);
    combo->addItem(QStringLiteral("Sub kick only (35–90 Hz)"), 9);
    combo->addItem(QStringLiteral("Ride / shimmer (5–16 kHz)"), 10);
    combo->addItem(QStringLiteral("— streaming split —"), 11);
    combo->addItem(QStringLiteral("Stream: kick"), 12);
    combo->addItem(QStringLiteral("Stream: snare"), 13);
    combo->addItem(QStringLiteral("Stream: hi-hat"), 14);
    combo->addItem(QStringLiteral("Stream: bass line"), 15);

    auto apply_preset = [owner, &cfg, low_slider, high_slider, on_changed](int preset) {
        switch(preset)
        {
        case 1:
            cfg.low_hz = 55;
            cfg.high_hz = 110;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Beat);
            cfg.sustain_reject = 0.72f;
            cfg.stem_target = static_cast<int>(AudioStemTarget::CustomHz);
            break;
        case 2:
            cfg.low_hz = 45;
            cfg.high_hz = 95;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Beat);
            cfg.sustain_reject = 0.78f;
            cfg.stem_target = static_cast<int>(AudioStemTarget::CustomHz);
            break;
        case 3:
            cfg.low_hz = 90;
            cfg.high_hz = 250;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Transient);
            cfg.stem_target = static_cast<int>(AudioStemTarget::CustomHz);
            break;
        case 4:
            cfg.low_hz = 40;
            cfg.high_hz = 180;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Beat);
            cfg.sustain_reject = 0.60f;
            cfg.stem_target = static_cast<int>(AudioStemTarget::CustomHz);
            break;
        case 5:
            cfg.low_hz = 150;
            cfg.high_hz = 400;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Transient);
            break;
        case 6:
            cfg.low_hz = 180;
            cfg.high_hz = 900;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Transient);
            break;
        case 7:
            cfg.low_hz = 3000;
            cfg.high_hz = 14000;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Transient);
            break;
        case 8:
            cfg.low_hz = 80;
            cfg.high_hz = 220;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Beat);
            cfg.sustain_reject = 0.65f;
            break;
        case 9:
            cfg.low_hz = 35;
            cfg.high_hz = 90;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Beat);
            cfg.sustain_reject = 0.80f;
            break;
        case 10:
            cfg.low_hz = 5000;
            cfg.high_hz = 16000;
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Transient);
            cfg.stem_target = static_cast<int>(AudioStemTarget::CustomHz);
            break;
        case 11:
            return;
        case 12:
            cfg.stem_target = static_cast<int>(AudioStemTarget::StreamKick);
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Beat);
            break;
        case 13:
            cfg.stem_target = static_cast<int>(AudioStemTarget::StreamSnare);
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Transient);
            break;
        case 14:
            cfg.stem_target = static_cast<int>(AudioStemTarget::StreamHihat);
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Transient);
            break;
        case 15:
            cfg.stem_target = static_cast<int>(AudioStemTarget::StreamBass);
            cfg.drive_mode = static_cast<int>(AudioDriveMode::Sustained);
            break;
        default:
            return;
        }
        NormalizeAudioReactiveSettings(cfg);
        if(low_slider)
        {
            low_slider->setValue(cfg.low_hz);
        }
        if(high_slider)
        {
            high_slider->setValue(cfg.high_hz);
        }
        if(owner)
        {
            if(QWidget* sustain_box = owner->property("audio_sustain_row").value<QWidget*>())
            {
                sustain_box->setVisible(static_cast<AudioDriveMode>(cfg.drive_mode) == AudioDriveMode::Beat);
            }
        }
        if(on_changed)
        {
            on_changed();
        }
    };

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), owner,
                     [combo, apply_preset](int) {
                         apply_preset(combo->currentData().toInt());
                     });
}

inline void AppendBeatWaveMotionRows(QVBoxLayout* layout,
                                     AudioReactiveSettings3D& cfg,
                                     QObject* owner,
                                     const std::function<void()>& on_changed)
{
    AppendSliderHBoxRow(layout,
                        QStringLiteral("Wave spread:"),
                        25,
                        800,
                        (int)(cfg.wave_spread * 100.0f),
                        QStringLiteral(
                            "How far and fast the pulse travels outward (works with Speed). Higher = wave "
                            "reaches the room edge sooner."),
                        36,
                        owner,
                        [](int v) { return QString::number(v / 100.0f, 'f', 1); },
                        [&cfg](int v) {
                            cfg.wave_spread = v / 100.0f;
                            NormalizeAudioReactiveSettings(cfg);
                        },
                        on_changed);
    AppendSliderHBoxRow(layout,
                        QStringLiteral("Wave fade:"),
                        12,
                        800,
                        (int)(cfg.wave_decay * 100.0f),
                        QStringLiteral(
                            "How quickly each beat pulse dims after the hit (flash, ring, and classic shell). "
                            "Higher = faster falloff to black."),
                        36,
                        owner,
                        [](int v) { return QString::number(v / 100.0f, 'f', 1); },
                        [&cfg](int v) {
                            cfg.wave_decay = v / 100.0f;
                            NormalizeAudioReactiveSettings(cfg);
                        },
                        on_changed);
}

inline void AppendAudioBeatWaveModeRow(QVBoxLayout* layout,
                                       AudioReactiveSettings3D& cfg,
                                       QObject* owner,
                                       const std::function<void()>& on_changed)
{
    if(!layout || !owner)
    {
        return;
    }

    EffectLabeledComboRow* labeled_row = AppendLabeledComboHBoxRow(layout, QStringLiteral("Beat wave:"));
    if(!labeled_row)
    {
        return;
    }
    QComboBox* combo = labeled_row->combo();
    combo->addItem(QStringLiteral("Classic wave (layered pulse)"),
                   static_cast<int>(AudioBeatWaveMode::ClassicWave));
    combo->addItem(QStringLiteral("Flash on beat, then expanding ring"),
                   static_cast<int>(AudioBeatWaveMode::FlashThenWave));
    combo->addItem(QStringLiteral("Expanding ring on beat (no flash)"),
                   static_cast<int>(AudioBeatWaveMode::WaveOnBeat));
    combo->addItem(QStringLiteral("Flash on beat only (no ring)"),
                   static_cast<int>(AudioBeatWaveMode::FlashOnBeat));
    combo->addItem(QStringLiteral("Off-beat expanding ring (delayed)"),
                   static_cast<int>(AudioBeatWaveMode::OffBeatWave));
    combo->addItem(QStringLiteral("Flash, then clearing wave outward"),
                   static_cast<int>(AudioBeatWaveMode::RecedingClear));
    combo->setToolTip(QStringLiteral(
        "Classic = layered shockwave pulse (recommended). Other modes use an expanding ring, a room "
        "flash, delayed ring, or a clearing wave — each behaves as named."));
    int idx = combo->findData(cfg.beat_wave_mode);
    if(idx < 0)
    {
        idx = 0;
    }
    combo->setCurrentIndex(idx);

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), owner,
                     [&cfg, combo, on_changed](int) {
                         cfg.beat_wave_mode = combo->currentData().toInt();
                         NormalizeAudioReactiveSettings(cfg);
                         if(on_changed)
                         {
                             on_changed();
                         }
                     });
}

inline void AppendAudioPulseColorModeRow(QVBoxLayout* layout,
                                        AudioReactiveSettings3D& cfg,
                                        QObject* owner,
                                        const std::function<void()>& on_changed)
{
    if(!layout || !owner)
    {
        return;
    }

    EffectLabeledComboRow* labeled_row = AppendLabeledComboHBoxRow(layout, QStringLiteral("Pulse color:"));
    if(!labeled_row)
    {
        return;
    }
    QComboBox* combo = labeled_row->combo();
    combo->addItem(QStringLiteral("Cycle per beat (red/blue/…)"),
                   static_cast<int>(AudioPulseColorMode::PerBeatCycle));
    combo->addItem(QStringLiteral("Same color every pulse"),
                   static_cast<int>(AudioPulseColorMode::Uniform));
    combo->addItem(QStringLiteral("Hue along ring / position"),
                   static_cast<int>(AudioPulseColorMode::SpatialAlongRing));
    combo->addItem(QStringLiteral("Audio gradient only"),
                   static_cast<int>(AudioPulseColorMode::AudioGradient));
    combo->setToolTip(QStringLiteral(
        "How colors are chosen: alternate swatches per beat, one fixed color, "
        "rainbow/gradient that changes along the wave, or the audio EQ gradient only."));
    int idx = combo->findData(cfg.pulse_color_mode);
    if(idx < 0)
    {
        idx = 0;
    }
    combo->setCurrentIndex(idx);

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), owner,
                     [&cfg, combo, on_changed](int) {
                         cfg.pulse_color_mode = combo->currentData().toInt();
                         NormalizeAudioReactiveSettings(cfg);
                         if(on_changed)
                         {
                             on_changed();
                         }
                     });
}

struct AudioFrequencyBandContext
{
    QSlider* low_hz_slider = nullptr;
    QSlider* high_hz_slider = nullptr;
};

inline AudioFrequencyBandContext AppendStandardFrequencyBandSection(
    QVBoxLayout* layout,
    AudioReactiveSettings3D& cfg,
    QObject* owner,
    const std::function<void()>& on_changed)
{
    AudioFrequencyBandContext ctx;
    AppendAudioSectionBody(layout, QStringLiteral("Frequency band"));
    AppendFrequencyBandRows(layout,
                            cfg,
                            owner,
                            on_changed,
                            &ctx.low_hz_slider,
                            &ctx.high_hz_slider);
    AppendBandPresetRow(layout, cfg, owner, ctx.low_hz_slider, ctx.high_hz_slider, on_changed);
    return ctx;
}

inline void AppendStandardDriveSection(QVBoxLayout* layout,
                                       AudioReactiveSettings3D& cfg,
                                       QObject* owner,
                                       const std::function<void()>& on_changed)
{
    AppendAudioSectionBody(layout, QStringLiteral("Drive"));
    QComboBox* drive_combo = nullptr;
    AppendAudioDriveModeRow(layout, cfg, owner, on_changed, &drive_combo);

    QWidget* sustain_box = new QWidget();
    QVBoxLayout* sustain_layout = new QVBoxLayout(sustain_box);
    sustain_layout->setContentsMargins(0, 0, 0, 0);
    sustain_layout->setSpacing(0);
    AppendSustainRejectRow(sustain_layout, cfg, owner, on_changed);
    layout->addWidget(sustain_box);
    if(owner)
    {
        owner->setProperty("audio_sustain_row", QVariant::fromValue(sustain_box));
    }

    const auto update_sustain_visibility = [sustain_box, &cfg]() {
        sustain_box->setVisible(static_cast<AudioDriveMode>(cfg.drive_mode) == AudioDriveMode::Beat);
    };
    update_sustain_visibility();
    if(drive_combo)
    {
        QObject::connect(drive_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), owner,
                         [update_sustain_visibility](int) { update_sustain_visibility(); });
    }
}

struct AudioResponseUiOptions
{
    bool use_onset_smoothing_label = false;
    bool include_falloff = false;
    bool include_peak_boost = true;
    QString falloff_label = QStringLiteral("Falloff:");
    int falloff_slider_min = 20;
    int falloff_slider_max = 800;
    QString falloff_tooltip;
    QString peak_boost_tooltip;
};

inline void AppendStandardResponseSection(QVBoxLayout* layout,
                                          AudioReactiveSettings3D& cfg,
                                          QObject* owner,
                                          const std::function<void()>& on_changed,
                                          const AudioResponseUiOptions& opts = {})
{
    AppendAudioSectionBody(layout, QStringLiteral("Response"));
    const QString smooth_label =
        opts.use_onset_smoothing_label ? QStringLiteral("Onset smoothing:")
                                       : QStringLiteral("Level smoothing:");
    const QString smooth_tip =
        opts.use_onset_smoothing_label
            ? QStringLiteral("Smooths the beat/onset detector (higher = fewer false triggers).")
            : QStringLiteral("Smooths band energy before mapping to visuals (higher = calmer movement).");
    AppendAudioSmoothingRow(layout, cfg, owner, on_changed, smooth_label, smooth_tip, 99);
    if(opts.include_falloff)
    {
        AppendAudioFalloffRow(layout,
                              cfg,
                              owner,
                              on_changed,
                              opts.falloff_label,
                              opts.falloff_slider_min,
                              opts.falloff_slider_max,
                              opts.falloff_tooltip);
    }
    if(opts.include_peak_boost)
    {
        AppendAudioPeakBoostRow(layout, cfg, owner, on_changed, opts.peak_boost_tooltip);
    }
}

inline void AppendStandardSpectrumAnalyzerSections(QVBoxLayout* layout,
                                                   AudioReactiveSettings3D& cfg,
                                                   QObject* owner,
                                                   const std::function<void()>& on_changed,
                                                   const AudioResponseUiOptions& response_opts = {})
{
    AppendStandardFrequencyBandSection(layout, cfg, owner, on_changed);
    AppendStandardResponseSection(layout, cfg, owner, on_changed, response_opts);
}

struct AudioBeatUiOptions
{
    bool include_pulse_color = true;
    bool include_shell_falloff = false;
    QString shell_falloff_tooltip;
};

inline void AppendStandardBeatWaveSection(QVBoxLayout* layout,
                                          AudioReactiveSettings3D& cfg,
                                          QObject* owner,
                                          const std::function<void()>& on_changed,
                                          const AudioBeatUiOptions& opts = {})
{
    AppendAudioSectionBody(layout, QStringLiteral("Beat waves"));
    AppendAudioBeatWaveModeRow(layout, cfg, owner, on_changed);
    AppendBeatWaveMotionRows(layout, cfg, owner, on_changed);
    if(opts.include_pulse_color)
    {
        AppendAudioPulseColorModeRow(layout, cfg, owner, on_changed);
    }
    if(opts.include_shell_falloff)
    {
        const QString tip =
            opts.shell_falloff_tooltip.isEmpty()
                ? QStringLiteral(
                      "Width of each outward-moving ring (lower = thinner ring, more space between beats).")
                : opts.shell_falloff_tooltip;
        AppendAudioFalloffRow(layout,
                              cfg,
                              owner,
                              on_changed,
                              QStringLiteral("Shell thickness:"),
                              20,
                              800,
                              tip);
    }
}

inline void AppendBeatSensitivityRow(QVBoxLayout* layout,
                                     float& threshold,
                                     QObject* owner,
                                     const std::function<void()>& on_changed)
{
    AppendOnsetThresholdRow(layout,
                            threshold,
                            owner,
                            on_changed,
                            QStringLiteral("Beat sensitivity:"),
                            QStringLiteral("Higher = fewer, stronger beat triggers."),
                            5,
                            92);
}

inline void SyncSettingsToHost(QWidget* host,
                               AudioReactiveSettings3D& cfg,
                               QObject* owner = nullptr)
{
    if(!host)
    {
        return;
    }

    NormalizeAudioReactiveSettings(cfg);

    const auto hz_label = [](int v) { return QStringLiteral("%1 Hz").arg(v); };
    const auto pct_label = [](int v) { return QString::number(v) + QStringLiteral("%"); };
    const auto smooth_label = [](int v) { return QString::number(v / 100.0f, 'f', 2); };
    const auto boost_label = [](int v) {
        return QString::number(v / 100.0f, 'f', 2) + QStringLiteral("x");
    };

    EffectUiSync::setSliderByCaption(host, QStringLiteral("Low Hz:"), cfg.low_hz, hz_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("High Hz:"), cfg.high_hz, hz_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Level smoothing:"),
                                     (int)(cfg.smoothing * 100.0f), smooth_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Onset smoothing:"),
                                     (int)(cfg.smoothing * 100.0f), smooth_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Sustain reject:"),
                                     (int)(cfg.sustain_reject * 100.0f), pct_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Effect sensitivity:"),
                                     (int)(cfg.peak_boost * 100.0f), boost_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Wave spread:"),
                                     (int)(cfg.wave_spread * 100.0f), pct_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Wave fade:"),
                                     (int)(cfg.wave_decay * 100.0f), pct_label);

    EffectUiSync::setComboDataByCaption(host, QStringLiteral("Drive:"), cfg.drive_mode);
    EffectUiSync::setComboDataByCaption(host, QStringLiteral("Beat wave:"), cfg.beat_wave_mode);
    EffectUiSync::setComboDataByCaption(host, QStringLiteral("Pulse color:"), cfg.pulse_color_mode);

    for(QWidget* child : host->findChildren<QWidget*>())
    {
        EffectSliderRow* row = dynamic_cast<EffectSliderRow*>(child);
        if(!row)
        {
            continue;
        }
        const QString cap = row->captionText();
        if(cap.startsWith(QStringLiteral("Falloff:")) || cap == QStringLiteral("Shell thickness:"))
        {
            row->syncSliderValue((int)(cfg.falloff * 100.0f),
                                 [](int v) { return QString::number(v / 100.0f, 'f', 1); });
        }
    }

    if(owner)
    {
        if(QWidget* sustain_box = owner->property("audio_sustain_row").value<QWidget*>())
        {
            sustain_box->setVisible(static_cast<AudioDriveMode>(cfg.drive_mode) == AudioDriveMode::Beat);
        }
    }
}

}

#endif
