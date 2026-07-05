// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMMAPPINGCORE_H
#define ROOMMAPPINGCORE_H

namespace RoomMappingCore
{

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

}

#endif
