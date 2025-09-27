/*---------------------------------------------------------*\
| SpatialEffectManager.cpp                                  |
|                                                           |
|   Manager for auto-registered 3D spatial effects         |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpatialEffectManager.h"
#include "LogManager.h"

SpatialEffectManager* SpatialEffectManager::instance = nullptr;

SpatialEffectManager* SpatialEffectManager::Get()
{
    if(instance == nullptr)
    {
        instance = new SpatialEffectManager();
    }
    return instance;
}

SpatialEffectManager::SpatialEffectManager()
{
}

SpatialEffectManager::~SpatialEffectManager()
{
}

std::map<std::string, std::vector<SpatialEffectInfo_List>> SpatialEffectManager::GetCategorizedEffects()
{
    return categorized_effects;
}

std::function<SpatialEffect*()> SpatialEffectManager::GetEffectConstructor(std::string classname)
{
    auto it = effect_constructors.find(classname);
    if(it != effect_constructors.end())
    {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> SpatialEffectManager::GetEffectNames()
{
    return effect_names;
}

std::size_t SpatialEffectManager::GetEffectsListSize()
{
    return effect_constructors.size();
}

std::vector<SpatialEffectInfo> SpatialEffectManager::GetAvailableEffects()
{
    std::vector<SpatialEffectInfo> effects;

    for(const auto& pair : effect_constructors)
    {
        SpatialEffect* effect = pair.second();
        if(effect)
        {
            effects.push_back(effect->GetEffectInfo());
            delete effect;
        }
    }

    return effects;
}

void SpatialEffectManager::RegisterEffect(std::string classname,
                                         std::string ui_name,
                                         std::string category,
                                         std::function<SpatialEffect*()> constructor)
{
    LOG_VERBOSE("[3D Spatial] Registering effect: %s (%s) in category: %s",
                ui_name.c_str(), classname.c_str(), category.c_str());

    effect_constructors[classname] = constructor;
    effect_names.push_back(classname);

    SpatialEffectInfo_List info;
    info.classname = classname;
    info.ui_name = ui_name;
    info.category = category;

    categorized_effects[category].push_back(info);
}

SpatialEffect* SpatialEffectManager::CreateEffect(std::string classname)
{
    auto constructor = GetEffectConstructor(classname);
    if(constructor)
    {
        return constructor();
    }
    return nullptr;
}