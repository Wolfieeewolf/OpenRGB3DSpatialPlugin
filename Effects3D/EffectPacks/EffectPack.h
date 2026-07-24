// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "RGBController.h"
#include "filesystem.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace EffectPack
{

constexpr int kFormatVersion = 2;
constexpr int kMaxDurationMs = 60000;
constexpr const char* kFormatId = "openrgb3d.effect_pack";
constexpr const char* kFileSuffix = ".oreffect.json";

inline bool IsPackFileName(const std::string& name)
{
    const std::string suffix = kFileSuffix;
    return name.size() >= suffix.size()
        && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

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
    Solid,      // Set Level
    Fade,
    Pulse,
    Wipe,
    Chase,
    Twinkle,
    ColorWash,
};

enum class Direction
{
    Left,
    Right,
    Up,
    Down,
};

struct Target
{
    TargetKind kind = TargetKind::All;
    std::string device_name;
    std::string zone_name;
    std::vector<int> led_indices;
};

struct GradientStop
{
    float pos = 0.0f; // 0..1
    RGBColor color = ToRGBColor(255, 0, 0);
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
    Direction direction = Direction::Right;
    float speed = 1.0f;          // relative speed multiplier
    float pulse_length = 0.25f;  // chase head size 0..1
    std::vector<GradientStop> gradient;
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
    /**
     * Scene controllers this pack owns on the timeline.
     * Empty = all scene controllers (e.g. seeded rainbow wash).
     * TargetKind::All means “all devices listed here” (or every scene device if empty).
     */
    std::vector<std::string> devices;
    std::vector<Track> tracks;
};

/** Map pack-local time into [0, duration) respecting loop mode. Returns false if finished (once). */
bool MapPlaybackTime(const Pack& pack, int elapsed_ms, bool event_active, int* out_local_ms);

/** Sample block gradient (or color_from/to / color fallback) at t in [0,1]. */
RGBColor SampleGradient(const Block& block, float t);

/** Fill empty gradients from color / color_from / color_to fields. */
void EnsureBlockGradient(Block* block);

/** Evaluate colour for a whole-target sample (no spatial axis). */
bool EvaluateBlock(const Block& block, int local_ms, RGBColor* out_color, float* out_intensity);

/**
 * Evaluate colour using a normalised spatial axis position in [0,1]
 * (0 = wipe/chase start side for the chosen direction).
 * twinkle_seed should be a stable LED identity (not spatial rank).
 */
bool EvaluateBlockAtAxis(const Block& block,
                         int local_ms,
                         float axis_pos,
                         int twinkle_seed,
                         RGBColor* out_color,
                         float* out_intensity);

/**
 * Evaluate colour for one LED in a strip of led_count (indices 0..led_count-1).
 * Index order is used as a fallback axis when world positions are unavailable.
 */
bool EvaluateBlockAtLed(const Block& block,
                        int local_ms,
                        int led_index,
                        int led_count,
                        RGBColor* out_color,
                        float* out_intensity);

/** Evaluate the last matching block colour for a track at local time (non-spatial). */
bool EvaluateTrackColor(const Track& track, int local_ms, RGBColor* out_color, float* out_intensity);

/** Spatial track evaluate for one LED (index-order axis fallback). */
bool EvaluateTrackColorAtLed(const Track& track,
                             int local_ms,
                             int led_index,
                             int led_count,
                             RGBColor* out_color,
                             float* out_intensity);

/** Active (top-most) block at local time, or nullptr. */
const Block* FindActiveBlock(const Track& track, int local_ms);

/**
 * World-space axis in [0,1] for wipe/chase/colorwash.
 * Left/Right use room X; Up/Down use room Y. If that axis has almost no span
 * (e.g. flat horizontal strip + Up), falls back to the dominant span axis.
 */
float WorldAxisPos(Direction dir,
                   float x, float y, float z,
                   float min_x, float max_x,
                   float min_y, float max_y,
                   float min_z, float max_z);

const char* BlockTypeDisplayName(BlockType t);

nlohmann::json ToJson(const Pack& pack);
bool FromJson(const nlohmann::json& j, Pack* out, std::string* error);
bool LoadFromFile(const filesystem::path& path, Pack* out, std::string* error);
bool SaveToFile(const filesystem::path& path, const Pack& pack, std::string* error);

Pack MakeExampleRainbowWash();

} // namespace EffectPack
