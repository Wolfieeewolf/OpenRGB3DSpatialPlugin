// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include "Audio/AudioInputManager.h"
#include "SettingsManager.h"
#include "LogManager.h"
#include "Effects3D/ScreenMirror3D/ScreenMirror3D.h"
#include <nlohmann/json.hpp>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QGroupBox>
#include <filesystem>

namespace filesystem = std::filesystem;
#include <set>
#include <algorithm>
#include <cmath>

static int MapHzToBandIndex(float hz, int bands, float f_min, float f_max)
{
    float clamped = hz;
    if(clamped < f_min) clamped = f_min;
    if(clamped > f_max) clamped = f_max;
    float log_ratio = std::log(f_max / f_min);
    float t = 0.0f;
    if(std::abs(log_ratio) > 1e-6f)
    {
        t = std::log(clamped / f_min) / log_ratio;
    }
    int idx = (int)std::floor(t * bands);
    if(idx < 0) idx = 0;
    if(idx > bands - 1) idx = bands - 1;
    return idx;
}


void OpenRGB3DSpatialTab::SetupAudioPanel(QVBoxLayout* parent_layout)
{
    QGroupBox* audio_group = new QGroupBox("Audio Input & Reactive Controls");
    audio_panel_group = audio_group;
    QVBoxLayout* layout = new QVBoxLayout(audio_group);

    QLabel* hdr = new QLabel("Audio Input (used by Audio effects)");
    hdr->setStyleSheet("font-weight: bold;");
    layout->addWidget(hdr);

    // Top controls: Start/Stop and Level meter
    QHBoxLayout* top_controls = new QHBoxLayout();
    audio_start_button = new QPushButton("Start Listening");
    audio_stop_button = new QPushButton("Stop");
    audio_stop_button->setEnabled(false);
    top_controls->addWidget(audio_start_button);
    top_controls->addWidget(audio_stop_button);
    top_controls->addStretch();
    layout->addLayout(top_controls);

    layout->addWidget(new QLabel("Level:"));
    audio_level_bar = new QProgressBar();
    audio_level_bar->setRange(0, 1000);
    audio_level_bar->setValue(0);
    audio_level_bar->setTextVisible(false);
    audio_level_bar->setFixedHeight(14);
    layout->addWidget(audio_level_bar);

    connect(audio_start_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_start_clicked);
    connect(audio_stop_button, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_stop_clicked);
    connect(AudioInputManager::instance(), &AudioInputManager::LevelUpdated, this, &OpenRGB3DSpatialTab::on_audio_level_updated);

    // Device selection
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
        connect(audio_device_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_device_changed);
        // Initialize selection to first device
        audio_device_combo->setCurrentIndex(0);
        on_audio_device_changed(0);
    }
    layout->addWidget(audio_device_combo);

    // Gain
    QHBoxLayout* gain_layout = new QHBoxLayout();
    gain_layout->addWidget(new QLabel("Gain:"));
    audio_gain_slider = new QSlider(Qt::Horizontal);
    audio_gain_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_gain_slider->setRange(1, 100); // maps to 0.1..10.0
    audio_gain_slider->setValue(10);     // 1.0x
    connect(audio_gain_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_gain_changed);
    gain_layout->addWidget(audio_gain_slider);
    // Numeric readout (e.g., 1.0x)
    audio_gain_value_label = new QLabel("1.0x");
    audio_gain_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    audio_gain_value_label->setMinimumWidth(48);
    gain_layout->addWidget(audio_gain_value_label);
    layout->addLayout(gain_layout);

    // Bands & crossovers
    QHBoxLayout* bands_layout = new QHBoxLayout();
    bands_layout->addWidget(new QLabel("Bands:"));
    audio_bands_combo = new QComboBox();
    audio_bands_combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    audio_bands_combo->addItems({"8", "16", "32"});
    audio_bands_combo->setCurrentText("16");
    connect(audio_bands_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_bands_changed);
    bands_layout->addWidget(audio_bands_combo);
    bands_layout->addStretch();
    layout->addLayout(bands_layout);

    // Audio Effects section
    QGroupBox* audio_fx_group = new QGroupBox("Audio Effects");
    QVBoxLayout* audio_fx_layout = new QVBoxLayout(audio_fx_group);
    QHBoxLayout* fx_row1 = new QHBoxLayout();
    fx_row1->addWidget(new QLabel("Effect:"));
    audio_effect_combo = new QComboBox();
    audio_effect_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_effect_combo->addItem("None");
    audio_effect_combo->setItemData(audio_effect_combo->count() - 1, QVariant(), kEffectRoleClassName);

    std::vector<EffectRegistration3D> all_effects = EffectListManager3D::get()->GetAllEffects();
    for(const EffectRegistration3D& reg : all_effects)
    {
        QString category = QString::fromStdString(reg.category);
        if(category.compare(QStringLiteral("Audio"), Qt::CaseInsensitive) != 0)
        {
            continue;
        }

        QString label = QString::fromStdString(reg.ui_name);
        audio_effect_combo->addItem(label);
        int row = audio_effect_combo->count() - 1;
        audio_effect_combo->setItemData(row, QString::fromStdString(reg.class_name), kEffectRoleClassName);
        audio_effect_combo->setItemData(row, QString::fromStdString(reg.category), Qt::ToolTipRole);
    }

    if(audio_effect_combo->count() <= 1)
    {
        audio_effect_combo->setEnabled(false);
    }
    fx_row1->addWidget(audio_effect_combo);
    audio_fx_layout->addLayout(fx_row1);

    // Zone selector (on its own row, matching Effects tab layout)
    QHBoxLayout* fx_row2 = new QHBoxLayout();
    fx_row2->addWidget(new QLabel("Zone:"));
    audio_effect_zone_combo = new QComboBox();
    audio_effect_zone_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fx_row2->addWidget(audio_effect_zone_combo);
    connect(audio_effect_zone_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_effect_zone_changed);
    audio_fx_layout->addLayout(fx_row2);

    // Origin selector (on its own row, matching Effects tab layout)
    QHBoxLayout* fx_row3 = new QHBoxLayout();
    fx_row3->addWidget(new QLabel("Origin:"));
    audio_effect_origin_combo = new QComboBox();
    audio_effect_origin_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    audio_effect_origin_combo->addItem("Room Center", QVariant(-1));
    connect(audio_effect_origin_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_effect_origin_changed);
    fx_row3->addWidget(audio_effect_origin_combo);
    audio_fx_layout->addLayout(fx_row3);

    // Per-effect Hz mapping
    // Dynamic effect controls (consistent with main Effects tab)
    audio_effect_controls_widget = new QWidget();
    audio_effect_controls_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    audio_effect_controls_layout = new QVBoxLayout(audio_effect_controls_widget);
    audio_effect_controls_layout->setContentsMargins(0,0,0,0);
    audio_effect_controls_widget->setLayout(audio_effect_controls_layout);
    audio_fx_layout->addWidget(audio_effect_controls_widget);

    // Standard Audio Controls panel (Hz, smoothing, falloff)
    SetupStandardAudioControls(audio_fx_layout);

    // Dynamic effect UI provides Start/Stop (consistent with main Effects tab)
    layout->addWidget(audio_fx_group);

    connect(audio_effect_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::SetupAudioEffectUI);
    audio_effect_combo->setCurrentIndex(0);  // Default to "None"
    SetupAudioEffectUI(0);  // Initialize with "None" selected (hides controls)

    // Help text
    QLabel* help = new QLabel("Use Effects > select 'Audio Level 3D' to react to audio.\nThis tab manages input device and sensitivity shared by audio effects.");
    help->setStyleSheet("color: gray; font-size: 10px;");
    help->setWordWrap(true);
    layout->addWidget(help);

    // Load persisted audio settings (device, gain, bands, audio controls)
    {
        nlohmann::json settings = GetPluginSettings();
        // Device index
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
        // Gain (slider 1..100)
        if(audio_gain_slider && settings.contains("AudioGain"))
        {
            int gv = settings["AudioGain"].get<int>();
            gv = std::max(1, std::min(100, gv));
            audio_gain_slider->blockSignals(true);
            audio_gain_slider->setValue(gv);
            audio_gain_slider->blockSignals(false);
            on_audio_gain_changed(gv);
        }
        // Bands (8/16/32)
        if(audio_bands_combo && settings.contains("AudioBands"))
        {
            int bc = settings["AudioBands"].get<int>();
            int idx = audio_bands_combo->findText(QString::number(bc));
            if(idx >= 0)
            {
                audio_bands_combo->blockSignals(true);
                audio_bands_combo->setCurrentIndex(idx);
                audio_bands_combo->blockSignals(false);
                on_audio_bands_changed(idx);
            }
        }
        // Standard Audio Controls
        if(audio_low_spin && settings.contains("AudioLowHz"))
        {
            audio_low_spin->blockSignals(true);
            audio_low_spin->setValue(settings["AudioLowHz"].get<int>());
            audio_low_spin->blockSignals(false);
        }
        if(audio_high_spin && settings.contains("AudioHighHz"))
        {
            audio_high_spin->blockSignals(true);
            audio_high_spin->setValue(settings["AudioHighHz"].get<int>());
            audio_high_spin->blockSignals(false);
        }
        if(audio_smooth_slider && settings.contains("AudioSmoothing"))
        {
            int sv = settings["AudioSmoothing"].get<int>();
            sv = std::max(0, std::min(99, sv));
            audio_smooth_slider->blockSignals(true);
            audio_smooth_slider->setValue(sv);
            audio_smooth_slider->blockSignals(false);
        }
        if(audio_falloff_slider && settings.contains("AudioFalloff"))
        {
            int fv = settings["AudioFalloff"].get<int>();
            fv = std::max(20, std::min(500, fv));
            audio_falloff_slider->blockSignals(true);
            audio_falloff_slider->setValue(fv);
            audio_falloff_slider->blockSignals(false);
        }
        if(audio_fft_combo && settings.contains("AudioFFTSize"))
        {
            int n = settings["AudioFFTSize"].get<int>();
            int idx = audio_fft_combo->findText(QString::number(n));
            if(idx >= 0)
            {
                audio_fft_combo->blockSignals(true);
                audio_fft_combo->setCurrentIndex(idx);
                audio_fft_combo->blockSignals(false);
                on_audio_fft_changed(idx);
            }
        }
        // Apply audio controls to effect UI if present
        on_audio_std_low_changed(audio_low_spin ? audio_low_spin->value() : 0.0);
        on_audio_std_smooth_changed(audio_smooth_slider ? audio_smooth_slider->value() : 60);
        on_audio_std_falloff_changed(audio_falloff_slider ? audio_falloff_slider->value() : 100);
    }
    // Populate origin combo after zones
    UpdateAudioEffectOriginCombo();

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
            name = QString::fromStdString(t->controller->name);
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
    int v = (int)std::round(level * 1000.0f);
    audio_level_bar->setValue(v);
}

void OpenRGB3DSpatialTab::on_audio_bands_changed(int index)
{
    int bands = audio_bands_combo->itemText(index).toInt();
    AudioInputManager::instance()->setBandsCount(bands);
    nlohmann::json settings = GetPluginSettings();
    settings["AudioBands"] = bands;
    SetPluginSettings(settings);
}

//


void OpenRGB3DSpatialTab::SetupAudioCustomEffectsUI(QVBoxLayout* parent_layout)
{
    if(audio_custom_group) return;
    audio_custom_group = new QGroupBox("Custom Audio Effects");
    QVBoxLayout* v = new QVBoxLayout(audio_custom_group);

    audio_custom_list = new QListWidget();
    audio_custom_list->setMinimumHeight(140);
    v->addWidget(audio_custom_list);

    QHBoxLayout* name_row = new QHBoxLayout();
    name_row->addWidget(new QLabel("Name:"));
    audio_custom_name_edit = new QLineEdit();
    name_row->addWidget(audio_custom_name_edit);
    v->addLayout(name_row);

    QHBoxLayout* btns = new QHBoxLayout();
    audio_custom_save_btn = new QPushButton("Save");
    audio_custom_load_btn = new QPushButton("Load");
    audio_custom_delete_btn = new QPushButton("Delete");
    audio_custom_add_to_stack_btn = new QPushButton("Add Selected to Stack");
    btns->addWidget(audio_custom_save_btn);
    btns->addWidget(audio_custom_load_btn);
    btns->addWidget(audio_custom_delete_btn);
    btns->addStretch();
    btns->addWidget(audio_custom_add_to_stack_btn);
    v->addLayout(btns);

    parent_layout->addWidget(audio_custom_group);

    connect(audio_custom_save_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_custom_save_clicked);
    connect(audio_custom_load_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_custom_load_clicked);
    connect(audio_custom_delete_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_custom_delete_clicked);
    connect(audio_custom_add_to_stack_btn, &QPushButton::clicked, this, &OpenRGB3DSpatialTab::on_audio_custom_add_to_stack_clicked);

    UpdateAudioCustomEffectsList();
}

std::string OpenRGB3DSpatialTab::GetAudioCustomEffectsDir()
{
    if(!resource_manager) return std::string();
    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "AudioCustomEffects";
    filesystem::create_directories(dir);
    return dir.string();
}

std::string OpenRGB3DSpatialTab::GetAudioCustomEffectPath(const std::string& name)
{
    filesystem::path base_dir(GetAudioCustomEffectsDir());
    filesystem::path file_path = base_dir / (name + ".audiocust.json");
    return file_path.string();
}

void OpenRGB3DSpatialTab::UpdateAudioCustomEffectsList()
{
    if(!audio_custom_list) return;
    audio_custom_list->clear();
    std::string dir = GetAudioCustomEffectsDir();
    if(dir.empty()) return;
    for(filesystem::directory_iterator entry(dir); entry != filesystem::directory_iterator(); ++entry)
    {
        if(entry->path().extension() == ".json")
        {
            std::string stem = entry->path().stem().string();
            if(stem.size() > 10 && stem.substr(stem.size()-10) == ".audiocust")
            {
                std::string name = stem.substr(0, stem.size()-10);
                audio_custom_list->addItem(QString::fromStdString(name));
            }
        }
    }
}

void OpenRGB3DSpatialTab::on_audio_custom_save_clicked()
{
    if(!audio_effect_combo) return;
    QString name = audio_custom_name_edit ? audio_custom_name_edit->text() : QString();
    if(name.trimmed().isEmpty())
    {
        name = QInputDialog::getText(this, "Save Custom Audio Effect", "Enter name:");
        if(name.trimmed().isEmpty()) return;
    }

    int eff_idx = audio_effect_combo->currentIndex();

    // Index 0 is "None"
    if(eff_idx <= 0) return;

    QString class_name_q = audio_effect_combo->itemData(eff_idx, kEffectRoleClassName).toString();
    if(class_name_q.isEmpty()) return;
    std::string class_name = class_name_q.toStdString();

    // Capture settings from the currently mounted audio effect UI
    if(!current_audio_effect_ui) { SetupAudioEffectUI(eff_idx); }
    if(!current_audio_effect_ui) return;
    nlohmann::json settings = current_audio_effect_ui->SaveSettings();

    int target = -1;
    if(audio_effect_zone_combo)
    {
        QVariant data = audio_effect_zone_combo->itemData(audio_effect_zone_combo->currentIndex());
        if(data.isValid()) target = data.toInt();
    }

    nlohmann::json j;
    j["name"] = name.toStdString();
    j["effect_class"] = class_name;
    j["target"] = target;
    j["settings"] = settings;
    j["blend_mode"] = (int)BlendMode::ADD;
    j["enabled"] = true;

    std::string path = GetAudioCustomEffectPath(name.toStdString());
    QFile file(QString::fromStdString(path));
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out << QString::fromStdString(j.dump(4));
        file.close();
    }
    UpdateAudioCustomEffectsList();
}

void OpenRGB3DSpatialTab::on_audio_custom_load_clicked()
{
    if(!audio_custom_list || audio_custom_list->currentRow() < 0) return;
    QListWidgetItem* item = audio_custom_list->currentItem();
    if(!item) return;
    QString name = item->text();
    std::string path = GetAudioCustomEffectPath(name.toStdString());
    if(!filesystem::exists(path)) return;
    QFile file(QString::fromStdString(path));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString json = file.readAll(); file.close();
    try
    {
        nlohmann::json j = nlohmann::json::parse(json.toStdString());
        std::string cls = j.value("effect_class", "");
        int idx = 0;
        if(audio_effect_combo)
        {
            for(int i = 1; i < audio_effect_combo->count(); ++i)
            {
                if(audio_effect_combo->itemData(i, kEffectRoleClassName).toString().toStdString() == cls)
                {
                    idx = i;
                    break;
                }
            }
            audio_effect_combo->setCurrentIndex(idx);
        }
        int target = j.value("target", -1);
        if(audio_effect_zone_combo)
        {
            int ti = audio_effect_zone_combo->findData(QVariant(target));
            if(ti >= 0) audio_effect_zone_combo->setCurrentIndex(ti);
        }
        if(j.contains("settings"))
        {
            const nlohmann::json& s = j["settings"];
            SetupAudioEffectUI(idx);
            if(current_audio_effect_ui) current_audio_effect_ui->LoadSettings(s);
        }
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to load audio custom preset: %s", e.what());
    }
    catch(...)
    {
        LOG_WARNING("[OpenRGB3DSpatialPlugin] Failed to load audio custom preset: unknown error");
    }
}

void OpenRGB3DSpatialTab::on_audio_custom_delete_clicked()
{
    if(!audio_custom_list || audio_custom_list->currentRow() < 0) return;
    QListWidgetItem* item = audio_custom_list->currentItem();
    if(!item) return;
    QString name = item->text();
    std::string path = GetAudioCustomEffectPath(name.toStdString());
    if(filesystem::exists(path)) filesystem::remove(path);
    UpdateAudioCustomEffectsList();
}

void OpenRGB3DSpatialTab::on_audio_custom_add_to_stack_clicked()
{
    if(!audio_custom_list || audio_custom_list->currentRow() < 0) return;
    QListWidgetItem* item = audio_custom_list->currentItem();
    if(!item) return;
    QString name = item->text();
    std::string path = GetAudioCustomEffectPath(name.toStdString());
    if(!filesystem::exists(path)) return;
    QFile file(QString::fromStdString(path));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString json = file.readAll(); file.close();
    try
    {
        nlohmann::json j = nlohmann::json::parse(json.toStdString());
        std::string cls = j.value("effect_class", "");
        if(cls.empty())
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Audio custom preset \"%s\" missing effect_class", name.toStdString().c_str());
            return;
        }

        int target = j.value("target", -1);
        bool enabled = j.value("enabled", true);
        BlendMode blend = BlendMode::ADD;
        if(j.contains("blend_mode") && j["blend_mode"].is_number_integer())
        {
            blend = (BlendMode)j["blend_mode"].get<int>();
        }

        const nlohmann::json* settings_ptr = nullptr;
        if(j.contains("settings"))
        {
            settings_ptr = &j["settings"];
        }

        AddEffectInstanceToStack(QString::fromStdString(cls),
                                 QString::fromStdString(j.value("name", name.toStdString())),
                                 target,
                                 blend,
                                 settings_ptr,
                                 enabled);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to add audio custom preset \"%s\": %s",
                  name.toStdString().c_str(),
                  e.what());
    }
}





void OpenRGB3DSpatialTab::OnAudioEffectParamsChanged()
{
    if(!current_audio_effect_ui || !running_audio_effect) return;
    nlohmann::json s = current_audio_effect_ui->SaveSettings();
    running_audio_effect->LoadSettings(s);
    if(viewport) viewport->UpdateColors();
}

 

void OpenRGB3DSpatialTab::SetupStandardAudioControls(QVBoxLayout* parent_layout)
{
    if(audio_std_group) return;
    audio_std_group = new QGroupBox("Audio Controls");
    QGridLayout* g = new QGridLayout(audio_std_group);
    int sr = AudioInputManager::instance()->getSampleRate();
    int nyq = std::max(2000, sr > 0 ? sr/2 : 24000);

    // Low/High Hz
    g->addWidget(new QLabel("Low Hz:"), 0, 0);
    audio_low_spin = new QDoubleSpinBox(audio_std_group);
    audio_low_spin->setRange(0, nyq); audio_low_spin->setDecimals(0); audio_low_spin->setValue(60);
    g->addWidget(audio_low_spin, 0, 1);

    g->addWidget(new QLabel("High Hz:"), 0, 2);
    audio_high_spin = new QDoubleSpinBox(audio_std_group);
    audio_high_spin->setRange(0, nyq); audio_high_spin->setDecimals(0); audio_high_spin->setValue(200);
    g->addWidget(audio_high_spin, 0, 3);

    // Smoothing
    g->addWidget(new QLabel("Smoothing:"), 1, 0);
    audio_smooth_slider = new QSlider(Qt::Horizontal, audio_std_group);
    audio_smooth_slider->setRange(0, 99); audio_smooth_slider->setValue(60);
    g->addWidget(audio_smooth_slider, 1, 1, 1, 3);
    audio_smooth_value_label = new QLabel("60%");
    audio_smooth_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    g->addWidget(audio_smooth_value_label, 1, 4);

    // Falloff (gamma shaping)
    g->addWidget(new QLabel("Falloff:"), 2, 0);
    audio_falloff_slider = new QSlider(Qt::Horizontal, audio_std_group);
    audio_falloff_slider->setRange(20, 500); // maps to 0.2 .. 5.0
    audio_falloff_slider->setValue(100);     // 1.0
    g->addWidget(audio_falloff_slider, 2, 1, 1, 3);
    audio_falloff_value_label = new QLabel("1.00x");
    audio_falloff_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    g->addWidget(audio_falloff_value_label, 2, 4);

    // FFT Size (advanced)
    g->addWidget(new QLabel("FFT Size:"), 3, 0);
    audio_fft_combo = new QComboBox(audio_std_group);
    audio_fft_combo->addItems({"512","1024","2048","4096","8192"});
    // Initialize to current analyzer size
    {
        int cur = AudioInputManager::instance()->getFFTSize();
        int idx = audio_fft_combo->findText(QString::number(cur));
        if(idx >= 0) audio_fft_combo->setCurrentIndex(idx);
    }
    g->addWidget(audio_fft_combo, 3, 1);
    g->setColumnStretch(1, 1);
    g->setColumnStretch(3, 1);

    parent_layout->addWidget(audio_std_group);

    connect(audio_low_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_audio_std_low_changed);
    connect(audio_high_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &OpenRGB3DSpatialTab::on_audio_std_high_changed);
    connect(audio_smooth_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_std_smooth_changed);
    connect(audio_falloff_slider, &QSlider::valueChanged, this, &OpenRGB3DSpatialTab::on_audio_std_falloff_changed);
    connect(audio_fft_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OpenRGB3DSpatialTab::on_audio_fft_changed);
}

void OpenRGB3DSpatialTab::UpdateAudioPanelVisibility(EffectInstance3D* instance)
{
    if(!audio_panel_group)
    {
        return;
    }

    bool show_panel = false;
    if(instance && !instance->effect_class_name.empty())
    {
        EffectRegistration3D info = EffectListManager3D::get()->GetEffectInfo(instance->effect_class_name);
        if(!info.category.empty())
        {
            QString category = QString::fromStdString(info.category);
            show_panel = (category.compare(QStringLiteral("Audio"), Qt::CaseInsensitive) == 0);
        }
    }

    audio_panel_group->setVisible(show_panel);
}

static inline float mapFalloff(int slider) { return std::max(0.2f, std::min(5.0f, slider / 100.0f)); }

void OpenRGB3DSpatialTab::on_audio_std_low_changed(double)
{
    if(!current_audio_effect_ui) return;
    nlohmann::json s = current_audio_effect_ui->SaveSettings();
    int lowhz = audio_low_spin ? (int)audio_low_spin->value() : 0;
    int highhz = audio_high_spin ? (int)audio_high_spin->value() : lowhz+1;
    // Write per-effect fields
    s["low_hz"] = lowhz;
    s["high_hz"] = highhz;
    // For band-based effects, also map Hz to band indices if keys exist
    if(s.contains("band_start") || s.contains("band_end"))
    {
        int bands = AudioInputManager::instance()->getBandsCount();
        float fs = (float)AudioInputManager::instance()->getSampleRate();
        if(bands <= 0) bands = 1;
        float f_min = std::max(1.0f, fs / (float)AudioInputManager::instance()->getFFTSize());
        float f_max = fs * 0.5f;
        if(f_max <= f_min) f_max = f_min + 1.0f;
        int bs = MapHzToBandIndex((float)lowhz, bands, f_min, f_max);
        int be = MapHzToBandIndex((float)highhz, bands, f_min, f_max);
        if(be <= bs) be = std::min(bs + 1, bands - 1);
        s["band_start"] = bs;
        s["band_end"] = be;
    }
    current_audio_effect_ui->LoadSettings(s);
    if(running_audio_effect) running_audio_effect->LoadSettings(s);
    if(viewport) viewport->UpdateColors();
    if(resource_manager)
    {
        nlohmann::json st = GetPluginSettings();
        st["AudioLowHz"] = lowhz;
        st["AudioHighHz"] = highhz;
        SetPluginSettings(st);
    }
}

void OpenRGB3DSpatialTab::on_audio_std_high_changed(double v)
{
    on_audio_std_low_changed(v);
}

void OpenRGB3DSpatialTab::on_audio_std_smooth_changed(int)
{
    if(!current_audio_effect_ui) return;
    nlohmann::json s = current_audio_effect_ui->SaveSettings();
    float smooth = audio_smooth_slider ? (audio_smooth_slider->value() / 100.0f) : 0.6f;
    if(smooth < 0.0f)
    {
        smooth = 0.0f;
    }
    if(smooth > 0.99f)
    {
        smooth = 0.99f;
    }
    s["smoothing"] = smooth;
    if(audio_smooth_value_label && audio_smooth_slider)
    {
        audio_smooth_value_label->setText(QString::number(audio_smooth_slider->value()) + "%");
    }
    current_audio_effect_ui->LoadSettings(s);
    if(running_audio_effect) running_audio_effect->LoadSettings(s);
    if(viewport) viewport->UpdateColors();
    if(resource_manager)
    {
        nlohmann::json st = GetPluginSettings();
        st["AudioSmoothing"] = audio_smooth_slider ? audio_smooth_slider->value() : 60;
        SetPluginSettings(st);
    }
}

void OpenRGB3DSpatialTab::on_audio_std_falloff_changed(int)
{
    if(!current_audio_effect_ui) return;
    nlohmann::json s = current_audio_effect_ui->SaveSettings();
    float fo = mapFalloff(audio_falloff_slider ? audio_falloff_slider->value() : 100);
    s["falloff"] = fo;
    if(audio_falloff_value_label)
    {
        audio_falloff_value_label->setText(QString::number(fo, 'f', 2) + "x");
    }
    current_audio_effect_ui->LoadSettings(s);
    if(running_audio_effect) running_audio_effect->LoadSettings(s);
    if(viewport) viewport->UpdateColors();
    if(resource_manager)
    {
        nlohmann::json st = GetPluginSettings();
        st["AudioFalloff"] = audio_falloff_slider ? audio_falloff_slider->value() : 100;
        SetPluginSettings(st);
    }
}

void OpenRGB3DSpatialTab::on_audio_effect_zone_changed(int index)
{
    (void)index;
    if(!audio_effect_zone_combo) return;
    QVariant data = audio_effect_zone_combo->itemData(audio_effect_zone_combo->currentIndex());
    if(!data.isValid()) return;
    int target = data.toInt();

    // Apply to running effect if one exists
    if(!effect_stack.empty() && effect_stack[0])
    {
        effect_stack[0]->zone_index = target;
        if(viewport) viewport->UpdateColors();
    }
    // Selection is already stored in combo and will be read when effect starts
}

void OpenRGB3DSpatialTab::on_audio_fft_changed(int)
{
    if(!audio_fft_combo) return;
    int n = audio_fft_combo->currentText().toInt();
    AudioInputManager::instance()->setFFTSize(n);
    // Re-apply Hz mapping for band-based effects since resolution changed
    on_audio_std_low_changed(audio_low_spin ? audio_low_spin->value() : 0.0);
    if(resource_manager)
    {
        nlohmann::json settings = GetPluginSettings();
        settings["AudioFFTSize"] = n;
        SetPluginSettings(settings);
    }
}

