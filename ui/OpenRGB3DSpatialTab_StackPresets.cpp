// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include "LogManager.h"
#include "filesystem.h"
#include <QInputDialog>
#include <QMessageBox>
#include <fstream>

std::string OpenRGB3DSpatialTab::GetStackPresetsPath()
{
    if(!resource_manager) return std::string();
    filesystem::path config_dir  = resource_manager->GetConfigurationDirectory();
    filesystem::path presets_dir = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin" / "StackPresets";

    filesystem::create_directories(presets_dir);

    return presets_dir.string();
}

void OpenRGB3DSpatialTab::LoadStackPresets()
{
    stack_presets.clear();

    std::string presets_dir = GetStackPresetsPath();
    if(presets_dir.empty()) return;
    filesystem::path presets_path(presets_dir);

    if(!filesystem::exists(presets_path))
    {
        return;
    }

    filesystem::directory_iterator end_iter;
    for(filesystem::directory_iterator entry(presets_path); entry != end_iter; ++entry)
    {
        if(entry->path().extension() == ".json")
        {
            std::string filename = entry->path().string();
            std::string stem     = entry->path().stem().string();

            if(stem.length() > 6 && stem.substr(stem.length() - 6) == ".stack")
            {
                std::ifstream file(filename);
                if(file.is_open())
                {
                    try
                    {
                        nlohmann::json j;
                        file >> j;
                        file.close();

                        std::unique_ptr<StackPreset3D> preset = StackPreset3D::FromJson(j);
                        if(preset)
                        {
                            stack_presets.push_back(std::move(preset));
                        }
                    }
                    catch(const std::exception& e)
                    {
                        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load stack preset: %s - %s",
                                 filename.c_str(), e.what());
                    }
                }
            }
        }
    }

    UpdateStackPresetsList();
    UpdateEffectCombo();  // Add presets to Effects tab dropdown
}

void OpenRGB3DSpatialTab::SaveStackPresets()
{
    std::string presets_dir = GetStackPresetsPath();
    if(presets_dir.empty()) return;
    filesystem::path presets_path(presets_dir);

    for(unsigned int i = 0; i < stack_presets.size(); i++)
    {
        filesystem::path file_path = presets_path / (stack_presets[i]->name + ".stack.json");
        std::string filename = file_path.string();

        std::ofstream file(filename);
        if(file.is_open())
        {
            try
            {
                nlohmann::json j = stack_presets[i]->ToJson();
                file << j.dump(4);
                if(file.fail() || file.bad())
                {
                    LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write stack preset: %s", filename.c_str());
                }
                file.close();
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Exception while saving stack preset: %s - %s", filename.c_str(), e.what());
                file.close();
            }
        }
        else
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open stack preset file for writing: %s", filename.c_str());
        }
    }
}

void OpenRGB3DSpatialTab::UpdateStackPresetsList()
{
    if(!stack_presets_list)
    {
        return;
    }

    stack_presets_list->clear();

    for(unsigned int i = 0; i < stack_presets.size(); i++)
    {
        stack_presets_list->addItem(QString::fromStdString(stack_presets[i]->name));
    }
}

void OpenRGB3DSpatialTab::on_save_stack_preset_clicked()
{
    if(effect_stack.empty())
    {
        QMessageBox::information(this, "No Effects",
                                "Please add some effects to the stack before saving.");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "Save Stack Preset",
                                        "Enter preset name:", QLineEdit::Normal,
                                        "", &ok);

    if(!ok || name.isEmpty())
    {
        return;
    }

    std::string preset_name = name.toStdString();

    for(unsigned int i = 0; i < stack_presets.size(); i++)
    {
        if(stack_presets[i]->name == preset_name)
        {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Overwrite Preset",
                "A preset with this name already exists. Overwrite?",
                QMessageBox::Yes | QMessageBox::No);

            if(reply == QMessageBox::Yes)
            {
                stack_presets.erase(stack_presets.begin() + i);
            }
            else
            {
                return;
            }
            break;
        }
    }

    std::unique_ptr<StackPreset3D> preset = StackPreset3D::CreateFromStack(preset_name, effect_stack);
    stack_presets.push_back(std::move(preset));

    SaveStackPresets();

    UpdateStackPresetsList();
    UpdateEffectCombo();

    QMessageBox::information(this, "Success",
                            QString("Stack preset \"%1\" saved successfully!").arg(name));
}

void OpenRGB3DSpatialTab::on_load_stack_preset_clicked()
{
    if(!stack_presets_list) return;
    int current_row = stack_presets_list->currentRow();

    if(current_row < 0 || current_row >= (int)stack_presets.size())
    {
        QMessageBox::information(this, "No Preset Selected",
                                "Please select a preset to load.");
        return;
    }

    StackPreset3D* preset = stack_presets[current_row].get();
    if(!preset) return;

    effect_stack.clear();

    for(unsigned int i = 0; i < preset->effect_instances.size(); i++)
    {
        if(!preset->effect_instances[i]) continue;
        nlohmann::json instance_json = preset->effect_instances[i]->ToJson();
        std::unique_ptr<EffectInstance3D> copied_instance = EffectInstance3D::FromJson(instance_json);
        if(copied_instance)
        {
            effect_stack.push_back(std::move(copied_instance));
        }
    }

    UpdateEffectStackList();

    if(!effect_stack.empty() && effect_stack_list)
    {
        effect_stack_list->setCurrentRow(0);
    }

    if(effect_timer && !effect_timer->isActive())
    {
        effect_timer->start(33);
    }

    QMessageBox::information(this, "Success",
                            QString("Stack preset \"%1\" loaded successfully!")
                            .arg(QString::fromStdString(preset->name)));
}

void OpenRGB3DSpatialTab::on_delete_stack_preset_clicked()
{
    if(!stack_presets_list) return;
    int current_row = stack_presets_list->currentRow();

    if(current_row < 0 || current_row >= (int)stack_presets.size())
    {
        QMessageBox::information(this, "No Preset Selected",
                                "Please select a preset to delete.");
        return;
    }

    StackPreset3D* preset = stack_presets[current_row].get();
    QString preset_name = QString::fromStdString(preset->name);

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Preset",
        QString("Are you sure you want to delete the preset \"%1\"?").arg(preset_name),
        QMessageBox::Yes | QMessageBox::No);

    if(reply != QMessageBox::Yes)
    {
        return;
    }

    std::string presets_dir = GetStackPresetsPath();
    if(presets_dir.empty()) return;
    filesystem::path presets_path(presets_dir);
    filesystem::path file_path = presets_path / (preset->name + ".stack.json");
    if(filesystem::exists(file_path))
    {
        filesystem::remove(file_path);
    }

    stack_presets.erase(stack_presets.begin() + current_row);

    UpdateStackPresetsList();
    UpdateEffectCombo();

    QMessageBox::information(this, "Success",
                            QString("Stack preset \"%1\" deleted successfully!").arg(preset_name));
}
