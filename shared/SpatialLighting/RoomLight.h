// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* One room-light model for the whole plugin: how lit LEDs spill light into the room.

   Used by the viewport's "Real light" overlay (GPU gather per grid point) and by
   Emitter + relay receivers (the real LEDs behind a TV, under a desk, ...): same falloff,
   linear-light sum and tone-map (the overlay has its own reach and no blockers). Light is summed in linear light, falls off
   smoothly to zero at the reach, and is tone-mapped back keeping its hue, so many lights
   brighten towards white instead of clipping and one far light fades out. */

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

/** Spill weight at squared distance d2: soft inverse-square inside a finite reach.
    r2 = reach squared, core2 = soft core size squared (same units). Matches the shader. */
inline float Spill(float d2, float r2, float core2)
{
    if(d2 >= r2)
    {
        return 0.0f;
    }
    /* Real falloff: 1 / distance squared (softened inside the core), with a gentle window
       that only tapers it to zero near the reach (so far light ends instead of lingering). */
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

/** A lit point in linear light. */
struct LitPoint
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

/** Merge neighbouring lit points into at most max_lights lights (centres weighted by
    brightness, colours summed). Cells start at a fraction of the reach and grow until the
    count fits. The spill is smooth, so the merge does not show. */
void ClusterLights(const std::vector<LitPoint>& lit, float reach, int max_lights, std::vector<LitPoint>& out);

/** Settings of one Emitter + relay layer, in room grid units. */
struct RelaySettings
{
    float reach = 28.0f;      /* light reach */
    /* Fixed grouping sizes (never derived from the reach, so slider moves cannot regroup
       lights and flip shadows): emitter LEDs per light, receiver LEDs per shadow group. */
    float light_cell = 6.0f;
    float group_cell = 6.0f;
    float core2 = kDefaultCore2;
    float exposure = 1.4f;
    bool occlusion = false;   /* blockers cast shadows */
    bool ambient_occlusion = false;
    /* Inputs of the ambient callback: part of the cache key so moving its slider re-shades. */
    float ambient_strength = 0.0f;
    float ambient_span = 0.0f;
    Vector3D ambient_centre{0.0f, 0.0f, 0.0f};
};

/** Blocker test between two room points: true when something is in the way. from_owner /
    to_owner are the controllers the two ends sit on (their own bodies never block). */
using SegmentBlockedFn = std::function<bool(const Vector3D& from, int from_owner, const Vector3D& to, int to_owner)>;
/** Ambient shade factor (0..1) at a receiver. */
using AmbientFn = std::function<float(const Vector3D& at, int owner)>;

/** Emitter + relay receivers lit by the emitters' real colours.

    Prepare() does the expensive, layout-only part: groups the emitter LEDs into lights
    and works out, for every receiver, which lights reach it, how strongly, and how much of
    each light the blockers let through. It is cached and only redone when an LED, a
    blocker or a setting that changes geometry moves. Shade() is then a short weighted sum
    per receiver each frame. */
class RelayModel
{
public:
    /** Returns true when the cache was rebuilt. layout_epoch changes when blockers change. */
    /** emitter_owner / receiver_owner: controller of each LED (lights and receiver groups never
        span two controllers, so a device's own housing can be skipped in shadow tests). */
    /** emitter_exact / receiver_exact (optional, 1 per LED): LEDs on a device with its own
        light blocker housing. Each becomes its own light / shadow group, so shadow rays start
        at the real LED inside its housing pocket, never at an average point inside solid
        housing. */
    bool Prepare(const std::vector<Vector3D>& emitters,
                 const std::vector<int>& emitter_owner,
                 const std::vector<Vector3D>& receivers,
                 const std::vector<int>& receiver_owner,
                 const std::vector<char>& emitter_exact,
                 const std::vector<char>& receiver_exact,
                 const RelaySettings& settings,
                 std::uint64_t layout_epoch,
                 const SegmentBlockedFn& blocked,
                 const AmbientFn& ambient);

    /** emitter_linear: 3 floats per emitter (linear light, same order as Prepare).
        out_linear: 3 floats per receiver, tone-mapped linear light (0..1). */
    void Shade(const float* emitter_linear, std::vector<float>& out_linear) const;

    int LightCount() const { return light_count_; }
    std::size_t TapCount() const { return taps_.size(); }
    int GroupCount() const { return group_count_; }

private:
    struct Tap
    {
        int light = 0;
        float weight = 0.0f;
    };

    std::uint64_t layout_sig_ = 0;
    std::uint64_t block_sig_ = 0;
    std::uint64_t reach_sig_ = 0;
    std::uint64_t amb_sig_ = 0;
    std::uint64_t weight_sig_ = 0;
    std::vector<Vector3D> emitters_;
    std::vector<std::vector<int>> members_;   /* light -> its emitter LEDs */
    std::vector<float> group_radius_;
    /* Shadow results per group, by light: kept across reach changes. */
    std::vector<std::unordered_map<int, float>> pass_cache_;
    bool valid_ = false;
    /* Layout kept between stages so Glow / AO changes re-weight without new blocker tests. */
    std::vector<Vector3D> receivers_;
    std::vector<Vector3D> centre_;
    std::vector<int> light_owner_;
    std::vector<int> receiver_group_;
    std::vector<Vector3D> group_centre_;
    std::vector<int> group_owner_;
    std::vector<std::vector<Tap>> group_pass_;
    std::vector<float> group_ambient_;
    std::size_t emitter_count_ = 0;
    std::size_t receiver_count_ = 0;
    std::vector<int> emitter_light_;      /* emitter -> light */
    int light_count_ = 0;
    int group_count_ = 0;
    std::vector<std::uint32_t> tap_begin_; /* receiver_count_ + 1 */
    std::vector<Tap> taps_;
    std::vector<float> ambient_;          /* per receiver */
    float exposure_ = 1.4f;
    mutable std::vector<float> light_sum_;
};

} // namespace RoomLight
