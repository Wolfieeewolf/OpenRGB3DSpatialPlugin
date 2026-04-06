// SPDX-License-Identifier: GPL-2.0-only

#ifndef LEDPOSITION3D_H
#define LEDPOSITION3D_H

#include "RGBController.h"

struct Vector3D
{
    float x;
    float y;
    float z;
};

struct Rotation3D
{
    float x;
    float y;
    float z;
};

struct Transform3D
{
    Vector3D    position;
    Rotation3D  rotation;
    Vector3D    scale;
};

struct LEDPosition3D
{
    RGBController*  controller;
    unsigned int    zone_idx;
    unsigned int    led_idx;
    Vector3D        local_position;
    Vector3D        world_position;
    Vector3D        room_position;
    RGBColor        preview_color;
};

class VirtualController3D;

struct ControllerTransform
{
    RGBController*      controller;
    VirtualController3D* virtual_controller;
    Transform3D         transform;
    std::vector<LEDPosition3D> led_positions;
    RGBColor            display_color;
    bool                hidden_by_virtual;

    float               led_spacing_mm_x;
    float               led_spacing_mm_y;
    float               led_spacing_mm_z;

    int                 granularity;
    int                 item_idx;

    bool                world_positions_dirty;
};

#endif
