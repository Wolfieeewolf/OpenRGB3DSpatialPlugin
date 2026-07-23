// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "RGBController.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace EffectPack
{

constexpr int kFormatVersion = 1;
constexpr int kMaxDurationMs = 60000;
constexpr const char* kFormatId = "openrgb3d.effect_pack";
constexpr const char* kFileSuffix = ".oreffect.json";

enum class LoopMode
{
    Once,
    Forever,
    WhileActive,
};

enum class TargetKind
{
    All,
    Device,
    Zone,
    Leds,
};

enum class BlockType
{
    Solid,
    Fade,
    Pulse,
};

struct Target
{
    TargetKind kind = TargetKind::All;
    std::string device_name;
    std::string zone_name;
    std::vector<int> led_indices;
};

struct Block
{
    BlockType type = BlockType::Solid;
    int start_ms = 0;
    int end_ms = 1000;
    RGBColor color = ToRGBColor(255, 0, 0);
    RGBColor color_from = ToRGBColor(0, 0, 0);
    RGBColor color_to = ToRGBColor(255, 255, 255);
    float intensity = 1.0f;
    float min_intensity = 0.15f;
    float max_intensity = 1.0f;
    int period_ms = 1000;
};

struct Track
{
    std::string name;
    Target target;
    std::vector<Block> blocks;
};

struct Pack
{
    std::string id;
    std::string name;
    int duration_ms = 1000;
    LoopMode loop = LoopMode::Once;
    int priority = 0;
    std::vector<Track> tracks;
};

/** Map pack-local time into [0, duration) respecting loop mode. Returns false if finished (once). */
bool MapPlaybackTime(const Pack& pack, int elapsed_ms, bool event_active, int* out_local_ms);

/** Evaluate the first matching block colour for a track at local time. */
bool EvaluateTrackColor(const Track& track, int local_ms, RGBColor* out_color, float* out_intensity);

nlohmann::json ToJson(const Pack& pack);
bool FromJson(const nlohmann::json& j, Pack* out, std::string* error);
bool LoadFromFile(const std::string& path, Pack* out, std::string* error);
bool SaveToFile(const std::string& path, const Pack& pack, std::string* error);

Pack MakeExampleRainbowWash();

} // namespace EffectPack
