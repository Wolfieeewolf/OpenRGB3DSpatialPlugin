/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_EffectProfiles.cpp                    |
|                                                           |
|   Effect Profile management (save/load effect stacks)    |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "LogManager.h"
#include "filesystem.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>
#include <fstream>

std::string OpenRGB3DSpatialTab::GetEffectProfilePath(const std::string& profile_name)
{
    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string profiles_dir = config_dir + "plugins/OpenRGB3DSpatialPlugin/EffectProfiles/";

    /*---------------------------------------------------------*\
    | Create directory if it doesn't exist                     |
    \*---------------------------------------------------------*/
    filesystem::create_directories(profiles_dir);

    return profiles_dir + profile_name + ".effectprofile.json";
}

void OpenRGB3DSpatialTab::SaveEffectProfile(const std::string& filename)
{
    /*---------------------------------------------------------*\
    | Validate we have an effect selected                      |
    \*---------------------------------------------------------*/
    if(!effect_combo || effect_combo->currentIndex() <= 0)
    {
        return;
    }

    if(!current_effect_ui)
    {
        return;
    }

    nlohmann::json profile_json;
    profile_json["version"] = 1;

    /*---------------------------------------------------------*\
    | Save effect type (combo index - 1 to skip "None")       |
    \*---------------------------------------------------------*/
    profile_json["effect_type"] = effect_combo->currentIndex() - 1;

    /*---------------------------------------------------------*\
    | Save selected zone                                       |
    \*---------------------------------------------------------*/
    if(effect_zone_combo)
    {
        profile_json["zone_index"] = effect_zone_combo->currentIndex();
    }

    /*---------------------------------------------------------*\
    | Save selected origin point                               |
    \*---------------------------------------------------------*/
    if(effect_origin_combo)
    {
        profile_json["origin_index"] = effect_origin_combo->currentIndex();
    }

    /*---------------------------------------------------------*\
    | Save effect parameters from the effect UI                |
    \*---------------------------------------------------------*/
    profile_json["effect_params"] = current_effect_ui->SaveSettings();

    // Save audio settings
    nlohmann::json audio_json;
#ifdef _WIN32
    
#else
    
#endif
    audio_json["device_index"] = (audio_device_combo ? audio_device_combo->currentIndex() : -1);
#ifdef _WIN32
    
#endif
    audio_json["gain_slider"] = (audio_gain_slider ? audio_gain_slider->value() : 10);
    audio_json["bands_count"] = (audio_bands_combo ? audio_bands_combo->currentText().toInt() : 16);
    // Crossovers removed from UI; omit from saved profile to avoid confusion
    profile_json["audio_settings"] = audio_json;

    /*---------------------------------------------------------*\
    | Write to file                                            |
    \*---------------------------------------------------------*/
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QString error_msg = QString("Failed to save effect profile:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
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
            .arg(QString::fromStdString(filename))
            .arg(file.errorString());
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

        /*---------------------------------------------------------*\
        | Validate profile has required fields                     |
        \*---------------------------------------------------------*/
        if(!profile_json.contains("effect_type"))
        {
            QMessageBox::critical(this, "Invalid Profile", "Effect profile is missing effect type");
            return;
        }

        int effect_type = profile_json["effect_type"].get<int>();

        /*---------------------------------------------------------*\
        | Set effect type in combo (add 1 to skip "None")         |
        \*---------------------------------------------------------*/
        if(effect_combo)
        {
            effect_combo->setCurrentIndex(effect_type + 1);
        }

        /*---------------------------------------------------------*\
        | Restore zone selection                                   |
        \*---------------------------------------------------------*/
        if(profile_json.contains("zone_index") && effect_zone_combo)
        {
            int zone_idx = profile_json["zone_index"].get<int>();
            if(zone_idx < effect_zone_combo->count())
            {
                effect_zone_combo->setCurrentIndex(zone_idx);
            }
        }

        /*---------------------------------------------------------*\
        | Restore origin point selection                           |
        \*---------------------------------------------------------*/
        if(profile_json.contains("origin_index") && effect_origin_combo)
        {
            int origin_idx = profile_json["origin_index"].get<int>();
            if(origin_idx < effect_origin_combo->count())
            {
                effect_origin_combo->setCurrentIndex(origin_idx);
            }
        }

        /*---------------------------------------------------------*\
        | Restore effect parameters                                |
        \*---------------------------------------------------------*/
        if(profile_json.contains("effect_params") && current_effect_ui)
        {
            current_effect_ui->LoadSettings(profile_json["effect_params"]);
        }

        // Restore audio settings
        if(profile_json.contains("audio_settings"))
        {
            const nlohmann::json& a = profile_json["audio_settings"];
            if(a.contains("device_index") && audio_device_combo && audio_device_combo->isEnabled())
            {
                int di = a["device_index"].get<int>();
                if(di >= 0 && di < audio_device_combo->count())
                {
                    audio_device_combo->setCurrentIndex(di);
                    on_audio_device_changed(di);
                }
            }
            if(a.contains("gain_slider") && audio_gain_slider)
            {
                int gv = a["gain_slider"].get<int>();
                audio_gain_slider->setValue(gv);
                on_audio_gain_changed(gv);
            }
            // input smoothing removed (now per-effect)
            if(a.contains("bands_count") && audio_bands_combo)
            {
                int bc = a["bands_count"].get<int>();
                int idx = audio_bands_combo->findText(QString::number(bc));
                if(idx >= 0)
                {
                    audio_bands_combo->setCurrentIndex(idx);
                    on_audio_bands_changed(idx);
                }
            }
            // Crossovers UI removed; ignore bass_upper_hz/mid_upper_hz if present
        }

        
    }
    catch(const std::exception& e)
    {
        QString error_msg = QString("Failed to parse effect profile:\n%1\n\nError: %2")
            .arg(QString::fromStdString(filename))
            .arg(QString::fromStdString(e.what()));
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

    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string profiles_dir = config_dir + "plugins/OpenRGB3DSpatialPlugin/EffectProfiles/";

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

    std::string config_dir = resource_manager->GetConfigurationDirectory().string();
    std::string config_file = config_dir + "plugins/OpenRGB3DSpatialPlugin/effect_profile_config.json";

    filesystem::create_directories(config_dir + "plugins/OpenRGB3DSpatialPlugin/");

    std::string profile_name = effect_profiles_combo->currentText().toStdString();
    bool auto_load_enabled = effect_auto_load_checkbox->isChecked();

    nlohmann::json config;
    config["auto_load_enabled"] = auto_load_enabled;
    config["auto_load_profile"]  = profile_name;

    std::ofstream file(config_file);
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

    std::string config_dir  = resource_manager->GetConfigurationDirectory().string();
    std::string config_file = config_dir + "plugins/OpenRGB3DSpatialPlugin/effect_profile_config.json";

    if(!filesystem::exists(config_file))
    {
        return;
    }

    std::ifstream file(config_file);
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
    | Validate that an effect is selected in Effects tab       |
    \*---------------------------------------------------------*/
    if(!effect_combo || effect_combo->currentIndex() <= 0)
    {
        QMessageBox::information(this, "No Effect Selected",
                                "Please select an effect in the Effects tab before saving a profile.");
        return;
    }

    if(!current_effect_ui)
    {
        QMessageBox::information(this, "No Effect",
                                "No effect configuration to save.");
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


