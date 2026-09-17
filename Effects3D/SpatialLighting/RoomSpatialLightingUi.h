// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMSPATIALLIGHTINGUI_H
#define ROOMSPATIALLIGHTINGUI_H

#include "SpatialLighting/SpatialLightingEngine.h"

#include <nlohmann/json.hpp>

namespace RoomSpatialLightingUi
{

struct RoomSpatialLightParams
{
    bool use_occlusion = false;
    bool use_room_walls = false;
    float ao_strength = 65.0f;
    float glow_radius_mm = 45.0f;
    float light_reach_mm = 280.0f;
    float room_fill = 35.0f;
};

SpatialLighting::OccluderBuildOptions BuildOccluderOptions(const RoomSpatialLightParams& params);

void SaveParamsToJson(nlohmann::json& out_object, const char* key, const RoomSpatialLightParams& params);

void LoadParamsFromJson(const nlohmann::json& settings,
                        const char* key,
                        RoomSpatialLightParams& params);

} // namespace RoomSpatialLightingUi

#endif
