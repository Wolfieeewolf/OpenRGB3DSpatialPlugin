/*---------------------------------------------------------*\
| SpatialEffectManager.h                                    |
|                                                           |
|   Manager for auto-registered 3D spatial effects         |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALEFFECTMANAGER_H
#define SPATIALEFFECTMANAGER_H

#include <functional>
#include <string>
#include <vector>
#include <map>
#include "SpatialEffect.h"

struct SpatialEffectInfo_List
{
    std::string classname;
    std::string ui_name;
    std::string category;
};

class SpatialEffectManager
{
public:
    static SpatialEffectManager* Get();
    static SpatialEffectManager* Instance() { return Get(); }

    SpatialEffectManager();
    ~SpatialEffectManager();

    std::map<std::string, std::vector<SpatialEffectInfo_List>> GetCategorizedEffects();
    std::function<SpatialEffect*()> GetEffectConstructor(std::string classname);
    std::vector<std::string> GetEffectNames();
    std::vector<SpatialEffectInfo> GetAvailableEffects();
    std::size_t GetEffectsListSize();

    void RegisterEffect(std::string classname,
                       std::string ui_name,
                       std::string category,
                       std::function<SpatialEffect*()> constructor);

    // Create effect instance by name
    SpatialEffect* CreateEffect(std::string classname);

private:
    static SpatialEffectManager* instance;

    std::map<std::string, std::function<SpatialEffect*()>> effect_constructors;
    std::map<std::string, std::vector<SpatialEffectInfo_List>> categorized_effects;
    std::vector<std::string> effect_names;
};

#endif // SPATIALEFFECTMANAGER_H