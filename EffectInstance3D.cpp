// SPDX-License-Identifier: GPL-2.0-only

#include "EffectInstance3D.h"
#include "EffectListManager3D.h"
#include "LogManager.h"

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

    if(j.contains("blend_mode"))
    {
        instance->blend_mode = (BlendMode)j["blend_mode"].get<int>();
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
        std::string effect_type = j["effect_type"].get<std::string>();
        if(effect_type == "WaveSurface")
        {
            instance->effect_class_name = "Wave";
            instance->saved_settings = std::make_unique<nlohmann::json>(j.contains("effect_settings") ? j["effect_settings"] : nlohmann::json::object());
            (*instance->saved_settings)["mode"] = 1;
            effect_type = "Wave";
        }
        else if(effect_type == "Wipe")
        {
            instance->effect_class_name = "TravelingLight";
            instance->saved_settings = std::make_unique<nlohmann::json>(j.contains("effect_settings") ? j["effect_settings"] : nlohmann::json::object());
            (*instance->saved_settings)["mode"] = 5;
            if(instance->saved_settings->contains("wipe_thickness"))
                (*instance->saved_settings)["wipe_thickness"] = (*instance->saved_settings)["wipe_thickness"];
            if(instance->saved_settings->contains("edge_shape"))
                (*instance->saved_settings)["wipe_edge_shape"] = (*instance->saved_settings)["edge_shape"];
            effect_type = "TravelingLight";
        }
        else if(effect_type == "MovingPanes")
        {
            instance->effect_class_name = "TravelingLight";
            instance->saved_settings = std::make_unique<nlohmann::json>(j.contains("effect_settings") ? j["effect_settings"] : nlohmann::json::object());
            (*instance->saved_settings)["mode"] = 6;
            effect_type = "TravelingLight";
        }
        else if(effect_type == "Beam")
        {
            instance->effect_class_name = "TravelingLight";
            instance->saved_settings = std::make_unique<nlohmann::json>(j.contains("effect_settings") ? j["effect_settings"] : nlohmann::json::object());
            int beam_mode = 7;
            if(instance->saved_settings && instance->saved_settings->contains("mode") && (*instance->saved_settings)["mode"].is_number_integer())
                beam_mode = ((*instance->saved_settings)["mode"].get<int>() == 1) ? 8 : 7;
            (*instance->saved_settings)["mode"] = beam_mode;
            if(instance->saved_settings->contains("beam_thickness"))
                (*instance->saved_settings)["beam_thickness"] = (*instance->saved_settings)["beam_thickness"];
            if(instance->saved_settings->contains("glow"))
                (*instance->saved_settings)["glow"] = (*instance->saved_settings)["glow"];
            effect_type = "TravelingLight";
        }
        else
        {
            instance->effect_class_name = effect_type;
            if(j.contains("effect_settings"))
                instance->saved_settings = std::make_unique<nlohmann::json>(j["effect_settings"]);
        }

        SpatialEffect3D* effect = EffectListManager3D::get()->CreateEffect(effect_type);
        if(effect)
        {
            instance->effect.reset(effect);

            if(instance->saved_settings)
            {
                effect->LoadSettings(*instance->saved_settings);
            }
        }
        else
        {
            LOG_ERROR("[EffectInstance3D] Failed to create effect '%s'", effect_type.c_str());
        }
    }

    return instance;
}
