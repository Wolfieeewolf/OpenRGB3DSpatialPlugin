// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "EffectListManager3D.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include <nlohmann/json.hpp>
#include <QToolTip>
#include <QPainter>

void OpenRGB3DSpatialTab::SetupAudioPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* audio_group = new QGroupBox("Audio Input");
    audio_panel_group = audio_group;
    QVBoxLayout* layout = new QVBoxLayout(audio_group);

    QHBoxLayout* top_controls = new QHBoxLayout();
    audio_start_button = new QPushButton("Start Listening");
    audio_stop_button = new QPushButton("Stop");
    audio_stop_button->setEnabled(false);
    QPushButton* restore_defaults_btn = new QPushButton("Restore defaults");
    top_controls->addWidget(audio_start_button);
    top_controls->addWidget(audio_stop_button);
    top_controls->addStretch();
    top_controls->addWidget(restore_defaults_btn);
    layout->addLayout(top_controls);

    connect(audio_start_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_start_clicked);
    connect(audio_stop_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_stop_clicked);
    connect(restore_defaults_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_restore_defaults_clicked);
    connect(AudioInputManager::instance(), &AudioInputManager::LevelUpdated, this, &OpenRGB3DSpatialTab::on_audio_level_updated);

    layout->addWidget(new QLabel("Level:"));
    audio_level_bar = new QProgressBar();
    audio_level_bar->setRange(0, 1000);
    audio_level_bar->setValue(0);
    audio_level_bar->setTextVisible(false);
    audio_level_bar->setFixedHeight(14);
    layout->addWidget(audio_level_bar);

    QHBoxLayout* bmh_row = new QHBoxLayout();
    bmh_row->addWidget(new QLabel("Bass:"));
    audio_bass_bar = new QProgressBar();
    audio_bass_bar->setRange(0, 1000);
    audio_bass_bar->setValue(0);
    audio_bass_bar->setTextVisible(false);
    audio_bass_bar->setFixedHeight(12);
    audio_bass_bar->setStyleSheet("QProgressBar::chunk { background-color: #2060c0; }");
    bmh_row->addWidget(audio_bass_bar, 1);
    bmh_row->addWidget(new QLabel("Mid:"));
    audio_mid_bar = new QProgressBar();
    audio_mid_bar->setRange(0, 1000);
    audio_mid_bar->setValue(0);
    audio_mid_bar->setTextVisible(false);
    audio_mid_bar->setFixedHeight(12);
    audio_mid_bar->setStyleSheet("QProgressBar::chunk { background-color: #20a020; }");
    bmh_row->addWidget(audio_mid_bar, 1);
    bmh_row->addWidget(new QLabel("High:"));
    audio_high_bar = new QProgressBar();
    audio_high_bar->setRange(0, 1000);
    audio_high_bar->setValue(0);
    audio_high_bar->setTextVisible(false);
    audio_high_bar->setFixedHeight(12);
    audio_high_bar->setStyleSheet("QProgressBar::chunk { background-color: #c06020; }");
    bmh_row->addWidget(audio_high_bar, 1);
    layout->addLayout(bmh_row);

    layout->addWidget(new QLabel("Spectrum:"));
    audio_spectrum_label = new QLabel();
    audio_spectrum_label->setFixedSize(280, 48);
    audio_spectrum_label->setMinimumHeight(48);
    audio_spectrum_label->setFrameStyle(QFrame::StyledPanel);
    audio_spectrum_label->setStyleSheet("background-color: #1a1a1a;");
    audio_spectrum_label->setAlignment(Qt::AlignCenter);
    audio_spectrum_label->setText("Start listening to see spectrum");
    layout->addWidget(audio_spectrum_label);

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

    QHBoxLayout* gain_layout = new QHBoxLayout();
    gain_layout->addWidget(new QLabel("Gain:"));
    audio_gain_slider = new QSlider(Qt::Horizontal);
    audio_gain_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_gain_slider->setRange(1, 500);
    audio_gain_slider->setValue(100);  /* default 10x (like Effects Plugin amplitude 100); use 200–500 if still quiet */
    connect(audio_gain_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_gain_changed);
    connect(audio_gain_slider, &QSlider::sliderMoved, [this](int v) {
        float g = std::max(0.1f, std::min(10.0f, v / 10.0f));
        QToolTip::showText(QCursor::pos(), QString("%1x").arg((double)g, 0, 'f', 1), nullptr);
    });
    gain_layout->addWidget(audio_gain_slider);
    audio_gain_value_label = new QLabel("10x");
    audio_gain_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    audio_gain_value_label->setMinimumWidth(48);
    gain_layout->addWidget(audio_gain_value_label);
    layout->addLayout(gain_layout);

    QHBoxLayout* bands_layout = new QHBoxLayout();
    bands_layout->addWidget(new QLabel("Bands:"));
    audio_bands_combo = new QComboBox();
    audio_bands_combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    audio_bands_combo->addItems({"8", "16", "32"});
    audio_bands_combo->setCurrentText("8");  /* 8 bands = more stable with band normalization; 16/32 work better after pipeline fix */
    connect(audio_bands_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OpenRGB3DSpatialTab::on_audio_bands_changed);
    bands_layout->addWidget(audio_bands_combo);
    bands_layout->addStretch();
    layout->addLayout(bands_layout);

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

    QLabel* help = new QLabel("Gain up to 50x. Use per-range EQ in each frequency range to isolate bass/mids/highs (dump other bands).");
    help->setStyleSheet("color: gray; font-size: 10px;");
    help->setWordWrap(true);
    layout->addWidget(help);
    SetupFrequencyRangeEffectsUI(layout);

    nlohmann::json settings = GetPluginSettings();
    if(audio_device_combo && audio_device_combo->isEnabled() && settings.contains("AudioDeviceIndex"))
    {
        int di = settings["AudioDeviceIndex"].get<int>();
        if(di >= 0 && di < audio_device_combo->count())
        {
            bool restore_signals = audio_device_combo->blockSignals(true);
            audio_device_combo->setCurrentIndex(di);
            audio_device_combo->blockSignals(restore_signals);
            on_audio_device_changed(di);
        }
    }

    if(audio_gain_slider && settings.contains("AudioGain"))
    {
        int gv = settings["AudioGain"].get<int>();
        gv = std::max(1, std::min(500, gv));
        bool restore_signals = audio_gain_slider->blockSignals(true);
        audio_gain_slider->setValue(gv);
        audio_gain_slider->blockSignals(restore_signals);
        on_audio_gain_changed(gv);
    }
    else if(audio_gain_slider)
    {
        on_audio_gain_changed(audio_gain_slider->value());  /* apply default gain (10x) to manager */
    }

    if(audio_bands_combo && settings.contains("AudioBands"))
    {
        int bc = settings["AudioBands"].get<int>();
        int bidx = audio_bands_combo->findText(QString::number(bc));
        if(bidx >= 0)
        {
            bool restore_signals = audio_bands_combo->blockSignals(true);
            audio_bands_combo->setCurrentIndex(bidx);
            audio_bands_combo->blockSignals(restore_signals);
            on_audio_bands_changed(bidx);
        }
    }

    if(audio_fft_combo && settings.contains("AudioFFTSize"))
    {
        int n = settings["AudioFFTSize"].get<int>();
        int fidx = audio_fft_combo->findText(QString::number(n));
        if(fidx >= 0)
        {
            bool restore_signals = audio_fft_combo->blockSignals(true);
            audio_fft_combo->setCurrentIndex(fidx);
            audio_fft_combo->blockSignals(restore_signals);
            on_audio_fft_changed(fidx);
        }
    }

    layout->addStretch();
    parent_layout->addWidget(audio_group);
    audio_group->setVisible(false);
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
    if(audio_spectrum_label)
    {
        audio_spectrum_label->setPixmap(QPixmap());
        audio_spectrum_label->setText("Start listening to see spectrum");
    }
    if(audio_bass_bar) audio_bass_bar->setValue(0);
    if(audio_mid_bar)  audio_mid_bar->setValue(0);
    if(audio_high_bar) audio_high_bar->setValue(0);
}

void OpenRGB3DSpatialTab::on_audio_restore_defaults_clicked()
{
    if(audio_device_combo && audio_device_combo->isEnabled() && audio_device_combo->count() > 0)
    {
        bool restore_signals = audio_device_combo->blockSignals(true);
        audio_device_combo->setCurrentIndex(0);
        audio_device_combo->blockSignals(restore_signals);
        on_audio_device_changed(0);
    }
    if(audio_gain_slider)
    {
        bool restore_signals = audio_gain_slider->blockSignals(true);
        audio_gain_slider->setValue(100);
        audio_gain_slider->blockSignals(restore_signals);
        on_audio_gain_changed(100);
    }
    if(audio_bands_combo)
    {
        int idx = audio_bands_combo->findText("8");
        if(idx >= 0)
        {
            bool restore_signals = audio_bands_combo->blockSignals(true);
            audio_bands_combo->setCurrentIndex(idx);
            audio_bands_combo->blockSignals(restore_signals);
            on_audio_bands_changed(idx);
        }
    }
    if(audio_fft_combo)
    {
        int idx = audio_fft_combo->findText("512");
        if(idx >= 0)
        {
            bool restore_signals = audio_fft_combo->blockSignals(true);
            audio_fft_combo->setCurrentIndex(idx);
            audio_fft_combo->blockSignals(restore_signals);
            on_audio_fft_changed(idx);
        }
    }
    nlohmann::json settings = GetPluginSettings();
    settings["AudioDeviceIndex"] = 0;
    settings["AudioGain"] = 100;
    settings["AudioBands"] = 8;
    settings["AudioFFTSize"] = 512;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::on_audio_level_updated(float level)
{
    if(audio_level_bar)
    {
        int v = (int)(level * 1000.0f);
        audio_level_bar->setValue(v);
    }
    if(AudioInputManager::instance()->isRunning())
    {
        if(audio_bass_bar)
            audio_bass_bar->setValue((int)(AudioInputManager::instance()->getBassLevel() * 1000.0f));
        if(audio_mid_bar)
            audio_mid_bar->setValue((int)(AudioInputManager::instance()->getMidLevel() * 1000.0f));
        if(audio_high_bar)
            audio_high_bar->setValue((int)(AudioInputManager::instance()->getTrebleLevel() * 1000.0f));
    }
    else
    {
        if(audio_bass_bar) audio_bass_bar->setValue(0);
        if(audio_mid_bar)  audio_mid_bar->setValue(0);
        if(audio_high_bar) audio_high_bar->setValue(0);
    }
    if(audio_spectrum_label && AudioInputManager::instance()->isRunning())
    {
        int w = audio_spectrum_label->width();
        int h = audio_spectrum_label->height();
        if(w < 16 || h < 8) return;
        auto snapshot = AudioInputManager::instance()->getSpectrumSnapshot(64);
        if(snapshot.bins.empty())
        {
            std::vector<float> bands;
            AudioInputManager::instance()->getBands(bands);
            if(!bands.empty())
            {
                QImage img(w, h, QImage::Format_RGB32);
                img.fill(qRgb(26, 26, 26));
                int n = (int)bands.size();
                int bar_w = std::max(1, (w - 2) / n);
                QPainter p(&img);
                for(int i = 0; i < n && (i * bar_w) < w; i++)
                {
                    float v = std::min(1.0f, std::max(0.0f, bands[i]));
                    int bh = (int)((float)(h - 2) * v);
                    if(bh > 0)
                    {
                        int x = 1 + i * bar_w;
                        p.fillRect(x, h - 1 - bh, std::max(1, bar_w - 1), bh, QColor(80, 180, 255));
                    }
                }
                audio_spectrum_label->setPixmap(QPixmap::fromImage(img));
                audio_spectrum_label->setText(QString());
            }
        }
        else
        {
            QImage img(w, h, QImage::Format_RGB32);
            img.fill(qRgb(26, 26, 26));
            int n = (int)snapshot.bins.size();
            if(n > 0)
            {
                int bar_w = std::max(1, (w - 2) / n);
                QPainter p(&img);
                for(int i = 0; i < n && (i * bar_w) < w; i++)
                {
                    float v = std::min(1.0f, std::max(0.0f, snapshot.bins[i]));
                    int bh = (int)((float)(h - 2) * v);
                    if(bh > 0)
                    {
                        int x = 1 + i * bar_w;
                        p.fillRect(x, h - 1 - bh, std::max(1, bar_w - 1), bh, QColor(80, 180, 255));
                    }
                }
                audio_spectrum_label->setPixmap(QPixmap::fromImage(img));
                audio_spectrum_label->setText(QString());
            }
        }
    }
    else if(audio_spectrum_label && !AudioInputManager::instance()->isRunning())
    {
        audio_spectrum_label->setPixmap(QPixmap());
        audio_spectrum_label->setText("Start listening to see spectrum");
    }
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
    float g = std::max(0.1f, std::min(50.0f, value / 10.0f));
    AudioInputManager::instance()->setGain(g);

    if(audio_gain_value_label)
    {
        audio_gain_value_label->setText(QString::number(g, 'f', (g >= 10.0f ? 0 : 1)) + "x");
    }

    nlohmann::json settings = GetPluginSettings();
    settings["AudioGain"] = value;
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::on_audio_bands_changed(int index)
{
    if(!audio_bands_combo || index < 0 || index >= audio_bands_combo->count())
    {
        return;
    }

    int bands = audio_bands_combo->itemText(index).toInt();
    if(bands <= 0)
    {
        return;
    }
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
    if(!effect_stack_list || !audio_panel_group)
    {
        return;
    }

    int row = effect_stack_list->currentRow();
    bool show = false;

    if(row >= 0 && row < (int)effect_stack.size())
    {
        EffectInstance3D* inst = effect_stack[row].get();
        if(inst)
        {
            show = IsAudioEffectClass(inst->effect_class_name);
        }
    }

    audio_panel_group->setVisible(show);
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

