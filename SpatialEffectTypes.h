/*---------------------------------------------------------*\
| SpatialEffectTypes.h                                      |
|                                                           |
|   Common types for 3D spatial effects                    |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALEFFECTTYPES_H
#define SPATIALEFFECTTYPES_H

#include "LEDPosition3D.h"
#include "RGBController.h"

enum SpatialEffectType
{
    SPATIAL_EFFECT_WAVE             = 0,
    SPATIAL_EFFECT_WIPE             = 1,
    SPATIAL_EFFECT_PLASMA           = 2,
    SPATIAL_EFFECT_SPIRAL           = 3,
    SPATIAL_EFFECT_DNA_HELIX        = 4,
    SPATIAL_EFFECT_BREATHING_SPHERE = 5,
    SPATIAL_EFFECT_EXPLOSION        = 6,
};

enum OriginPreset
{
    ORIGIN_ROOM_CENTER      = 0,    // Center of entire room (current default)
    ORIGIN_FLOOR_CENTER     = 1,    // Center of floor
    ORIGIN_CEILING_CENTER   = 2,    // Center of ceiling
    ORIGIN_FRONT_WALL       = 3,    // Center of front wall (user facing)
    ORIGIN_BACK_WALL        = 4,    // Center of back wall
    ORIGIN_LEFT_WALL        = 5,    // Center of left wall
    ORIGIN_RIGHT_WALL       = 6,    // Center of right wall
    ORIGIN_FLOOR_FRONT      = 7,    // Front edge of floor
    ORIGIN_FLOOR_BACK       = 8,    // Back edge of floor
    ORIGIN_CUSTOM           = 9,    // Custom X,Y,Z coordinates
};

struct SpatialEffectParams
{
    SpatialEffectType   type;
    unsigned int        speed;
    unsigned int        brightness;
    RGBColor            color_start;
    RGBColor            color_end;
    bool                use_gradient;

    /*---------------------------------------------------------*\
    | 3D Spatial Controls                                      |
    \*---------------------------------------------------------*/
    Vector3D            scale_3d;           // Scale per axis (X, Y, Z)
    Vector3D            origin;             // Center point for effect (custom coordinates)
    OriginPreset        origin_preset;      // Room-based origin preset
    Rotation3D          rotation;           // Rotation around each axis
    Vector3D            direction;          // Direction vector for directional effects

    /*---------------------------------------------------------*\
    | Effect-specific controls                                 |
    \*---------------------------------------------------------*/
    float               thickness;          // For beam/wave thickness
    float               intensity;          // For effect intensity
    float               falloff;            // Distance falloff factor
    unsigned int        num_arms;           // For spiral, star effects
    unsigned int        frequency;          // For wave frequency
    bool                reverse;            // Reverse direction
    bool                mirror_x;           // Mirror across X axis
    bool                mirror_y;           // Mirror across Y axis
    bool                mirror_z;           // Mirror across Z axis
};

#endif