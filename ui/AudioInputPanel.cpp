// SPDX-License-Identifier: GPL-2.0-only

#include "AudioInputPanel.h"
#include "AudioAdvancedSettingsDialog.h"
#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "EffectInfoLabel.h"
#include "EffectLabeledComboRow.h"
#include "EffectSectionHeading.h"
#include "EffectSliderRow.h"
#include "PluginUiUtils.h"
#include "ui_AudioInputPanel.h"

#include <QComboBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSignalBlocker>
#include <QSizePolicy>

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
    ui->bassBar->setFixedHeight(12);
    ui->midBar->setFixedHeight(12);
    ui->highBar->setFixedHeight(12);
    ui->kickStemBar->setFixedHeight(10);
    ui->snareStemBar->setFixedHeight(10);
    ui->hihatStemBar->setFixedHeight(10);
    ui->bassStemBar->setFixedHeight(10);
    const QString gain_tip = QStringLiteral(
        "Master capture level for all audio effects and the spectrum. "
        "Per-effect strength is Effect sensitivity on each audio effect.");
    const QString clarity_tip = QStringLiteral(
        "Separates instruments in the analyzer (reduces muddy bleed between bands). "
        "Affects all effects; does not replace per-effect smoothing.");
    const QString isolation_tip = QStringLiteral(
        "How tightly each effect's Hz range is separated from neighboring bands. "
        "Set once here for every audio effect (not duplicated on individual effects).");

    ui->gainRow->setCaptionText(QStringLiteral("Input gain:"));
    ui->gainRow->setValueLabelMinimumWidth(48);
    ui->gainRow->configure(1, 500, 100, gain_tip);

    ui->clarityRow->setCaptionText(QStringLiteral("Mix clarity:"));
    ui->clarityRow->setValueLabelMinimumWidth(40);
    ui->clarityRow->configure(0, 100, 60, clarity_tip);

    ui->isolationRow->setCaptionText(QStringLiteral("Band isolation:"));
    ui->isolationRow->setValueLabelMinimumWidth(40);
    ui->isolationRow->configure(0, 100, 55, isolation_tip);

    ui->deviceRow->setCaptionText(QStringLiteral("Input Device:"));
    ui->deviceRow->combo()->setMinimumWidth(200);
    ui->deviceRow->combo()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ui->mixPresetRow->setCaptionText(QStringLiteral("Mix EQ preset:"));
    ui->mixPresetRow->combo()->setToolTip(QStringLiteral(
        "Mixing-board style EQ curve on the per-band sliders below (8/16/32 bands). "
        "Shapes what the analyzer hears; pair with Band isolation and each effect's Hz preset."));

    ui->bandsRow->setCaptionText(QStringLiteral("Bands:"));
    ui->fftRow->setCaptionText(QStringLiteral("FFT Size:"));

    ui->eqHeading->setTitle(QStringLiteral("Equalizer:"));

    ui->helpLabel->setText(QStringLiteral(
        "Bands = analyzer + EQ slider count (8/16/32). Spectrum preview matches that count. "
        "For Spotify/YouTube: try 16 bands, Mix EQ \"Streaming music\", isolation ~55–70%, "
        "effect Preset \"Stream: kick/snare/hat/bass\". Stem meters are heuristic, not AI."));

    connect(ui->startButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::audioStartClicked);
    connect(ui->stopButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::audioStopClicked);
    connect(ui->advancedSettingsButton, &QPushButton::clicked, tab, [tab]() {
        AudioAdvancedSettingsDialog::run(tab, tab);
    });
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
    connect(ui->clarityRow->slider(), &QSlider::valueChanged, tab, &OpenRGB3DSpatialTab::audioClarityChanged);
    connect(ui->isolationRow->slider(), &QSlider::valueChanged, tab, &OpenRGB3DSpatialTab::audioIsolationChanged);

    ui->mixPresetRow->combo()->addItem(QStringLiteral("Flat (unity)"), 0);
    ui->mixPresetRow->combo()->addItem(QStringLiteral("Isolate kick"), 1);
    ui->mixPresetRow->combo()->addItem(QStringLiteral("Isolate snare"), 2);
    ui->mixPresetRow->combo()->addItem(QStringLiteral("Hi-hat / cymbals"), 3);
    ui->mixPresetRow->combo()->addItem(QStringLiteral("Bass (cut sub hum)"), 4);
    ui->mixPresetRow->combo()->addItem(QStringLiteral("Full drum kit"), 5);
    ui->mixPresetRow->combo()->addItem(QStringLiteral("Cut muddy low-mid"), 6);
    ui->mixPresetRow->combo()->addItem(QStringLiteral("Reduce vocals (experimental)"), 7);
    ui->mixPresetRow->combo()->addItem(QStringLiteral("Streaming music (Spotify/YT)"), 8);
    connect(ui->mixPresetRow->combo(), QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::audioMixPresetChanged);

    ui->bandsRow->combo()->addItems({QStringLiteral("8"), QStringLiteral("16"), QStringLiteral("32")});
    ui->bandsRow->combo()->setCurrentText(QStringLiteral("16"));
    connect(ui->bandsRow->combo(), QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::audioBandsChanged);

    ui->fftRow->combo()->addItems({QStringLiteral("512"), QStringLiteral("1024"), QStringLiteral("2048"),
                                   QStringLiteral("4096"), QStringLiteral("8192")});
    {
        const int cur_fft = AudioInputManager::instance()->getFFTSize();
        const int idx     = ui->fftRow->combo()->findText(QString::number(cur_fft));
        if(idx >= 0)
        {
            ui->fftRow->combo()->setCurrentIndex(idx);
        }
    }
    connect(ui->fftRow->combo(), QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::audioFftChanged);

    {
        const int bands = ui->bandsRow->combo()->currentText().toInt();
        if(bands == 8 || bands == 16 || bands == 32)
        {
            AudioInputManager::instance()->setBandsCount(bands);
        }
    }
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

    if(settings.contains("AudioMixClarity"))
    {
        int cv = settings["AudioMixClarity"].get<int>();
        cv     = std::max(0, std::min(100, cv));
        const QSignalBlocker block(ui->clarityRow->slider());
        ui->clarityRow->slider()->setValue(cv);
        tab->audioClarityChanged(cv);
    }
    else
    {
        tab->audioClarityChanged(ui->clarityRow->slider()->value());
    }

    if(settings.contains("AudioBandIsolation"))
    {
        int iv = settings["AudioBandIsolation"].get<int>();
        iv     = std::max(0, std::min(100, iv));
        const QSignalBlocker block(ui->isolationRow->slider());
        ui->isolationRow->slider()->setValue(iv);
        tab->audioIsolationChanged(iv);
    }
    else
    {
        tab->audioIsolationChanged(ui->isolationRow->slider()->value());
    }

    if(settings.contains("AudioBands"))
    {
        const int bc   = settings["AudioBands"].get<int>();
        const int bidx = ui->bandsRow->combo()->findText(QString::number(bc));
        if(bidx >= 0)
        {
            const QSignalBlocker block(ui->bandsRow->combo());
            ui->bandsRow->combo()->setCurrentIndex(bidx);
            tab->audioBandsChanged(bidx);
        }
    }

    if(settings.contains("AudioFFTSize"))
    {
        const int n    = settings["AudioFFTSize"].get<int>();
        const int fidx = ui->fftRow->combo()->findText(QString::number(n));
        if(fidx >= 0)
        {
            const QSignalBlocker block(ui->fftRow->combo());
            ui->fftRow->combo()->setCurrentIndex(fidx);
            tab->audioFftChanged(fidx);
        }
    }

    if(settings.contains("AudioEqMixPreset"))
    {
        int pidx = settings["AudioEqMixPreset"].get<int>();
        pidx     = std::clamp(pidx, 0, ui->mixPresetRow->combo()->count() - 1);
        const QSignalBlocker block(ui->mixPresetRow->combo());
        ui->mixPresetRow->combo()->setCurrentIndex(pidx);
        tab->audioMixPresetChanged(pidx);
    }

    if(settings.contains("AudioEqGain") && settings["AudioEqGain"].is_array())
    {
        try
        {
            const nlohmann::json& arr  = settings["AudioEqGain"];
            const int             bands = AudioInputManager::instance()->getEqBandCount();
            for(int b = 0; b < bands; ++b)
            {
                AudioInputManager::instance()->setEqGain(b, ResampleSavedEqGain(arr, b, bands));
            }
        }
        catch(const std::exception&)
        {
        }
        tab->sync_audio_eq_sliders_from_manager();
    }

    AudioAdvancedSettingsDialog::applyFromPluginSettings(tab->GetPluginSettings());

    setVisible(false);
}

QPushButton* AudioInputPanel::startButton() const { return ui->startButton; }
QPushButton* AudioInputPanel::stopButton() const { return ui->stopButton; }
QProgressBar* AudioInputPanel::levelBar() const { return ui->levelBar; }
QProgressBar* AudioInputPanel::bassBar() const { return ui->bassBar; }
QProgressBar* AudioInputPanel::midBar() const { return ui->midBar; }
QProgressBar* AudioInputPanel::highBar() const { return ui->highBar; }
QProgressBar* AudioInputPanel::kickStemBar() const { return ui->kickStemBar; }
QProgressBar* AudioInputPanel::snareStemBar() const { return ui->snareStemBar; }
QProgressBar* AudioInputPanel::hihatStemBar() const { return ui->hihatStemBar; }
QProgressBar* AudioInputPanel::bassStemBar() const { return ui->bassStemBar; }
QLabel* AudioInputPanel::spectrumLabel() const { return ui->spectrumLabel; }
QComboBox* AudioInputPanel::deviceCombo() const { return ui->deviceRow->combo(); }
QSlider* AudioInputPanel::gainSlider() const { return ui->gainRow->slider(); }
QLabel* AudioInputPanel::gainValueLabel() const { return ui->gainRow->valueLabel(); }
QSlider* AudioInputPanel::claritySlider() const { return ui->clarityRow->slider(); }
QLabel* AudioInputPanel::clarityValueLabel() const { return ui->clarityRow->valueLabel(); }
QSlider* AudioInputPanel::isolationSlider() const { return ui->isolationRow->slider(); }
QLabel* AudioInputPanel::isolationValueLabel() const { return ui->isolationRow->valueLabel(); }
QComboBox* AudioInputPanel::mixPresetCombo() const { return ui->mixPresetRow->combo(); }
QComboBox* AudioInputPanel::bandsCombo() const { return ui->bandsRow->combo(); }
QComboBox* AudioInputPanel::fftCombo() const { return ui->fftRow->combo(); }
QLabel* AudioInputPanel::eqCaption() const { return ui->eqHeading->titleLabel(); }
QScrollArea* AudioInputPanel::eqScroll() const { return ui->eqScroll; }
