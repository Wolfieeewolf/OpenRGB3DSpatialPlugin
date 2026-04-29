// SPDX-License-Identifier: GPL-2.0-only

#ifndef VOXELMAPPING_H
#define VOXELMAPPING_H

#include "GameTelemetryBridge.h"
#include "RGBController.h"
#include "VoxelRoomCore.h"

/**
 * Maps room-layout LED positions into an RGBA voxel volume (see VoxelRoomCore).
 * Typical source: telemetry snapshots (pose + voxel_frame); also used by the effect stack blend path.
 */
namespace VoxelMapping
{

/**
 * Sample RGB at a room point. RGB is 0 for outside volume / no sample.
 * If out_got_room_sample is non-null, it is set true when a voxel was mapped (including black); false when no blend should occur.
 */
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

} // namespace VoxelMapping

#endif
