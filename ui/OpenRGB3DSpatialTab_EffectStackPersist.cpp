// SPDX-License-Identifier: GPL-2.0-only


#include "OpenRGB3DSpatialTab.h"
#include "LogManager.h"
#include "filesystem.h"
#include <fstream>

std::string OpenRGB3DSpatialTab::GetEffectStackPath()
{
    filesystem::path config_dir  = resource_manager->GetConfigurationDirectory();
    filesystem::path plugin_dir  = config_dir / "plugins" / "settings" / "OpenRGB3DSpatialPlugin";
    filesystem::path stack_file  = plugin_dir / "effect_stack.json";

    /*---------------------------------------------------------*\
    | Create directory if it doesn't exist                     |
    \*---------------------------------------------------------*/
    filesystem::create_directories(plugin_dir);

    return stack_file.string();
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
        if(effect_stack[i])
        {
            j["effects"].push_back(effect_stack[i]->ToJson());
        }
    }

    /*---------------------------------------------------------*\
    | Write to file                                            |
    \*---------------------------------------------------------*/
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
                std::unique_ptr<EffectInstance3D> instance = EffectInstance3D::FromJson(effects_array[i]);
                if(instance)
                {
                    effect_stack.push_back(std::move(instance));
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

        
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("[OpenRGB3DSpatialPlugin] Failed to load effect stack: %s - %s",
                 stack_file.c_str(), e.what());
    }
}
