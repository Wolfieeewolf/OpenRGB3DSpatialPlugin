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
#include <string>

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
};

struct ControllerTransform
{
    RGBController*      controller;
    Transform3D         transform;
    std::vector<LEDPosition3D> led_positions;
};

#endif