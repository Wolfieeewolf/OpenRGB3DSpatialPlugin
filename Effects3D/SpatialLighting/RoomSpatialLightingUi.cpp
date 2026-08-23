// SPDX-License-Identifier: GPL-2.0-only

#include "RoomSpatialLightingUi.h"

namespace RoomSpatialLightingUi
{

SpatialLighting::OccluderBuildOptions BuildOccluderOptions(const RoomSpatialLightParams& params)
{
    SpatialLighting::OccluderBuildOptions options{};
    options.display_planes = params.use_occlusion;
    options.room_walls = params.use_occlusion && params.use_room_walls;
    options.controllers = params.use_occlusion;
    options.light_blockers = params.use_occlusion;
    return options;
}

void SaveParamsToJson(nlohmann::json& out_object, const char* key, const RoomSpatialLightParams& params)
{
    nlohmann::json& o = out_object[key];
    o["occlusion"] = params.use_occlusion;
    o["room_walls"] = params.use_room_walls;
    o["ao"] = params.ao_strength;
    o["glow_mm"] = params.glow_radius_mm;
    o["reach_mm"] = params.light_reach_mm;
    o["room_fill"] = params.room_fill;
}

void LoadParamsFromJson(const nlohmann::json& settings,
                        const char* key,
                        RoomSpatialLightParams& params)
{
    if(!settings.contains(key) || !settings[key].is_object())
    {
        return;
    }

    const auto& rc = settings[key];
    if(rc.contains("occlusion"))
    {
        params.use_occlusion = rc["occlusion"].get<bool>();
    }
    if(rc.contains("room_walls"))
    {
        params.use_room_walls = rc["room_walls"].get<bool>();
    }
    if(rc.contains("ao"))
    {
        params.ao_strength = rc["ao"].get<float>();
    }
    if(rc.contains("glow_mm"))
    {
        params.glow_radius_mm = rc["glow_mm"].get<float>();
    }
    if(rc.contains("reach_mm"))
    {
        params.light_reach_mm = rc["reach_mm"].get<float>();
    }
    if(rc.contains("room_fill"))
    {
        params.room_fill = rc["room_fill"].get<float>();
    }
}

} // namespace RoomSpatialLightingUi
