// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "AudioAdvancedSettingsDialog.h"
#include "AudioEqBandColumn.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "PluginUiUtils.h"
#include "PluginLog.h"
#include <QToolTip>
#include <QPainter>
#include <QFrame>
#include <QFont>
#include <QSignalBlocker>
#include <QScrollArea>
#include <cmath>

namespace {

float JsonToGain(const nlohmann::json& v)
{
    if(v.is_number_float() || v.is_number_integer() || v.is_number_unsigned())
    {
        return std::clamp(v.get<float>(), 0.0f, 2.0f);
    }
    return 1.0f;
}

float ResampleSavedEqGain(const nlohmann::json& arr, int band_index, int bands_count)
{
    if(!arr.is_array() || arr.empty() || bands_count <= 0 || band_index < 0 || band_index >= bands_count)
    {
        return 1.0f;
    }
    const int n = static_cast<int>(arr.size());
    if(n == bands_count)
    {
        return JsonToGain(arr[band_index]);
    }
    const float t = ((float)band_index + 0.5f) / (float)bands_count;
    const float ri = t * (float)n - 0.5f;
    const int i0 = std::clamp((int)std::floor(ri), 0, n - 1);
    const int i1 = std::min(i0 + 1, n - 1);
    const float frac = std::clamp(ri - (float)i0, 0.0f, 1.0f);
    const float g0 = JsonToGain(arr[i0]);
    const float g1 = JsonToGain(arr[i1]);
    return std::clamp(g0 * (1.0f - frac) + g1 * frac, 0.0f, 2.0f);
}

} // namespace

void OpenRGB3DSpatialTab::rebuildAudioEqSliders(bool persist_settings)
{
    if(audio_eq_rebuilding || !audioEqScroll())
    {
        return;
    }
    audio_eq_rebuilding = true;

    if(audio_eq_container)
    {
        const QSignalBlocker block_scroll(audio_eq_container);
        audioEqScroll()->takeWidget();
        delete audio_eq_container;
        audio_eq_container = nullptr;
        audio_eq_row_layout = nullptr;
    }
    audio_eq_sliders.clear();

    audio_eq_container = new QWidget();
    audio_eq_row_layout = new QHBoxLayout(audio_eq_container);
    audio_eq_row_layout->setContentsMargins(0, 0, 0, 0);
    audio_eq_row_layout->setSpacing(2);
    audioEqScroll()->setWidget(audio_eq_container);

    AudioInputManager* audio = AudioInputManager::instance();
    const int bands = std::max(1, audio->getEqBandCount());
    const int sr = audio->getSampleRate();
    const int fft_n = audio->getFFTSize();

    if(audioEqCaption())
    {
        audioEqCaption()->setText(
            QStringLiteral("Equalizer (%1 log-spaced bands — one slider per analyzer band):").arg(bands));
    }

    nlohmann::json settings = GetPluginSettings();
    const nlohmann::json* saved_eq = nullptr;
    if(settings.contains("AudioEqGain") && settings["AudioEqGain"].is_array())
    {
        saved_eq = &settings["AudioEqGain"];
    }

    auto fmt_hz = [](float hz) -> QString {
        if(hz < 1000.0f)
        {
            return QString::number((int)std::round(hz));
        }
        return QString::number(hz / 1000.0f, 'f', 1) + QStringLiteral("k");
    };

    for(int b = 0; b < bands; ++b)
    {
        float hz_lo = 0.0f;
        float hz_hi = 0.0f;
        AudioInputManager::AnalysisBandHzRange(b, bands, sr, fft_n, hz_lo, hz_hi);
        const float center = AudioInputManager::AnalysisBandCenterHz(b, bands, sr, fft_n);

        QString label_text;
        if(bands <= 12 && (hz_hi - hz_lo) < 2500.0f)
        {
            label_text = QStringLiteral("%1–%2").arg(fmt_hz(hz_lo), fmt_hz(hz_hi));
        }
        else
        {
            label_text = fmt_hz(center);
        }

        auto* band_col = new AudioEqBandColumn();
        band_col->setCaptionText(label_text);
        band_col->setCaptionToolTip(QStringLiteral("Band %1: %2–%3 Hz (geometric mean %4 Hz)")
                                      .arg(b + 1)
                                      .arg((int)std::round(hz_lo))
                                      .arg((int)std::round(hz_hi))
                                      .arg((int)std::round(center)));
        band_col->applyCaptionStyle();

        float g = saved_eq ? ResampleSavedEqGain(*saved_eq, b, bands) : audio->getEqGain(b);
        audio->setEqGain(b, g);

        QSlider* eq_slider = band_col->gainSlider();
        eq_slider->setValue((int)std::round(g * 100.0f));
        eq_slider->setToolTip(QStringLiteral("Band %1 (%2–%3 Hz): gain 0.0–2.0")
                                  .arg(b + 1)
                                  .arg((int)std::round(hz_lo))
                                  .arg((int)std::round(hz_hi)));
        connect(eq_slider, &QSlider::valueChanged, this, [this, b]() { audioEqChanged(b); });
        audio_eq_sliders.push_back(eq_slider);
        audio_eq_row_layout->addWidget(band_col);
    }

    if(persist_settings)
    {
        try
        {
            settings["AudioEqGain"] = nlohmann::json::array();
            for(int b = 0; b < bands; ++b)
            {
                settings["AudioEqGain"].push_back(audio->getEqGain(b));
            }
            SetPluginSettings(settings);
        }
        catch(const std::exception& e)
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to persist audio EQ gains: %s", e.what());
        }
    }

    audio_eq_rebuilding = false;
}

void OpenRGB3DSpatialTab::audioStartClicked()
{
    AudioInputManager::instance()->start();
    audioStartButton()->setEnabled(false);
    audioStopButton()->setEnabled(true);
}

void OpenRGB3DSpatialTab::audioStopClicked()
{
    AudioInputManager::instance()->stop();
    audioStartButton()->setEnabled(true);
    audioStopButton()->setEnabled(false);
    if(audioLevelBar()) audioLevelBar()->setValue(0);
    if(audioSpectrumLabel())
    {
        audioSpectrumLabel()->setPixmap(QPixmap());
        audioSpectrumLabel()->setText("Start listening to see spectrum");
    }
    if(audioBassBar()) audioBassBar()->setValue(0);
    if(audioMidBar())  audioMidBar()->setValue(0);
    if(audioHighBar()) audioHighBar()->setValue(0);
}

void OpenRGB3DSpatialTab::audioRestoreDefaultsClicked()
{
    if(audioDeviceCombo() && audioDeviceCombo()->isEnabled() && audioDeviceCombo()->count() > 0)
    {
        bool restore_signals = audioDeviceCombo()->blockSignals(true);
        audioDeviceCombo()->setCurrentIndex(0);
        audioDeviceCombo()->blockSignals(restore_signals);
        audioDeviceChanged(0);
    }
    if(audioGainSlider())
    {
        bool restore_signals = audioGainSlider()->blockSignals(true);
        audioGainSlider()->setValue(100);
        audioGainSlider()->blockSignals(restore_signals);
        audioGainChanged(100);
    }
    if(audioBandsCombo())
    {
        int idx = audioBandsCombo()->findText("16");
        if(idx >= 0)
        {
            bool restore_signals = audioBandsCombo()->blockSignals(true);
            audioBandsCombo()->setCurrentIndex(idx);
            audioBandsCombo()->blockSignals(restore_signals);
            audioBandsChanged(idx);
        }
    }
    if(audioFftCombo())
    {
        int idx = audioFftCombo()->findText("512");
        if(idx >= 0)
        {
            bool restore_signals = audioFftCombo()->blockSignals(true);
            audioFftCombo()->setCurrentIndex(idx);
            audioFftCombo()->blockSignals(restore_signals);
            audioFftChanged(idx);
        }
    }
    AudioInputManager::instance()->resetEq();
    AudioAdvancedSettingsDialog::resetToFactoryDefaults();
    nlohmann::json settings = GetPluginSettings();
    settings["AudioDeviceIndex"] = 0;
    settings["AudioGain"] = 100;
    settings["AudioMixClarity"] = 60;
    if(audioClaritySlider())
    {
        QSignalBlocker block(*audioClaritySlider());
        audioClaritySlider()->setValue(60);
        audioClarityChanged(60);
    }
    settings["AudioBandIsolation"] = 62;
    if(audioIsolationSlider())
    {
        QSignalBlocker block(*audioIsolationSlider());
        audioIsolationSlider()->setValue(62);
        audioIsolationChanged(62);
    }
    settings["AudioEqMixPreset"] = 8;
    if(audioMixPresetCombo())
    {
        QSignalBlocker block(*audioMixPresetCombo());
        audioMixPresetCombo()->setCurrentIndex(8);
        audioMixPresetChanged(8);
    }
    settings["AudioBands"] = 16;
    settings["AudioFFTSize"] = 512;
    AudioAdvancedSettingsDialog::writeToPluginSettings(settings);
    sync_audio_eq_sliders_from_manager();
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::audioEqChanged(int band_index)
{
    if(audio_eq_rebuilding || band_index < 0 || band_index >= static_cast<int>(audio_eq_sliders.size()))
    {
        return;
    }
    QSlider* slider = audio_eq_sliders[(size_t)band_index];
    if(!slider)
    {
        return;
    }
    float g = std::clamp(slider->value() / 100.0f, 0.0f, 2.0f);
    AudioInputManager::instance()->setEqGain(band_index, g);

    nlohmann::json settings = GetPluginSettings();
    if(!settings.contains("AudioEqGain") || !settings["AudioEqGain"].is_array())
    {
        settings["AudioEqGain"] = nlohmann::json::array();
    }
    while(static_cast<int>(settings["AudioEqGain"].size()) <= band_index)
    {
        settings["AudioEqGain"].push_back(1.0f);
    }
    settings["AudioEqGain"][band_index] = g;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::audioLevelUpdated(float level)
{
    if(audio_eq_rebuilding)
    {
        return;
    }
    if(audioLevelBar())
    {
        int v = (int)(level * 1000.0f);
        audioLevelBar()->setValue(v);
    }
    if(AudioInputManager::instance()->isRunning())
    {
        if(audioBassBar())
            audioBassBar()->setValue((int)(AudioInputManager::instance()->getBassLevel() * 1000.0f));
        if(audioMidBar())
            audioMidBar()->setValue((int)(AudioInputManager::instance()->getMidLevel() * 1000.0f));
        if(audioHighBar())
            audioHighBar()->setValue((int)(AudioInputManager::instance()->getTrebleLevel() * 1000.0f));
        const AudioInputManager::StreamStemLevels stems = AudioInputManager::instance()->getStreamStemLevels();
        if(audioKickStemBar())
            audioKickStemBar()->setValue((int)(stems.kick * 1000.0f));
        if(audioSnareStemBar())
            audioSnareStemBar()->setValue((int)(stems.snare * 1000.0f));
        if(audioHihatStemBar())
            audioHihatStemBar()->setValue((int)(stems.hihat * 1000.0f));
        if(audioBassStemBar())
            audioBassStemBar()->setValue((int)(stems.bass * 1000.0f));
    }
    else
    {
        if(audioBassBar()) audioBassBar()->setValue(0);
        if(audioMidBar())  audioMidBar()->setValue(0);
        if(audioHighBar()) audioHighBar()->setValue(0);
        if(audioKickStemBar()) audioKickStemBar()->setValue(0);
        if(audioSnareStemBar()) audioSnareStemBar()->setValue(0);
        if(audioHihatStemBar()) audioHihatStemBar()->setValue(0);
        if(audioBassStemBar()) audioBassStemBar()->setValue(0);
    }
    if(audioSpectrumLabel() && !audio_eq_rebuilding && AudioInputManager::instance()->isRunning())
    {
        constexpr int kSpectrumW = 280;
        constexpr int kSpectrumH = 48;
        std::vector<float> bands;
        AudioInputManager::instance()->getBands(bands);
        if(!bands.empty())
        {
            QImage img(kSpectrumW, kSpectrumH, QImage::Format_RGB32);
            img.fill(qRgb(26, 26, 26));
            const int n = static_cast<int>(bands.size());
            const int bar_w = std::max(1, (kSpectrumW - 2) / n);
            QPainter p(&img);
            for(int i = 0; i < n && (i * bar_w) < kSpectrumW; i++)
            {
                float v = std::min(1.0f, std::max(0.0f, bands[i]));
                int bh = (int)((float)(kSpectrumH - 2) * v);
                if(bh > 0)
                {
                    int x = 1 + i * bar_w;
                    p.fillRect(x, kSpectrumH - 1 - bh, std::max(1, bar_w - 1), bh, QColor(80, 180, 255));
                }
            }
            audioSpectrumLabel()->setPixmap(QPixmap::fromImage(img));
            audioSpectrumLabel()->setText(QString());
        }
    }
    else if(audioSpectrumLabel() && !AudioInputManager::instance()->isRunning())
    {
        audioSpectrumLabel()->setPixmap(QPixmap());
        audioSpectrumLabel()->setText("Start listening to see spectrum");
    }
}

void OpenRGB3DSpatialTab::audioDeviceChanged(int index)
{
    AudioInputManager::instance()->setDeviceByIndex(index);

    nlohmann::json settings = GetPluginSettings();
    settings["AudioDeviceIndex"] = index;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::audioGainChanged(int value)
{
    float g = std::max(0.1f, std::min(50.0f, value / 10.0f));
    AudioInputManager::instance()->setGain(g);

    if(audioGainValueLabel())
    {
        audioGainValueLabel()->setText(QString::number(g, 'f', (g >= 10.0f ? 0 : 1)) + "x");
    }

    nlohmann::json settings = GetPluginSettings();
    settings["AudioGain"] = value;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::audioClarityChanged(int value)
{
    value = std::max(0, std::min(100, value));
    AudioInputManager::instance()->setMixClarity(value / 100.0f);

    if(audioClarityValueLabel())
    {
        audioClarityValueLabel()->setText(QStringLiteral("%1%").arg(value));
    }

    nlohmann::json settings = GetPluginSettings();
    settings["AudioMixClarity"] = value;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::audioBandsChanged(int index)
{
    if(!audioBandsCombo() || index < 0 || index >= audioBandsCombo()->count())
    {
        return;
    }

    int bands = audioBandsCombo()->itemText(index).toInt();
    if(bands <= 0)
    {
        return;
    }
    AudioInputManager::instance()->setBandsCount(bands);
    rebuildAudioEqSliders();

    nlohmann::json settings = GetPluginSettings();
    settings["AudioBands"] = bands;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::audioFftChanged(int)
{
    if(!audioFftCombo())
    {
        return;
    }

    int n = audioFftCombo()->currentText().toInt();
    AudioInputManager::instance()->setFFTSize(n);
    rebuildAudioEqSliders();

    nlohmann::json settings = GetPluginSettings();
    settings["AudioFFTSize"] = n;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::audioIsolationChanged(int value)
{
    value = std::max(0, std::min(100, value));
    AudioInputManager::instance()->setBandIsolation(value / 100.0f);

    if(audioIsolationValueLabel())
    {
        audioIsolationValueLabel()->setText(QStringLiteral("%1%").arg(value));
    }

    nlohmann::json settings = GetPluginSettings();
    settings["AudioBandIsolation"] = value;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::sync_audio_eq_sliders_from_manager()
{
    if(audio_eq_rebuilding)
    {
        return;
    }

    const int bands = AudioInputManager::instance()->getEqBandCount();
    if(static_cast<int>(audio_eq_sliders.size()) != bands)
    {
        rebuildAudioEqSliders(false);
        return;
    }

    try
    {
        nlohmann::json settings = GetPluginSettings();
        settings["AudioEqGain"] = nlohmann::json::array();
        for(int b = 0; b < bands; ++b)
        {
            float g = AudioInputManager::instance()->getEqGain(b);
            if(b < static_cast<int>(audio_eq_sliders.size()) && audio_eq_sliders[(size_t)b])
            {
                QSignalBlocker block(*audio_eq_sliders[(size_t)b]);
                audio_eq_sliders[(size_t)b]->setValue((int)std::round(g * 100.0f));
            }
            settings["AudioEqGain"].push_back(g);
        }
        SetPluginSettings(settings);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to restore audio EQ gains: %s", e.what());
    }
}

void OpenRGB3DSpatialTab::audioMixPresetChanged(int index)
{
    if(!audioMixPresetCombo() || index < 0 || index >= audioMixPresetCombo()->count())
    {
        return;
    }

    const int preset = audioMixPresetCombo()->itemData(index).toInt();
    AudioInputManager::instance()->applyEqMixPreset(preset);
    sync_audio_eq_sliders_from_manager();

    nlohmann::json settings = GetPluginSettings();
    settings["AudioEqMixPreset"] = preset;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::UpdateAudioPanelVisibility()
{
    if(!ui || !ui->audioInputPanel)
    {
        return;
    }

    bool show = false;
    for(const std::unique_ptr<EffectInstance3D>& inst_ptr : effect_stack)
    {
        const EffectInstance3D* inst = inst_ptr.get();
        if(inst && IsAudioEffectClass(inst->effect_class_name))
        {
            show = true;
            break;
        }
    }

    ui->audioInputPanel->setVisible(show);
}

bool OpenRGB3DSpatialTab::IsAudioEffectClass(const std::string& class_name) const
{
    if(class_name.empty())
    {
        return false;
    }

    EffectRegistration3D info = EffectListManager3D::get()->GetEffectInfo(class_name);
    return info.category == "Audio";
}

