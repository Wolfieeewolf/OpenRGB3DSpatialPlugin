// SPDX-License-Identifier: GPL-2.0-only

#include "OpenRGB3DSpatialTab.h"
#include "LogManager.h"
#include "filesystem.h"
#include <QSignalBlocker>
#include <fstream>

std::string OpenRGB3DSpatialTab::GetEffectStackPath()
{
    if(!resource_manager) return std::string();
    filesystem::path config_dir  = resource_manager->GetConfigurationDirectory();
    filesystem::path plugin_dir  = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin";
    filesystem::path stack_file  = plugin_dir / "effect_stack.json";

    filesystem::create_directories(plugin_dir);

    return stack_file.string();
}

void OpenRGB3DSpatialTab::SaveEffectStack()
{
    std::string stack_file = GetEffectStackPath();
    if(stack_file.empty()) return;

    nlohmann::json j;
    j["version"] = 1;
    j["effects"] = nlohmann::json::array();

    for(unsigned int i = 0; i < effect_stack.size(); i++)
    {
        if(!effect_stack[i]) continue;
        try
        {
            j["effects"].push_back(effect_stack[i]->ToJson());
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to serialize effect %u: %s", (unsigned)i, e.what());
        }
    }

    std::ofstream file(stack_file);
    if(file.is_open())
    {
        try
        {
            file << j.dump(4);
            if(file.fail() || file.bad())
            {
                LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to write effect stack to file: %s", stack_file.c_str());
            }
            file.close();
        }
        catch(const std::exception& e)
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Exception while saving effect stack: %s - %s", stack_file.c_str(), e.what());
            file.close();
        }
    }
    else
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open effect stack file for writing: %s", stack_file.c_str());
    }
}

void OpenRGB3DSpatialTab::LoadEffectStack()
{
    std::string stack_file = GetEffectStackPath();
    if(stack_file.empty()) return;
    filesystem::path stack_path(stack_file);

    if(!filesystem::exists(stack_path))
    {
        return;
    }

    std::ifstream file(stack_path.string());
    if(!file.is_open())
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to open effect stack file: %s", stack_file.c_str());
        return;
    }

    try
    {
        nlohmann::json j;
        file >> j;
        file.close();

        if(!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() != 1)
        {
            LOG_ERROR("[OpenRGB3DSpatialPlugin] Unsupported effect stack version in file: %s", stack_file.c_str());
            return;
        }

        if(j.contains("effects") && j["effects"].is_array())
        {
            RebuildEffectStackFromJson(j["effects"]);
        }
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load effect stack: %s - %s",
                 stack_file.c_str(), e.what());
    }
}

bool OpenRGB3DSpatialTab::RebuildEffectStackFromJson(const nlohmann::json& effects_array)
{
    LoadStackEffectControls(nullptr);
    effect_stack.clear();

    if(!effects_array.is_array())
    {
        return false;
    }

    bool loaded_any = false;
    for(unsigned int i = 0; i < effects_array.size(); i++)
    {
        std::unique_ptr<EffectInstance3D> instance = EffectInstance3D::FromJson(effects_array[i]);
        if(!instance)
        {
            continue;
        }
        if(!EffectListManager3D::get()->IsEffectRegistered(instance->effect_class_name))
        {
            LOG_WARNING("[OpenRGB3DSpatialPlugin] Skipping stack layer (effect no longer available): %s",
                        instance->effect_class_name.c_str());
            continue;
        }
        effect_stack.push_back(std::move(instance));
        loaded_any = true;
    }
    return loaded_any;
}

void OpenRGB3DSpatialTab::ApplyLoadedStackSelection(int desired_index)
{
    if(desired_index < 0 || desired_index >= (int)effect_stack.size())
    {
        desired_index = effect_stack.empty() ? -1 : 0;
    }

    bool restore_stack_list_signals = false;
    if(effect_stack_list)
    {
        restore_stack_list_signals = effect_stack_list->blockSignals(true);
    }
    UpdateEffectStackList();

    if(effect_stack_list)
    {
        effect_stack_list->setCurrentRow(desired_index);
    }

    if(!effect_stack.empty() && desired_index >= 0 && desired_index < (int)effect_stack.size())
    {
        EffectInstance3D* instance = effect_stack[desired_index].get();
        LoadStackEffectControls(instance);
        if(effect_zone_combo)
        {
            QSignalBlocker zb(effect_zone_combo);
            int zi = effect_zone_combo->findData(instance->zone_index);
            if(zi >= 0)
            {
                effect_zone_combo->setCurrentIndex(zi);
            }
        }
        if(effect_combo && desired_index < effect_combo->count())
        {
            QSignalBlocker cb(effect_combo);
            effect_combo->setCurrentIndex(desired_index);
        }
        UpdateEffectCombo();
        if(effect_combo && desired_index < effect_combo->count())
        {
            QSignalBlocker cb(effect_combo);
            effect_combo->setCurrentIndex(desired_index);
        }
        UpdateAudioPanelVisibility();
    }
    else
    {
        ClearCustomEffectUI();
    }

    if(effect_stack_list)
    {
        effect_stack_list->blockSignals(restore_stack_list_signals);
    }
}
