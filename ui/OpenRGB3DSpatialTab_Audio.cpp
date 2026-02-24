/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_Audio.cpp                             |
|                                                           |
|   Audio input management and frequency range effects      |
|                                                           |
|   Date: 2026-01-27                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "EffectListManager3D.h"
#include "EffectInstance3D.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include <nlohmann/json.hpp>

void OpenRGB3DSpatialTab::SetupAudioPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* audio_group = new QGroupBox("Audio Input");
    audio_panel_group = audio_group;
    QVBoxLayout* layout = new QVBoxLayout(audio_group);

    /*---------------------------------------------------------*\
    | Start/Stop Controls                                       |
    \*---------------------------------------------------------*/
    QHBoxLayout* top_controls = new QHBoxLayout();
    audio_start_button = new QPushButton("Start Listening");
    audio_stop_button = new QPushButton("Stop");
    audio_stop_button->setEnabled(false);
    top_controls->addWidget(audio_start_button);
    top_controls->addWidget(audio_stop_button);
    top_controls->addStretch();
    layout->addLayout(top_controls);

    connect(audio_start_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_start_clicked);
    connect(audio_stop_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_stop_clicked);
    connect(AudioInputManager::instance(), &AudioInputManager::LevelUpdated, this, &OpenRGB3DSpatialTab::on_audio_level_updated);

    /*---------------------------------------------------------*\
    | Level Meter                                               |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Level:"));
    audio_level_bar = new QProgressBar();
    audio_level_bar->setRange(0, 1000);
    audio_level_bar->setValue(0);
    audio_level_bar->setTextVisible(false);
    audio_level_bar->setFixedHeight(14);
    layout->addWidget(audio_level_bar);

    /*---------------------------------------------------------*\
    | Device Selection                                          |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Input Device:"));
    audio_device_combo = new QComboBox();
    audio_device_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_device_combo->setMinimumWidth(200);
    
    QStringList devs = AudioInputManager::instance()->listInputDevices();
    if(devs.isEmpty())
    {
        audio_device_combo->addItem("No input devices detected");
        audio_device_combo->setEnabled(false);
    }
    else
    {
        audio_device_combo->addItems(devs);
        connect(audio_device_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
                this, &OpenRGB3DSpatialTab::on_audio_device_changed);
        audio_device_combo->setCurrentIndex(0);
        on_audio_device_changed(0);
    }
    layout->addWidget(audio_device_combo);

    /*---------------------------------------------------------*\
    | Gain Control                                              |
    \*---------------------------------------------------------*/
    QHBoxLayout* gain_layout = new QHBoxLayout();
    gain_layout->addWidget(new QLabel("Gain:"));
    audio_gain_slider = new QSlider(Qt::Horizontal);
    audio_gain_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_gain_slider->setRange(1, 100);
    audio_gain_slider->setValue(10);
    connect(audio_gain_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_gain_changed);
    gain_layout->addWidget(audio_gain_slider);
    
    audio_gain_value_label = new QLabel("1.0x");
    audio_gain_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    audio_gain_value_label->setMinimumWidth(48);
    gain_layout->addWidget(audio_gain_value_label);
    layout->addLayout(gain_layout);

    /*---------------------------------------------------------*\
    | Bands Selection                                           |
    \*---------------------------------------------------------*/
    QHBoxLayout* bands_layout = new QHBoxLayout();
    bands_layout->addWidget(new QLabel("Bands:"));
    audio_bands_combo = new QComboBox();
    audio_bands_combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    audio_bands_combo->addItems({"8", "16", "32"});
    audio_bands_combo->setCurrentText("16");
    connect(audio_bands_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &OpenRGB3DSpatialTab::on_audio_bands_changed);
    bands_layout->addWidget(audio_bands_combo);
    bands_layout->addStretch();
    layout->addLayout(bands_layout);

    /*---------------------------------------------------------*\
    | FFT Size                                                  |
    \*---------------------------------------------------------*/
    QHBoxLayout* fft_layout = new QHBoxLayout();
    fft_layout->addWidget(new QLabel("FFT Size:"));
    audio_fft_combo = new QComboBox();
    audio_fft_combo->addItems({"512","1024","2048","4096","8192"});
    int cur_fft = AudioInputManager::instance()->getFFTSize();
    int idx = audio_fft_combo->findText(QString::number(cur_fft));
    if(idx >= 0) audio_fft_combo->setCurrentIndex(idx);
    connect(audio_fft_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &OpenRGB3DSpatialTab::on_audio_fft_changed);
    fft_layout->addWidget(audio_fft_combo);
    fft_layout->addStretch();
    layout->addLayout(fft_layout);

    /*---------------------------------------------------------*\
    | Help Text                                                 |
    \*---------------------------------------------------------*/
    QLabel* help = new QLabel("Configure audio input for frequency range effects below.");
    help->setStyleSheet("color: gray; font-size: 10px;");
    help->setWordWrap(true);
    layout->addWidget(help);
    
    /*---------------------------------------------------------*\
    | Frequency Range Effects (Multi-Band System)               |
    \*---------------------------------------------------------*/
    SetupFrequencyRangeEffectsUI(layout);

    /*---------------------------------------------------------*\
    | Load Saved Settings                                       |
    \*---------------------------------------------------------*/
    nlohmann::json settings = GetPluginSettings();
    
    if(audio_device_combo && audio_device_combo->isEnabled() && settings.contains("AudioDeviceIndex"))
    {
        int di = settings["AudioDeviceIndex"].get<int>();
        if(di >= 0 && di < audio_device_combo->count())
        {
            audio_device_combo->blockSignals(true);
            audio_device_combo->setCurrentIndex(di);
            audio_device_combo->blockSignals(false);
            on_audio_device_changed(di);
        }
    }
    
    if(audio_gain_slider && settings.contains("AudioGain"))
    {
        int gv = settings["AudioGain"].get<int>();
        gv = std::max(1, std::min(100, gv));
        audio_gain_slider->blockSignals(true);
        audio_gain_slider->setValue(gv);
        audio_gain_slider->blockSignals(false);
        on_audio_gain_changed(gv);
    }
    
    if(audio_bands_combo && settings.contains("AudioBands"))
    {
        int bc = settings["AudioBands"].get<int>();
        int bidx = audio_bands_combo->findText(QString::number(bc));
        if(bidx >= 0)
        {
            audio_bands_combo->blockSignals(true);
            audio_bands_combo->setCurrentIndex(bidx);
            audio_bands_combo->blockSignals(false);
            on_audio_bands_changed(bidx);
        }
    }
    
    if(audio_fft_combo && settings.contains("AudioFFTSize"))
    {
        int n = settings["AudioFFTSize"].get<int>();
        int fidx = audio_fft_combo->findText(QString::number(n));
        if(fidx >= 0)
        {
            audio_fft_combo->blockSignals(true);
            audio_fft_combo->setCurrentIndex(fidx);
            audio_fft_combo->blockSignals(false);
            on_audio_fft_changed(fidx);
        }
    }

    layout->addStretch();
    parent_layout->addWidget(audio_group);
    audio_group->setVisible(false); // shown only when an audio effect is selected
}

void OpenRGB3DSpatialTab::on_audio_start_clicked()
{
    AudioInputManager::instance()->start();
    audio_start_button->setEnabled(false);
    audio_stop_button->setEnabled(true);
}

void OpenRGB3DSpatialTab::on_audio_stop_clicked()
{
    AudioInputManager::instance()->stop();
    audio_start_button->setEnabled(true);
    audio_stop_button->setEnabled(false);
    if(audio_level_bar) audio_level_bar->setValue(0);
}

void OpenRGB3DSpatialTab::on_audio_level_updated(float level)
{
    if(!audio_level_bar) return;
    int v = (int)(level * 1000.0f);
    audio_level_bar->setValue(v);
}

void OpenRGB3DSpatialTab::on_audio_device_changed(int index)
{
    AudioInputManager::instance()->setDeviceByIndex(index);
    
    nlohmann::json settings = GetPluginSettings();
    settings["AudioDeviceIndex"] = index;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::on_audio_gain_changed(int value)
{
    float g = std::max(0.1f, std::min(10.0f, value / 10.0f));
    AudioInputManager::instance()->setGain(g);
    
    if(audio_gain_value_label)
    {
        audio_gain_value_label->setText(QString::number(g, 'f', (g < 10.0f ? 1 : 0)) + "x");
    }
    
    nlohmann::json settings = GetPluginSettings();
    settings["AudioGain"] = value;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::on_audio_bands_changed(int index)
{
    int bands = audio_bands_combo->itemText(index).toInt();
    AudioInputManager::instance()->setBandsCount(bands);
    
    nlohmann::json settings = GetPluginSettings();
    settings["AudioBands"] = bands;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::on_audio_fft_changed(int)
{
    if(!audio_fft_combo) return;
    
    int n = audio_fft_combo->currentText().toInt();
    AudioInputManager::instance()->setFFTSize(n);

    nlohmann::json settings = GetPluginSettings();
    settings["AudioFFTSize"] = n;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::UpdateAudioPanelVisibility()
{
    bool show_audio = false;

    if(effect_stack_list)
    {
        int row = effect_stack_list->currentRow();
        if(row >= 0 && row < (int)effect_stack.size())
        {
            EffectInstance3D* inst = effect_stack[row].get();
            if(inst && !inst->effect_class_name.empty())
            {
                EffectRegistration3D info = EffectListManager3D::get()->GetEffectInfo(inst->effect_class_name);
                show_audio = (info.category.compare("Audio") == 0);
            }
        }
    }

    if(audio_panel_group)   audio_panel_group->setVisible(show_audio);
    if(freq_ranges_group)   freq_ranges_group->setVisible(show_audio);
}
