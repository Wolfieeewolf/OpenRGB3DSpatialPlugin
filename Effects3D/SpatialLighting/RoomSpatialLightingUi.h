// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMSPATIALLIGHTINGUI_H
#define ROOMSPATIALLIGHTINGUI_H

#include "SpatialLighting/SpatialLightingEngine.h"

#include <nlohmann/json.hpp>

struct GridContext3D;

namespace RoomSpatialLightingUi
{

constexpr float kPlacementMarginFrac = 0.1f;

struct RoomSpatialLightParams
{
    int placement_mode = 0;
    float custom_u = 0.15f;
    float custom_v = 0.15f;
    float custom_w = 0.12f;
    bool use_occlusion = true;
    bool use_room_walls = false;
    bool use_controller_occlusion = true;
    float ao_strength = 65.0f;
    float glow_radius_mm = 45.0f;
    float light_reach_mm = 280.0f;
    float room_fill = 35.0f;
};

SpatialLighting::Vec3 PlacementPosition(const GridContext3D& grid, const RoomSpatialLightParams& params);

SpatialLighting::OccluderBuildOptions BuildOccluderOptions(const RoomSpatialLightParams& params);

void SaveParamsToJson(nlohmann::json& out_object, const char* key, const RoomSpatialLightParams& params);

void LoadParamsFromJson(const nlohmann::json& settings,
                        const char* primary_key,
                        const char* legacy_key,
                        RoomSpatialLightParams& params);

} // namespace RoomSpatialLightingUi

#endif
