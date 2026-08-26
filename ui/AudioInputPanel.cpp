// SPDX-License-Identifier: GPL-2.0-only

#include "AudioInputPanel.h"
#include "AudioAnalyzerSettings.h"
#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "EffectInfoLabel.h"
#include "EffectLabeledComboRow.h"
#include "EffectSectionHeading.h"
#include "EffectSliderRow.h"
#include "PluginUiUtils.h"
#include "ui_AudioInputPanel.h"
#include "PluginLog.h"

#include <QComboBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSignalBlocker>
#include <QSizePolicy>

#include <algorithm>
#include <cmath>

AudioInputPanel::AudioInputPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::AudioInputPanel)
{
    ui->setupUi(this);
}

AudioInputPanel::~AudioInputPanel()
{
    delete ui;
}

void AudioInputPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

    ui->spectrumLabel->setScaledContents(false);
    ui->levelBar->setFixedHeight(14);

    const QString amp_tip = QStringLiteral(
        "Capture amplitude (OpenRGB-style). Master level for all audio effects.");
    const QString decay_tip = QStringLiteral(
        "How fast analyzer peaks fall (OpenRGB “Decay”). Higher = smoother, less twitchy.");

    ui->gainRow->setCaptionText(QStringLiteral("Amplitude:"));
    ui->gainRow->setValueLabelMinimumWidth(48);
    ui->gainRow->configure(1, 500, 100, amp_tip);

    ui->decayRow->setCaptionText(QStringLiteral("Decay:"));
    ui->decayRow->setValueLabelMinimumWidth(40);
    ui->decayRow->configure(0, 99, 80, decay_tip);

    ui->deviceRow->setCaptionText(QStringLiteral("Audio device:"));
    ui->deviceRow->combo()->setMinimumWidth(200);
    ui->deviceRow->combo()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ui->eqHeading->setTitle(QStringLiteral("Equalizer"));

    ui->helpLabel->setText(QStringLiteral(
        "Same idea as OpenRGB Effects: pick a device, set Amplitude / Decay, shape the EQ. "
        "Each effect then chooses its Listen role."));

    connect(ui->startButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::audioStartClicked);
    connect(ui->stopButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::audioStopClicked);
    connect(ui->restoreDefaultsButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::audioRestoreDefaultsClicked);
    connect(AudioInputManager::instance(), &AudioInputManager::LevelUpdated, tab,
            &OpenRGB3DSpatialTab::audioLevelUpdated);

    const QStringList devs = AudioInputManager::instance()->listInputDevices();
    if(devs.isEmpty())
    {
        ui->deviceRow->combo()->addItem(QStringLiteral("No input devices detected"));
        ui->deviceRow->combo()->setEnabled(false);
    }
    else
    {
        ui->deviceRow->combo()->addItems(devs);
        connect(ui->deviceRow->combo(), QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
                &OpenRGB3DSpatialTab::audioDeviceChanged);
        ui->deviceRow->combo()->setCurrentIndex(0);
        tab->audioDeviceChanged(0);
    }

    connect(ui->gainRow->slider(), &QSlider::valueChanged, tab, &OpenRGB3DSpatialTab::audioGainChanged);

    connect(ui->decayRow->slider(), &QSlider::valueChanged, tab, [tab](int value) {
        value = std::clamp(value, 0, 99);
        AudioInputManager::instance()->setSmoothing(value / 100.0f);
        nlohmann::json settings = tab->GetPluginSettings();
        settings["AudioSmoothingPct"] = value;
        tab->SetPluginSettings(settings);
    });

    /* Locked analyzer shape: 16 bands / 512 FFT. */
    AudioInputManager::instance()->setBandsCount(16);
    AudioInputManager::instance()->setFFTSize(512);
    tab->rebuildAudioEqSliders(false);
    connect(ui->eqResetButton, &QPushButton::clicked, tab, [tab]() {
        AudioInputManager::instance()->resetEq();
        tab->sync_audio_eq_sliders_from_manager();
    });

    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());

    nlohmann::json settings = tab->GetPluginSettings();
    if(ui->deviceRow->combo()->isEnabled() && settings.contains("AudioDeviceIndex"))
    {
        const int di = settings["AudioDeviceIndex"].get<int>();
        if(di >= 0 && di < ui->deviceRow->combo()->count())
        {
            const QSignalBlocker block(ui->deviceRow->combo());
            ui->deviceRow->combo()->setCurrentIndex(di);
            tab->audioDeviceChanged(di);
        }
    }

    if(settings.contains("AudioGain"))
    {
        int gv = settings["AudioGain"].get<int>();
        gv     = std::max(1, std::min(500, gv));
        const QSignalBlocker block(ui->gainRow->slider());
        ui->gainRow->slider()->setValue(gv);
        tab->audioGainChanged(gv);
    }
    else
    {
        tab->audioGainChanged(ui->gainRow->slider()->value());
    }

    if(settings.contains("AudioSmoothingPct"))
    {
        int dv = settings["AudioSmoothingPct"].get<int>();
        dv = std::max(0, std::min(99, dv));
        const QSignalBlocker block(ui->decayRow->slider());
        ui->decayRow->slider()->setValue(dv);
        AudioInputManager::instance()->setSmoothing(dv / 100.0f);
    }
    else
    {
        const int dv = ui->decayRow->slider()->value();
        AudioInputManager::instance()->setSmoothing(dv / 100.0f);
    }

    AudioInputManager::instance()->setBandsCount(16);
    AudioInputManager::instance()->setFFTSize(512);
    tab->rebuildAudioEqSliders(false);

    if(settings.contains("AudioEqGain") && settings["AudioEqGain"].is_array())
    {
        try
        {
            const nlohmann::json& arr  = settings["AudioEqGain"];
            const int             bands = AudioInputManager::instance()->getEqBandCount();
            for(int b = 0; b < bands; ++b)
            {
                AudioInputManager::instance()->setEqGain(
                    b, AudioAnalyzerSettings::resampleSavedEqGain(arr, b, bands));
            }
        }
        catch(const std::exception& e)
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to restore AudioEqGain: %s", e.what());
        }
        tab->sync_audio_eq_sliders_from_manager();
    }

    AudioAnalyzerSettings::applyFromPluginSettings(tab->GetPluginSettings());

    setVisible(false);
}

QPushButton* AudioInputPanel::startButton() const { return ui->startButton; }
QPushButton* AudioInputPanel::stopButton() const { return ui->stopButton; }
QProgressBar* AudioInputPanel::levelBar() const { return ui->levelBar; }
QLabel* AudioInputPanel::spectrumLabel() const { return ui->spectrumLabel; }
QComboBox* AudioInputPanel::deviceCombo() const { return ui->deviceRow->combo(); }
QSlider* AudioInputPanel::gainSlider() const { return ui->gainRow->slider(); }
QLabel* AudioInputPanel::gainValueLabel() const { return ui->gainRow->valueLabel(); }
QSlider* AudioInputPanel::decaySlider() const { return ui->decayRow->slider(); }
QLabel* AudioInputPanel::eqCaption() const { return ui->eqHeading->titleLabel(); }
QScrollArea* AudioInputPanel::eqScroll() const { return ui->eqScroll; }
