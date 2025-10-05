/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab_EffectStackPersist.cpp                |
|                                                           |
|   Effect Stack persistence (auto-save session state)     |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "OpenRGB3DSpatialTab.h"
#include "LogManager.h"
#include "filesystem.h"
#include <fstream>

std::string OpenRGB3DSpatialTab::GetEffectStackPath()
{
    std::string config_dir  = resource_manager->GetConfigurationDirectory().string();
    std::string stack_file  = config_dir + "plugins/OpenRGB3DSpatialPlugin/effect_stack.json";

    /*---------------------------------------------------------*\
    | Create directory if it doesn't exist                     |
    \*---------------------------------------------------------*/
    std::string dir = config_dir + "plugins/OpenRGB3DSpatialPlugin/";
    filesystem::create_directories(dir);

    return stack_file;
}

void OpenRGB3DSpatialTab::SaveEffectStack()
{
    std::string stack_file = GetEffectStackPath();

    nlohmann::json j;
    j["version"] = 1;
    j["effects"] = nlohmann::json::array();

    /*---------------------------------------------------------*\
    | Save each effect in the stack                           |
    \*---------------------------------------------------------*/
    for(unsigned int i = 0; i < effect_stack.size(); i++)
    {
        j["effects"].push_back(effect_stack[i]->ToJson());
    }

    /*---------------------------------------------------------*\
    | Write to file                                            |
    \*---------------------------------------------------------*/
    std::ofstream file(stack_file);
    if(file.is_open())
    {
        file << j.dump(4);
        file.close();
        LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Effect stack saved (%d effects)", (int)effect_stack.size());
    }
    else
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to save effect stack: %s", stack_file.c_str());
    }
}

void OpenRGB3DSpatialTab::LoadEffectStack()
{
    std::string stack_file = GetEffectStackPath();

    if(!filesystem::exists(stack_file))
    {
        LOG_VERBOSE("[OpenRGB3DSpatialPlugin] No saved effect stack found");
        return;
    }

    std::ifstream file(stack_file);
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

        /*---------------------------------------------------------*\
        | Clear current stack                                      |
        \*---------------------------------------------------------*/
        effect_stack.clear();

        /*---------------------------------------------------------*\
        | Load effects from JSON                                   |
        \*---------------------------------------------------------*/
        if(j.contains("effects") && j["effects"].is_array())
        {
            const nlohmann::json& effects_array = j["effects"];
            for(unsigned int i = 0; i < effects_array.size(); i++)
            {
                EffectInstance3D* instance = EffectInstance3D::FromJson(effects_array[i]);
                if(instance)
                {
                    effect_stack.push_back(std::unique_ptr<EffectInstance3D>(instance));
                }
            }
        }

        /*---------------------------------------------------------*\
        | Update UI                                                |
        \*---------------------------------------------------------*/
        UpdateEffectStackList();
        if(!effect_stack.empty())
        {
            effect_stack_list->setCurrentRow(0);
        }

        LOG_VERBOSE("[OpenRGB3DSpatialPlugin] Effect stack loaded (%d effects)", (int)effect_stack.size());
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load effect stack: %s - %s",
                 stack_file.c_str(), e.what());
    }
}
