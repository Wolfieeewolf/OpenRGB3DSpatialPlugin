// SPDX-License-Identifier: GPL-2.0-only

#include "RoomSpatialLightingUi.h"

#include "SpatialEffect3D.h"

namespace RoomSpatialLightingUi
{

SpatialLighting::Vec3 PlacementPosition(const GridContext3D& grid, const RoomSpatialLightParams& params)
{
    const float mx = grid.width * kPlacementMarginFrac;
    const float my = grid.height * kPlacementMarginFrac;
    const float mz = grid.depth * kPlacementMarginFrac;

    switch(params.placement_mode)
    {
    case 1:
        return {grid.center_x, grid.center_y, grid.center_z};
    case 2:
        return {grid.max_x - mx, grid.max_y - my, grid.max_z - mz};
    case 3:
        return {grid.min_x + grid.width * params.custom_u,
                grid.min_y + grid.height * params.custom_v,
                grid.min_z + grid.depth * params.custom_w};
    case 0:
    default:
        return {grid.min_x + mx, grid.min_y + my, grid.min_z + mz};
    }
}

SpatialLighting::OccluderBuildOptions BuildOccluderOptions(const RoomSpatialLightParams& params)
{
    SpatialLighting::OccluderBuildOptions options{};
    options.display_planes = params.use_occlusion;
    options.room_walls = params.use_occlusion && params.use_room_walls;
    options.controllers = params.use_occlusion && params.use_controller_occlusion;
    return options;
}

void SaveParamsToJson(nlohmann::json& out_object, const char* key, const RoomSpatialLightParams& params)
{
    nlohmann::json& o = out_object[key];
    o["placement"] = params.placement_mode;
    o["custom_u"] = params.custom_u;
    o["custom_v"] = params.custom_v;
    o["custom_w"] = params.custom_w;
    o["occlusion"] = params.use_occlusion;
    o["controller_occlusion"] = params.use_controller_occlusion;
    o["room_walls"] = params.use_room_walls;
    o["ao"] = params.ao_strength;
    o["glow_mm"] = params.glow_radius_mm;
    o["reach_mm"] = params.light_reach_mm;
    o["room_fill"] = params.room_fill;
}

void LoadParamsFromJson(const nlohmann::json& settings,
                        const char* primary_key,
                        const char* legacy_key,
                        RoomSpatialLightParams& params)
{
    const nlohmann::json* src = nullptr;
    if(settings.contains(primary_key) && settings[primary_key].is_object())
    {
        src = &settings[primary_key];
    }
    else if(legacy_key != nullptr && settings.contains(legacy_key) && settings[legacy_key].is_object())
    {
        src = &settings[legacy_key];
    }
    if(src == nullptr)
    {
        return;
    }

    const auto& rc = *src;
    if(rc.contains("placement"))
    {
        params.placement_mode = rc["placement"].get<int>();
    }
    if(rc.contains("custom_u"))
    {
        params.custom_u = rc["custom_u"].get<float>();
    }
    if(rc.contains("custom_v"))
    {
        params.custom_v = rc["custom_v"].get<float>();
    }
    if(rc.contains("custom_w"))
    {
        params.custom_w = rc["custom_w"].get<float>();
    }
    if(rc.contains("occlusion"))
    {
        params.use_occlusion = rc["occlusion"].get<bool>();
    }
    if(rc.contains("controller_occlusion"))
    {
        params.use_controller_occlusion = rc["controller_occlusion"].get<bool>();
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
