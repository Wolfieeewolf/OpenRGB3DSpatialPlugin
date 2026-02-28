// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "EffectListManager3D.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include <nlohmann/json.hpp>

void OpenRGB3DSpatialTab::SetupAudioPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* audio_group = new QGroupBox("Audio Input");
    audio_panel_group = audio_group;
    QVBoxLayout* layout = new QVBoxLayout(audio_group);

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

    layout->addWidget(new QLabel("Level:"));
    audio_level_bar = new QProgressBar();
    audio_level_bar->setRange(0, 1000);
    audio_level_bar->setValue(0);
    audio_level_bar->setTextVisible(false);
    audio_level_bar->setFixedHeight(14);
    layout->addWidget(audio_level_bar);

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
    audio_gain_slider->setRange(1, 100);
    audio_gain_slider->setValue(10);
    connect(audio_gain_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_gain_changed);
    gain_layout->addWidget(audio_gain_slider);
    audio_gain_value_label = new QLabel("1.0x");
    audio_gain_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    audio_gain_value_label->setMinimumWidth(48);
    gain_layout->addWidget(audio_gain_value_label);
    layout->addLayout(gain_layout);

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

    QLabel* help = new QLabel("Configure audio input for frequency range effects below.");
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
    audio_group->setVisible(false);
}

void OpenRGB3DSpatialTab::on_audio_effect_start_clicked()
{
    if(!audio_effect_combo) return;
    int eff_idx = audio_effect_combo->currentIndex();

    // Index 0 is "None"
    if(eff_idx <= 0) return;
    QString class_name_q = audio_effect_combo->itemData(eff_idx, kEffectRoleClassName).toString();
    if(class_name_q.isEmpty()) return;
    std::string class_name = class_name_q.toStdString();

    // Build a single-effect stack from current audio effect UI settings
    effect_stack.clear();
    if(!current_audio_effect_ui) { SetupAudioEffectUI(eff_idx); }
    nlohmann::json settings = current_audio_effect_ui ? current_audio_effect_ui->SaveSettings() : nlohmann::json();
    SpatialEffect3D* eff = EffectListManager3D::get()->CreateEffect(class_name);
    if(!eff) return;
    std::unique_ptr<EffectInstance3D> inst = std::make_unique<EffectInstance3D>();
    inst->name = class_name;
    inst->effect_class_name = class_name;
    inst->effect.reset(eff);

    int target = -1;
    if(audio_effect_zone_combo)
    {
        QVariant data = audio_effect_zone_combo->itemData(audio_effect_zone_combo->currentIndex());
        if(data.isValid()) target = data.toInt();
    }
    inst->zone_index = target; // -1 all, >=0 zone, <=-1000 controller
    inst->blend_mode = BlendMode::ADD;
    inst->enabled = true;
    inst->id = next_effect_instance_id++;

    // Apply per-effect settings captured from UI
    eff->LoadSettings(settings);
    inst->saved_settings = std::make_unique<nlohmann::json>(settings);

    // Connect ScreenMirror3D screen preview signal to viewport
    if(class_name == "ScreenMirror3D")
    {
        ScreenMirror3D* screen_mirror = dynamic_cast<ScreenMirror3D*>(eff);
        if(screen_mirror && viewport)
        {
            connect(screen_mirror, &ScreenMirror3D::ScreenPreviewChanged,
                    viewport, &LEDViewport3D::SetShowScreenPreview);
            connect(screen_mirror, &ScreenMirror3D::TestPatternChanged,
                    viewport, &LEDViewport3D::SetShowTestPattern);
            screen_mirror->SetReferencePoints(&reference_points);
        }
    }

    effect_stack.push_back(std::move(inst));
    UpdateEffectStackList();

    // Start rendering (ensure controllers in custom mode, start timer)
    bool has_valid_controller = false;
    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform) continue;
        if(transform->virtual_controller)
        {
            VirtualController3D* virtual_ctrl = transform->virtual_controller;
            const std::vector<GridLEDMapping>& mappings = virtual_ctrl->GetMappings();
            std::set<RGBController*> controllers_to_set;
            for(unsigned int i = 0; i < mappings.size(); i++)
            {
                if(mappings[i].controller) controllers_to_set.insert(mappings[i].controller);
            }
            for(std::set<RGBController*>::iterator it = controllers_to_set.begin(); it != controllers_to_set.end(); ++it)
            {
                (*it)->SetCustomMode();
                has_valid_controller = true;
            }
            continue;
        }
        RGBController* controller = transform->controller;
        if(!controller) continue;
        controller->SetCustomMode();
        has_valid_controller = true;
    }
    if(has_valid_controller && effect_timer && !effect_timer->isActive())
    {
        effect_time = 0.0f;
        effect_elapsed.restart();
        unsigned int target_fps = 30;
        for(size_t i = 0; i < effect_stack.size(); i++)
        {
            if(effect_stack[i])
            {
                unsigned int f = effect_stack[i]->GetEffectiveTargetFPS();
                if(f > target_fps) target_fps = f;
            }
        }
        if(target_fps < 1) target_fps = 30;
        int interval_ms = (int)(1000 / target_fps);
        if(interval_ms < 1) interval_ms = 1;
        effect_timer->start(interval_ms);
    }
    running_audio_effect = eff;
    if(audio_effect_start_button) audio_effect_start_button->setEnabled(false);
    if(audio_effect_stop_button) audio_effect_stop_button->setEnabled(true);
}

void OpenRGB3DSpatialTab::on_audio_effect_stop_clicked()
{
    if(effect_timer && effect_timer->isActive()) effect_timer->stop();
    effect_stack.clear();
    running_audio_effect = nullptr;
    UpdateEffectStackList();
    if(audio_effect_start_button) audio_effect_start_button->setEnabled(true);
    if(audio_effect_stop_button) audio_effect_stop_button->setEnabled(false);
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

void OpenRGB3DSpatialTab::SetupAudioEffectUI(int eff_index)
{
    if(!audio_effect_controls_widget || !audio_effect_controls_layout) return;
    if(!audio_effect_combo) return;
    // Clear previous controls
    while(QLayoutItem* it = audio_effect_controls_layout->takeAt(0))
    {
        if(QWidget* w = it->widget()) { w->deleteLater(); }
        delete it;
    }
    current_audio_effect_ui = nullptr;

    // Handle "None" option (index 0)
    if(eff_index <= 0)
    {
        // Hide effect controls and audio controls panel when "None" is selected
        if(audio_effect_controls_widget) audio_effect_controls_widget->hide();
        if(audio_std_group) audio_std_group->hide();
        return;
    }

    // Show controls for actual effects
    if(audio_effect_controls_widget) audio_effect_controls_widget->show();
    if(audio_std_group) audio_std_group->show();

    QString class_name_q = audio_effect_combo->itemData(eff_index, kEffectRoleClassName).toString();
    if(class_name_q.isEmpty())
    {
        if(audio_effect_controls_widget) audio_effect_controls_widget->hide();
        if(audio_std_group) audio_std_group->hide();
        return;
    }
    SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(class_name_q.toStdString());
    if(!effect) return;
    effect->setParent(audio_effect_controls_widget);
    effect->CreateCommonEffectControls(audio_effect_controls_widget);
    effect->SetupCustomUI(audio_effect_controls_widget);
    current_audio_effect_ui = effect;
    // Hook Start/Stop from effect's own buttons to our audio handlers
    audio_effect_start_button = effect->GetStartButton();
    audio_effect_stop_button  = effect->GetStopButton();
    if(audio_effect_start_button)
    {
        // Avoid duplicate connections by disconnecting previous
        QObject::disconnect(audio_effect_start_button, nullptr, this, nullptr);
        connect(audio_effect_start_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_effect_start_clicked);
    }
    if(audio_effect_stop_button)
    {
        QObject::disconnect(audio_effect_stop_button, nullptr, this, nullptr);
        connect(audio_effect_stop_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_effect_stop_clicked);
        audio_effect_stop_button->setEnabled(false);
    }
    connect(effect, &SpatialEffect3D::ParametersChanged, this, &OpenRGB3DSpatialTab::OnAudioEffectParamsChanged);
    // Sync standard audio controls from effect settings
    if(audio_std_group && current_audio_effect_ui)
    {
        nlohmann::json s = current_audio_effect_ui->SaveSettings();
        if(audio_low_spin && s.contains("low_hz")) audio_low_spin->setValue(s["low_hz"].get<int>());
        if(audio_high_spin && s.contains("high_hz")) audio_high_spin->setValue(s["high_hz"].get<int>());
        if(audio_smooth_slider && s.contains("smoothing"))
        {
            int sv = (int)std::round(std::max(0.0f, std::min(0.99f, s["smoothing"].get<float>())) * 100.0f);
            audio_smooth_slider->setValue(sv);
        }
        if(audio_falloff_slider && s.contains("falloff"))
        {
            int fv = (int)std::round(std::max(0.2f, std::min(5.0f, s["falloff"].get<float>())) * 100.0f);
            audio_falloff_slider->setValue(fv);
        }
    }
    // Apply current origin selection to the new UI
    if(audio_effect_origin_combo)
    {
        int idx = audio_effect_origin_combo->currentIndex();
        on_audio_effect_origin_changed(idx);
    }
    audio_effect_controls_widget->updateGeometry();
    audio_effect_controls_widget->update();
}

void OpenRGB3DSpatialTab::UpdateAudioEffectOriginCombo()
{
    if(!audio_effect_origin_combo) return;
    audio_effect_origin_combo->blockSignals(true);
    audio_effect_origin_combo->clear();
    audio_effect_origin_combo->addItem("Room Center", QVariant(-1));
    for(size_t i = 0; i < reference_points.size(); ++i)
    {
        VirtualReferencePoint3D* ref = reference_points[i].get();
        if(!ref) continue;
        QString name = QString::fromStdString(ref->GetName());
        QString type = QString(VirtualReferencePoint3D::GetTypeName(ref->GetType()));
        audio_effect_origin_combo->addItem(QString("%1 (%2)").arg(name).arg(type), QVariant((int)i));
    }
    audio_effect_origin_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::UpdateAudioEffectZoneCombo()
{
    if(!audio_effect_zone_combo) return;

    // Save current selection to restore after rebuild
    int saved_index = audio_effect_zone_combo->currentIndex();
    if(saved_index < 0)
    {
        saved_index = 0;  // Default to "All Controllers"
    }

    audio_effect_zone_combo->blockSignals(true);
    audio_effect_zone_combo->clear();

    // Add "All Controllers" option with data -1
    audio_effect_zone_combo->addItem("All Controllers", QVariant(-1));

    // Add all zones with their index as data
    if(zone_manager)
    {
        for(int i = 0; i < zone_manager->GetZoneCount(); i++)
        {
            Zone3D* zone = zone_manager->GetZone(i);
            if(zone)
            {
                QString zone_name = QString::fromStdString(zone->GetName());
                audio_effect_zone_combo->addItem(zone_name, QVariant(i));
            }
        }
    }

    /*---------------------------------------------------------*\
    | Add individual controllers with encoded index           |
    | Data is encoded as -(controller_index) - 1000           |
    \*---------------------------------------------------------*/
    for(unsigned int ci = 0; ci < controller_transforms.size(); ci++)
    {
        ControllerTransform* t = controller_transforms[ci].get();
        QString name;
        if(t && t->controller)
        {
            name = QString::fromStdString(t->controller->GetName());
        }
        else if(t && t->virtual_controller)
        {
            name = QString("[Virtual] ") + QString::fromStdString(t->virtual_controller->GetName());
        }
        else
        {
            name = QString("Controller %1").arg((int)ci);
        }
        audio_effect_zone_combo->addItem(QString("(Controller) %1").arg(name), QVariant(-(int)ci - 1000));
    }

    // Restore previous selection (or default to 0 if invalid)
    if(saved_index < audio_effect_zone_combo->count())
    {
        audio_effect_zone_combo->setCurrentIndex(saved_index);
    }
    else
    {
        audio_effect_zone_combo->setCurrentIndex(0);  // Default to "All Controllers"
    }

    audio_effect_zone_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::on_audio_effect_origin_changed(int index)
{
    if(!audio_effect_origin_combo) return;
    int ref_idx = audio_effect_origin_combo->itemData(index).toInt();

    ReferenceMode mode = REF_MODE_ROOM_CENTER;
    Vector3D origin = {0.0f, 0.0f, 0.0f};
    if(ref_idx >= 0 && ref_idx < (int)reference_points.size())
    {
        VirtualReferencePoint3D* ref = reference_points[ref_idx].get();
        if(ref)
        {
            origin = ref->GetPosition();
            mode = REF_MODE_CUSTOM_POINT;
        }
    }

    if(current_audio_effect_ui)
    {
        current_audio_effect_ui->SetCustomReferencePoint(origin);
        current_audio_effect_ui->SetReferenceMode(mode);
    }
    if(running_audio_effect)
    {
        running_audio_effect->SetCustomReferencePoint(origin);
        running_audio_effect->SetReferenceMode(mode);
    }
    if(viewport) viewport->UpdateColors();
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
    if(!effect_stack_list || !audio_panel_group)
    {
        return;
    }

    int row = effect_stack_list->currentRow();
    bool show = false;

    if(row >= 0 && row < (int)effect_stack.size())
    {
        EffectInstance3D* inst = effect_stack[row].get();
        if(inst && !inst->effect_class_name.empty())
        {
            EffectRegistration3D info = EffectListManager3D::get()->GetEffectInfo(inst->effect_class_name);
            show = (info.category == "Audio");
        }
    }

    audio_panel_group->setVisible(show);
}
