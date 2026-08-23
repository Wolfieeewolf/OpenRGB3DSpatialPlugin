// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTLISTMANAGER3D_H
#define EFFECTLISTMANAGER3D_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <utility>

class SpatialEffect3D;

struct EffectRegistration3D
{
    std::string class_name;
    std::string ui_name;
    std::string category;
    std::string game_id;
    std::string game_label;
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
                       const std::string& game_id,
                       const std::string& game_label,
                       std::function<SpatialEffect3D*()> constructor)
    {
        EffectRegistration3D reg;
        reg.class_name = class_name;
        reg.ui_name = ui_name;
        reg.category = category;
        reg.game_id = game_id;
        reg.game_label = game_label;
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
        std::map<std::string, EffectRegistration3D>::iterator it = effects.find(class_name);
        if(it != effects.end())
        {
            return it->second.constructor();
        }
        return nullptr;
    }

    bool IsEffectRegistered(const std::string& class_name) const
    {
        if(class_name.empty())
        {
            return false;
        }
        return effects.find(class_name) != effects.end();
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
        std::map<std::string, EffectRegistration3D>::const_iterator it = effects.find(class_name);
        if(it != effects.end())
        {
            return it->second;
        }
        return EffectRegistration3D();
    }

    std::map<std::string, std::vector<EffectRegistration3D>> GetCategorizedEffects() const
    {
        std::map<std::string, std::vector<EffectRegistration3D>> result;
        for(size_t i = 0; i < effect_order.size(); i++)
        {
            std::map<std::string, EffectRegistration3D>::const_iterator it = effects.find(effect_order[i]);
            if(it != effects.end())
            {
                result[it->second.category].push_back(it->second);
            }
        }
        for(std::pair<const std::string, std::vector<EffectRegistration3D>>& entry : result)
        {
            std::sort(entry.second.begin(), entry.second.end(),
                      [](const EffectRegistration3D& a, const EffectRegistration3D& b) { return a.ui_name < b.ui_name; });
        }
        return result;
    }

    /** Unique game_id / game_label pairs from Game-category registrations, sorted by label. */
    std::vector<std::pair<std::string, std::string>> GetRegisteredGames() const
    {
        std::map<std::string, std::string> games;
        for(size_t i = 0; i < effect_order.size(); i++)
        {
            std::map<std::string, EffectRegistration3D>::const_iterator it = effects.find(effect_order[i]);
            if(it == effects.end())
            {
                continue;
            }
            const EffectRegistration3D& reg = it->second;
            if(reg.category != "Game" || reg.game_id.empty())
            {
                continue;
            }
            games[reg.game_id] = reg.game_label;
        }

        std::vector<std::pair<std::string, std::string>> result;
        result.reserve(games.size());
        for(std::map<std::string, std::string>::const_iterator git = games.begin(); git != games.end(); ++git)
        {
            result.emplace_back(git->first, git->second);
        }
        std::sort(result.begin(), result.end(),
                  [](const std::pair<std::string, std::string>& a,
                     const std::pair<std::string, std::string>& b) { return a.second < b.second; });
        return result;
    }

private:
    EffectListManager3D() {}

    std::map<std::string, EffectRegistration3D> effects;
    std::vector<std::string> effect_order;
};

#endif
