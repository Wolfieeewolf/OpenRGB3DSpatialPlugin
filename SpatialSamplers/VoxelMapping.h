// SPDX-License-Identifier: GPL-2.0-only

#ifndef VOXELMAPPING_H
#define VOXELMAPPING_H

#include "GameTelemetryBridge.h"
#include "RGBController.h"
#include "VoxelRoomCore.h"

namespace VoxelMapping
{

RGBColor SampleAtRoomGrid(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                          float heading_offset_deg,
                          float room_to_world_scale,
                          float alpha_cutoff,
                          float grid_x,
                          float grid_y,
                          float grid_z,
                          float origin_x,
                          float origin_y,
                          float origin_z,
                          bool* out_got_room_sample = nullptr);

}

#endif
