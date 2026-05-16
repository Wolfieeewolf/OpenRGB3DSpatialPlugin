// SPDX-License-Identifier: GPL-2.0-only

#ifndef MINECRAFTGAMESETTINGS_H
#define MINECRAFTGAMESETTINGS_H

#include "RGBController.h"
#include <nlohmann/json.hpp>
#include <cstdint>

namespace MinecraftGame
{

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
    float world_light_mix = 0.62f;
    float world_tint_vividness = 1.25f;
    float world_tint_smoothing = 0.72f;
    float world_tint_directional = 0.46f;
    float world_tint_dir_sharpness = 1.8f;
    float lightning_flash_strength = 0.72f;
    float lightning_flash_decay_s = 0.28f;
    float lightning_directional_mix = 0.65f;
    float lightning_dir_sharpness = 1.35f;
    float damage_flash_strength = 1.0f;
    float base_brightness = 1.0f;
    bool enable_health_gradient = true;
    bool health_per_heart_strip = false;
    bool health_per_heart_indexed = false;
    int health_leds_per_heart = 1;
    int health_strip_axis = 0;
    bool health_strip_invert = false;
    bool enable_hunger_gradient = true;
    bool enable_air_gradient = true;
    bool enable_durability_gradient = true;
    bool hunger_per_strip = false;
    bool air_per_strip = false;
    bool durability_per_strip = false;
    float hunger_mix = 0.45f;
    float air_mix = 0.55f;
    float durability_mix = 0.50f;
    bool enable_damage_flash = true;
    bool enable_ambient_world_tint = true;
    bool enable_lightning_flash = true;
    float tint_layer_ground_end = 0.30f;
    float tint_layer_sky_start = 0.68f;
    int spatial_mapping_mode = 0;
    int spatial_layer_profile_mode = 0;
    float spatial_center_size = 0.14f;
    float spatial_blend_softness = 0.10f;
    float spatial_heading_offset_deg = 0.0f;
    float spatial_compass_offset_deg = -45.0f;
    float spatial_voxel_room_scale = 0.18f;
    float spatial_voxel_mix = 0.78f;
    bool spatial_debug_sweep_enabled = false;
    float spatial_debug_sweep_hz = 2.0f;
    float spatial_floor_offset = 0.0f;
    float spatial_desk_offset = 0.0f;
    float spatial_upper_offset = 0.0f;
    float biome_sky_overlay = 0.18f;
    float env_rain_darken_sky = 0.45f;
    float env_thunder_darken_sky = 0.35f;
    float damage_directional_mix = 0.80f;
    float damage_dir_sharpness = 1.35f;
};

void SettingsToJson(const Settings& s, nlohmann::json& out);
void SettingsFromJson(const nlohmann::json& j, Settings& s);

}

#endif
