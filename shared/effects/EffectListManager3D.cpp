// SPDX-License-Identifier: GPL-2.0-only

#include "EffectListManager3D.h"

#include <algorithm>

EffectListManager3D* EffectListManager3D::get()
{
    static EffectListManager3D instance;
    return &instance;
}

void EffectListManager3D::RegisterEffect(const std::string& class_name,
                                         const std::string& ui_name,
                                         const std::string& category,
                                         const std::string& game_id,
                                         const std::string& game_label,
                                         std::function<SpatialEffect3D*()> constructor,
                                         SpatialEffectSettingsLayout settings_layout)
{
    EffectRegistration3D reg;
    reg.class_name = class_name;
    reg.ui_name = ui_name;
    reg.category = category;
    reg.game_id = game_id;
    reg.game_label = game_label;
    reg.constructor = std::move(constructor);
    reg.settings_layout = settings_layout;

    const auto existing = effects.find(class_name);
    if(existing != effects.end())
    {
        existing->second = reg;
        return;
    }

    effects[class_name] = reg;
    effect_order.push_back(class_name);
}

SpatialEffect3D* EffectListManager3D::CreateEffect(const std::string& class_name)
{
    const auto it = effects.find(class_name);
    if(it != effects.end())
    {
        return it->second.constructor();
    }
    return nullptr;
}

bool EffectListManager3D::IsEffectRegistered(const std::string& class_name) const
{
    if(class_name.empty())
    {
        return false;
    }
    return effects.find(class_name) != effects.end();
}

std::vector<EffectRegistration3D> EffectListManager3D::GetAllEffects() const
{
    std::vector<EffectRegistration3D> result;
    result.reserve(effect_order.size());
    for(const std::string& name : effect_order)
    {
        const auto it = effects.find(name);
        if(it != effects.end())
        {
            result.push_back(it->second);
        }
    }
    return result;
}

EffectRegistration3D EffectListManager3D::GetEffectInfo(const std::string& class_name) const
{
    const auto it = effects.find(class_name);
    if(it != effects.end())
    {
        return it->second;
    }
    return EffectRegistration3D();
}

std::map<std::string, std::vector<EffectRegistration3D>> EffectListManager3D::GetCategorizedEffects() const
{
    std::map<std::string, std::vector<EffectRegistration3D>> result;
    for(const std::string& name : effect_order)
    {
        const auto it = effects.find(name);
        if(it != effects.end())
        {
            result[it->second.category].push_back(it->second);
        }
    }
    for(auto& entry : result)
    {
        std::sort(entry.second.begin(), entry.second.end(),
                  [](const EffectRegistration3D& a, const EffectRegistration3D& b) {
                      return a.ui_name < b.ui_name;
                  });
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> EffectListManager3D::GetRegisteredGames() const
{
    std::map<std::string, std::string> games;
    for(const std::string& name : effect_order)
    {
        const auto it = effects.find(name);
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
    for(const auto& git : games)
    {
        result.emplace_back(git.first, git.second);
    }
    std::sort(result.begin(), result.end(),
              [](const std::pair<std::string, std::string>& a,
                 const std::pair<std::string, std::string>& b) { return a.second < b.second; });
    return result;
}
