// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include "LogManager.h"
#include "filesystem.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>
#include <QSignalBlocker>
#include <fstream>
#include <algorithm>
#include <cmath>

std::string OpenRGB3DSpatialTab::GetEffectProfilePath(const std::string& profile_name)
{
    filesystem::path base_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path profiles_dir = base_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "EffectProfiles";

    /*---------------------------------------------------------*\
    | Create directory if it doesn't exist                     |
    \*---------------------------------------------------------*/
    filesystem::create_directories(profiles_dir);

    return (profiles_dir / (profile_name + ".effectprofile.json")).string();
}

void OpenRGB3DSpatialTab::SaveEffectProfile(const std::string& filename)
{
    if(effect_stack.empty())
    {
        QMessageBox::warning(this, "Nothing to Save", "Add at least one effect to the stack before saving a profile.");
        return;
    }

    nlohmann::json profile_json;
    profile_json["version"] = 3;

    nlohmann::json stack_json = nlohmann::json::array();
    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        if(effect_stack[i])
        {
            stack_json.push_back(effect_stack[i]->ToJson());
        }
    }
    profile_json["stack"] = stack_json;
    profile_json["selected_stack_index"] = effect_stack_list ? effect_stack_list->currentRow() : -1;

    if(effect_origin_combo)
    {
        profile_json["origin_index"] = effect_origin_combo->currentIndex();
    }

    // Legacy compatibility: keep the first stack entry serialized as a "primary" effect
    if(!effect_stack.empty() && effect_stack[0])
    {
        EffectInstance3D* primary = effect_stack[0].get();
        profile_json["effect_class"] = primary->effect_class_name;
        profile_json["effect_type"] = -1;
        profile_json["zone_target"] = primary->zone_index;
        profile_json["zone_index"] = primary->zone_index;

        if(primary->effect)
        {
            profile_json["effect_params"] = primary->effect->SaveSettings();
        }
        else if(primary->saved_settings && !primary->saved_settings->empty())
        {
            profile_json["effect_params"] = *primary->saved_settings;
        }
    }

    // Save audio settings (input + currently configured audio effect UI)
    nlohmann::json audio_json;
    int device_index = (audio_device_combo ? audio_device_combo->currentIndex() : -1);
    audio_json["device_index"] = device_index;
    audio_json["device_name"] = (audio_device_combo && device_index >= 0)
        ? audio_device_combo->currentText().toStdString()
        : std::string();
    audio_json["gain_slider"] = (audio_gain_slider ? audio_gain_slider->value() : 10);
    audio_json["bands_count"] = (audio_bands_combo ? audio_bands_combo->currentText().toInt() : 16);
    audio_json["low_hz"] = (audio_low_spin ? (int)audio_low_spin->value() : 60);
    audio_json["high_hz"] = (audio_high_spin ? (int)audio_high_spin->value() : 200);
    audio_json["smoothing_slider"] = (audio_smooth_slider ? audio_smooth_slider->value() : 60);
    audio_json["falloff_slider"] = (audio_falloff_slider ? audio_falloff_slider->value() : 100);
    audio_json["fft_index"] = (audio_fft_combo ? audio_fft_combo->currentIndex() : -1);
    audio_json["fft_value"] = (audio_fft_combo && audio_fft_combo->currentIndex() >= 0)
        ? audio_fft_combo->currentText().toInt()
        : 1024;

    if(audio_effect_combo && audio_effect_combo->currentIndex() > 0)
    {
        int idx = audio_effect_combo->currentIndex();
        QString class_name = audio_effect_combo->itemData(idx, kEffectRoleClassName).toString();
        if(!class_name.isEmpty())
        {
            audio_json["active_effect_class"] = class_name.toStdString();
            audio_json["active_effect_index"] = idx;
        }

        if(audio_effect_zone_combo)
        {
            QVariant zone_data = audio_effect_zone_combo->itemData(audio_effect_zone_combo->currentIndex());
            audio_json["active_effect_zone"] = zone_data.isValid() ? zone_data.toInt() : -1;
        }

        if(audio_effect_origin_combo)
        {
            audio_json["active_effect_origin"] = audio_effect_origin_combo->currentIndex();
        }

        if(current_audio_effect_ui)
        {
            nlohmann::json audio_effect_settings = current_audio_effect_ui->SaveSettings();
            if(!audio_effect_settings.is_null())
            {
                audio_json["active_effect_settings"] = audio_effect_settings;
            }
        }
    }

    profile_json["audio_settings"] = audio_json;

    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to save effect profile:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename), file.errorString());
        QMessageBox::critical(this, "Save Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to save effect profile: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    QTextStream out(&file);
    out << QString::fromStdString(profile_json.dump(4));
    file.close();
}

void OpenRGB3DSpatialTab::LoadEffectProfile(const std::string& filename)
{
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to load effect profile:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename), file.errorString());
        QMessageBox::critical(this, "Load Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load effect profile: %s - %s",
                  filename.c_str(), file.errorString().toStdString().c_str());
        return;
    }

    QString json_str = file.readAll();
    file.close();

    try
    {
        nlohmann::json profile_json = nlohmann::json::parse(json_str.toStdString());

        effect_stack.clear();

        bool has_stack_array = profile_json.contains("stack") && profile_json["stack"].is_array();
        if(has_stack_array)
        {
            const nlohmann::json& stack_json = profile_json["stack"];
            for(size_t i = 0; i < stack_json.size(); i++)
            {
                std::unique_ptr<EffectInstance3D> instance = EffectInstance3D::FromJson(stack_json[i]);
                if(instance)
                {
                    effect_stack.push_back(std::move(instance));
                }
            }
        }
        else if(profile_json.contains("effect_class"))
        {
            std::unique_ptr<EffectInstance3D> legacy_instance = std::make_unique<EffectInstance3D>();
            legacy_instance->effect_class_name = profile_json["effect_class"].get<std::string>();
            legacy_instance->name = legacy_instance->effect_class_name;
            legacy_instance->enabled = true;

            if(profile_json.contains("zone_target"))
            {
                legacy_instance->zone_index = profile_json["zone_target"].get<int>();
            }
            else if(profile_json.contains("zone_index"))
            {
                legacy_instance->zone_index = DecodeLegacyZoneIndex(profile_json["zone_index"].get<int>());
            }

            if(profile_json.contains("effect_params"))
            {
                legacy_instance->saved_settings = std::make_unique<nlohmann::json>(profile_json["effect_params"]);
            }

            effect_stack.push_back(std::move(legacy_instance));
        }

        UpdateEffectStackList();

        int desired_index = -1;
        if(profile_json.contains("selected_stack_index"))
        {
            desired_index = profile_json["selected_stack_index"].get<int>();
        }

        if(effect_stack_list)
        {
            if(desired_index >= 0 && desired_index < (int)effect_stack.size())
            {
                effect_stack_list->setCurrentRow(desired_index);
            }
            else if(!effect_stack.empty())
            {
                effect_stack_list->setCurrentRow(0);
            }
            else
            {
                effect_stack_list->setCurrentRow(-1);
            }
        }

        if(effect_origin_combo && profile_json.contains("origin_index"))
        {
            int origin_idx = profile_json["origin_index"].get<int>();
            if(origin_idx >= 0 && origin_idx < effect_origin_combo->count())
            {
                effect_origin_combo->setCurrentIndex(origin_idx);
            }
        }

        if(!effect_stack.empty())
        {
            int current_row = effect_stack_list ? effect_stack_list->currentRow() : 0;
            if(current_row < 0 || current_row >= (int)effect_stack.size())
            {
                current_row = 0;
                if(effect_stack_list)
                {
                    effect_stack_list->setCurrentRow(current_row);
                }
            }
            LoadStackEffectControls(effect_stack[current_row].get());
        }
        else
        {
            ClearCustomEffectUI();
        }

        // Restore audio settings
        if(profile_json.contains("audio_settings"))
        {
            const nlohmann::json& a = profile_json["audio_settings"];

            if(audio_device_combo && audio_device_combo->count() > 0)
            {
                bool applied = false;
                if(a.contains("device_index"))
                {
                    int di = a["device_index"].get<int>();
                    if(di >= 0 && di < audio_device_combo->count())
                    {
                        audio_device_combo->setCurrentIndex(di);
                        on_audio_device_changed(di);
                        applied = true;
                    }
                }
                if(!applied && a.contains("device_name"))
                {
                    QString device_name = QString::fromStdString(a["device_name"].get<std::string>());
                    int idx = audio_device_combo->findText(device_name);
                    if(idx >= 0)
                    {
                        audio_device_combo->setCurrentIndex(idx);
                        on_audio_device_changed(idx);
                    }
                }
            }

            if(a.contains("gain_slider") && audio_gain_slider)
            {
                int gv = std::max(1, std::min(100, a["gain_slider"].get<int>()));
                {
                    QSignalBlocker blocker(audio_gain_slider);
                    audio_gain_slider->setValue(gv);
                }
                on_audio_gain_changed(gv);
            }

            if(a.contains("bands_count") && audio_bands_combo)
            {
                int bc = a["bands_count"].get<int>();
                int idx = audio_bands_combo->findText(QString::number(bc));
                if(idx >= 0)
                {
                    audio_bands_combo->blockSignals(true);
                    audio_bands_combo->setCurrentIndex(idx);
                    audio_bands_combo->blockSignals(false);
                    on_audio_bands_changed(idx);
                }
            }

            if(a.contains("low_hz") && audio_low_spin)
            {
                QSignalBlocker blocker(audio_low_spin);
                audio_low_spin->setValue(a["low_hz"].get<int>());
            }
            if(a.contains("high_hz") && audio_high_spin)
            {
                QSignalBlocker blocker(audio_high_spin);
                audio_high_spin->setValue(a["high_hz"].get<int>());
            }

            if(a.contains("smoothing_slider") && audio_smooth_slider)
            {
                int sv = std::max(0, std::min(99, a["smoothing_slider"].get<int>()));
                QSignalBlocker blocker(audio_smooth_slider);
                audio_smooth_slider->setValue(sv);
            }

            if(a.contains("falloff_slider") && audio_falloff_slider)
            {
                int fv = std::max(20, std::min(500, a["falloff_slider"].get<int>()));
                QSignalBlocker blocker(audio_falloff_slider);
                audio_falloff_slider->setValue(fv);
            }

                if(a.contains("fft_index") && audio_fft_combo)
                {
                    int fi = a["fft_index"].get<int>();
                    if(fi >= 0 && fi < audio_fft_combo->count())
                    {
                        {
                            QSignalBlocker blocker(audio_fft_combo);
                            audio_fft_combo->setCurrentIndex(fi);
                        }
                        on_audio_fft_changed(fi);
                    }
                    else if(a.contains("fft_value"))
                    {
                        QString value = QString::number(a["fft_value"].get<int>());
                        int idx = audio_fft_combo->findText(value);
                        if(idx >= 0)
                        {
                            {
                                QSignalBlocker blocker(audio_fft_combo);
                                audio_fft_combo->setCurrentIndex(idx);
                            }
                            on_audio_fft_changed(idx);
                        }
                    }
            }

            // Re-apply derived controls after silent updates
            if(audio_low_spin) on_audio_std_low_changed(audio_low_spin->value());
            if(audio_smooth_slider) on_audio_std_smooth_changed(audio_smooth_slider->value());
            if(audio_falloff_slider) on_audio_std_falloff_changed(audio_falloff_slider->value());

            if(audio_effect_combo && a.contains("active_effect_class"))
            {
                QString active_class = QString::fromStdString(a["active_effect_class"].get<std::string>());
                int active_idx = audio_effect_combo->findData(active_class, kEffectRoleClassName);
                if(active_idx > 0)
                {
                    audio_effect_combo->blockSignals(true);
                    audio_effect_combo->setCurrentIndex(active_idx);
                    audio_effect_combo->blockSignals(false);
                    SetupAudioEffectUI(active_idx);
                }

                if(audio_effect_zone_combo && a.contains("active_effect_zone"))
                {
                    int zone_value = a["active_effect_zone"].get<int>();
                    int zone_idx = audio_effect_zone_combo->findData(zone_value);
                    if(zone_idx >= 0)
                    {
                        QSignalBlocker blocker(audio_effect_zone_combo);
                        audio_effect_zone_combo->setCurrentIndex(zone_idx);
                    }
                    on_audio_effect_zone_changed(audio_effect_zone_combo->currentIndex());
                }

                if(audio_effect_origin_combo && a.contains("active_effect_origin"))
                {
                    int origin_idx = a["active_effect_origin"].get<int>();
                    if(origin_idx >= 0 && origin_idx < audio_effect_origin_combo->count())
                    {
                        QSignalBlocker blocker(audio_effect_origin_combo);
                        audio_effect_origin_combo->setCurrentIndex(origin_idx);
                    }
                    on_audio_effect_origin_changed(audio_effect_origin_combo->currentIndex());
                }

                if(a.contains("active_effect_settings") && current_audio_effect_ui)
                {
                    current_audio_effect_ui->LoadSettings(a["active_effect_settings"]);
                }
            }
        }

        
    }
    catch(const std::exception& e)
    {
        QString error_msg = QString("Failed to parse effect profile:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename), QString::fromStdString(e.what()));
        QMessageBox::critical(this, "Parse Failed", error_msg);
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to parse effect profile: %s - %s",
                  filename.c_str(), e.what());
    }
}

void OpenRGB3DSpatialTab::PopulateEffectProfileDropdown()
{
    if(!effect_profiles_combo)
    {
        return;
    }

    effect_profiles_combo->blockSignals(true);
    effect_profiles_combo->clear();

    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path profiles_dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "EffectProfiles";

    if(!filesystem::exists(profiles_dir))
    {
        effect_profiles_combo->blockSignals(false);
        return;
    }

    /*---------------------------------------------------------*\
    | Scan for .effectprofile.json files                       |
    \*---------------------------------------------------------*/
    for(filesystem::directory_iterator entry(profiles_dir); entry != filesystem::directory_iterator(); ++entry)
    {
        if(entry->path().extension() == ".json")
        {
            std::string filename = entry->path().filename().string();
            std::string stem     = entry->path().stem().string();

            /*---------------------------------------------------------*\
            | Only show .effectprofile.json files                      |
            \*---------------------------------------------------------*/
            if(stem.length() > 14 && stem.substr(stem.length() - 14) == ".effectprofile")
            {
                std::string profile_name = stem.substr(0, stem.length() - 14);
                effect_profiles_combo->addItem(QString::fromStdString(profile_name));
            }
        }
    }

    effect_profiles_combo->blockSignals(false);
}

void OpenRGB3DSpatialTab::SaveCurrentEffectProfileName()
{
    if(!effect_profiles_combo || !effect_auto_load_checkbox)
    {
        return;
    }

    filesystem::path config_dir = resource_manager->GetConfigurationDirectory();
    filesystem::path plugin_dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin";
    filesystem::path config_file = plugin_dir / "effect_profile_config.json";

    filesystem::create_directories(plugin_dir);

    std::string profile_name = effect_profiles_combo->currentText().toStdString();
    bool auto_load_enabled = effect_auto_load_checkbox->isChecked();

    nlohmann::json config;
    config["auto_load_enabled"] = auto_load_enabled;
    config["auto_load_profile"]  = profile_name;

    std::ofstream file(config_file.string());
    if(file.is_open())
    {
        file << config.dump(4);
        file.close();
    }
}

void OpenRGB3DSpatialTab::TryAutoLoadEffectProfile()
{
    if(!effect_profiles_combo || !effect_auto_load_checkbox)
    {
        return;
    }

    filesystem::path config_dir  = resource_manager->GetConfigurationDirectory();
    filesystem::path config_file = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "effect_profile_config.json";

    if(!filesystem::exists(config_file))
    {
        return;
    }

    std::ifstream file(config_file.string());
    if(!file.is_open())
    {
        return;
    }

    try
    {
        nlohmann::json config;
        file >> config;
        file.close();

        bool auto_load_enabled = false;
        std::string profile_name;

        if(config.contains("auto_load_enabled"))
        {
            auto_load_enabled = config["auto_load_enabled"].get<bool>();
        }

        if(config.contains("auto_load_profile"))
        {
            profile_name = config["auto_load_profile"].get<std::string>();
        }

        /*---------------------------------------------------------*\
        | Restore checkbox state                                   |
        \*---------------------------------------------------------*/
        effect_auto_load_checkbox->blockSignals(true);
        effect_auto_load_checkbox->setChecked(auto_load_enabled);
        effect_auto_load_checkbox->blockSignals(false);

        /*---------------------------------------------------------*\
        | Restore profile selection                                |
        \*---------------------------------------------------------*/
        if(!profile_name.empty())
        {
            int index = effect_profiles_combo->findText(QString::fromStdString(profile_name));
            if(index >= 0)
            {
                effect_profiles_combo->blockSignals(true);
                effect_profiles_combo->setCurrentIndex(index);
                effect_profiles_combo->blockSignals(false);
            }
        }

        /*---------------------------------------------------------*\
        | Auto-load if enabled                                     |
        \*---------------------------------------------------------*/
        if(auto_load_enabled && !profile_name.empty())
        {
            std::string profile_path = GetEffectProfilePath(profile_name);

            if(filesystem::exists(profile_path))
            {
                LoadEffectProfile(profile_path);
            }
        }
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to auto-load effect profile: %s", e.what());
    }
}

void OpenRGB3DSpatialTab::on_save_effect_profile_clicked()
{
    /*---------------------------------------------------------*\
    | Validate that the stack has at least one effect          |
    \*---------------------------------------------------------*/
    if(effect_stack.empty())
    {
        QMessageBox::information(this, "No Effect Selected",
                                "Add at least one effect to the stack before saving a profile.");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "Save Effect Profile",
                                        "Enter profile name:", QLineEdit::Normal,
                                        "", &ok);

    if(!ok || name.isEmpty())
    {
        return;
    }

    std::string profile_name = name.toStdString();
    std::string profile_path = GetEffectProfilePath(profile_name);

    /*---------------------------------------------------------*\
    | Check if profile already exists                          |
    \*---------------------------------------------------------*/
    if(filesystem::exists(profile_path))
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Overwrite Profile",
            QString("Effect profile \"%1\" already exists. Overwrite?").arg(name),
            QMessageBox::Yes | QMessageBox::No);

        if(reply != QMessageBox::Yes)
        {
            return;
        }
    }

    /*---------------------------------------------------------*\
    | Save the profile                                         |
    \*---------------------------------------------------------*/
    SaveEffectProfile(profile_path);

    /*---------------------------------------------------------*\
    | Update dropdown                                          |
    \*---------------------------------------------------------*/
    PopulateEffectProfileDropdown();

    /*---------------------------------------------------------*\
    | Select the newly saved profile                           |
    \*---------------------------------------------------------*/
    int index = effect_profiles_combo->findText(name);
    if(index >= 0)
    {
        effect_profiles_combo->setCurrentIndex(index);
    }

    QMessageBox::information(this, "Success",
                            QString("Effect profile \"%1\" saved successfully!").arg(name));
}

void OpenRGB3DSpatialTab::on_load_effect_profile_clicked()
{
    if(!effect_profiles_combo || effect_profiles_combo->currentIndex() < 0)
    {
        QMessageBox::information(this, "No Profile Selected",
                                "Please select an effect profile to load.");
        return;
    }

    QString profile_name = effect_profiles_combo->currentText();
    std::string profile_path = GetEffectProfilePath(profile_name.toStdString());

    if(!filesystem::exists(profile_path))
    {
        QMessageBox::critical(this, "Profile Not Found",
                            QString("Effect profile \"%1\" not found.").arg(profile_name));
        return;
    }

    LoadEffectProfile(profile_path);

    QMessageBox::information(this, "Success",
                            QString("Effect profile \"%1\" loaded successfully!\n\nClick Start in the Effects tab to begin.").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_delete_effect_profile_clicked()
{
    if(!effect_profiles_combo || effect_profiles_combo->currentIndex() < 0)
    {
        QMessageBox::information(this, "No Profile Selected",
                                "Please select an effect profile to delete.");
        return;
    }

    QString profile_name = effect_profiles_combo->currentText();

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Profile",
        QString("Are you sure you want to delete effect profile \"%1\"?").arg(profile_name),
        QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes)
    {
        return;
    }

    std::string profile_path = GetEffectProfilePath(profile_name.toStdString());

    if(filesystem::exists(profile_path))
    {
        filesystem::remove(profile_path);
    }

    /*---------------------------------------------------------*\
    | Update dropdown                                          |
    \*---------------------------------------------------------*/
    PopulateEffectProfileDropdown();

    QMessageBox::information(this, "Success",
                            QString("Effect profile \"%1\" deleted successfully!").arg(profile_name));
}

void OpenRGB3DSpatialTab::on_effect_profile_changed(int)
{
    /*---------------------------------------------------------*\
    | Just update the auto-load config when selection changes  |
    \*---------------------------------------------------------*/
    SaveCurrentEffectProfileName();
}
