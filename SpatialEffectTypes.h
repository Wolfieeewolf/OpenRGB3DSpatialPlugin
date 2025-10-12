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

#include <string>
#include <vector>
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
    SPATIAL_EFFECT_RAIN             = 7,
    SPATIAL_EFFECT_TORNADO          = 8,
};

/*---------------------------------------------------------*\
| Reference Point System                                   |
\*---------------------------------------------------------*/
enum ReferencePointType
{
    REF_POINT_USER          = 0,    // User position (green stick figure)
    REF_POINT_MONITOR       = 1,    // Monitor/screen
    REF_POINT_CHAIR         = 2,    // Chair
    REF_POINT_DESK          = 3,    // Desk
    REF_POINT_SPEAKER_LEFT  = 4,    // Left speaker
    REF_POINT_SPEAKER_RIGHT = 5,    // Right speaker
    REF_POINT_DOOR          = 6,    // Room door
    REF_POINT_WINDOW        = 7,    // Window
    REF_POINT_BED           = 8,    // Bed
    REF_POINT_TV            = 9,    // TV
    REF_POINT_CUSTOM        = 10    // Custom user-defined
};

/*---------------------------------------------------------*\
| Forward declaration for VirtualReferencePoint3D         |
| Full definition is in VirtualReferencePoint3D.h         |
\*---------------------------------------------------------*/
class VirtualReferencePoint3D;

struct UserPosition3D
{
    float x;                        // User X position in grid
    float y;                        // User Y position in grid
    float z;                        // User Z position in grid
    bool visible;                   // Show/hide stick figure

    UserPosition3D() : x(0.0f), y(0.0f), z(0.0f), visible(true) {}
    UserPosition3D(float x_, float y_, float z_) : x(x_), y(y_), z(z_), visible(true) {}
};

/*---------------------------------------------------------*\
| Reference Mode for Effect Origin                         |
| Determines where effects originate from in 3D space      |
\*---------------------------------------------------------*/
enum ReferenceMode
{
    REF_MODE_ROOM_CENTER    = 0,    // Default: Effects use room center (0,0,0)
    REF_MODE_USER_POSITION  = 1,    // Effects use user head position as origin
    REF_MODE_CUSTOM_POINT   = 2     // Effect-specific custom reference point (future)
};

/*---------------------------------------------------------*\
| Common Effect Axis Types                                 |
\*---------------------------------------------------------*/
enum EffectAxis
{
    AXIS_X              = 0,    // Left to Right
    AXIS_Y              = 1,    // Front to Back
    AXIS_Z              = 2,    // Bottom to Top
    AXIS_RADIAL         = 3,    // Outward from center
    AXIS_CUSTOM         = 4     // Custom direction vector
};

/*---------------------------------------------------------*\
| Multi-Reference Point Effects                            |
\*---------------------------------------------------------*/
struct MultiPointConfig
{
    std::vector<int>    reference_point_ids;    // IDs of reference points to use
    int                 primary_point_id;       // Main reference point
    int                 secondary_point_id;     // Secondary reference point (for dual-point effects)
    bool                use_all_points;         // Use all selected points simultaneously
    float               point_influence;        // How much each point affects the effect (0.0-1.0)

    MultiPointConfig() : primary_point_id(-1), secondary_point_id(-1), use_all_points(false), point_influence(1.0f) {}
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
    | Common Effect Controls (all effects have these)         |
    \*---------------------------------------------------------*/
    EffectAxis          axis;               // Primary axis for effect
    bool                reverse;            // Reverse direction
    Vector3D            direction;          // Custom direction vector (for AXIS_CUSTOM)

    /*---------------------------------------------------------*\
    | Multi-Reference Point System                             |
    \*---------------------------------------------------------*/
    MultiPointConfig    multi_points;       // Multiple reference points configuration

    /*---------------------------------------------------------*\
    | 3D Spatial Controls                                      |
    \*---------------------------------------------------------*/
    Vector3D            scale_3d;           // Scale per axis (X, Y, Z)
    Vector3D            origin;             // Center point for effect (custom coordinates)
    Rotation3D          rotation;           // Rotation around each axis

    /*---------------------------------------------------------*\
    | Effect-specific controls                                 |
    \*---------------------------------------------------------*/
    float               thickness;          // For beam/wave thickness
    float               intensity;          // For effect intensity
    float               falloff;            // Distance falloff factor
    unsigned int        num_arms;           // For spiral, star effects
    unsigned int        frequency;          // For wave frequency
    bool                mirror_x;           // Mirror across X axis
    bool                mirror_y;           // Mirror across Y axis
    bool                mirror_z;           // Mirror across Z axis
};

#endif
