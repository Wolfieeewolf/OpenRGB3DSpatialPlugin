/*---------------------------------------------------------*\
| EffectListManager3D.h                                     |
|                                                           |
|   Manages registered 3D spatial effects                  |
|                                                           |
|   Date: 2025-10-01                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef EFFECTLISTMANAGER3D_H
#define EFFECTLISTMANAGER3D_H

#include <string>
#include <vector>
#include <map>
#include <functional>

class SpatialEffect3D;

struct EffectRegistration3D
{
    std::string class_name;
    std::string ui_name;
    std::string category;
    std::function<SpatialEffect3D*()> constructor;
};

class EffectListManager3D
{
public:
    static EffectListManager3D* get()
    {
        static EffectListManager3D instance;
        return &instance;
    }

    void RegisterEffect(const std::string& class_name,
                       const std::string& ui_name,
                       const std::string& category,
                       std::function<SpatialEffect3D*()> constructor)
    {
        EffectRegistration3D reg;
        reg.class_name = class_name;
        reg.ui_name = ui_name;
        reg.category = category;
        reg.constructor = constructor;

        effects[class_name] = reg;
        effect_order.push_back(class_name);
    }

    SpatialEffect3D* CreateEffect(const std::string& class_name)
    {
        if(effects.find(class_name) != effects.end())
        {
            return effects[class_name].constructor();
        }
        return nullptr;
    }

    std::vector<std::string> GetEffectNames() const
    {
        return effect_order;
    }

    std::vector<EffectRegistration3D> GetAllEffects() const
    {
        std::vector<EffectRegistration3D> result;
        for(size_t i = 0; i < effect_order.size(); i++)
        {
            result.push_back(effects.at(effect_order[i]));
        }
        return result;
    }

    EffectRegistration3D GetEffectInfo(const std::string& class_name) const
    {
        if(effects.find(class_name) != effects.end())
        {
            return effects.at(class_name);
        }
        return EffectRegistration3D();
    }

private:
    EffectListManager3D() {}
    std::map<std::string, EffectRegistration3D> effects;
    std::vector<std::string> effect_order;
};

#endif
