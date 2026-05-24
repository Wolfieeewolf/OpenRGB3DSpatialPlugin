// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftGameSettings.h"

#include <algorithm>

namespace MinecraftGame
{

static void put_bool(nlohmann::json& j, const char* k, bool v) { j[k] = v; }
static void put_f(nlohmann::json& j, const char* k, float v) { j[k] = v; }
static bool get_bool(const nlohmann::json& j, const char* k, bool d)
{
    return j.contains(k) && j[k].is_boolean() ? j[k].get<bool>() : d;
}
static float get_f(const nlohmann::json& j, const char* k, float d)
{
    return j.contains(k) && j[k].is_number() ? j[k].get<float>() : d;
}
static void put_i(nlohmann::json& j, const char* k, int v) { j[k] = v; }
static int get_i(const nlohmann::json& j, const char* k, int d)
{
    if(!j.contains(k) || !j[k].is_number())
    {
        return d;
    }
    if(j[k].is_number_integer())
    {
        return j[k].get<int>();
    }
    return (int)j[k].get<double>();
}

void SettingsToJson(const Settings& s, nlohmann::json& j)
{
    put_f(j, "damage_flash_decay_s", s.damage_flash_decay_s);
    put_f(j, "world_light_mix", s.world_light_mix);
    put_f(j, "world_tint_vividness", s.world_tint_vividness);
    put_f(j, "world_tint_smoothing", s.world_tint_smoothing);
    put_f(j, "world_tint_directional", s.world_tint_directional);
    put_f(j, "world_tint_dir_sharpness", s.world_tint_dir_sharpness);
    put_f(j, "lightning_flash_strength", s.lightning_flash_strength);
    put_f(j, "lightning_flash_decay_s", s.lightning_flash_decay_s);
    put_f(j, "lightning_directional_mix", s.lightning_directional_mix);
    put_f(j, "lightning_dir_sharpness", s.lightning_dir_sharpness);
    put_f(j, "damage_flash_strength", s.damage_flash_strength);
    put_f(j, "base_brightness", s.base_brightness);
    put_bool(j, "enable_health_gradient", s.enable_health_gradient);
    put_bool(j, "health_per_heart_strip", s.health_per_heart_strip);
    put_bool(j, "health_per_heart_indexed", s.health_per_heart_indexed);
    put_i(j, "health_leds_per_heart", s.health_leds_per_heart);
    put_i(j, "health_strip_axis", s.health_strip_axis);
    put_bool(j, "health_strip_invert", s.health_strip_invert);
    put_bool(j, "enable_hunger_gradient", s.enable_hunger_gradient);
    put_bool(j, "enable_air_gradient", s.enable_air_gradient);
    put_bool(j, "enable_durability_gradient", s.enable_durability_gradient);
    put_bool(j, "hunger_per_strip", s.hunger_per_strip);
    put_bool(j, "air_per_strip", s.air_per_strip);
    put_bool(j, "durability_per_strip", s.durability_per_strip);
    put_f(j, "hunger_mix", s.hunger_mix);
    put_f(j, "air_mix", s.air_mix);
    put_f(j, "durability_mix", s.durability_mix);
    put_bool(j, "enable_damage_flash", s.enable_damage_flash);
    put_bool(j, "enable_ambient_world_tint", s.enable_ambient_world_tint);
    put_bool(j, "enable_lightning_flash", s.enable_lightning_flash);
    put_f(j, "tint_layer_ground_end", s.tint_layer_ground_end);
    put_f(j, "tint_layer_sky_start", s.tint_layer_sky_start);
    put_i(j, "spatial_mapping_mode", s.spatial_mapping_mode);
    put_i(j, "spatial_layer_profile_mode", s.spatial_layer_profile_mode);
    put_f(j, "spatial_center_size", s.spatial_center_size);
    put_f(j, "spatial_blend_softness", s.spatial_blend_softness);
    put_f(j, "spatial_heading_offset_deg", s.spatial_heading_offset_deg);
    put_f(j, "spatial_compass_offset_deg", s.spatial_compass_offset_deg);
    put_f(j, "spatial_voxel_room_scale", s.spatial_voxel_room_scale);
    put_f(j, "spatial_voxel_mix", s.spatial_voxel_mix);
    put_bool(j, "spatial_debug_sweep_enabled", s.spatial_debug_sweep_enabled);
    put_f(j, "spatial_debug_sweep_hz", s.spatial_debug_sweep_hz);
    put_f(j, "spatial_floor_offset", s.spatial_floor_offset);
    put_f(j, "spatial_desk_offset", s.spatial_desk_offset);
    put_f(j, "spatial_upper_offset", s.spatial_upper_offset);
    put_f(j, "biome_sky_overlay", s.biome_sky_overlay);
    put_f(j, "env_rain_darken_sky", s.env_rain_darken_sky);
    put_f(j, "env_thunder_darken_sky", s.env_thunder_darken_sky);
    put_f(j, "damage_directional_mix", s.damage_directional_mix);
    put_f(j, "damage_dir_sharpness", s.damage_dir_sharpness);
}

void SettingsFromJson(const nlohmann::json& j, Settings& s)
{
    s.damage_flash_decay_s = get_f(j, "damage_flash_decay_s", s.damage_flash_decay_s);
    s.world_light_mix = get_f(j, "world_light_mix", s.world_light_mix);
    s.world_tint_vividness = get_f(j, "world_tint_vividness", s.world_tint_vividness);
    s.world_tint_smoothing = get_f(j, "world_tint_smoothing", s.world_tint_smoothing);
    s.world_tint_directional = get_f(j, "world_tint_directional", s.world_tint_directional);
    s.world_tint_dir_sharpness = get_f(j, "world_tint_dir_sharpness", s.world_tint_dir_sharpness);
    s.lightning_flash_strength = get_f(j, "lightning_flash_strength", s.lightning_flash_strength);
    s.lightning_flash_decay_s = get_f(j, "lightning_flash_decay_s", s.lightning_flash_decay_s);
    s.lightning_directional_mix = get_f(j, "lightning_directional_mix", s.lightning_directional_mix);
    s.lightning_dir_sharpness = get_f(j, "lightning_dir_sharpness", s.lightning_dir_sharpness);
    s.damage_flash_strength = get_f(j, "damage_flash_strength", s.damage_flash_strength);
    s.base_brightness = get_f(j, "base_brightness", s.base_brightness);
    s.enable_health_gradient = get_bool(j, "enable_health_gradient", s.enable_health_gradient);
    s.health_per_heart_strip = get_bool(j, "health_per_heart_strip", s.health_per_heart_strip);
    s.health_per_heart_indexed = get_bool(j, "health_per_heart_indexed", s.health_per_heart_indexed);
    s.health_leds_per_heart = get_i(j, "health_leds_per_heart", s.health_leds_per_heart);
    s.health_strip_axis = get_i(j, "health_strip_axis", s.health_strip_axis);
    s.health_strip_invert = get_bool(j, "health_strip_invert", s.health_strip_invert);
    s.enable_hunger_gradient = get_bool(j, "enable_hunger_gradient", s.enable_hunger_gradient);
    s.enable_air_gradient = get_bool(j, "enable_air_gradient", s.enable_air_gradient);
    s.enable_durability_gradient = get_bool(j, "enable_durability_gradient", s.enable_durability_gradient);
    s.hunger_per_strip = get_bool(j, "hunger_per_strip", s.hunger_per_strip);
    s.air_per_strip = get_bool(j, "air_per_strip", s.air_per_strip);
    s.durability_per_strip = get_bool(j, "durability_per_strip", s.durability_per_strip);
    s.hunger_mix = get_f(j, "hunger_mix", s.hunger_mix);
    s.air_mix = get_f(j, "air_mix", s.air_mix);
    s.durability_mix = get_f(j, "durability_mix", s.durability_mix);
    s.enable_damage_flash = get_bool(j, "enable_damage_flash", s.enable_damage_flash);
    s.enable_ambient_world_tint = get_bool(j, "enable_ambient_world_tint", s.enable_ambient_world_tint);
    s.enable_lightning_flash = get_bool(j, "enable_lightning_flash", s.enable_lightning_flash);
    s.tint_layer_ground_end = get_f(j, "tint_layer_ground_end", s.tint_layer_ground_end);
    s.tint_layer_sky_start = get_f(j, "tint_layer_sky_start", s.tint_layer_sky_start);
    s.spatial_mapping_mode = get_i(j, "spatial_mapping_mode", s.spatial_mapping_mode);
    s.spatial_layer_profile_mode = get_i(j, "spatial_layer_profile_mode", s.spatial_layer_profile_mode);
    s.spatial_center_size = get_f(j, "spatial_center_size", s.spatial_center_size);
    s.spatial_blend_softness = get_f(j, "spatial_blend_softness", s.spatial_blend_softness);
    s.spatial_heading_offset_deg = get_f(j, "spatial_heading_offset_deg", s.spatial_heading_offset_deg);
    s.spatial_compass_offset_deg = get_f(j, "spatial_compass_offset_deg", s.spatial_compass_offset_deg);
    s.spatial_voxel_room_scale = get_f(j, "spatial_voxel_room_scale", s.spatial_voxel_room_scale);
    s.spatial_voxel_mix = get_f(j, "spatial_voxel_mix", s.spatial_voxel_mix);
    s.spatial_debug_sweep_enabled = get_bool(j, "spatial_debug_sweep_enabled", s.spatial_debug_sweep_enabled);
    s.spatial_debug_sweep_hz = get_f(j, "spatial_debug_sweep_hz", s.spatial_debug_sweep_hz);
    s.spatial_floor_offset = get_f(j, "spatial_floor_offset", s.spatial_floor_offset);
    s.spatial_desk_offset = get_f(j, "spatial_desk_offset", s.spatial_desk_offset);
    s.spatial_upper_offset = get_f(j, "spatial_upper_offset", s.spatial_upper_offset);
    s.biome_sky_overlay = get_f(j, "biome_sky_overlay", s.biome_sky_overlay);
    s.env_rain_darken_sky = get_f(j, "env_rain_darken_sky", s.env_rain_darken_sky);
    s.env_thunder_darken_sky = get_f(j, "env_thunder_darken_sky", s.env_thunder_darken_sky);
    s.damage_directional_mix = get_f(j, "damage_directional_mix", s.damage_directional_mix);
    s.damage_dir_sharpness = get_f(j, "damage_dir_sharpness", s.damage_dir_sharpness);

    s.damage_flash_decay_s = std::clamp(s.damage_flash_decay_s, 0.10f, 0.90f);
    s.world_light_mix = std::clamp(s.world_light_mix, 0.0f, 1.0f);
    s.world_tint_vividness = std::clamp(s.world_tint_vividness, 0.60f, 2.00f);
    s.world_tint_smoothing = std::clamp(s.world_tint_smoothing, 0.0f, 0.95f);
    s.world_tint_directional = std::clamp(s.world_tint_directional, 0.0f, 1.0f);
    s.world_tint_dir_sharpness = std::clamp(s.world_tint_dir_sharpness, 0.8f, 3.2f);
    s.lightning_flash_strength = std::clamp(s.lightning_flash_strength, 0.0f, 1.5f);
    s.lightning_flash_decay_s = std::clamp(s.lightning_flash_decay_s, 0.08f, 0.90f);
    s.lightning_directional_mix = std::clamp(s.lightning_directional_mix, 0.0f, 1.0f);
    s.lightning_dir_sharpness = std::clamp(s.lightning_dir_sharpness, 0.5f, 5.0f);
    s.damage_flash_strength = std::clamp(s.damage_flash_strength, 0.0f, 1.0f);
    s.base_brightness = std::clamp(s.base_brightness, 0.8f, 1.5f);

    s.hunger_mix = std::clamp(s.hunger_mix, 0.0f, 1.0f);
    s.air_mix = std::clamp(s.air_mix, 0.0f, 1.0f);
    s.durability_mix = std::clamp(s.durability_mix, 0.0f, 1.0f);
    s.tint_layer_ground_end = std::clamp(s.tint_layer_ground_end, 0.08f, 0.55f);
    s.tint_layer_sky_start = std::clamp(s.tint_layer_sky_start, 0.40f, 0.92f);
    if(s.spatial_mapping_mode != 0 && s.spatial_mapping_mode != 1 && s.spatial_mapping_mode != 2)
    {
        s.spatial_mapping_mode = 0;
    }
    if(s.spatial_layer_profile_mode != 0 && s.spatial_layer_profile_mode != 3 && s.spatial_layer_profile_mode != 4)
    {
        s.spatial_layer_profile_mode = 0;
    }
    s.spatial_center_size = std::clamp(s.spatial_center_size, 0.02f, 0.65f);
    s.spatial_blend_softness = std::clamp(s.spatial_blend_softness, 0.02f, 0.35f);
    s.spatial_heading_offset_deg = std::clamp(s.spatial_heading_offset_deg, -180.0f, 180.0f);
    s.spatial_compass_offset_deg = std::clamp(s.spatial_compass_offset_deg, -180.0f, 180.0f);
    s.spatial_voxel_room_scale = std::clamp(s.spatial_voxel_room_scale, 0.02f, 0.80f);
    s.spatial_voxel_mix = std::clamp(s.spatial_voxel_mix, 0.0f, 1.0f);
    s.spatial_debug_sweep_hz = std::clamp(s.spatial_debug_sweep_hz, 0.2f, 12.0f);
    s.spatial_floor_offset = std::clamp(s.spatial_floor_offset, -0.30f, 0.30f);
    s.spatial_desk_offset = std::clamp(s.spatial_desk_offset, -0.30f, 0.30f);
    s.spatial_upper_offset = std::clamp(s.spatial_upper_offset, -0.30f, 0.30f);
    s.biome_sky_overlay = std::clamp(s.biome_sky_overlay, 0.0f, 1.0f);
    s.env_rain_darken_sky = std::clamp(s.env_rain_darken_sky, 0.0f, 1.0f);
    s.env_thunder_darken_sky = std::clamp(s.env_thunder_darken_sky, 0.0f, 1.0f);
    s.damage_directional_mix = std::clamp(s.damage_directional_mix, 0.0f, 1.0f);
    s.damage_dir_sharpness = std::clamp(s.damage_dir_sharpness, 0.5f, 5.0f);

    s.health_leds_per_heart = std::max(1, std::min(32, s.health_leds_per_heart));
    s.health_strip_axis = std::max(0, std::min(3, s.health_strip_axis));
    if(s.tint_layer_sky_start < s.tint_layer_ground_end + 0.04f)
    {
        s.tint_layer_sky_start = std::min(0.92f, s.tint_layer_ground_end + 0.04f);
    }
}

}
