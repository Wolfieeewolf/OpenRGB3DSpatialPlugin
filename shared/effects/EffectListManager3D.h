// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTLISTMANAGER3D_H
#define EFFECTLISTMANAGER3D_H

#include "SpatialEffectTypes.h"

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

class SpatialEffect3D;

struct EffectRegistration3D
{
    std::string class_name;
    std::string ui_name;
    std::string category;
    std::string game_id;
    std::string game_label;
    std::function<SpatialEffect3D*()> constructor;
    SpatialEffectSettingsLayout settings_layout = SpatialEffectSettingsLayout::FullWithTransport;
};

class EffectListManager3D
{
public:
    static EffectListManager3D* get();

    void RegisterEffect(const std::string& class_name,
                        const std::string& ui_name,
                        const std::string& category,
                        const std::string& game_id,
                        const std::string& game_label,
                        std::function<SpatialEffect3D*()> constructor,
                        SpatialEffectSettingsLayout settings_layout =
                            SpatialEffectSettingsLayout::FullWithTransport);

    SpatialEffect3D* CreateEffect(const std::string& class_name);

    bool IsEffectRegistered(const std::string& class_name) const;

    std::vector<EffectRegistration3D> GetAllEffects() const;

    EffectRegistration3D GetEffectInfo(const std::string& class_name) const;

    std::map<std::string, std::vector<EffectRegistration3D>> GetCategorizedEffects() const;

    /** Unique game_id / game_label pairs from Game-category registrations, sorted by label. */
    std::vector<std::pair<std::string, std::string>> GetRegisteredGames() const;

private:
    EffectListManager3D() = default;

    std::map<std::string, EffectRegistration3D> effects;
    std::vector<std::string> effect_order;
};

#endif
