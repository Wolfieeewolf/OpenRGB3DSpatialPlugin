// SPDX-License-Identifier: GPL-2.0-or-later

#include "RoomLightScene.h"

#include "ControllerLayout3D.h"
#include "GridSpaceUtils.h"
#include "SpatialLighting/SpatialLightingSceneProvider.h"
#include "VirtualController3D.h"

#include <cstring>

namespace RoomLightScene
{

namespace
{

struct HashMix
{
    std::uint64_t h = 1469598103934665603ull;
    void Word(std::uint64_t v) { h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); }
    void F(float f)
    {
        std::uint32_t u = 0;
        std::memcpy(&u, &f, sizeof(u));
        Word(u);
    }
};

const std::vector<SpatialLighting::OccluderAabb>& NoAabbs()
{
    static const std::vector<SpatialLighting::OccluderAabb> none;
    return none;
}

} // namespace

float RayRange(float grid_scale_mm)
{
    return std::max(1e-3f, MMToGridUnits(10000.0f, grid_scale_mm));
}

void Gather(const std::vector<std::unique_ptr<ControllerTransform>>* transforms, float grid_scale_mm, Blockers& out)
{
    out.quads.clear();
    out.grids.clear();
    HashMix m;
    m.F(grid_scale_mm);
    /* Blocker grids come from the scene's own device list (the provider's); a device view
       window shows copies, with no room around them. */
    if(transforms && SpatialLightingSceneProvider::instance()->controllers() == transforms)
    {
        SpatialLighting::AppendDisplayPlaneOccluders(out.quads, grid_scale_mm);
        SpatialLighting::AppendScenePropOccluders(out.quads, grid_scale_mm);
        SpatialLighting::BuildBlockerGridOccluders(out.grids, grid_scale_mm);
    }
    m.Word(out.quads.size());
    for(const SpatialLighting::OccluderQuad& q : out.quads)
    {
        for(const auto& c : q.corners)
        {
            m.F(c.x);
            m.F(c.y);
            m.F(c.z);
        }
    }
    m.Word(out.grids.size());
    for(const SpatialLighting::BlockerGridOccluder& g : out.grids)
    {
        m.Word((std::uint64_t)(std::int64_t)g.controller_index);
        m.Word((std::uint64_t)g.width | ((std::uint64_t)g.height << 20) | ((std::uint64_t)g.depth << 40));
        for(float e : g.x_edges) m.F(e);
        for(float e : g.y_edges) m.F(e);
        for(float e : g.z_edges) m.F(e);
        for(std::size_t i = 0; i < g.dense_cells.size(); ++i)
        {
            if(g.dense_cells[i])
            {
                m.Word(i);
            }
        }
        if(transforms && g.controller_index >= 0 && g.controller_index < (int)transforms->size()
           && (*transforms)[(std::size_t)g.controller_index])
        {
            const Transform3D& t = (*transforms)[(std::size_t)g.controller_index]->transform;
            m.F(t.position.x); m.F(t.position.y); m.F(t.position.z);
            m.F(t.rotation.x); m.F(t.rotation.y); m.F(t.rotation.z);
            m.F(t.scale.x); m.F(t.scale.y); m.F(t.scale.z);
        }
    }
    out.signature = m.h;
}

RoomLight::RayTestFn OwnHousingTest(const Blockers& blockers)
{
    return [&blockers](const Vector3D& a, int owner, const Vector3D& b) {
        return SpatialLighting::SegmentBlockedByOwnHousingIn(blockers.grids, a.x, a.y, a.z, b.x, b.y, b.z, owner);
    };
}

RoomLight::RayTestFn SceneTest(const Blockers& blockers)
{
    return [&blockers](const Vector3D& a, int owner, const Vector3D& b) {
        return SpatialLighting::SegmentIsOccludedSkip2(a.x, a.y, a.z, b.x, b.y, b.z, NoAabbs(), blockers.quads,
                                                       blockers.grids, nullptr, nullptr, owner, -1, true);
    };
}

bool HasHousing(const ControllerTransform* t)
{
    return t && t->virtual_controller && !t->virtual_controller->GetLightBlockers().empty();
}

void CollectLeds(const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
                 const std::function<bool(int transform_index)>& include,
                 Leds& out)
{
    out = Leds{};
    for(std::size_t ti = 0; ti < transforms.size(); ++ti)
    {
        const ControllerTransform* t = transforms[ti].get();
        if(!t || t->hidden_by_virtual || (include && !include((int)ti)))
        {
            continue;
        }
        const char housed = HasHousing(t) ? 1 : 0;
        for(std::size_t li = 0; li < t->led_positions.size(); ++li)
        {
            const LEDPosition3D& led = t->led_positions[li];
            out.pos.push_back(led.world_position);
            out.owner.push_back((int)ti);
            out.housed.push_back(housed);
            out.led_id.push_back((int)li);
            out.led.push_back(&led);
        }
    }
}

void BuildLightsWithRayMaps(const Leds& leds,
                            const Blockers& blockers,
                            float grid_scale_mm,
                            int max_lights,
                            std::vector<RoomLight::Light>& lights,
                            std::vector<int>& led_light,
                            std::vector<const float*>& ray_maps)
{
    RoomLight::BuildLights(leds.pos, leds.owner, leds.housed, leds.led_id,
                           MMToGridUnits(60.0f, grid_scale_mm), max_lights, lights, led_light);
    RoomLight::RayMapCache& cache = RoomLight::SharedRayMaps();
    cache.Begin(blockers.signature, RayRange(grid_scale_mm));
    cache.Ensure(lights, OwnHousingTest(blockers), SceneTest(blockers), ray_maps);
}

} // namespace RoomLightScene
