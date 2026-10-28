// SPDX-License-Identifier: GPL-2.0-only

#include "EffectInstance3D.h"
#include "PluginLog.h"

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

        if(effect)
        {
            j["effect_settings"] = effect->SaveSettings();
        }
        else if(saved_settings)
        {
            j["effect_settings"] = *saved_settings;
        }
    }

    return j;
}

std::unique_ptr<EffectInstance3D> EffectInstance3D::FromJson(const nlohmann::json& j)
{
    std::unique_ptr<EffectInstance3D> instance = std::make_unique<EffectInstance3D>();

    if(j.contains("name"))
    {
        instance->name = j["name"].get<std::string>();
    }

    if(j.contains("zone_index"))
    {
        instance->zone_index = j["zone_index"].get<int>();
    }

    if(j.contains("blend_mode") && j["blend_mode"].is_number_integer())
    {
        int bm = j["blend_mode"].get<int>();
        if(bm >= (int)BlendMode::NO_BLEND && bm <= (int)BlendMode::MIN)
        {
            instance->blend_mode = (BlendMode)bm;
        }
    }

    if(j.contains("enabled"))
    {
        instance->enabled = j["enabled"].get<bool>();
    }

    if(j.contains("id"))
    {
        instance->id = j["id"].get<int>();
    }

    if(j.contains("effect_type"))
    {
        instance->effect_class_name = j["effect_type"].get<std::string>();
        if(j.contains("effect_settings"))
        {
            instance->saved_settings = std::make_unique<nlohmann::json>(j["effect_settings"]);
        }
    }

    return instance;
}
