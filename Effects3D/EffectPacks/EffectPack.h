// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "RGBController.h"
#include "filesystem.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace EffectPack
{

constexpr int kFormatVersion = 4;
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
    Zone,       // OpenRGB hardware zone on a device
    Leds,
    SceneZone,  // left-panel ZoneManager3D group (Desk / Wall / …)
};

enum class BlockType
{
    // Basic
    Solid,
    Fade,
    Pulse,
    Wipe,
    Chase,
    Twinkle,
    Alternating,
    Strobe,
    Spin,
    Candle,
    Dissolve,
    Wave,
    // Pixel (plane)
    ColorWash,
    Plasma,
    Snow,
    Fire,
    Balls,
    Bars,
    Scanner,
    // Volume (3D)
    SphereWipe,
    Orbit,
    Ripple,
    Meteor,
    Noise3D,
    Burst,
};

enum class Direction
{
    Left,
    Right,
    Up,
    Down,
    Forward,
    Back,
    PosX,
    NegX,
    PosY,
    NegY,
    PosZ,
    NegZ,
};

enum class AxisSpace
{
    Room,      // shared world AABB across target LEDs
    Device,    // per-controller local space
    Sequence,  // 0..1 along ordered controllers/LEDs (axis effects); Volume uses Room XYZ
};

enum class AxisMode
{
    Preset,
    Custom,
};

struct Target
{
    TargetKind kind = TargetKind::All;
    std::string device_name;
    std::string zone_name;
    std::string scene_zone_name; // TargetKind::SceneZone
    /** Under a scene zone: synthetic “All LEDs” row (same LED set, distinct track). */
    bool flatten_leds = false;
    std::vector<int> led_indices;
};

/** True for All / SceneZone group rows (default Room space). */
inline bool TargetIsMultiDeviceGroup(const Target& t)
{
    return t.kind == TargetKind::All || t.kind == TargetKind::SceneZone;
}

struct GradientStop
{
    float pos = 0.0f;
    RGBColor color = ToRGBColor(255, 0, 0);
};

struct CurvePoint
{
    float pos = 0.0f;
    float value = 1.0f;
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
    AxisSpace axis_space = AxisSpace::Device;
    AxisMode axis_mode = AxisMode::Preset;
    float axis_yaw_deg = 0.0f;
    float axis_pitch_deg = 0.0f;
    float speed = 1.0f;
    float pulse_length = 0.25f;
    std::vector<GradientStop> gradient;
    std::vector<CurvePoint> intensity_curve;
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
    std::vector<std::string> devices;
    std::vector<Track> tracks;
};

bool MapPlaybackTime(const Pack& pack, int elapsed_ms, bool event_active, int* out_local_ms);

RGBColor SampleGradient(const Block& block, float t);
float SampleCurve(const std::vector<CurvePoint>& curve, float t);
void EnsureBlockGradient(Block* block);
void ApplyBuiltinIntensityCurve(Block* block, const char* preset_id);
const char* MatchBuiltinIntensityCurve(const std::vector<CurvePoint>& curve);
/** Fill block gradient from a shared catalog preset id. accent used by white_color. */
bool ApplyGradientPresetId(Block* block, const char* preset_id, RGBColor accent = ToRGBColor(255, 80, 40));
float BlockProgress(const Block& block, int local_ms);

bool EvaluateBlock(const Block& block, int local_ms, RGBColor* out_color, float* out_intensity);

bool EvaluateBlockAtAxis(const Block& block,
                         int local_ms,
                         float axis_pos,
                         int twinkle_seed,
                         RGBColor* out_color,
                         float* out_intensity);

/** Sample-space position + AABB (device-local or room). Flat axes are treated as centered. */
bool EvaluateBlockAtWorld(const Block& block,
                          int local_ms,
                          float x, float y, float z,
                          float min_x, float max_x,
                          float min_y, float max_y,
                          float min_z, float max_z,
                          int twinkle_seed,
                          RGBColor* out_color,
                          float* out_intensity);

bool EvaluateBlockAtLed(const Block& block,
                        int local_ms,
                        int led_index,
                        int led_count,
                        RGBColor* out_color,
                        float* out_intensity);

bool EvaluateTrackColor(const Track& track, int local_ms, RGBColor* out_color, float* out_intensity);

bool EvaluateTrackColorAtLed(const Track& track,
                             int local_ms,
                             int led_index,
                             int led_count,
                             RGBColor* out_color,
                             float* out_intensity);

const Block* FindActiveBlock(const Track& track, int local_ms);

bool DirectionInvertsAxis(Direction dir);
int DirectionPreferredAxis(Direction dir);
void AxisUnitVector(const Block& block, float* out_x, float* out_y, float* out_z);

float WorldAxisPos(Direction dir,
                   float x, float y, float z,
                   float min_x, float max_x,
                   float min_y, float max_y,
                   float min_z, float max_z);

float WorldSpinAngle(Direction dir,
                     float x, float y, float z,
                     float min_x, float max_x,
                     float min_y, float max_y,
                     float min_z, float max_z);

float SampleAxisPos(const Block& block,
                    float x, float y, float z,
                    float min_x, float max_x,
                    float min_y, float max_y,
                    float min_z, float max_z);

float SampleSpinAngle(const Block& block,
                      float x, float y, float z,
                      float min_x, float max_x,
                      float min_y, float max_y,
                      float min_z, float max_z);

bool BlockNeedsWorldEval(BlockType t);
bool BlockNeedsDirection(BlockType t);
/** Room, or Sequence with a Volume/Pixel type (falls back to room XYZ). */
bool BlockUsesSharedWorldBounds(const Block& block);
/** Sequence space on axis-style effects (Wipe/Chase/…). */
bool BlockUsesSequenceAxis(const Block& block);

const char* BlockTypeDisplayName(BlockType t);

nlohmann::json ToJson(const Pack& pack);
bool FromJson(const nlohmann::json& j, Pack* out, std::string* error);
bool LoadFromFile(const filesystem::path& path, Pack* out, std::string* error);
bool SaveToFile(const filesystem::path& path, const Pack& pack, std::string* error);

Pack MakeExampleRainbowWash();
Pack MakeExampleDeskRipple();
Pack MakeExampleSequenceWipe();

} // namespace EffectPack
