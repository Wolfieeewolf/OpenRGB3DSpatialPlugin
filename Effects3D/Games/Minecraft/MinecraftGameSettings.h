// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTGAMESETTINGS_H
#define MINECRAFTGAMESETTINGS_H

#include "RGBController.h"
#include <nlohmann/json.hpp>
#include <cstdint>

namespace MinecraftGame
{

/** Which logical Minecraft layers this effect instance renders (stack-friendly). */
constexpr std::uint32_t ChHealth = 1u << 0;
constexpr std::uint32_t ChHunger = 1u << 1;
constexpr std::uint32_t ChAir = 1u << 2;
constexpr std::uint32_t ChDurability = 1u << 3;
constexpr std::uint32_t ChDamage = 1u << 4;
constexpr std::uint32_t ChWorldTint = 1u << 5;
constexpr std::uint32_t ChLightning = 1u << 6;
constexpr std::uint32_t ChAll = 0xFFFFFFFFu;

struct WorldTintSmoothState
{
    bool has_smoothed = false;
    unsigned long long last_sample_ms = 0;
    RGBColor smooth_sky = (RGBColor)0x00FFBEAA;
    RGBColor smooth_mid = (RGBColor)0x0078B48C;
    RGBColor smooth_ground = (RGBColor)0x00507864;
};

struct Settings
{
    float damage_flash_decay_s = 0.35f;
    float world_light_mix = 0.85f;
    float world_tint_smoothing = 0.72f;
    float world_tint_directional = 0.46f;
    float world_tint_dir_sharpness = 1.8f;
    float lightning_flash_strength = 0.90f;
    float lightning_flash_decay_s = 0.28f;
    float damage_flash_strength = 1.0f;
    float base_brightness = 1.12f;
    bool enable_health_gradient = true;
    bool enable_hunger_gradient = true;
    bool enable_air_gradient = true;
    bool enable_durability_gradient = true;
    float hunger_mix = 0.45f;
    float air_mix = 0.55f;
    float durability_mix = 0.50f;
    bool enable_damage_flash = true;
    bool enable_ambient_world_tint = true;
    bool enable_lightning_flash = true;
    float tint_layer_ground_end = 0.36f;
    float tint_layer_sky_start = 0.54f;
    float biome_sky_overlay = 0.28f;
    float env_rain_darken_sky = 0.45f;
    float env_thunder_darken_sky = 0.35f;
    float damage_directional_mix = 0.80f;
    float damage_dir_sharpness = 1.35f;
};

void SettingsToJson(const Settings& s, nlohmann::json& out);
void SettingsFromJson(const nlohmann::json& j, Settings& s);

} // namespace MinecraftGame

#endif
