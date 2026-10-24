// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "RGBController.h"
#include "filesystem.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Action
{

constexpr int kFormatVersion = 4;
constexpr int kMaxDurationMs = 60000;
constexpr const char* kFormatId = "openrgb3d.action";
constexpr const char* kFileSuffix = ".action.json";

inline bool IsActionFileName(const std::string& name)
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
    bool flatten_leds = false;
    std::vector<int> led_indices;
};

inline bool TargetEquals(const Target& a, const Target& b)
{
    if(a.kind != b.kind || a.flatten_leds != b.flatten_leds)
    {
        return false;
    }
    switch(a.kind)
    {
        case TargetKind::All:
            return true;
        case TargetKind::Device:
            return a.device_name == b.device_name;
        case TargetKind::Zone:
            return a.device_name == b.device_name && a.zone_name == b.zone_name;
        case TargetKind::Leds:
            return a.device_name == b.device_name
                && a.zone_name == b.zone_name
                && a.led_indices == b.led_indices;
        case TargetKind::SceneZone:
            return a.scene_zone_name == b.scene_zone_name;
    }
    return false;
}

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

struct Clip
{
    std::string effect_id;
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
    /**
     * Scene reference point name for world/field/particle origin (empty = room center).
     * Matched against VirtualReferencePoint3D::GetName().
     */
    std::string origin_ref;
    /** Offset from origin as % of half-room size (−100…100), same as SpatialEffect3D. */
    float origin_offset_x = 0.0f;
    float origin_offset_y = 0.0f;
    float origin_offset_z = 0.0f;
    /** Spatial scale of the effect (0.05…2.0; 1 = default room footprint). */
    float effect_size = 1.0f;
    bool reverse = false;
    /** Mirror sample U (left↔right on media / axis). */
    bool flip_h = false;
    /** Mirror sample V (top↔bottom on media / height). */
    bool flip_v = false;
    /** Clockwise 90° steps in UV (0–3). Media / 2D mapping; 180° also mirrors 1D axis. */
    int rotate_quarters = 0;
    std::vector<GradientStop> gradient;
    /** Intensity envelope over clip length. Empty = flat 1. */
    std::vector<CurvePoint> intensity_curve;
    /** Intensity waveform within each period_ms cycle. Empty = flat 1. */
    std::vector<CurvePoint> period_curve;
    std::string media_path;
    std::string media_text;
    bool media_scroll = false;
};

struct Track
{
    std::string name;
    Target target;
    std::vector<Clip> clips;
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

RGBColor SampleGradient(const Clip& clip, float t);
/** Pick a discrete gradient stop by index (wraps). No interpolation. */
RGBColor SampleGradientStop(const Clip& clip, int index);
/** Number of unique colours in the clip gradient (1 if empty / solid). */
int UniqueGradientStopCount(const Clip& clip);
float SampleCurve(const std::vector<CurvePoint>& curve, float t);
void EnsureClipGradient(Clip* clip);
void ApplyBuiltinIntensityCurve(Clip* clip, const char* preset_id);
const char* MatchBuiltinIntensityCurve(const std::vector<CurvePoint>& curve);
void ApplyBuiltinPeriodCurve(Clip* clip, const char* preset_id);
const char* MatchBuiltinPeriodCurve(const std::vector<CurvePoint>& curve);
bool ApplyGradientPresetId(Clip* clip, const char* preset_id, RGBColor accent = ToRGBColor(255, 80, 40));
float ClipProgress(const Clip& clip, int local_ms);
float ClipPeriodProgress(const Clip& clip, int local_ms);
void ModulateClipIntensity(const Clip& clip, float clip_progress, float period_progress, float* intensity);
/** Rotate (CW quarters) then flip H/V around UV center. */
void ApplyClipUvTransform(const Clip& clip, float* u, float* v);
/** 1D axis mirror: flip_h and 180° rotate (90/270 leave axis unchanged). */
float ApplyClipAxisMirror(const Clip& clip, float axis);

bool IsMediaEffect(const std::string& effect_id);
bool IsMediaClip(const Clip& clip);

bool EvaluateClipAtAxis(const Clip& clip,
                         int local_ms,
                         float axis_pos,
                         int twinkle_seed,
                         RGBColor* out_color,
                         float* out_intensity);

bool EvaluateClipAtWorld(const Clip& clip,
                          int local_ms,
                          float x, float y, float z,
                          float min_x, float max_x,
                          float min_y, float max_y,
                          float min_z, float max_z,
                          int twinkle_seed,
                          RGBColor* out_color,
                          float* out_intensity);

bool EvaluateClipAtLed(const Clip& clip,
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

const Clip* FindActiveClip(const Track& track, int local_ms);

bool DirectionInvertsAxis(Direction dir);
int DirectionPreferredAxis(Direction dir);
/** Opposite named / axis direction (Left↔Right, Forward↔Back, …). */
Direction OppositeDirection(Direction dir);
void AxisUnitVector(const Clip& clip, float* out_x, float* out_y, float* out_z);

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

float SampleAxisPos(const Clip& clip,
                    float x, float y, float z,
                    float min_x, float max_x,
                    float min_y, float max_y,
                    float min_z, float max_z);

float SampleSpinAngle(const Clip& clip,
                      float x, float y, float z,
                      float min_x, float max_x,
                      float min_y, float max_y,
                      float min_z, float max_z);

bool ClipUsesSharedWorldBounds(const Clip& clip);
bool ClipUsesSequenceAxis(const Clip& clip);

/** Room-normalized origin (0..1) + size for field/particle sampling. */
struct ClipOrigin01
{
    float ox = 0.5f;
    float oy = 0.5f;
    float oz = 0.5f;
    float size = 1.0f;
};

struct NamedWorldPoint
{
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/**
 * Resolve clip origin into room-normalized coords.
 * Missing / unknown origin_ref → room center; offset uses half-room % like Spatial.
 */
ClipOrigin01 ResolveClipOrigin01(const Clip& clip,
                                 float min_x, float max_x,
                                 float min_y, float max_y,
                                 float min_z, float max_z,
                                 const std::vector<NamedWorldPoint>& refs);

/** Last resolved origin for the active prepare/eval (script `ox`/`oy`/`oz`/`effect_size`). */
void SetActiveClipOrigin01(const ClipOrigin01& origin);
ClipOrigin01 GetActiveClipOrigin01();

nlohmann::json ToJson(const Pack& pack);
bool FromJson(const nlohmann::json& j, Pack* out, std::string* error);
bool LoadFromFile(const filesystem::path& path, Pack* out, std::string* error);
bool SaveToFile(const filesystem::path& path, const Pack& pack, std::string* error);

} // namespace Action
