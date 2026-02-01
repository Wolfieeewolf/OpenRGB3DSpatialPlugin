/*---------------------------------------------------------*\
| EffectInstance3D.h                                        |
|                                                           |
|   Represents a single effect instance in the stack       |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef EFFECTINSTANCE3D_H
#define EFFECTINSTANCE3D_H

#include <string>
#include <memory>
#include "SpatialEffect3D.h"
#include "RGBController.h"
#include "EffectListManager3D.h"
#include <nlohmann/json.hpp>

/** Blend modes for combining multiple effects. */
enum class BlendMode
{
    NO_BLEND,   // No blending - effect runs independently
    REPLACE,    // Replace existing color (last effect wins)
    ADD,        // Add colors together (brighten)
    MULTIPLY,   // Multiply colors (darken)
    SCREEN,     // Screen blend (brighten without overexposure)
    MAX,        // Take brightest channel
    MIN         // Take darkest channel
};

inline const char* BlendModeToString(BlendMode mode)
{
    switch(mode)
    {
        case BlendMode::NO_BLEND:  return "No Blend";
        case BlendMode::REPLACE:   return "Replace";
        case BlendMode::ADD:       return "Add";
        case BlendMode::MULTIPLY:  return "Multiply";
        case BlendMode::SCREEN:    return "Screen";
        case BlendMode::MAX:       return "Max";
        case BlendMode::MIN:       return "Min";
        default:                   return "Unknown";
    }
}

/** One effect in the stack. */
struct EffectInstance3D
{
    std::string name;                           // User-friendly name ("Wave on Desk")
    std::string effect_class_name;              // Class name for serialization ("Wave3D")
    std::unique_ptr<SpatialEffect3D> effect;   // The actual effect object
    int zone_index;                             // -1 = All Controllers, >=0 = specific zone
    BlendMode blend_mode;                       // How to combine with other effects
    bool enabled;                               // Is this effect active?
    int id;                                     // Unique ID for this instance
    std::unique_ptr<nlohmann::json> saved_settings; // Saved effect settings for lazy loading

    EffectInstance3D()
        : name("New Effect")
        , effect_class_name("")
        , effect(nullptr)
        , zone_index(-1)
        , blend_mode(BlendMode::NO_BLEND)
        , enabled(true)
        , id(0)
        , saved_settings(nullptr)
    {
    }

    unsigned int GetEffectiveTargetFPS() const
    {
        return (effect && enabled) ? effect->GetTargetFPSSetting() : 0;
    }

    std::string GetDisplayName() const
    {
        std::string zone_name = (zone_index == -1) ? "All" : "Zone " + std::to_string(zone_index);

        // Use effect name if available, otherwise use stored name or class_name
        std::string effect_type;
        if(effect)
        {
            effect_type = effect->GetEffectInfo().effect_name;
        }
        else if(!name.empty() && name != "New Effect")
        {
            effect_type = name;
        }
        else if(!effect_class_name.empty())
        {
            // Get UI name from effect manager
            EffectRegistration3D info = EffectListManager3D::get()->GetEffectInfo(effect_class_name);
            effect_type = info.ui_name.empty() ? effect_class_name : info.ui_name;
        }
        else
        {
            effect_type = "None";
        }

        return effect_type + " - " + zone_name + " - " + BlendModeToString(blend_mode);
    }

    // Serialization
    nlohmann::json ToJson() const;
    static std::unique_ptr<EffectInstance3D> FromJson(const nlohmann::json& j);
};

// Blend two colors together
inline RGBColor BlendColors(RGBColor base, RGBColor overlay, BlendMode mode)
{
    unsigned char base_r = RGBGetRValue(base);
    unsigned char base_g = RGBGetGValue(base);
    unsigned char base_b = RGBGetBValue(base);

    unsigned char overlay_r = RGBGetRValue(overlay);
    unsigned char overlay_g = RGBGetGValue(overlay);
    unsigned char overlay_b = RGBGetBValue(overlay);

    unsigned char result_r, result_g, result_b;

    switch(mode)
    {
        case BlendMode::NO_BLEND:
            // No blending - return overlay as-is (effect runs independently)
            return overlay;

        case BlendMode::REPLACE:
            return overlay;

        case BlendMode::ADD:
            result_r = (unsigned char)std::min(255, (int)base_r + (int)overlay_r);
            result_g = (unsigned char)std::min(255, (int)base_g + (int)overlay_g);
            result_b = (unsigned char)std::min(255, (int)base_b + (int)overlay_b);
            break;

        case BlendMode::MULTIPLY:
            result_r = (unsigned char)((base_r * overlay_r) / 255);
            result_g = (unsigned char)((base_g * overlay_g) / 255);
            result_b = (unsigned char)((base_b * overlay_b) / 255);
            break;

        case BlendMode::SCREEN:
            result_r = (unsigned char)(255 - ((255 - base_r) * (255 - overlay_r)) / 255);
            result_g = (unsigned char)(255 - ((255 - base_g) * (255 - overlay_g)) / 255);
            result_b = (unsigned char)(255 - ((255 - base_b) * (255 - overlay_b)) / 255);
            break;

        case BlendMode::MAX:
            result_r = std::max(base_r, overlay_r);
            result_g = std::max(base_g, overlay_g);
            result_b = std::max(base_b, overlay_b);
            break;

        case BlendMode::MIN:
            result_r = std::min(base_r, overlay_r);
            result_g = std::min(base_g, overlay_g);
            result_b = std::min(base_b, overlay_b);
            break;

        default:
            return base;
    }

    return ToRGBColor(result_r, result_g, result_b);
}

#endif // EFFECTINSTANCE3D_H
