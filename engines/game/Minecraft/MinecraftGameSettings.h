// SPDX-License-Identifier: GPL-2.0-or-later

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
constexpr std::uint32_t ChRoomAmbilight = 1u << 7;

struct Settings
{
    float damage_flash_decay_s = 0.35f;
    /** Fill open-air LED rays with outdoor sky/weather when the column can see sky. */
    bool room_ambilight_sky_enabled = true;
    /**
     * Cubemap face resolution (size_x == size_y; size_z == 6). Cost is LED-count driven —
     * higher values mainly improve direction→texel precision for WYSIWYG LED mapping.
     */
    int room_ambilight_cubemap_face_size = 128;
    /**
     * Block texture UV sample resolution in the Fabric mod (64…4096).
     * Higher = less blocky / more photo-like grain; costs RAM + async decode budget.
     */
    int room_ambilight_texture_uv_dim = 512;
    /** Which way is "straight ahead" in the game: 0 = the room's front (-Z), 1 = toward a display. */
    int room_ambilight_facing = 0;
    /** Display plane id for facing 1 (the screen you play on); -1 = the first one. */
    int room_ambilight_facing_display_id = -1;
    /** Front LEDs from the game's rendered picture (the rest from rays). */
    bool room_ambilight_screen_capture = true;
    /** Fraction of the screen from each edge that fades from picture to rays (0..0.5). */
    float room_ambilight_screen_edge = 0.15f;
    /** Width the game picture is shrunk to before it is read back (64..1280). */
    int room_ambilight_screen_width = 320;
    float damage_flash_strength = 1.0f;
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
