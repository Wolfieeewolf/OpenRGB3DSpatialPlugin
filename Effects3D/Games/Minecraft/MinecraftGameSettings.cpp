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
    put_f(j, "world_heading_offset_deg", s.world_heading_offset_deg);
    put_f(j, "room_vr_mix", s.room_vr_mix);
    put_f(j, "room_vr_heading_offset_deg", s.room_vr_heading_offset_deg);
    put_f(j, "room_vr_pos_offset_forward_blocks", s.room_vr_pos_offset_forward_blocks);
    put_f(j, "room_vr_pos_offset_right_blocks", s.room_vr_pos_offset_right_blocks);
    put_f(j, "room_vr_pos_offset_up_blocks", s.room_vr_pos_offset_up_blocks);
    put_f(j, "room_vr_scale_tune", s.room_vr_scale_tune);
    put_f(j, "room_vr_saturation", s.room_vr_saturation);
    put_f(j, "room_vr_contrast", s.room_vr_contrast);
    put_bool(j, "room_vr_sharp_sampling", s.room_vr_sharp_sampling);
    put_i(j, "room_vr_sample_target_cells", s.room_vr_sample_target_cells);
    put_f(j, "lightning_flash_strength", s.lightning_flash_strength);
    put_f(j, "lightning_flash_decay_s", s.lightning_flash_decay_s);
    put_f(j, "lightning_directional_mix", s.lightning_directional_mix);
    put_f(j, "lightning_dir_sharpness", s.lightning_dir_sharpness);
    put_f(j, "damage_flash_strength", s.damage_flash_strength);
    put_f(j, "base_brightness", s.base_brightness);
    put_bool(j, "health_per_heart_strip", s.health_per_heart_strip);
    put_bool(j, "health_per_heart_indexed", s.health_per_heart_indexed);
    put_i(j, "health_leds_per_heart", s.health_leds_per_heart);
    put_i(j, "health_strip_axis", s.health_strip_axis);
    put_bool(j, "health_strip_invert", s.health_strip_invert);
    put_bool(j, "hunger_per_strip", s.hunger_per_strip);
    put_bool(j, "air_per_strip", s.air_per_strip);
    put_bool(j, "durability_per_strip", s.durability_per_strip);
    put_f(j, "hunger_mix", s.hunger_mix);
    put_f(j, "air_mix", s.air_mix);
    put_f(j, "durability_mix", s.durability_mix);
    put_f(j, "damage_directional_mix", s.damage_directional_mix);
    put_f(j, "damage_dir_sharpness", s.damage_dir_sharpness);
}

void SettingsFromJson(const nlohmann::json& j, Settings& s)
{
    s.damage_flash_decay_s = get_f(j, "damage_flash_decay_s", s.damage_flash_decay_s);
    s.world_light_mix = get_f(j, "world_light_mix", s.world_light_mix);
    s.world_heading_offset_deg = get_f(j, "world_heading_offset_deg", s.world_heading_offset_deg);
    s.room_vr_mix = get_f(j, "room_vr_mix", get_f(j, "room_world_mix", s.room_vr_mix));
    s.room_vr_heading_offset_deg =
        get_f(j, "room_vr_heading_offset_deg", get_f(j, "room_heading_offset_deg", s.room_vr_heading_offset_deg));
    s.room_vr_pos_offset_forward_blocks = get_f(j, "room_vr_pos_offset_forward_blocks", s.room_vr_pos_offset_forward_blocks);
    s.room_vr_pos_offset_right_blocks = get_f(j, "room_vr_pos_offset_right_blocks", s.room_vr_pos_offset_right_blocks);
    s.room_vr_pos_offset_up_blocks = get_f(j, "room_vr_pos_offset_up_blocks", s.room_vr_pos_offset_up_blocks);
    s.room_vr_scale_tune = get_f(j, "room_vr_scale_tune", get_f(j, "room_scale_tune", s.room_vr_scale_tune));
    s.room_vr_saturation = get_f(j, "room_vr_saturation", s.room_vr_saturation);
    s.room_vr_contrast = get_f(j, "room_vr_contrast", s.room_vr_contrast);
    s.room_vr_sharp_sampling = get_bool(j, "room_vr_sharp_sampling", s.room_vr_sharp_sampling);
    s.room_vr_sample_target_cells = get_i(j, "room_vr_sample_target_cells", s.room_vr_sample_target_cells);
    s.lightning_flash_strength = get_f(j, "lightning_flash_strength", s.lightning_flash_strength);
    s.lightning_flash_decay_s = get_f(j, "lightning_flash_decay_s", s.lightning_flash_decay_s);
    s.lightning_directional_mix = get_f(j, "lightning_directional_mix", s.lightning_directional_mix);
    s.lightning_dir_sharpness = get_f(j, "lightning_dir_sharpness", s.lightning_dir_sharpness);
    s.damage_flash_strength = get_f(j, "damage_flash_strength", s.damage_flash_strength);
    s.base_brightness = get_f(j, "base_brightness", s.base_brightness);
    s.health_per_heart_strip = get_bool(j, "health_per_heart_strip", s.health_per_heart_strip);
    s.health_per_heart_indexed = get_bool(j, "health_per_heart_indexed", s.health_per_heart_indexed);
    s.health_leds_per_heart = get_i(j, "health_leds_per_heart", s.health_leds_per_heart);
    s.health_strip_axis = get_i(j, "health_strip_axis", s.health_strip_axis);
    s.health_strip_invert = get_bool(j, "health_strip_invert", s.health_strip_invert);
    s.hunger_per_strip = get_bool(j, "hunger_per_strip", s.hunger_per_strip);
    s.air_per_strip = get_bool(j, "air_per_strip", s.air_per_strip);
    s.durability_per_strip = get_bool(j, "durability_per_strip", s.durability_per_strip);
    s.hunger_mix = get_f(j, "hunger_mix", s.hunger_mix);
    s.air_mix = get_f(j, "air_mix", s.air_mix);
    s.durability_mix = get_f(j, "durability_mix", s.durability_mix);
    s.damage_directional_mix = get_f(j, "damage_directional_mix", s.damage_directional_mix);
    s.damage_dir_sharpness = get_f(j, "damage_dir_sharpness", s.damage_dir_sharpness);

    s.damage_flash_decay_s = std::clamp(s.damage_flash_decay_s, 0.10f, 0.90f);
    s.world_light_mix = std::clamp(s.world_light_mix, 0.0f, 1.0f);
    s.world_heading_offset_deg = std::clamp(s.world_heading_offset_deg, -180.0f, 180.0f);
    s.room_vr_mix = std::clamp(s.room_vr_mix, 0.0f, 1.0f);
    s.room_vr_heading_offset_deg = std::clamp(s.room_vr_heading_offset_deg, -180.0f, 180.0f);
    s.room_vr_pos_offset_forward_blocks = std::clamp(s.room_vr_pos_offset_forward_blocks, -8.0f, 8.0f);
    s.room_vr_pos_offset_right_blocks = std::clamp(s.room_vr_pos_offset_right_blocks, -8.0f, 8.0f);
    s.room_vr_pos_offset_up_blocks = std::clamp(s.room_vr_pos_offset_up_blocks, -8.0f, 8.0f);
    s.room_vr_scale_tune = std::clamp(s.room_vr_scale_tune, 0.5f, 2.0f);
    s.room_vr_saturation = std::clamp(s.room_vr_saturation, 0.8f, 2.5f);
    s.room_vr_contrast = std::clamp(s.room_vr_contrast, 0.8f, 2.0f);
    s.room_vr_sample_target_cells = std::clamp(s.room_vr_sample_target_cells, 4096, 512000);
    s.lightning_flash_strength = std::clamp(s.lightning_flash_strength, 0.0f, 1.5f);
    s.lightning_flash_decay_s = std::clamp(s.lightning_flash_decay_s, 0.08f, 0.90f);
    s.lightning_directional_mix = std::clamp(s.lightning_directional_mix, 0.0f, 1.0f);
    s.lightning_dir_sharpness = std::clamp(s.lightning_dir_sharpness, 0.5f, 5.0f);
    s.damage_flash_strength = std::clamp(s.damage_flash_strength, 0.0f, 1.0f);
    s.base_brightness = std::clamp(s.base_brightness, 0.8f, 1.5f);

    s.hunger_mix = std::clamp(s.hunger_mix, 0.0f, 1.0f);
    s.air_mix = std::clamp(s.air_mix, 0.0f, 1.0f);
    s.durability_mix = std::clamp(s.durability_mix, 0.0f, 1.0f);
    s.damage_directional_mix = std::clamp(s.damage_directional_mix, 0.0f, 1.0f);
    s.damage_dir_sharpness = std::clamp(s.damage_dir_sharpness, 0.5f, 5.0f);

    s.health_leds_per_heart = std::max(1, std::min(32, s.health_leds_per_heart));
    s.health_strip_axis = std::max(0, std::min(3, s.health_strip_axis));
}

}
