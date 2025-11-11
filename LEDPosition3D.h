/*---------------------------------------------------------*\
| LEDPosition3D.h                                           |
|                                                           |
|   Individual LED 3D position tracking                    |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

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
    Vector3D        effect_world_position;
    RGBColor        preview_color;  // Used for viewport preview rendering
};

// Forward declaration
class VirtualController3D;

struct ControllerTransform
{
    RGBController*      controller;
    VirtualController3D* virtual_controller;
    Transform3D         transform;
    std::vector<LEDPosition3D> led_positions;
    RGBColor            display_color;
    bool                hidden_by_virtual;

    /*---------------------------------------------------------*\
    | LED Physical Spacing (in millimeters)                    |
    | Used to calculate real-world grid positions              |
    \*---------------------------------------------------------*/
    float               led_spacing_mm_x;
    float               led_spacing_mm_y;
    float               led_spacing_mm_z;

    /*---------------------------------------------------------*\
    | Granularity (0=whole device, 1=zone, 2=LED)             |
    | Only relevant for physical controllers, not virtual      |
    \*---------------------------------------------------------*/
    int                 granularity;
    int                 item_idx;  // Zone or LED index based on granularity

    /*---------------------------------------------------------*\
    | World Position Cache                                      |
    | Pre-computed world positions for performance             |
    \*---------------------------------------------------------*/
    bool                world_positions_dirty;  // True when transform changes
};

#endif
