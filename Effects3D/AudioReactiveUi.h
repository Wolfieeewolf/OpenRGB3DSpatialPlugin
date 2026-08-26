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
                        25,
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

inline void AppendRegisterRoleRow(QVBoxLayout* layout,
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

    EffectLabeledComboRow* labeled_row = AppendLabeledComboHBoxRow(layout, QStringLiteral("Role:"));
    if(!labeled_row)
    {
        return;
    }
    QComboBox* combo = labeled_row->combo();
    combo->addItem(QStringLiteral("Low — kick / bass punch"),
                   static_cast<int>(AudioRegisterRole::Low));
    combo->addItem(QStringLiteral("Mid — voice / melody"),
                   static_cast<int>(AudioRegisterRole::Mid));
    combo->addItem(QStringLiteral("High — sparkle / notes"),
                   static_cast<int>(AudioRegisterRole::High));
    combo->addItem(QStringLiteral("Mixed — full overview"),
                   static_cast<int>(AudioRegisterRole::Mixed));
    combo->setToolTip(QStringLiteral(
        "What this effect listens for. Sets drive feel and a starting Hz band — "
        "you can still refine Low/High Hz below."));
    int idx = combo->findData(cfg.register_role);
    if(idx < 0)
    {
        idx = combo->findData(static_cast<int>(AudioRegisterRole::Mixed));
    }
    combo->setCurrentIndex(std::max(0, idx));

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), owner,
                     [&cfg, combo, low_slider, high_slider, on_changed](int) {
                         cfg.register_role = combo->currentData().toInt();
                         ApplyAudioRegisterRole(cfg);
                         NormalizeAudioReactiveSettings(cfg);
                         if(low_slider)
                         {
                             low_slider->setValue(cfg.low_hz);
                         }
                         if(high_slider)
                         {
                             high_slider->setValue(cfg.high_hz);
                         }
                         if(on_changed)
                         {
                             on_changed();
                         }
                     });
}

struct AudioFrequencyBandUiOptions
{
    bool include_hz_sliders = true;
    bool include_role = true;
};

struct AudioFrequencyBandContext
{
    QSlider* low_hz_slider = nullptr;
    QSlider* high_hz_slider = nullptr;
};

inline AudioFrequencyBandContext AppendStandardFrequencyBandSection(
    QVBoxLayout* layout,
    AudioReactiveSettings3D& cfg,
    QObject* owner,
    const std::function<void()>& on_changed,
    const AudioFrequencyBandUiOptions& opts = {})
{
    AudioFrequencyBandContext ctx;
    AppendAudioSectionBody(layout, QStringLiteral("Listen"));
    QWidget* hz_box = new QWidget();
    QVBoxLayout* hz_layout = new QVBoxLayout(hz_box);
    hz_layout->setContentsMargins(0, 0, 0, 0);
    hz_layout->setSpacing(0);
    if(opts.include_hz_sliders)
    {
        AppendFrequencyBandRows(hz_layout,
                                cfg,
                                owner,
                                on_changed,
                                &ctx.low_hz_slider,
                                &ctx.high_hz_slider);
    }
    if(opts.include_role)
    {
        AppendRegisterRoleRow(layout, cfg, owner, ctx.low_hz_slider, ctx.high_hz_slider, on_changed);
    }
    if(opts.include_hz_sliders)
    {
        layout->addWidget(hz_box);
    }
    return ctx;
}

struct AudioResponseUiOptions
{
    bool use_onset_smoothing_label = false;
    bool include_smoothing = true;
    bool include_falloff = false;
    bool include_peak_boost = true;
    QString falloff_label = QStringLiteral("Edge / thickness:");
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
    AppendAudioSectionBody(layout, QStringLiteral("Feel"));
    if(opts.include_smoothing)
    {
        const QString smooth_label =
            opts.use_onset_smoothing_label ? QStringLiteral("Trigger lag:")
                                           : QStringLiteral("Motion lag:");
        const QString smooth_tip =
            opts.use_onset_smoothing_label
                ? QStringLiteral(
                      "How sticky the beat detector is (higher = fewer false triggers). "
                      "Different from Decay on the Audio Input panel.")
                : QStringLiteral(
                      "How quickly this effect follows the band (higher = calmer). "
                      "Different from Decay on the Audio Input panel.");
        AppendAudioSmoothingRow(layout, cfg, owner, on_changed, smooth_label, smooth_tip, 99);
    }
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

struct AudioBeatUiOptions
{
    bool include_pulse_color = true;
    bool include_shell_falloff = false;
    bool include_spread_fade = true;
    QString shell_falloff_tooltip;
};

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
    combo->addItem(QStringLiteral("Cycle per beat"),
                   static_cast<int>(AudioPulseColorMode::PerBeatCycle));
    combo->addItem(QStringLiteral("Same color every pulse"),
                   static_cast<int>(AudioPulseColorMode::Uniform));
    combo->addItem(QStringLiteral("Hue along ring / position"),
                   static_cast<int>(AudioPulseColorMode::SpatialAlongRing));
    combo->addItem(QStringLiteral("Follow notes (ColorChord)"),
                   static_cast<int>(AudioPulseColorMode::FollowNotes));
    combo->setToolTip(QStringLiteral(
        "How colors are chosen. Enable the effect Rainbow checkbox for a full spatial rainbow. "
        "Follow notes maps pitch-classes to HSV hue (ColorChord)."));
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

inline void AppendStandardBeatWaveSection(QVBoxLayout* layout,
                                          AudioReactiveSettings3D& cfg,
                                          QObject* owner,
                                          const std::function<void()>& on_changed,
                                          const AudioBeatUiOptions& opts = {})
{
    AppendAudioSectionBody(layout, QStringLiteral("Wave"));
    AppendAudioBeatWaveModeRow(layout, cfg, owner, on_changed);
    if(opts.include_spread_fade)
    {
        AppendBeatWaveMotionRows(layout, cfg, owner, on_changed);
    }
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
                            QStringLiteral("Beat trigger:"),
                            QStringLiteral(
                                "How loud a hit must be to spawn a pulse (higher = fewer triggers). "
                                "Not the same as Effect sensitivity."),
                            5,
                            92);
}

inline void SyncSettingsToHost(QWidget* host, AudioReactiveSettings3D& cfg)
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
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Motion lag:"),
                                     (int)(cfg.smoothing * 100.0f), smooth_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Trigger lag:"),
                                     (int)(cfg.smoothing * 100.0f), smooth_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Effect sensitivity:"),
                                     (int)(cfg.peak_boost * 100.0f), boost_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Wave spread:"),
                                     (int)(cfg.wave_spread * 100.0f), pct_label);
    EffectUiSync::setSliderByCaption(host, QStringLiteral("Wave fade:"),
                                     (int)(cfg.wave_decay * 100.0f), pct_label);

    EffectUiSync::setComboDataByCaption(host, QStringLiteral("Role:"), cfg.register_role);
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
        if(cap.startsWith(QStringLiteral("Falloff:"))
           || cap.startsWith(QStringLiteral("Edge"))
           || cap.startsWith(QStringLiteral("Fill edge:"))
           || cap.startsWith(QStringLiteral("Bar edge:"))
           || cap == QStringLiteral("Shell thickness:"))
        {
            row->syncSliderValue((int)(cfg.falloff * 100.0f),
                                 [](int v) { return QString::number(v / 100.0f, 'f', 1); });
        }
    }
}

}

#endif
