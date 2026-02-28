/*---------------------------------------------------------*\
| FrequencyRangeEffect3D.h                                  |
|                                                           |
|   Multi-band audio effects data structure                 |
|                                                           |
|   Date: 2026-01-27                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef FREQUENCYRANGEEFFECT3D_H
#define FREQUENCYRANGEEFFECT3D_H

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "SpatialEffectTypes.h"
#include "SpatialEffect3D.h"

struct FrequencyRangeEffect3D
{
    int                                 id                  = -1;
    std::string                         name                = "Range";
    bool                                enabled             = true;
    
    float                               low_hz              = 20.0f;
    float                               high_hz             = 200.0f;
    
    std::string                         effect_class_name;
    int                                 zone_index          = -1;
    int                                 origin_ref_index    = -1;
    
    Vector3D                            position            = {0.0f, 0.0f, 0.0f};
    Vector3D                            rotation            = {0.0f, 0.0f, 0.0f};
    Vector3D                            scale               = {1.0f, 1.0f, 1.0f};
    
    nlohmann::json                      effect_settings;
    
    float                               smoothing           = 0.7f;
    float                               sensitivity         = 1.0f;
    float                               attack              = 0.05f;
    float                               decay               = 0.2f;
    
    std::unique_ptr<SpatialEffect3D>    effect_instance;
    float                               current_level       = 0.0f;
    float                               smoothed_level      = 0.0f;
    
    nlohmann::json SaveToJSON() const
    {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["enabled"] = enabled;
        j["low_hz"] = low_hz;
        j["high_hz"] = high_hz;
        j["effect_class_name"] = effect_class_name;
        j["zone_index"] = zone_index;
        j["origin_ref_index"] = origin_ref_index;
        j["position"] = {position.x, position.y, position.z};
        j["rotation"] = {rotation.x, rotation.y, rotation.z};
        j["scale"] = {scale.x, scale.y, scale.z};
        j["effect_settings"] = effect_settings;
        j["smoothing"] = smoothing;
        j["sensitivity"] = sensitivity;
        j["attack"] = attack;
        j["decay"] = decay;
        return j;
    }
    
    void LoadFromJSON(const nlohmann::json& j)
    {
        if(j.contains("id")) id = j["id"].get<int>();
        if(j.contains("name")) name = j["name"].get<std::string>();
        if(j.contains("enabled")) enabled = j["enabled"].get<bool>();
        if(j.contains("low_hz")) low_hz = j["low_hz"].get<float>();
        if(j.contains("high_hz")) high_hz = j["high_hz"].get<float>();
        if(j.contains("effect_class_name")) effect_class_name = j["effect_class_name"].get<std::string>();
        if(j.contains("zone_index")) zone_index = j["zone_index"].get<int>();
        if(j.contains("origin_ref_index")) origin_ref_index = j["origin_ref_index"].get<int>();
        
        if(j.contains("position") && j["position"].is_array() && j["position"].size() == 3)
        {
            position.x = j["position"][0].get<float>();
            position.y = j["position"][1].get<float>();
            position.z = j["position"][2].get<float>();
        }
        
        if(j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() == 3)
        {
            rotation.x = j["rotation"][0].get<float>();
            rotation.y = j["rotation"][1].get<float>();
            rotation.z = j["rotation"][2].get<float>();
        }
        
        if(j.contains("scale") && j["scale"].is_array() && j["scale"].size() == 3)
        {
            scale.x = j["scale"][0].get<float>();
            scale.y = j["scale"][1].get<float>();
            scale.z = j["scale"][2].get<float>();
        }
        
        if(j.contains("effect_settings")) effect_settings = j["effect_settings"];
        if(j.contains("smoothing")) smoothing = j["smoothing"].get<float>();
        if(j.contains("sensitivity")) sensitivity = j["sensitivity"].get<float>();
        if(j.contains("attack")) attack = j["attack"].get<float>();
        if(j.contains("decay")) decay = j["decay"].get<float>();
        
        current_level   = 0.0f;
        smoothed_level  = 0.0f;
        effect_instance.reset();
    }
};

#endif
