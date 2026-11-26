// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionApplier.h"
#include "ActionBuiltinGpu.h"
#include "ActionMedia.h"
#include "ActionParticleField.h"
#include "ActionRibbonField.h"
#include "ActionScript.h"
#include "ActionVolumeField.h"
#include "ActionApplierDetail.h"
#include "ActionColorMath.h"
#include "ZoneManager3D.h"
#include "GpuRoomGridOverlay.h"
#include "SpatialEffect3D.h"
#include "SpatialOffscreenGlPool.h"
#include "EffectInstance3D.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Action
{

namespace
{

std::int64_t SteadyNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* Frame bookkeeping. A plain ApplyPackFrame is its own frame; inside
   Begin/EndCompositeFrame all packs share one frame (cleared once, flushed once). */
unsigned int g_frame_id = 0;
bool g_composite_active = false;
bool g_composite_force_hw = false;
std::unordered_set<RGBControllerInterface*> g_composite_touched;
int g_trail_clock_ms = 0;

/* Per-controller flush pacing: as fast as each device keeps up with, never more often
   than kMinFlushGapMs. A device whose UpdateLEDs is slow is paced by its own cost, so
   it cannot hold back the others (the old single 45 ms gate capped every device). */
constexpr std::int64_t kMinFlushGapMs = 12;

struct FlushPace
{
    std::int64_t last_ms = 0;
    float cost_ms = 0.0f;
};

std::unordered_map<RGBControllerInterface*, FlushPace> g_flush_pace;

PostPaintHook g_post_paint_hook;

void RunPostPaintHook(std::unordered_set<RGBControllerInterface*>& touched)
{
    if(g_post_paint_hook)
    {
        g_post_paint_hook(touched);
    }
}

void FlushControllers(const std::unordered_set<RGBControllerInterface*>& touched, bool force)
{
    if(g_flush_pace.size() > 512)
    {
        g_flush_pace.clear(); /* controllers come and go; keys are never dereferenced */
    }
    const std::int64_t now = SteadyNowMs();
    for(RGBControllerInterface* c : touched)
    {
        if(!c)
        {
            continue;
        }
        FlushPace& pace = g_flush_pace[c];
        const std::int64_t gap = std::max<std::int64_t>(kMinFlushGapMs, (std::int64_t)(pace.cost_ms * 1.5f));
        if(!force && pace.last_ms != 0 && now - pace.last_ms < gap)
        {
            continue;
        }
        const auto t0 = std::chrono::steady_clock::now();
        try
        {
            c->UpdateLEDs();
        }
        catch(...)
        {
        }
        const float cost = (float)std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now() - t0).count() / 1000.0f;
        pace.cost_ms = (pace.cost_ms <= 0.0f) ? cost : (pace.cost_ms * 0.8f + cost * 0.2f);
        pace.last_ms = now;
    }
}

} // namespace

void SetPostPaintHook(PostPaintHook hook)
{
    g_post_paint_hook = std::move(hook);
}

int MeasureTickMs(std::int64_t* last_tick_ms)
{
    const std::int64_t now = SteadyNowMs();
    int dt = kPlaybackTickMs;
    if(last_tick_ms && *last_tick_ms > 0)
    {
        const std::int64_t gap = now - *last_tick_ms;
        dt = (gap >= 0 && gap <= 250) ? (int)gap : kPlaybackTickMs;
    }
    if(last_tick_ms)
    {
        *last_tick_ms = now;
    }
    return dt;
}

void BeginCompositeFrame()
{
    ++g_frame_id;
    g_composite_active = true;
    g_composite_force_hw = false;
    g_composite_touched.clear();
    g_trail_clock_ms = (int)(SteadyNowMs() & 0x3FFFFFFF);
}

void EndCompositeFrame(bool force_hw_update)
{
    if(!g_composite_active)
    {
        return;
    }
    g_composite_active = false;
    g_composite_touched.erase(nullptr);
    RunPostPaintHook(g_composite_touched);
    FlushControllers(g_composite_touched, force_hw_update || g_composite_force_hw);
    g_composite_touched.clear();
}

namespace applier_detail
{

int TrailClockMs()
{
    return g_trail_clock_ms;
}

void ApplyToControllerAll(RGBControllerInterface* c, RGBColor color, std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c)
    {
        return;
    }
    c->SetAllColors(color);
    touched->insert(c);
}

void ApplyToZone(RGBControllerInterface* c, int zone, RGBColor color, std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c || zone < 0 || (unsigned int)zone >= c->GetZoneCount())
    {
        return;
    }
    c->SetAllZoneColors(zone, color);
    touched->insert(c);
}

void ApplyToLeds(RGBControllerInterface* c, const std::vector<int>& indices, RGBColor color,
                 std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c)
    {
        return;
    }
    const unsigned int led_count = c->GetLEDCount();
    bool any = false;
    for(int idx : indices)
    {
        if(idx < 0 || (unsigned int)idx >= led_count)
        {
            continue;
        }
        c->SetColor((unsigned int)idx, color);
        any = true;
    }
    if(any)
    {
        touched->insert(c);
    }
}

} // namespace applier_detail

void PrepareControllersForPreview(const std::vector<RGBControllerInterface*>& controllers)
{
    for(RGBControllerInterface* c : controllers)
    {
        if(!c)
        {
            continue;
        }
        try
        {
            c->SetCustomMode();
        }
        catch(...)
        {
        }
    }
}

ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                          bool force_hw_update,
                          ZoneManager3D* zone_manager)
{
    return ApplyPackFrame(pack, local_ms, controllers, transforms, force_hw_update, zone_manager, nullptr);
}

namespace
{

bool LayerZoneIncludesTransform(int layer_zone_index,
                                int transform_index,
                                ZoneManager3D* zone_manager)
{
    if(layer_zone_index == -1)
    {
        return true;
    }
    if(layer_zone_index <= -1000)
    {
        return transform_index == -(layer_zone_index + 1000);
    }
    if(zone_manager && layer_zone_index >= 0)
    {
        if(Zone3D* zone = zone_manager->GetZone(layer_zone_index))
        {
            return zone->ContainsController(transform_index);
        }
        return false;
    }
    return true;
}

/* Effect field overlay for the Action Editor. Every room-space clip that plays this frame
   is sampled on one n^3 lattice over the room grid (on the GPU, with the same UV mapping
   the LEDs use), and the samples are blended in lane / track order with each clip's own
   blend in linear light, exactly like the LEDs. The result is one overlay layer, so any
   number of stacked effects show, with their real blends. */
constexpr int kOverlayLatticeN = 20;

struct OverlayLattice
{
    bool started = false;
    bool any = false;
    float min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
    std::vector<Vector3D> points; // index = x + n * (y + n * z)
    std::vector<float> mix;       // linear light, 3 per point
};

void StartOverlayLattice(OverlayLattice* lat, const ApplySpatialContext& ctx)
{
    if(lat->started)
    {
        return;
    }
    lat->started = true;
    lat->min_x = ctx.grid_min_x; lat->max_x = ctx.grid_max_x;
    lat->min_y = ctx.grid_min_y; lat->max_y = ctx.grid_max_y;
    lat->min_z = ctx.grid_min_z; lat->max_z = ctx.grid_max_z;
    const int n = kOverlayLatticeN;
    lat->points.resize((size_t)n * n * n);
    lat->mix.assign((size_t)n * n * n * 3u, 0.0f);
    auto at = [n](float lo, float hi, int k) { return lo + (hi - lo) * (float)k / (float)(n - 1); };
    for(int z = 0; z < n; ++z)
    {
        for(int y = 0; y < n; ++y)
        {
            for(int x = 0; x < n; ++x)
            {
                lat->points[(size_t)x + (size_t)n * ((size_t)y + (size_t)n * (size_t)z)] =
                    Vector3D{at(lat->min_x, lat->max_x, x), at(lat->min_y, lat->max_y, y), at(lat->min_z, lat->max_z, z)};
            }
        }
    }
}

void BlendIntoLattice(OverlayLattice* lat, const Clip& clip, size_t i, float r, float g, float b)
{
    namespace cm = colormath;
    const int mode = (int)clip.blend;
    float* m = &lat->mix[i * 3u];
    m[0] = cm::Blend(mode, m[0], cm::SrgbToLinear(r));
    m[1] = cm::Blend(mode, m[1], cm::SrgbToLinear(g));
    m[2] = cm::Blend(mode, m[2], cm::SrgbToLinear(b));
    lat->any = true;
}

/** Sample one clip (just painted, so its GPU program is still prepared) into the lattice. */
void AccumulateClipOverlay(const Clip& clip, int local_ms, const ApplySpatialContext& ctx, OverlayLattice* lat)
{
    if(!ctx.has_room_grid || clip.axis_space == AxisSpace::Device || ClipUsesSequenceAxis(clip)
       || !ClipUsesRoomBoxBounds(clip) || IsMediaClip(clip))
    {
        return;
    }
    const std::string id = script::FileId(clip);
    const bool field = script::UsesField(id);
    const bool particles = script::UsesParticles(id);
    const bool ribbons = script::UsesRibbons(id);
    const bool builtin = builtin_gpu::Uses(id);
    const bool gpu_field = field || particles || ribbons || builtin;

    if(gpu_field && ClipFieldUsesRoomGrid(clip, &ctx))
    {
        StartOverlayLattice(lat, ctx);
        const int n = (int)lat->points.size();
        const Vector3D origin = ResolveClipOriginGrid(clip, ctx);
        std::vector<float> xyz((size_t)n * 3u);
        std::vector<char> outside((size_t)n, 0);
        for(int k = 0; k < n; ++k)
        {
            float u = 0.5f, v = 0.5f, w = 0.5f;
            if(!RoomGridFieldUv(clip, ctx, origin, lat->points[(size_t)k], &u, &v, &w))
            {
                outside[(size_t)k] = 1;
            }
            ApplyClipUvTransform(clip, &u, &v);
            xyz[(size_t)k * 3u + 0u] = u;
            xyz[(size_t)k * 3u + 1u] = v;
            xyz[(size_t)k * 3u + 2u] = w;
        }
        std::vector<float> rgb((size_t)n * 3u, 0.0f);
        bool ok = false;
        if(field)
        {
            ok = volume_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
        }
        else if(particles)
        {
            ok = particle_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
        }
        else if(ribbons)
        {
            ok = ribbon_field::EvaluateAtPoints(xyz.data(), n, rgb.data());
        }
        else
        {
            ok = builtin_gpu::EvaluateAtPoints(xyz.data(), n, rgb.data());
        }
        if(!ok)
        {
            return;
        }
        float intensity = std::clamp(clip.intensity, 0.0f, 1.0f);
        ModulateClipIntensity(clip, ClipSpanProgress(clip, local_ms), ClipPeriodProgress(clip, local_ms), &intensity);
        for(int k = 0; k < n; ++k)
        {
            const float r = rgb[(size_t)k * 3u + 0u];
            const float g = rgb[(size_t)k * 3u + 1u];
            const float b = rgb[(size_t)k * 3u + 2u];
            /* Same rule as the LEDs: outside the clip box or too dark leaves what is below. */
            if(outside[(size_t)k] || std::max(r, std::max(g, b)) < 0.02f)
            {
                continue;
            }
            BlendIntoLattice(lat, clip, (size_t)k,
                             std::clamp(r * intensity, 0.0f, 1.0f),
                             std::clamp(g * intensity, 0.0f, 1.0f),
                             std::clamp(b * intensity, 0.0f, 1.0f));
        }
        return;
    }

    /* Effect scripts (.fx) on the room box: the same GPU batch the LEDs use, fed the
       lattice points as if they were lights. */
    if(!gpu_field && script::GpuCapable(id) && ctx.has_world_bounds)
    {
        StartOverlayLattice(lat, ctx);
        const int n = (int)lat->points.size();
        const bool world_script = script::UsesWorld(id);
        const bool spin = EffectAxisAngle(ClipFileId(clip));
        std::vector<script::GpuLedInput> inputs((size_t)n);
        for(int k = 0; k < n; ++k)
        {
            const Vector3D& p = lat->points[(size_t)k];
            if(world_script)
            {
                BuildScriptInputAtWorld(clip, p.x, p.y, p.z, ctx.min_x, ctx.max_x, ctx.min_y, ctx.max_y,
                                        ctx.min_z, ctx.max_z, k, &inputs[(size_t)k]);
            }
            else
            {
                const float axis = spin
                    ? SampleSpinAngle(clip, p.x, p.y, p.z, ctx.min_x, ctx.max_x, ctx.min_y, ctx.max_y, ctx.min_z, ctx.max_z)
                    : SampleAxisPos(clip, p.x, p.y, p.z, ctx.min_x, ctx.max_x, ctx.min_y, ctx.max_y, ctx.min_z, ctx.max_z);
                BuildScriptInputAtAxis(clip, axis, k, &inputs[(size_t)k]);
            }
        }
        std::vector<RGBColor> colors;
        std::vector<unsigned char> lit;
        if(!script::RunBatchGpu(id, clip, local_ms, inputs, &colors, &lit)
           || colors.size() < (size_t)n || lit.size() < (size_t)n)
        {
            return;
        }
        for(int k = 0; k < n; ++k)
        {
            if(!lit[(size_t)k])
            {
                continue;
            }
            const RGBColor c = colors[(size_t)k];
            BlendIntoLattice(lat, clip, (size_t)k,
                             (float)RGBGetRValue(c) / 255.0f,
                             (float)RGBGetGValue(c) / 255.0f,
                             (float)RGBGetBValue(c) / 255.0f);
        }
    }
}

/** The finished lattice as one overlay layer (n x n^2 RGBA8 atlas sampled over the room). */
void AppendLatticeOverlayLayer(const OverlayLattice& lat, std::vector<GpuRoomGridOverlayLayer>* out)
{
    if(!lat.any || !out)
    {
        return;
    }
    const int n = kOverlayLatticeN;
    GpuRoomGridOverlayLayer layer;
    layer.kind = GpuRoomGridOverlayLayer::KindVolumeAtlas;
    layer.atlas_n = n;
    layer.atlas_rgba.resize((size_t)n * n * n * 4u);
    for(size_t i = 0; i < (size_t)n * n * n; ++i)
    {
        /* Atlas texel (x, z * n + y) holds point (x, y, z); points are x-fastest the same way. */
        unsigned char* t = &layer.atlas_rgba[i * 4u];
        t[0] = (unsigned char)colormath::LinearToSrgb8(lat.mix[i * 3u + 0u]);
        t[1] = (unsigned char)colormath::LinearToSrgb8(lat.mix[i * 3u + 1u]);
        t[2] = (unsigned char)colormath::LinearToSrgb8(lat.mix[i * 3u + 2u]);
        t[3] = 255;
    }
    layer.finish_mode = GpuRoomGridOverlayLayer::FinishRgb;
    layer.blend_mode = (int)BlendMode::REPLACE; /* already the finished mix of every clip */
    layer.sample_room = true;
    layer.volume_tools = false;
    layer.volume_shape = 0;
    layer.room_min[0] = lat.min_x;
    layer.room_min[1] = lat.min_y;
    layer.room_min[2] = lat.min_z;
    layer.room_max[0] = lat.max_x;
    layer.room_max[1] = lat.max_y;
    layer.room_max[2] = lat.max_z;
    layer.origin[0] = 0.5f * (lat.min_x + lat.max_x);
    layer.origin[1] = 0.5f * (lat.min_y + lat.max_y);
    layer.origin[2] = 0.5f * (lat.min_z + lat.max_z);
    layer.half_ext[0] = 0.5f * (lat.max_x - lat.min_x);
    layer.half_ext[1] = 0.5f * (lat.max_y - lat.min_y);
    layer.half_ext[2] = 0.5f * (lat.max_z - lat.min_z);
    layer.brightness = 1.0f;
    out->push_back(std::move(layer));
}

} // namespace

ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                          bool force_hw_update,
                          ZoneManager3D* zone_manager,
                          const ApplySpatialContext* spatial,
                          std::vector<GpuRoomGridOverlayLayer>* overlay_out)
{
    ApplyStats stats;
    std::unordered_set<RGBControllerInterface*> touched;
    OverlayLattice overlay_lattice;
    const bool use_transforms = transforms && !transforms->empty();
    const RGBColor off = ToRGBColor(0, 0, 0);
    const int layer_zone = spatial ? spatial->layer_zone_index : -1;
    static std::uint64_t s_field_sequence = 0;
    const std::uint64_t field_sequence = ++s_field_sequence;
    /* A lone pack is a frame of its own; inside a composite frame the first pack to
       reach an LED clears it and later packs blend over what is there. */
    const bool first_in_frame = !g_composite_active || g_composite_touched.empty();
    if(!g_composite_active)
    {
        ++g_frame_id;
        g_trail_clock_ms = (int)(SteadyNowMs() & 0x3FFFFFFF);
    }

    if(use_transforms)
    {
        for(int ti = 0; ti < (int)transforms->size(); ++ti)
        {
            ControllerTransform* transform = (*transforms)[(size_t)ti].get();
            if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
            {
                continue;
            }
            if(!LayerZoneIncludesTransform(layer_zone, ti, zone_manager))
            {
                continue;
            }
            for(LEDPosition3D& led : transform->led_positions)
            {
                if(led.frame_stamp != g_frame_id)
                {
                    led.preview_color = off;
                    led.frame_stamp = g_frame_id;
                }
            }
        }
    }
    else if(first_in_frame)
    {
        for(RGBControllerInterface* c : controllers)
        {
            if(applier_detail::PackIncludesController(pack, c))
            {
                applier_detail::ApplyToControllerAll(c, off, &touched);
            }
        }
    }

    /* Room FX tracks paint after every other track, so they sit over the whole show. */
    std::vector<const Track*> track_order;
    track_order.reserve(pack.tracks.size());
    for(int pass = 0; pass < 2; ++pass)
    {
        for(const Track& t : pack.tracks)
        {
            if(t.target.room_fx == (pass == 1))
            {
                track_order.push_back(&t);
            }
        }
    }
    for(const Track* track_ptr : track_order)
    {
        const Track& track = *track_ptr;
        if(use_transforms)
        {
            /* Layer lanes paint in order, so each lane blends over the ones before it
               with its clips' blend modes. Muted lanes (or lanes outside a solo) skip. */
            const int lanes = TrackLayerCount(track);
            bool track_painted = false;
            for(int lane = 0; lane < lanes; ++lane)
            {
                if(!TrackLayerAudible(track, lane))
                {
                    continue;
                }
                const Clip* top = FindActiveClipOnLayer(track, local_ms, lane);
                if(!top)
                {
                    continue;
                }
                /* Motion keys: play a copy with this moment's centre, size and axis. */
                Clip moving;
                if(!top->motion.empty())
                {
                    moving = *top;
                    ApplyClipMotion(&moving, local_ms);
                    top = &moving;
                }
                applier_detail::SampleBounds shared_bounds;
                applier_detail::SeqAxesMap seq_map;
                const bool want_room_box = top && ClipUsesRoomBoxBounds(*top);
                const bool want_track_union = top && ClipUsesTrackWorldUnion(*top);
                const bool want_sequence = top && ClipUsesSequenceAxis(*top);
                if(want_track_union)
                {
                    std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> ordered;
                    applier_detail::BuildOrderedSequenceLeds(pack, track, transforms, zone_manager, &ordered);
                    for(const auto& pair : ordered)
                    {
                        if(pair.second)
                        {
                            applier_detail::ExpandBounds(&shared_bounds, pair.second->world_position);
                        }
                    }
                }
                if(top)
                {
                    const std::string effect_id = script::FileId(*top);
                    /* A clip on its own lights (a scene zone, a device in room frame) is centred
                       on those lights: origin, offsets and size resolve against their box, so a
                       pulse on a "PC case" zone grows from the middle of the case. Reference
                       points still win when one is picked. */
                    ApplySpatialContext row_ctx;
                    const ApplySpatialContext* origin_ctx = spatial;
                    if(want_track_union && shared_bounds.valid)
                    {
                        if(spatial)
                        {
                            row_ctx = *spatial;
                        }
                        row_ctx.has_world_bounds = true;
                        row_ctx.has_room_grid = false;
                        row_ctx.min_x = shared_bounds.min_x;
                        row_ctx.max_x = shared_bounds.max_x;
                        row_ctx.min_y = shared_bounds.min_y;
                        row_ctx.max_y = shared_bounds.max_y;
                        row_ctx.min_z = shared_bounds.min_z;
                        row_ctx.max_z = shared_bounds.max_z;
                        origin_ctx = &row_ctx;
                    }
                    BindClipOrigin(*top, origin_ctx);
                    if(script::UsesField(effect_id))
                    {
                        volume_field::PrepareForClip(*top, local_ms, field_sequence);
                    }
                    if(script::UsesParticles(effect_id))
                    {
                        particle_field::PrepareForClip(*top, local_ms, field_sequence);
                    }
                    if(script::UsesRibbons(effect_id))
                    {
                        ribbon_field::PrepareForClip(*top, local_ms, field_sequence);
                    }
                    if(IsMediaClip(*top))
                    {
                        PrepareMediaForClip(*top, local_ms, field_sequence);
                    }
                    if(builtin_gpu::Uses(effect_id))
                    {
                        builtin_gpu::PrepareForClip(*top, local_ms, field_sequence);
                    }
                }
                if(want_room_box)
                {
                    if(!spatial || !spatial->has_world_bounds)
                    {
                        continue;
                    }
                    shared_bounds.min_x = spatial->min_x;
                    shared_bounds.max_x = spatial->max_x;
                    shared_bounds.min_y = spatial->min_y;
                    shared_bounds.max_y = spatial->max_y;
                    shared_bounds.min_z = spatial->min_z;
                    shared_bounds.max_z = spatial->max_z;
                    shared_bounds.valid = true;
                    shared_bounds.room_ctx = spatial;
                }
                if(want_sequence)
                {
                    std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> ordered;
                    applier_detail::BuildOrderedSequenceLeds(pack, track, transforms, zone_manager, &ordered);
                    // Sequence march must match the zone filter used when painting, or visible
                    // LEDs get the wrong slice of the 0..1 wipe.
                    if(layer_zone != -1)
                    {
                        std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> filtered;
                        filtered.reserve(ordered.size());
                        for(const auto& pair : ordered)
                        {
                            int ti = -1;
                            for(int i = 0; i < (int)transforms->size(); ++i)
                            {
                                if((*transforms)[(size_t)i].get() == pair.first)
                                {
                                    ti = i;
                                    break;
                                }
                            }
                            if(LayerZoneIncludesTransform(layer_zone, ti, zone_manager))
                            {
                                filtered.push_back(pair);
                            }
                        }
                        ordered.swap(filtered);
                    }
                    applier_detail::BuildSequenceAxesMap(ordered, &seq_map);
                }

                int painted = 0;
                for(int ti = 0; ti < (int)transforms->size(); ++ti)
                {
                    ControllerTransform* transform = (*transforms)[(size_t)ti].get();
                    if(!LayerZoneIncludesTransform(layer_zone, ti, zone_manager))
                    {
                        continue;
                    }
                    if(!TrackAppliesToTransform(pack, track, transform, ti, zone_manager))
                    {
                        continue;
                    }
                    const std::vector<float>* seq_axes = nullptr;
                    auto it = seq_map.find(transform);
                    if(it != seq_map.end())
                    {
                        seq_axes = &it->second;
                    }
                    painted += applier_detail::PaintTransformTargetSpatial(transform, track, top, local_ms, &touched,
                                                           shared_bounds.valid ? &shared_bounds : nullptr,
                                                           seq_axes);
                }
                if(painted > 0)
                {
                    track_painted = true;
                    stats.viewport_leds_painted += painted;
                }
                /* Effect field overlay: sample this lane's clip while its GPU program is still
                   prepared (the next lane re-prepares it), blended like the LEDs. */
                if(overlay_out && top && spatial)
                {
                    AccumulateClipOverlay(*top, local_ms, *spatial, &overlay_lattice);
                }
            }
            if(track_painted)
            {
                ++stats.tracks_applied;
            }
            continue;
        }

        RGBColor color = ToRGBColor(0, 0, 0);
        float intensity = 0.0f;
        if(!EvaluateTrackColor(track, local_ms, &color, &intensity))
        {
            continue;
        }
        ++stats.tracks_applied;

        switch(track.target.kind)
        {
            case TargetKind::All:
                for(RGBControllerInterface* c : controllers)
                {
                    if(applier_detail::PackIncludesController(pack, c))
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Device:
                for(RGBControllerInterface* c : controllers)
                {
                    if(applier_detail::PackIncludesController(pack, c)
                       && ControllerMatchesDevice(c, track.target.device_name))
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Zone:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!applier_detail::PackIncludesController(pack, c)
                       || !ControllerMatchesDevice(c, track.target.device_name))
                    {
                        continue;
                    }
                    const int zone = FindZoneIndex(c, track.target.zone_name);
                    if(zone >= 0)
                    {
                        applier_detail::ApplyToZone(c, zone, color, &touched);
                    }
                    else if(track.target.zone_name.empty())
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Leds:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!applier_detail::PackIncludesController(pack, c)
                       || !ControllerMatchesDevice(c, track.target.device_name))
                    {
                        continue;
                    }
                    applier_detail::ApplyToLeds(c, track.target.led_indices, color, &touched);
                }
                break;
            case TargetKind::SceneZone:
                break;
            default:
            {
                const TargetKind unused = track.target.kind;
                (void)unused;
                break;
            }
        }
    }

    if(use_transforms)
    {
        for(int ti = 0; ti < (int)transforms->size(); ++ti)
        {
            ControllerTransform* transform = (*transforms)[(size_t)ti].get();
            if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
            {
                continue;
            }
            if(!LayerZoneIncludesTransform(layer_zone, ti, zone_manager))
            {
                continue;
            }
            for(LEDPosition3D& led : transform->led_positions)
            {
                /* Afterglow from trail clips sits under this frame's colour (brightest wins). */
                const RGBColor glow = applier_detail::TrailGlowAt(led, g_trail_clock_ms);
                if(glow != off)
                {
                    const RGBColor lit = ToRGBColor(std::max(RGBGetRValue(led.preview_color), RGBGetRValue(glow)),
                                                    std::max(RGBGetGValue(led.preview_color), RGBGetGValue(glow)),
                                                    std::max(RGBGetBValue(led.preview_color), RGBGetBValue(glow)));
                    if(lit != led.preview_color)
                    {
                        led.preview_color = lit;
                        applier_detail::ApplyColorToMappedLed(led, transform->controller, lit, &touched);
                    }
                    continue;
                }
                if(led.preview_color != off)
                {
                    continue;
                }
                applier_detail::ApplyColorToMappedLed(led, transform->controller, off, &touched);
            }
        }
    }

    if(overlay_out)
    {
        AppendLatticeOverlayLayer(overlay_lattice, overlay_out);
    }

    stats.controllers_touched = (int)touched.size();
    if(g_composite_active)
    {
        /* Flushed once in EndCompositeFrame, after every pack has painted. */
        g_composite_touched.insert(touched.begin(), touched.end());
        g_composite_force_hw = g_composite_force_hw || force_hw_update;
        if(g_composite_touched.empty())
        {
            g_composite_touched.insert(nullptr); /* mark "a pack ran" for first_in_frame */
        }
    }
    else
    {
        RunPostPaintHook(touched);
        FlushControllers(touched, force_hw_update);
    }
    return stats;
}


} // namespace Action
