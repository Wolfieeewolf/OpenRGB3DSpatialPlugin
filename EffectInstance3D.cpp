/*---------------------------------------------------------*\
| EffectInstance3D.cpp                                      |
|                                                           |
|   Represents a single effect instance in the stack       |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "EffectInstance3D.h"
#include "EffectListManager3D.h"

nlohmann::json EffectInstance3D::ToJson() const
{
    nlohmann::json j;

    j["name"] = name;
    j["zone_index"] = zone_index;
    j["blend_mode"] = (int)blend_mode;
    j["enabled"] = enabled;
    j["id"] = id;

    if(!effect_class_name.empty())
    {
        j["effect_type"] = effect_class_name;

        // Save effect parameters
        if(effect)
        {
            j["effect_settings"] = effect->SaveSettings();
        }
    }

    return j;
}

EffectInstance3D* EffectInstance3D::FromJson(const nlohmann::json& j)
{
    EffectInstance3D* instance = new EffectInstance3D();

    if(j.contains("name"))
        instance->name = j["name"].get<std::string>();

    if(j.contains("zone_index"))
        instance->zone_index = j["zone_index"].get<int>();

    if(j.contains("blend_mode"))
        instance->blend_mode = (BlendMode)j["blend_mode"].get<int>();

    if(j.contains("enabled"))
        instance->enabled = j["enabled"].get<bool>();

    if(j.contains("id"))
        instance->id = j["id"].get<int>();

    // Restore effect from type
    if(j.contains("effect_type"))
    {
        std::string effect_type = j["effect_type"].get<std::string>();
        instance->effect_class_name = effect_type;

        SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(effect_type);
        if(effect)
        {
            instance->effect.reset(effect);

            // Load effect parameters if they exist
            if(j.contains("effect_settings"))
            {
                effect->LoadSettings(j["effect_settings"]);
            }
        }
    }

    return instance;
}
