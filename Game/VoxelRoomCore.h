// SPDX-License-Identifier: GPL-2.0-only

#ifndef VOXELROOMCORE_H
#define VOXELROOMCORE_H

#include "RGBController.h"

#include <vector>

namespace VoxelRoomCore
{

struct VoxelGrid
{
    bool valid = false;
    int size_x = 0;
    int size_y = 0;
    int size_z = 0;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float min_z = 0.0f;
    float voxel_size = 1.0f;
    // Packed RGBA bytes per voxel, index = ((x * size_y + y) * size_z + z) * 4.
    std::vector<unsigned char> rgba;
};

struct Basis
{
    float forward_x = 0.0f;
    float forward_y = 0.0f;
    float forward_z = 1.0f;
    float up_x = 0.0f;
    float up_y = 1.0f;
    float up_z = 0.0f;
    bool valid = false;
};

struct RoomSamplePoint
{
    float room_x = 0.0f;
    float room_y = 0.0f;
    float room_z = 0.0f;
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    float origin_z = 0.0f;
};

struct MapperSettings
{
    float room_to_world_scale = 0.18f;
    float alpha_cutoff = 0.02f;
};

RGBColor ComputeRoomMappedVoxelColor(const VoxelGrid& grid,
                                     const Basis& basis,
                                     const RoomSamplePoint& sample,
                                     float anchor_world_x,
                                     float anchor_world_y,
                                     float anchor_world_z,
                                     const MapperSettings& settings,
                                     bool* out_used_voxel);

}

#endif
