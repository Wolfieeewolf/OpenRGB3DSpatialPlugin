/*---------------------------------------------------------*\
| StackPreset3D.cpp                                         |
|                                                           |
|   Stack preset implementation                            |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "StackPreset3D.h"

nlohmann::json StackPreset3D::ToJson() const
{
    nlohmann::json j;
    j["name"]    = name;
    j["effects"] = nlohmann::json::array();

    for(unsigned int i = 0; i < effect_instances.size(); i++)
    {
        if(effect_instances[i])
            j["effects"].push_back(effect_instances[i]->ToJson());
    }

    return j;
}

std::unique_ptr<StackPreset3D> StackPreset3D::FromJson(const nlohmann::json& j)
{
    std::unique_ptr<StackPreset3D> preset = std::make_unique<StackPreset3D>();

    if(j.contains("name"))
    {
        preset->name = j["name"].get<std::string>();
    }

    if(j.contains("effects") && j["effects"].is_array())
    {
        const nlohmann::json& effects_array = j["effects"];
        for(unsigned int i = 0; i < effects_array.size(); i++)
        {
            std::unique_ptr<EffectInstance3D> instance = EffectInstance3D::FromJson(effects_array[i]);
            if(instance)
            {
                preset->effect_instances.push_back(std::move(instance));
            }
        }
    }

    return preset;
}

std::unique_ptr<StackPreset3D> StackPreset3D::CreateFromStack(
    const std::string& preset_name,
    const std::vector<std::unique_ptr<EffectInstance3D>>& stack)
{
    std::unique_ptr<StackPreset3D> preset = std::make_unique<StackPreset3D>();
    preset->name = preset_name;

    // Deep copy each effect instance
    for(unsigned int i = 0; i < stack.size(); i++)
    {
        if(!stack[i]) continue;
        // Convert to JSON and back to create a deep copy
        nlohmann::json instance_json = stack[i]->ToJson();
        std::unique_ptr<EffectInstance3D> copied_instance = EffectInstance3D::FromJson(instance_json);
        if(copied_instance)
        {
            preset->effect_instances.push_back(std::move(copied_instance));
        }
    }

    return preset;
}
