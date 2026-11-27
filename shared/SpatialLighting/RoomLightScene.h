// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* The room as the light engine (RoomLight) sees it: which LEDs make which lights, and what
   blocks light. One place, used by the Real light overlay and Emitter + relay alike, so both
   always agree. Blockers always count: display planes, props and every custom controller's
   light blocker cells (a device's own cells shape its light; other devices' cells cast
   shadows). */

#include "RoomLight.h"
#include "SpatialLighting/BlockerGridOccluder.h"
#include "SpatialLighting/SpatialLightingEngine.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

struct ControllerTransform;

namespace RoomLightScene
{

/** How far ray maps reach (10 m, the longest light reach any setting allows). */
float RayRange(float grid_scale_mm);

/** What blocks light in the room, plus a signature that changes whenever any of it moves. */
struct Blockers
{
    std::vector<SpatialLighting::OccluderQuad> quads;
    std::vector<SpatialLighting::BlockerGridOccluder> grids;
    std::uint64_t signature = 0;
};

/** Gather the room's blockers for these transforms (the scene's own list). */
void Gather(const std::vector<std::unique_ptr<ControllerTransform>>* transforms, float grid_scale_mm, Blockers& out);

/** Ray tests for RoomLight::RayMapCache / RelayModel over these blockers. */
RoomLight::RayTestFn OwnHousingTest(const Blockers& blockers);
RoomLight::RayTestFn SceneTest(const Blockers& blockers);

/** True for a device whose own light blocker cells shape its light (its LEDs stay single). */
bool HasHousing(const ControllerTransform* t);

/** LEDs of the given transforms, ready for RoomLight::BuildLights. */
struct Leds
{
    std::vector<Vector3D> pos;
    std::vector<int> owner;    /* transform index */
    std::vector<char> housed;
    std::vector<int> led_id;   /* index within its transform */
    std::vector<const LEDPosition3D*> led;
};
void CollectLeds(const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
                 const std::function<bool(int transform_index)>& include,
                 Leds& out);

/** Lights from LEDs, with the shared fixed cell (~6 cm; housed LEDs alone), and their ray maps
    from the shared cache. */
void BuildLightsWithRayMaps(const Leds& leds,
                            const Blockers& blockers,
                            float grid_scale_mm,
                            int max_lights,
                            std::vector<RoomLight::Light>& lights,
                            std::vector<int>& led_light,
                            std::vector<const float*>& ray_maps);

} // namespace RoomLightScene
