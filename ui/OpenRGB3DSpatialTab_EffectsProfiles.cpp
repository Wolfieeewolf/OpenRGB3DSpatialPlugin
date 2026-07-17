// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "EffectListManager3D.h"
#include "EffectStackBlendRow.h"
#include "PluginSettingsPaths.h"
#include "SettingsManager.h"
#include "PluginLog.h"
#include "ui_OpenRGB3DSpatialTab.h"
#include <QFile>
#include <QMessageBox>
#include <QInputDialog>
#include <QTextStream>
#include <fstream>
std::string OpenRGB3DSpatialTab::GetEffectProfilePath(const std::string& profile_name)
{
    if(!resource_manager)
    {
        return std::string();
    }
    PluginSettingsPaths::EnsurePluginDataLayout(resource_manager);
    return PluginSettingsPaths::EffectProfileFile(resource_manager, profile_name).string();
}

void OpenRGB3DSpatialTab::SaveEffectProfile(const std::string& filename)
{
    if(effect_stack.empty())
    {
        QMessageBox::warning(this, "Nothing to Save", "Add at least one effect to the stack before saving a profile.");
        return;
    }

    nlohmann::json profile_json;
    profile_json["version"] = 8;

    nlohmann::json stack_json = nlohmann::json::array();
    for(size_t i = 0; i < effect_stack.size(); i++)
    {
        if(effect_stack[i])
        {
            stack_json.push_back(effect_stack[i]->ToJson());
        }
    }
    profile_json["stack"] = stack_json;
    profile_json["selected_stack_index"] = effectStackList() ? effectStackList()->currentRow() : -1;

    if(effectOriginCombo())
    {
        profile_json["origin_item_data"] = effectOriginCombo()->currentData().toInt();
    }

    nlohmann::json audio_json;
    int device_index = (audioDeviceCombo() ? audioDeviceCombo()->currentIndex() : -1);
    audio_json["device_index"] = device_index;
    audio_json["gain_slider"] = (audioGainSlider() ? audioGainSlider()->value() : 10);
    audio_json["bands_count"] = (audioBandsCombo() ? audioBandsCombo()->currentText().toInt() : 16);
    audio_json["fft_index"] = (audioFftCombo() ? audioFftCombo()->currentIndex() : -1);

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

        const int kSupportedVersion = 8;
        if(!profile_json.contains("version") || !profile_json["version"].is_number_integer())
        {
            QMessageBox::critical(this, "Invalid Profile",
                "This effect profile has no version number and cannot be loaded.");
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect profile missing version: %s", filename.c_str());
            return;
        }
        int version = profile_json["version"].get<int>();
        if(version != kSupportedVersion)
        {
            QMessageBox::critical(this, "Unsupported Profile",
                QString("This effect profile has version %1. This plugin supports version %2 only.")
                    .arg(version).arg(kSupportedVersion));
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect profile unsupported version %d: %s", version, filename.c_str());
            return;
        }

        if(!profile_json.contains("stack") || !profile_json["stack"].is_array())
        {
            QMessageBox::critical(this, "Invalid Profile",
                "This effect profile is missing a stack array and cannot be loaded.");
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Effect profile missing stack array: %s", filename.c_str());
            return;
        }
        RebuildEffectStackFromJson(profile_json["stack"]);

        int desired_index = -1;
        if(profile_json.contains("selected_stack_index"))
        {
            desired_index = profile_json["selected_stack_index"].get<int>();
        }
        if(desired_index < 0 || desired_index >= (int)effect_stack.size())
        {
            desired_index = effect_stack.empty() ? -1 : 0;
        }

        ApplyLoadedStackSelection(desired_index);

        if(effectOriginCombo() && profile_json.contains("origin_item_data") &&
           profile_json["origin_item_data"].is_number_integer())
        {
            int ref_data = profile_json["origin_item_data"].get<int>();
            int idx = effectOriginCombo()->findData(QVariant(ref_data));
            if(idx >= 0)
            {
                effectOriginCombo()->setCurrentIndex(idx);
            }
        }

        if(profile_json.contains("audio_settings"))
        {
            const nlohmann::json& a = profile_json["audio_settings"];

            if(audioDeviceCombo() && audioDeviceCombo()->count() > 0 && a.contains("device_index"))
            {
                int di = a["device_index"].get<int>();
                if(di >= 0 && di < audioDeviceCombo()->count())
                {
                    audioDeviceCombo()->setCurrentIndex(di);
                    audioDeviceChanged(di);
                }
            }

            if(a.contains("gain_slider") && audioGainSlider())
            {
                int gv = std::max(1, std::min(500, a["gain_slider"].get<int>()));
                {
                    QSignalBlocker blocker(audioGainSlider());
                    audioGainSlider()->setValue(gv);
                }
                audioGainChanged(gv);
            }

            if(a.contains("bands_count") && audioBandsCombo())
            {
                int bc = a["bands_count"].get<int>();
                int idx = audioBandsCombo()->findText(QString::number(bc));
                if(idx >= 0)
                {
                    audioBandsCombo()->blockSignals(true);
                    audioBandsCombo()->setCurrentIndex(idx);
                    audioBandsCombo()->blockSignals(false);
                    audioBandsChanged(idx);
                }
            }

            if(a.contains("fft_index") && audioFftCombo())
                {
                    int fi = a["fft_index"].get<int>();
                    if(fi >= 0 && fi < audioFftCombo()->count())
                    {
                        {
                            QSignalBlocker blocker(audioFftCombo());
                            audioFftCombo()->setCurrentIndex(fi);
                        }
                        audioFftChanged(fi);
                    }
            }
        }

        SaveEffectStack();
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
    if(!resource_manager || !effectProfilesCombo())
    {
        return;
    }

    bool restore_signals = effectProfilesCombo()->blockSignals(true);
    effectProfilesCombo()->clear();

    const filesystem::path plugin_root = PluginSettingsPaths::PluginRoot(resource_manager);
    if(!filesystem::exists(plugin_root))
    {
        effectProfilesCombo()->blockSignals(restore_signals);
        return;
    }

    std::error_code iter_ec;
    for(const filesystem::directory_entry& entry :
        filesystem::directory_iterator(plugin_root, iter_ec))
    {
        if(iter_ec || !entry.is_regular_file() || !PluginSettingsPaths::IsEffectProfileFile(entry.path()))
        {
            continue;
        }

        const std::string stem = entry.path().stem().string();
        const std::string profile_name = stem.substr(0, stem.length() - 14);
        effectProfilesCombo()->addItem(QString::fromStdString(profile_name));
    }

    nlohmann::json settings = GetPluginSettings();
    if(settings.contains("EffectSelectedProfile"))
    {
        const QString saved = QString::fromStdString(settings["EffectSelectedProfile"].get<std::string>());
        if(!saved.isEmpty())
        {
            const int index = effectProfilesCombo()->findText(saved);
            if(index >= 0)
            {
                effectProfilesCombo()->setCurrentIndex(index);
            }
        }
    }

    effectProfilesCombo()->blockSignals(restore_signals);
}

void OpenRGB3DSpatialTab::SaveCurrentEffectProfileName()
{
    if(!resource_manager || !effectProfilesCombo() || !autoLoadEffectProfileCheckbox())
    {
        return;
    }

    nlohmann::json settings = GetPluginSettings();
    settings["EffectSelectedProfile"] = effectProfilesCombo()->currentText().toStdString();
    settings["EffectAutoLoadEnabled"]   = autoLoadEffectProfileCheckbox()->isChecked();
    SetPluginSettings(settings);
}

void OpenRGB3DSpatialTab::TryAutoLoadEffectProfile()
{
    if(!resource_manager || !effectProfilesCombo() || !autoLoadEffectProfileCheckbox())
    {
        return;
    }

    nlohmann::json settings = GetPluginSettings();

    bool auto_load_enabled = false;
    std::string profile_name;

    if(settings.contains("EffectAutoLoadEnabled"))
    {
        auto_load_enabled = settings["EffectAutoLoadEnabled"].get<bool>();
    }
    if(settings.contains("EffectSelectedProfile"))
    {
        profile_name = settings["EffectSelectedProfile"].get<std::string>();
    }

    bool restore_auto_load_signals = autoLoadEffectProfileCheckbox()->blockSignals(true);
    autoLoadEffectProfileCheckbox()->setChecked(auto_load_enabled);
    autoLoadEffectProfileCheckbox()->blockSignals(restore_auto_load_signals);

    if(!profile_name.empty())
    {
        int index = effectProfilesCombo()->findText(QString::fromStdString(profile_name));
        if(index >= 0)
        {
            bool restore_profile_combo_signals = effectProfilesCombo()->blockSignals(true);
            effectProfilesCombo()->setCurrentIndex(index);
            effectProfilesCombo()->blockSignals(restore_profile_combo_signals);
        }
    }

    if(auto_load_enabled && !profile_name.empty())
    {
        const std::string profile_path = GetEffectProfilePath(profile_name);
        if(!profile_path.empty() && filesystem::exists(profile_path))
        {
            LoadEffectProfile(profile_path);
        }
    }
}

void OpenRGB3DSpatialTab::saveEffectProfileClicked()
{
    if(effect_stack.empty())
    {
        QMessageBox::information(this, "No Effect Selected",
                                "Add at least one effect to the stack before saving a profile.");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "Save Effect Profile",
                                        "Enter profile name (letters, numbers, underscore, hyphen, period only):",
                                        QLineEdit::Normal, "", &ok);

    if(!ok || name.isEmpty())
    {
        return;
    }

    QRegularExpression safe_name("^[\\w\\-\\.]+$");
    if(!safe_name.match(name).hasMatch())
    {
        QMessageBox::warning(this, "Invalid Profile Name",
            "Profile name may only contain letters, numbers, underscore (_), hyphen (-), and period (.).");
        return;
    }

    std::string profile_name = name.toStdString();
    std::string profile_path = GetEffectProfilePath(profile_name);
    if(profile_path.empty()) return;

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

    SaveEffectProfile(profile_path);

    PopulateEffectProfileDropdown();

    int index = effectProfilesCombo()->findText(name);
    if(index >= 0)
    {
        effectProfilesCombo()->setCurrentIndex(index);
    }

    QMessageBox::information(this, "Success",
                            QString("Effect profile \"%1\" saved successfully!").arg(name));
}

void OpenRGB3DSpatialTab::loadEffectProfileClicked()
{
    if(!effectProfilesCombo() || effectProfilesCombo()->currentIndex() < 0)
    {
        QMessageBox::information(this, "No Profile Selected",
                                "Please select an effect profile to load.");
        return;
    }

    QString profile_name = effectProfilesCombo()->currentText();
    std::string profile_path = GetEffectProfilePath(profile_name.toStdString());
    if(profile_path.empty()) return;

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

void OpenRGB3DSpatialTab::deleteEffectProfileClicked()
{
    if(!effectProfilesCombo() || effectProfilesCombo()->currentIndex() < 0)
    {
        QMessageBox::information(this, "No Profile Selected",
                                "Please select an effect profile to delete.");
        return;
    }

    QString profile_name = effectProfilesCombo()->currentText();

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Profile",
        QString("Are you sure you want to delete effect profile \"%1\"?").arg(profile_name),
        QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes)
    {
        return;
    }

    std::string profile_path = GetEffectProfilePath(profile_name.toStdString());
    if(profile_path.empty()) return;

    if(filesystem::exists(profile_path))
    {
        filesystem::remove(profile_path);
    }

    PopulateEffectProfileDropdown();
    SaveCurrentEffectProfileName();

    QMessageBox::information(this, "Success",
                            QString("Effect profile \"%1\" deleted successfully!").arg(profile_name));
}

void OpenRGB3DSpatialTab::effectProfileChanged(int)
{
    SaveCurrentEffectProfileName();
}


