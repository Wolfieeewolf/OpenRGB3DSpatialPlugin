// SPDX-License-Identifier: GPL-2.0-or-later
/* Room-grid "Real light" overlay: the room as the lights would actually light it.
   Built from the colour every LED is showing right now (whatever drives it: the effect
   stack, the Action Editor's stacked lanes and blends, triggers, Movie Sync), so it always
   matches the hardware.

   Each light sends rays out in every direction (a ray map, RoomLight::kRayMapSize of them)
   and records how far each gets: a device's own light blocker housing stops a ray at once
   (a keyboard key only shines up, a speaker only out of its openings), and display planes,
   props and other devices' blocker cells stop it where they stand (real shadows). The GPU
   then lights each grid point only if the ray towards it got that far. Blockers always count
   here: they are part of the room, not an option.

   Lights are fixed by position (housed LEDs one by one, the rest grouped in cells), so their
   ray maps are cached and only re-cast when a device, blocker or the reach changes. */

#include "LEDViewport3D.h"
#include "viewport/MeshBatch.h"
#include "viewport/ViewportMath.h"
#include "GridSpaceUtils.h"
#include "SpatialLighting/RoomLight.h"
#include "SpatialLighting/LightOutput.h"
#include "SpatialLighting/BlockerGridOccluder.h"
#include "SpatialLighting/SpatialLightingEngine.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "VirtualController3D.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <thread>
#include <unordered_map>

namespace
{
constexpr int kMaxRealLights = 768;
/* Texels per light row: position, linear colour, then the ray map (4 depths per texel). */
constexpr int kRayTexels = RoomLight::kRayMapSize / 4;
constexpr int kTexelsPerLight = 2 + kRayTexels;

/* sRGB byte -> linear light (table: called for every lit LED every frame). */
const float* SrgbToLinearTable()
{
    static float table[256];
    static bool ready = false;
    if(!ready)
    {
        for(int i = 0; i < 256; ++i)
        {
            const float c = (float)i / 255.0f;
            table[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
        ready = true;
    }
    return table;
}

bool GlobalLedIndex(RGBControllerInterface* c, unsigned int zone, unsigned int led, unsigned int* out)
{
    if(!c || zone >= c->GetZoneCount() || led >= c->GetZoneLEDsCount(zone))
    {
        return false;
    }
    *out = c->GetZoneStartIndex(zone) + led;
    return *out < c->GetLEDCount();
}

struct HashMix
{
    uint64_t h = 1469598103934665603ull;
    void Word(uint64_t v) { h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); }
    void F(float f)
    {
        uint32_t u = 0;
        std::memcpy(&u, &f, sizeof(u));
        Word(u);
    }
    void V(const Vector3D& v) { F(v.x); F(v.y); F(v.z); }
};

uint64_t CellKey(float x, float y, float z, float inv, int owner)
{
    const int64_t ix = (int64_t)std::floor(x * inv);
    const int64_t iy = (int64_t)std::floor(y * inv);
    const int64_t iz = (int64_t)std::floor(z * inv);
    const uint64_t key = ((uint64_t)(ix & 0x1FFFFF) << 42) | ((uint64_t)(iy & 0x1FFFFF) << 21) | (uint64_t)(iz & 0x1FFFFF);
    return key ^ ((uint64_t)(owner + 2) * 0x9E3779B97F4A7C15ull);
}

/* One light: where it sits, whose LEDs it holds, its colour this frame. */
struct Light
{
    uint64_t key = 0;
    Vector3D pos{0.0f, 0.0f, 0.0f};
    int owner = -1;
    int count = 0;
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

/* Cached ray map of one light. */
struct RayMapEntry
{
    Vector3D pos{0.0f, 0.0f, 0.0f};
    int owner = -1;
    float depth[RoomLight::kRayMapSize];
};

} // namespace

void LEDViewport3D::SetRealLightReachMm(float mm)
{
    real_light_reach_mm_ = std::clamp(mm, 50.0f, 10000.0f);
    update();
}

int LEDViewport3D::buildRealLightTexels(float reach_units)
{
    real_light_texels_.clear();
    if(!controller_transforms)
    {
        return 0;
    }
    const float* lin = SrgbToLinearTable();

    /* 1. Lights, fixed by position. Housed devices: one light per LED (its own housing shapes
          it). Others: LEDs grouped in cells a fraction of the reach across. */
    std::vector<Light> lights;
    std::unordered_map<uint64_t, int> index;
    float cell = std::max(1e-3f, std::max(reach_units * 0.12f, MMToGridUnits(60.0f, grid_scale_mm)));
    for(int attempt = 0; attempt < 10; ++attempt)
    {
        lights.clear();
        index.clear();
        const float inv = 1.0f / cell;
        for(size_t ti = 0; ti < controller_transforms->size(); ++ti)
        {
            const auto& t = (*controller_transforms)[ti];
            if(!t || t->hidden_by_virtual)
            {
                continue;
            }
            const int owner = (int)ti;
            const bool housed = t->virtual_controller && !t->virtual_controller->GetLightBlockers().empty();
            for(size_t li = 0; li < t->led_positions.size(); ++li)
            {
                const LEDPosition3D& led = t->led_positions[li];
                const Vector3D& p = led.world_position;
                const uint64_t key = housed ? (0xB0C5000000000000ull ^ (((uint64_t)ti << 24) | (uint64_t)li))
                                            : CellKey(p.x, p.y, p.z, inv, owner);
                auto it = index.find(key);
                int k = 0;
                if(it == index.end())
                {
                    k = (int)lights.size();
                    index.emplace(key, k);
                    Light l;
                    l.key = key;
                    l.owner = owner;
                    lights.push_back(l);
                }
                else
                {
                    k = it->second;
                }
                Light& l = lights[(size_t)k];
                l.pos.x += p.x;
                l.pos.y += p.y;
                l.pos.z += p.z;
                ++l.count;

                /* Same colour the LED points draw (preview colour, or the device's own when unset). */
                RGBColor c = led.preview_color;
                unsigned int global = 0;
                if(c == 0x00FFFFFF && led.controller
                   && GlobalLedIndex(led.controller, led.zone_idx, led.led_idx, &global))
                {
                    c = led.controller->GetColor(global);
                }
                /* Real output: lumens at full white for this device's LEDs. */
                const float scale = LightOutput::LedScale(t.get(), led);
                l.r += lin[RGBGetRValue(c)] * scale;
                l.g += lin[RGBGetGValue(c)] * scale;
                l.b += lin[RGBGetBValue(c)] * scale;
            }
        }
        if((int)lights.size() <= kMaxRealLights)
        {
            break;
        }
        cell *= 1.5f;
    }
    for(Light& l : lights)
    {
        const float n = (float)std::max(1, l.count);
        l.pos.x /= n;
        l.pos.y /= n;
        l.pos.z /= n;
    }
    if((int)lights.size() > kMaxRealLights)
    {
        lights.resize((size_t)kMaxRealLights);
    }
    /* Only lit lights go to the GPU. */
    std::vector<const Light*> lit;
    for(const Light& l : lights)
    {
        if(l.r + l.g + l.b > 1e-5f)
        {
            lit.push_back(&l);
        }
    }
    if(lit.empty())
    {
        return 0;
    }

    /* 2. The room's blockers (always on for Real light): display planes, props, and every
          custom controller's light blocker cells. Cheap to gather; hashed so ray maps are only
          re-cast when one of them (or the reach) changes. */
    /* Only the main room view: blocker grids are built from the scene's own devices (a device
       view window shows copies, and has no room around them). */
    std::vector<SpatialLighting::OccluderQuad> quads;
    std::vector<SpatialLighting::BlockerGridOccluder> grids;
    if(SpatialLightingSceneProvider::instance()->controllers() == controller_transforms)
    {
        SpatialLighting::AppendDisplayPlaneOccluders(quads, grid_scale_mm);
        SpatialLighting::AppendScenePropOccluders(quads, grid_scale_mm);
        SpatialLighting::BuildBlockerGridOccluders(grids, grid_scale_mm);
    }

    HashMix sig;
    sig.F(reach_units);
    sig.Word(quads.size());
    for(const SpatialLighting::OccluderQuad& q : quads)
    {
        for(const auto& c : q.corners)
        {
            sig.F(c.x);
            sig.F(c.y);
            sig.F(c.z);
        }
    }
    sig.Word(grids.size());
    for(const SpatialLighting::BlockerGridOccluder& g : grids)
    {
        sig.Word((uint64_t)g.controller_index);
        sig.V(g.world_min);
        sig.V(g.world_max);
        sig.Word((uint64_t)g.width | ((uint64_t)g.height << 20) | ((uint64_t)g.depth << 40));
        for(float e : g.x_edges) sig.F(e);
        for(float e : g.y_edges) sig.F(e);
        for(float e : g.z_edges) sig.F(e);
        for(size_t i = 0; i < g.dense_cells.size(); ++i)
        {
            if(g.dense_cells[i])
            {
                sig.Word(i);
            }
        }
        const auto& t = (*controller_transforms)[(size_t)g.controller_index];
        if(t)
        {
            sig.V(t->transform.position);
            sig.F(t->transform.rotation.x);
            sig.F(t->transform.rotation.y);
            sig.F(t->transform.rotation.z);
            sig.V(t->transform.scale);
        }
    }

    static uint64_t s_scene_sig = 0;
    static std::unordered_map<uint64_t, RayMapEntry> s_maps;
    if(sig.h != s_scene_sig)
    {
        s_maps.clear();
        s_scene_sig = sig.h;
    }

    /* 3. Cast the rays for lights that are new or have moved. */
    std::vector<std::pair<uint64_t, const Light*>> todo;
    for(const Light* l : lit)
    {
        auto it = s_maps.find(l->key);
        if(it == s_maps.end() || it->second.owner != l->owner || std::fabs(it->second.pos.x - l->pos.x) > 1e-4f
           || std::fabs(it->second.pos.y - l->pos.y) > 1e-4f || std::fabs(it->second.pos.z - l->pos.z) > 1e-4f)
        {
            todo.emplace_back(l->key, l);
        }
    }
    if(!todo.empty())
    {
        static const std::vector<SpatialLighting::OccluderAabb> kNoAabbs;
        const float reach = reach_units;
        std::vector<RayMapEntry> built(todo.size());
        const auto cast = [&](size_t begin, size_t end) {
            for(size_t i = begin; i < end; ++i)
            {
                const Light& l = *todo[i].second;
                RayMapEntry& e = built[i];
                e.pos = l.pos;
                e.owner = l.owner;
                const Vector3D& p = l.pos;
                const auto scene_blocked = [&](const Vector3D& q) {
                    return SpatialLighting::SegmentIsOccludedSkip2(p.x, p.y, p.z, q.x, q.y, q.z, kNoAabbs, quads, grids,
                                                                   nullptr, nullptr, l.owner, -1, true);
                };
                for(int d = 0; d < RoomLight::kRayMapSize; ++d)
                {
                    const Vector3D dir = RoomLight::RayMapDirection(d);
                    const Vector3D q = {p.x + dir.x * reach, p.y + dir.y * reach, p.z + dir.z * reach};
                    /* Its own housing first: a blocked direction gets no light at all. */
                    if(SpatialLighting::SegmentBlockedByOwnHousingIn(grids, p.x, p.y, p.z, q.x, q.y, q.z, l.owner))
                    {
                        e.depth[d] = 0.0f;
                        continue;
                    }
                    if(!scene_blocked(q))
                    {
                        e.depth[d] = reach;
                        continue;
                    }
                    /* Something is in the way: find roughly where (to ~1.5% of the reach). */
                    float lo = 0.0f;
                    float hi = reach;
                    for(int step = 0; step < 6; ++step)
                    {
                        const float mid = 0.5f * (lo + hi);
                        const Vector3D m = {p.x + dir.x * mid, p.y + dir.y * mid, p.z + dir.z * mid};
                        if(scene_blocked(m))
                        {
                            hi = mid;
                        }
                        else
                        {
                            lo = mid;
                        }
                    }
                    e.depth[d] = lo;
                }
            }
        };
        unsigned int threads = std::min<unsigned int>(8u, std::max(1u, std::thread::hardware_concurrency()));
        if(todo.size() < 8)
        {
            threads = 1;
        }
        if(threads <= 1)
        {
            cast(0, todo.size());
        }
        else
        {
            std::vector<std::thread> pool;
            std::vector<std::pair<size_t, size_t>> redo;
            const size_t chunk = (todo.size() + threads - 1) / threads;
            for(unsigned int t = 0; t < threads; ++t)
            {
                const size_t b = (size_t)t * chunk;
                const size_t e = std::min(todo.size(), b + chunk);
                if(b >= e)
                {
                    break;
                }
                try
                {
                    pool.emplace_back([&cast, b, e]() {
                        try
                        {
                            cast(b, e);
                        }
                        catch(...)
                        {
                        }
                    });
                }
                catch(...)
                {
                    redo.emplace_back(b, e);
                }
            }
            for(std::thread& th : pool)
            {
                th.join();
            }
            for(const auto& r : redo)
            {
                cast(r.first, r.second);
            }
        }
        for(size_t i = 0; i < todo.size(); ++i)
        {
            s_maps[todo[i].first] = built[i];
        }
        /* Keep the cache to lights that still exist. */
        if(s_maps.size() > lights.size() * 2 + 64)
        {
            std::unordered_map<uint64_t, RayMapEntry> keep;
            for(const Light& l : lights)
            {
                auto it = s_maps.find(l.key);
                if(it != s_maps.end())
                {
                    keep.emplace(it->first, it->second);
                }
            }
            s_maps.swap(keep);
        }
    }

    /* 4. Upload: per light row, position, colour and its ray map. */
    const int n = (int)lit.size();
    real_light_texels_.assign((size_t)n * (size_t)kTexelsPerLight * 4u, 0.0f);
    for(int i = 0; i < n; ++i)
    {
        const Light& l = *lit[(size_t)i];
        float* t = &real_light_texels_[(size_t)i * (size_t)kTexelsPerLight * 4u];
        t[0] = l.pos.x;
        t[1] = l.pos.y;
        t[2] = l.pos.z;
        t[3] = 1.0f;
        t[4] = l.r;
        t[5] = l.g;
        t[6] = l.b;
        t[7] = 1.0f;
        auto it = s_maps.find(l.key);
        for(int d = 0; d < RoomLight::kRayMapSize; ++d)
        {
            t[8 + d] = (it != s_maps.end()) ? it->second.depth[d] : reach_units;
        }
    }
    return n;
}

void LEDViewport3D::drawRealLightOverlay()
{
    if(!room_grid_overlay_batch_.IsValid())
    {
        return;
    }
    const float reach = std::max(0.05f, MMToGridUnits(real_light_reach_mm_, grid_scale_mm));
    const int lights = buildRealLightTexels(reach);

    if(real_light_tex_ == 0)
    {
        GLuint id = 0;
        glGenTextures(1, &id);
        real_light_tex_ = id;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, real_light_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    static const std::vector<float> none((size_t)kTexelsPerLight * 4u, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kTexelsPerLight, std::max(1, lights), 0, GL_RGBA, GL_FLOAT,
                 lights > 0 ? real_light_texels_.data() : none.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    ViewportMat4 projection;
    ViewportMat4 view;
    std::memcpy(projection.m, pick_projection_, sizeof(float) * 16);
    std::memcpy(view.m, pick_scene_modelview_, sizeof(float) * 16);
    const ViewportMat4 mvp = ViewportMath::ModelViewProjection(projection, view, ViewportMath::Identity());

    /* Additive, no depth writes: dots read as lit haze and add up where light overlaps,
       while devices in front still hide what is behind them. */
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    gl_prog_real_light_point_.Bind();
    gl_prog_real_light_point_.SetUniformMat4("u_mvp", mvp.m);
    gl_prog_real_light_point_.SetUniform1f("u_point_size", std::clamp(room_grid_point_size * 2.2f, 2.0f, 40.0f));
    gl_prog_real_light_point_.SetUniform1i("u_lights", 0);
    gl_prog_real_light_point_.SetUniform1i("u_light_count", lights);
    gl_prog_real_light_point_.SetUniform1f("u_reach", reach);
    gl_prog_real_light_point_.SetUniform1f("u_exposure", 1.4f * std::max(0.05f, room_grid_brightness));
    gl_prog_real_light_point_.SetUniform1f("u_floor", 0.02f);
    room_grid_overlay_batch_.Draw(MeshBatch::Primitive::Points);
    gl_prog_real_light_point_.Unbind();

    glBindTexture(GL_TEXTURE_2D, 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
}
