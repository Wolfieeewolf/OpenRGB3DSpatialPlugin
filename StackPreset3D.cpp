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
    j["name"] = name;
    j["effects"] = nlohmann::json::array();

    for(const auto& instance : effect_instances)
    {
        j["effects"].push_back(instance->ToJson());
    }

    return j;
}

std::unique_ptr<StackPreset3D> StackPreset3D::FromJson(const nlohmann::json& j)
{
    auto preset = std::make_unique<StackPreset3D>();

    if(j.contains("name"))
    {
        preset->name = j["name"].get<std::string>();
    }

    if(j.contains("effects") && j["effects"].is_array())
    {
        for(const auto& effect_json : j["effects"])
        {
            EffectInstance3D* instance = EffectInstance3D::FromJson(effect_json);
            if(instance)
            {
                preset->effect_instances.push_back(std::unique_ptr<EffectInstance3D>(instance));
            }
        }
    }

    return preset;
}

std::unique_ptr<StackPreset3D> StackPreset3D::CreateFromStack(
    const std::string& preset_name,
    const std::vector<std::unique_ptr<EffectInstance3D>>& stack)
{
    auto preset = std::make_unique<StackPreset3D>();
    preset->name = preset_name;

    // Deep copy each effect instance
    for(const auto& instance : stack)
    {
        // Convert to JSON and back to create a deep copy
        nlohmann::json instance_json = instance->ToJson();
        EffectInstance3D* copied_instance = EffectInstance3D::FromJson(instance_json);
        if(copied_instance)
        {
            preset->effect_instances.push_back(std::unique_ptr<EffectInstance3D>(copied_instance));
        }
    }

    return preset;
}
