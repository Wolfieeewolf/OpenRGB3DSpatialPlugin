// SPDX-License-Identifier: GPL-2.0-only

#ifndef STACKPRESET3D_H
#define STACKPRESET3D_H

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "EffectInstance3D.h"

struct StackPreset3D
{
    std::string name;
    std::vector<std::unique_ptr<EffectInstance3D>> effect_instances;

    // Serialization
    nlohmann::json ToJson() const;
    static std::unique_ptr<StackPreset3D> FromJson(const nlohmann::json& j);

    // Helper to create a copy of the stack
    static std::unique_ptr<StackPreset3D> CreateFromStack(
        const std::string& preset_name,
        const std::vector<std::unique_ptr<EffectInstance3D>>& stack);
};

#endif // STACKPRESET3D_H
