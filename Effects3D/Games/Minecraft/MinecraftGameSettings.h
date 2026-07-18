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
constexpr std::uint32_t ChRoomVrTint = 1u << 7;

struct Settings
{
    float damage_flash_decay_s = 0.35f;
    float world_light_mix = 0.85f;
    float world_heading_offset_deg = 0.0f;
    float room_vr_mix = 1.0f;
    float room_vr_heading_offset_deg = 0.0f;
    /** Player anchor shift in MC blocks (local forward/right/up), not room rotation. */
    float room_vr_pos_offset_forward_blocks = 0.0f;
    float room_vr_pos_offset_right_blocks = 0.0f;
    float room_vr_pos_offset_up_blocks = 0.0f;
    float room_vr_scale_tune = 1.0f;
    float room_vr_saturation = 1.35f;
    float room_vr_contrast = 1.08f;
    bool room_vr_sharp_sampling = false;
    /** Maximum sample cells the mod may compute per frame. The actual grid is auto-sized to match
     *  Minecraft block resolution at the current scale (one cell per MC block per axis), so most
     *  configurations use far fewer cells than this ceiling and update at full 20 Hz.
     *  Only increase if you want very fine zoom (high blocks-per-metre × high MC world scale). */
    int room_vr_sample_target_cells = 65536;
    float lightning_flash_strength = 0.72f;
    float lightning_flash_decay_s = 0.28f;
    float lightning_directional_mix = 0.65f;
    float lightning_dir_sharpness = 1.35f;
    float damage_flash_strength = 1.0f;
    float base_brightness = 1.0f;
    bool health_per_heart_strip = false;
    bool health_per_heart_indexed = false;
    int health_leds_per_heart = 1;
    int health_strip_axis = 0;
    bool health_strip_invert = false;
    bool hunger_per_strip = false;
    bool air_per_strip = false;
    bool durability_per_strip = false;
    float hunger_mix = 0.45f;
    float air_mix = 0.55f;
    float durability_mix = 0.50f;
    float damage_directional_mix = 0.80f;
    float damage_dir_sharpness = 1.35f;
};

void SettingsToJson(const Settings& s, nlohmann::json& out);
void SettingsFromJson(const nlohmann::json& j, Settings& s);

}

#endif
