// SPDX-License-Identifier: GPL-2.0-only

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

        std::map<std::string, EffectRegistration3D>::iterator existing = effects.find(class_name);
        if(existing != effects.end())
        {
            existing->second = reg;
            return;
        }

        effects[class_name] = reg;
        effect_order.push_back(class_name);
    }

    SpatialEffect3D* CreateEffect(const std::string& class_name)
    {
        std::string resolved = class_name;
        if(class_name == "Comet3D" || class_name == "ZigZag3D" || class_name == "Visor3D")
            resolved = "TravelingLight3D";
        else if(class_name == "CrossingBeams3D" || class_name == "RotatingBeam3D")
            resolved = "Beam3D";
        if(effects.find(resolved) != effects.end())
        {
            return effects[resolved].constructor();
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
            std::map<std::string, EffectRegistration3D>::const_iterator it = effects.find(effect_order[i]);
            if(it != effects.end())
            {
                result.push_back(it->second);
            }
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
