// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "RGBController.h"
#include "filesystem.h"
#include <nlohmann/json.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Action
{

constexpr int kFormatVersion = 5;
/** Oldest pack version FromJson/LoadFromFile still accept (defaults filled in memory). */
constexpr int kMinLoadFormatVersion = 4;
constexpr int kMaxDurationMs = 60000;
/** Fail-closed ceiling for pack JSON on disk / HTTP-adjacent loads. */
constexpr std::size_t kMaxPackJsonBytes = 2u * 1024u * 1024u;
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
    Room,      // world XYZ sample frame
    Device,    // per-controller local space
    Sequence,  // 0..1 along ordered controllers/LEDs (axis effects); Volume uses Room XYZ
};

/** Which AABB normalizes UV / world samples (separate from AxisSpace travel frame). */
enum class PlayBounds
{
    TrackLights, // LEDs matched by this track's target (union across devices when not Device space)
    RoomBox,     // full room / Movie Sync spatial box
};

enum class AxisMode
{
    Preset,
    Custom,
};

/** How a clip's colour combines with the tracks painted before it this frame. */
enum class ClipBlend
{
    Normal,   // replace what is below (classic behaviour)
    Add,      // light adds up: flashes and hits punch through
    Screen,   // soft add, never blows out as hard as Add
    Lighten,  // brightest channel wins
    Multiply, // tint / dim what is below
    Darken,   // darkest channel wins (shadows, cut-outs)
};

/** Longest light trail a clip may leave behind (ms). */
constexpr int kMaxTrailMs = 10000;

const char* ClipBlendToString(ClipBlend blend);
bool ClipBlendFromString(const std::string& s, ClipBlend* out);

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

/** Canvas a new clip gets on this row. "All" plays on the whole room; a scene zone is its
    own little room (centred on the zone, offsets and size relative to it); device, zone
    and LED rows stay on their own lights. */
inline PlayBounds DefaultPlayBoundsFor(const Target& t)
{
    return t.kind == TargetKind::All ? PlayBounds::RoomBox : PlayBounds::TrackLights;
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

/**
 * Motion keyframe: where the clip's centre, size and axis are at one moment.
 * `at` is a fraction of the clip's length (0 = start, 1 = end), so keys stretch with it.
 */
struct MotionKey
{
    float at = 0.0f;
    float offset_x = 0.0f;   /* % of half-room, like Clip::origin_offset_x */
    float offset_y = 0.0f;
    float offset_z = 0.0f;
    float size = 1.0f;       /* like Clip::effect_size */
    float yaw_deg = 0.0f;    /* like Clip::axis_yaw_deg (Custom axis) */
    float pitch_deg = 0.0f;
};

/** Fail-closed ceiling on motion keys per clip read from a pack file. */
constexpr std::size_t kMaxMotionKeys = 64;

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
    /** Canvas for UV/world normalize — TrackLights keeps marquees on the row's lights. */
    PlayBounds play_bounds = PlayBounds::TrackLights;
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
    /** Layering: how this clip combines with lower tracks. */
    ClipBlend blend = ClipBlend::Normal;
    /** Afterglow: lights this clip touched fade out over this many ms (0 = cut instantly). */
    int trail_ms = 0;
    /** Layer lane inside its track: 0 paints first; each higher lane paints over the ones before it. */
    int layer = 0;
    /** Keyframed centre / size / axis over the clip, sorted by `at`. Empty = static placement. */
    std::vector<MotionKey> motion;
    /** Straight moves between keys instead of easing in and out of each. */
    bool motion_linear = false;
};

/** Most layer lanes one track may hold. */
constexpr int kMaxTrackLayers = 16;

/** One effect layer inside a track (xLights / Vixen style). */
struct TrackLayer
{
    std::string name;
    bool muted = false;
    /** Editor audition only; never saved. */
    bool solo = false;
};

/** Timing marker on the pack timeline (a beat, a cue). Authoring aid only: playback ignores it. */
struct Marker
{
    int ms = 0;
    std::string label;
};

/** Fail-closed ceiling on markers read from a pack file. */
constexpr std::size_t kMaxMarkers = 4000;

struct Track
{
    std::string name;
    Target target;
    std::vector<Clip> clips;
    /** Per-lane settings; may be shorter than the lane count (missing lanes use defaults). */
    std::vector<TrackLayer> layers;
    /** Timeline shows each layer as its own full-height lane. */
    bool layers_open = false;
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
    /** Sorted by time. Optional in the file ("markers"); older builds ignore it. */
    std::vector<Marker> markers;
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
/** Linear 0..1 over the clip span (no speed-based wrap). For one-shot GPU timelines. */
float ClipSpanProgress(const Clip& clip, int local_ms);
/**
 * Span scaled by speed, clamped to [0, 1] (no wrap). Script wipe/dissolve/fade and
 * gradient fallbacks — once finishes early when speed > 1 instead of restarting.
 */
float ClipSweepProgress(const Clip& clip, int local_ms);
/** Period-curve phase; uses GpuAnimMs when set so forever/while_active wraps stay smooth. */
float ClipPeriodProgress(const Clip& clip, int local_ms);
void ModulateClipIntensity(const Clip& clip, float clip_progress, float period_progress, float* intensity);
/**
 * Continuous animation clock for GPU fields (loop-safe). Set from Player::ElapsedMs
 * before ApplyPackFrame so u_time does not jump when LocalMs wraps on forever/while_active.
 * Pass -1 to clear (Prepare falls back to local_ms).
 */
void SetGpuAnimMs(int anim_ms);
int GpuAnimMsOr(int local_ms);

/**
 * Several packs in one frame (trigger and Movie Sync runtimes): call Begin once,
 * ApplyPackFrame for each pack in priority order, then End once. Later packs blend
 * over earlier ones instead of clearing them, and controllers are flushed once.
 */
void BeginCompositeFrame();

/** Playback tick for Action previews and runtimes (~60 fps). */
constexpr int kPlaybackTickMs = 16;

/** Wall-clock frame step for a playback timer: the real time since the last tick,
    or one nominal tick after a pause (so resuming never jumps ahead). */
int MeasureTickMs(std::int64_t* last_tick_ms);
void EndCompositeFrame(bool force_hw_update);
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

/** Placement at a fraction of the clip (0..1) with its motion keys applied (needs keys). */
MotionKey SampleClipMotion(const Clip& clip, float at);
/** Fraction of the clip's length that pack-local time `local_ms` falls on (clamped 0..1). */
float ClipFractionAt(const Clip& clip, int local_ms);
/** Overwrite the clip's offsets, size and yaw / pitch with its motion at `local_ms` (no-op without keys). */
void ApplyClipMotion(Clip* clip, int local_ms);
/** Insert or replace (within half a percent) a key, keeping keys sorted; returns its index. */
int UpsertMotionKey(Clip* clip, const MotionKey& key);

/** Topmost clip playing on an audible layer (highest lane; what a single-colour readout shows). */
const Clip* FindActiveClip(const Track& track, int local_ms);
/** Clip playing on one layer lane (last one wins when clips overlap on that lane). */
const Clip* FindActiveClipOnLayer(const Track& track, int local_ms, int layer);
/** Lanes in a track: at least 1, enough for every clip and named layer, at most kMaxTrackLayers. */
int TrackLayerCount(const Track& track);
/** False when the layer is muted, or another layer on the track is soloed. */
bool TrackLayerAudible(const Track& track, int layer);
/** Lane for a new clip over [start_ms, end_ms): `preferred` when free there, else a free lane
    above it, else a new lane on top (up to kMaxTrackLayers), else a free lane below.
    `ignore` skips a clip already on the track. */
int ChooseClipLayer(const Track& track, int start_ms, int end_ms, int preferred, const Clip* ignore = nullptr);

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

/** play_bounds == RoomBox — sample against ApplySpatialContext room AABB. */
bool ClipUsesRoomBoxBounds(const Clip& clip);
/** TrackLights + non-Device travel — one world AABB over the track's LEDs. */
bool ClipUsesTrackWorldUnion(const Clip& clip);
bool ClipUsesSequenceAxis(const Clip& clip);

/** Unique non-empty origin_ref names used by any clip in the pack (share/calibration list). */
std::vector<std::string> CollectPackOriginRefs(const Pack& pack);

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
