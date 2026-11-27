// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* One room-light engine for the whole plugin: how lit LEDs light the room.

   Shared by the viewport's "Real light" overlay (GPU gather per grid point) and Emitter +
   relay receivers (the real LEDs behind a TV, under a desk, ...), so what the overlay shows
   is what the receivers get:
     - LEDs are grouped into lights by position (BuildLights); LEDs on a device with its own
       light blocker housing stay one light each.
     - Every light casts a ray map (RayMapCache): how far its light gets in each of 216
       directions. Its own housing stops a ray at once; display planes, props and other
       devices' housings stop it where they stand.
     - Light falls off as 1 / distance squared, is summed in linear light and tone-mapped
       keeping its hue.
   Pure maths and caching; the room's blockers come in through callbacks (RoomLightScene). */

#include "RGBController.h"
#include "LEDPosition3D.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace RoomLight
{

/** Default soft-core size as a fraction of the reach, squared (0.3 of the reach). */
constexpr float kDefaultCore2 = 0.09f;

/** Spill weight at squared distance d2: 1 / distance squared (softened inside the core), with
    a gentle window that only tapers it to zero near the reach. Matches the shader. */
inline float Spill(float d2, float r2, float core2)
{
    if(d2 >= r2)
    {
        return 0.0f;
    }
    const float q = d2 / r2;
    const float w = 1.0f - q * q;
    return w * w / (1.0f + d2 / std::max(1e-6f, core2));
}

/** Hue-keeping exposure tone-map of linear light (in place). Matches the shader. */
inline void ToneMap(float& r, float& g, float& b, float exposure)
{
    const float m = std::max(r, std::max(g, b));
    if(m <= 1e-6f)
    {
        r = g = b = 0.0f;
        return;
    }
    const float k = (1.0f - std::exp(-m * exposure)) / m;
    r *= k;
    g *= k;
    b *= k;
}

/* Ray map: per light, how far its light gets in each of kRayMapSize directions (0 = it does
   not get out at all). Directions are the cells of a cube around the light: 6 faces of
   kRayMapRes x kRayMapRes. Face f = axis (f / 2: x, y, z) and sign (f % 2: +, -); on a face the
   two other axes, in x, y, z order, give s and t in -1..1. The Real light shader uses the same
   layout (keep them in step). */
constexpr int kRayMapRes = 6;
constexpr int kRayMapSize = 6 * kRayMapRes * kRayMapRes;

/** Unit direction through the centre of ray map cell `index`. */
inline Vector3D RayMapDirection(int index)
{
    const int face = index / (kRayMapRes * kRayMapRes);
    const int cell = index % (kRayMapRes * kRayMapRes);
    const int u = cell % kRayMapRes;
    const int v = cell / kRayMapRes;
    const float s = ((float)u + 0.5f) / (float)kRayMapRes * 2.0f - 1.0f;
    const float t = ((float)v + 0.5f) / (float)kRayMapRes * 2.0f - 1.0f;
    const int axis = face / 2;
    const float sign = (face % 2 == 0) ? 1.0f : -1.0f;
    float d[3] = {0.0f, 0.0f, 0.0f};
    d[axis] = sign;
    d[axis == 0 ? 1 : 0] = s;
    d[axis == 2 ? 1 : 2] = t;
    const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    return {d[0] / l, d[1] / l, d[2] / l};
}

/** 0..1: how much of a light reaches a point at `dist` along unit direction `dn` (bilinear over
    the four nearest rays, so shadow edges are soft). CPU twin of the shader's rayVisibility. */
float RayVisibility(const float* depth, const Vector3D& dn, float dist, float tolerance);

/** One light, fixed by position (so its ray map can be cached). */
struct Light
{
    std::uint64_t key = 0;
    Vector3D pos{0.0f, 0.0f, 0.0f};
    int owner = -1;
};

/** Group LEDs into lights: LEDs with housed[i] set are one light each; the rest share cells
    `cell` across (per owner). The cell grows only if the count would pass max_lights.
    led_light[i] = the light LED i belongs to. Keys depend only on position and owner, so the
    same LEDs make the same lights wherever this is called. */
void BuildLights(const std::vector<Vector3D>& pos,
                 const std::vector<int>& owner,
                 const std::vector<char>& housed,
                 const std::vector<int>& led_id,
                 float cell,
                 int max_lights,
                 std::vector<Light>& lights,
                 std::vector<int>& led_light);

/** Blocker tests for ray casting: the light's own housing (from = the light, owner = its
    device) and the rest of the room. */
using RayTestFn = std::function<bool(const Vector3D& from, int owner, const Vector3D& to)>;

/** Ray maps of lights, cached across frames and shared by everyone in the plugin. Begin()
    with the room's blocker signature drops stale maps; Ensure() casts maps for lights that are
    new or have moved (on worker threads) and returns one depth array per light. */
class RayMapCache
{
public:
    void Begin(std::uint64_t scene_signature, float max_dist);
    void Ensure(const std::vector<Light>& lights,
                const RayTestFn& own_housing,
                const RayTestFn& scene,
                std::vector<const float*>& out);
    float MaxDist() const { return max_dist_; }
    std::uint64_t Signature() const { return signature_; }

private:
    struct Entry
    {
        Vector3D pos{0.0f, 0.0f, 0.0f};
        int owner = -1;
        float depth[kRayMapSize];
    };
    std::uint64_t signature_ = 0;
    float max_dist_ = 0.0f;
    std::unordered_map<std::uint64_t, Entry> maps_;
};

/** The plugin's one ray map cache (GUI thread). */
RayMapCache& SharedRayMaps();

/** Settings of one Emitter + relay layer, in room grid units. */
struct RelaySettings
{
    float reach = 28.0f;
    float core2 = kDefaultCore2;
    float exposure = 1.4f;
    bool occlusion = false;   /* blockers (ray maps, housings) count */
    bool ambient_occlusion = false;
    /* Inputs of the ambient callback: part of the cache key so moving its slider re-shades. */
    float ambient_strength = 0.0f;
    float ambient_span = 0.0f;
    Vector3D ambient_centre{0.0f, 0.0f, 0.0f};
};

/** Ambient shade factor (0..1) at a receiver. */
using AmbientFn = std::function<float(const Vector3D& at, int owner)>;

/** Emitter + relay receivers lit by the emitters' real colours, through the shared lights
    and ray maps. Prepare() works out, per receiver, which lights reach it and how strongly
    (cached; redone only when lights, ray maps, receivers or settings change). Shade() is then a
    short weighted sum per receiver each frame. */
class RelayModel
{
public:
    /** lights / ray_maps: the emitters' lights and their ray maps (same order), ray_sig the
        ray map cache signature. receiver_housed + receiver_housing: a receiver inside its own
        housing only gets light through its openings. Returns true when anything was redone. */
    bool Prepare(const std::vector<Light>& lights,
                 const std::vector<const float*>& ray_maps,
                 std::uint64_t ray_sig,
                 const std::vector<Vector3D>& receivers,
                 const std::vector<int>& receiver_owner,
                 const std::vector<char>& receiver_housed,
                 const RelaySettings& settings,
                 const RayTestFn& receiver_housing,
                 const AmbientFn& ambient);

    /** light_linear: 3 floats per light (linear light, summed over its LEDs).
        out_linear: 3 floats per receiver, tone-mapped linear light (0..1). */
    void Shade(const std::vector<float>& light_linear, std::vector<float>& out_linear) const;

    int LightCount() const { return light_count_; }
    std::size_t TapCount() const { return taps_.size(); }

private:
    struct Tap
    {
        int light = 0;
        float weight = 0.0f;
    };
    std::uint64_t tap_sig_ = 0;
    std::uint64_t amb_sig_ = 0;
    bool valid_ = false;
    std::size_t receiver_count_ = 0;
    int light_count_ = 0;
    std::vector<std::uint32_t> tap_begin_;
    std::vector<Tap> taps_;
    std::vector<float> ambient_;
    float exposure_ = 1.4f;
};

} // namespace RoomLight
