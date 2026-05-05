// SPDX-License-Identifier: GPL-2.0-only

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
    SPATIAL_EFFECT_LIGHTNING        = 9,
    SPATIAL_EFFECT_MATRIX           = 10,
    SPATIAL_EFFECT_BOUNCING_BALL    = 11,
    SPATIAL_EFFECT_COMET            = 12,
    SPATIAL_EFFECT_SKY_LIGHTNING    = 13,
    SPATIAL_EFFECT_TEXTURE_PROJECTION = 14,
    SPATIAL_EFFECT_OMNI_SHAPE_TEXTURE = 15,
    /** Virtual 1D strip unfolded through 3D (shell or extruded fill); host for strip/matrix kernels. */
    SPATIAL_EFFECT_STRIP_SHELL_PATTERN = 16,
    /** Rotating spotlight cones in normalized volume (mapped-cube style 3D). */
    SPATIAL_EFFECT_ROTATING_CONE_SPOTLIGHTS = 17,
    /** 3D sin/cos pulse field: hue from interference, value from h³/2. */
    SPATIAL_EFFECT_SINPULSE_3D = 18,
    /** Bouncing orbs: independent balls with wall-reflect vectors in normalized room coordinates. */
    SPATIAL_EFFECT_BOUNCER_3D = 19,
    /** Hex lattice field with animated zoom and triangular hue shaping. */
    SPATIAL_EFFECT_HONEYCOMB_3D = 20,
    /** Diagnostic axis sweep: red=X, green=Y, blue=Z moving band. */
    SPATIAL_EFFECT_RGB_XYZ_SWEEP_3D = 21,
    /** Diagnostic octants: static XYZ high/low encoded as RGB. */
    SPATIAL_EFFECT_RGB_XYZ_OCTANTS_3D = 22,
    /** Complementary hue gradient along room depth with center dim. */
    SPATIAL_EFFECT_COMPLEMENTS_3D = 23,
    /** Fast pulse field from animated XYZ sinusoid mix. */
    SPATIAL_EFFECT_FAST_PULSE_3D = 24,
    /** Integer XOR-derived 3D interference pattern. */
    SPATIAL_EFFECT_XORCERY_3D = 25,
};

enum ReferencePointType
{
    REF_POINT_USER          = 0,
    REF_POINT_MONITOR       = 1,
    REF_POINT_CHAIR         = 2,
    REF_POINT_DESK          = 3,
    REF_POINT_SPEAKER_LEFT  = 4,
    REF_POINT_SPEAKER_RIGHT = 5,
    REF_POINT_DOOR          = 6,
    REF_POINT_WINDOW        = 7,
    REF_POINT_BED           = 8,
    REF_POINT_TV            = 9,
    REF_POINT_CUSTOM        = 10
};

class VirtualReferencePoint3D;

struct UserPosition3D
{
    float x;
    float y;
    float z;
    bool visible;

    UserPosition3D() : x(0.0f), y(0.0f), z(0.0f), visible(true) {}
    UserPosition3D(float x_, float y_, float z_) : x(x_), y(y_), z(z_), visible(true) {}
};

enum ReferenceMode
{
    REF_MODE_ROOM_CENTER    = 0,
    REF_MODE_USER_POSITION  = 1,
    REF_MODE_CUSTOM_POINT   = 2,
    REF_MODE_TARGET_ZONE_CENTER = 3,
    /** Use (0,0,0) as origin — no room center or virtual ref point; patterns use raw grid coordinates. */
    REF_MODE_WORLD_ORIGIN   = 4,
    /**
     * Average position of mapped LEDs when the grid provides it; else box center.
     * Default for new effects: patterns follow real hardware, not an abstract room midpoint.
     */
    REF_MODE_LED_CENTROID   = 5
};

enum EffectAxis
{
    AXIS_X      = 0,
    AXIS_Y      = 1,
    AXIS_Z      = 2,
    AXIS_RADIAL = 3,
    AXIS_CUSTOM = 4
};

/** How room compass + vertical strata reshape palette / rainbow (see SpatialEffect3D). */
enum class SpatialMappingMode : int
{
    Off = 0,
    SubtleTint = 1,
    CompassPalette = 2,
    /** Palette follows the full room volume; reference point still biases the mapping. */
    VoxelVolume = 3,
};

/**
 * Extra motion from telemetry voxel / room axes (applied after spatial mapping).
 * Volume color blend remains on the Mix slider separately.
 */
enum class VoxelDriveMode : int
{
    Off = 0,
    LumaField = 1,
    ScrollRoomX = 2,
    ScrollRoomY = 3,
    ScrollRoomZ = 4,
    VolumeRoll = 5,
};

enum SurfaceMask
{
    SURF_FLOOR   = 1,
    SURF_CEIL    = 2,
    SURF_WALL_XM = 4,
    SURF_WALL_XP = 8,
    SURF_WALL_ZM = 16,
    SURF_WALL_ZP = 32,
    SURF_ALL     = 63
};

struct MultiPointConfig
{
    std::vector<int>    reference_point_ids;
    int                 primary_point_id;
    int                 secondary_point_id;
    bool                use_all_points;
    float               point_influence;

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

    EffectAxis          axis;
    bool                reverse;
    Vector3D            direction;

    MultiPointConfig    multi_points;

    Vector3D            scale_3d;
    Vector3D            origin;
    Rotation3D          rotation;

    float               thickness;
    float               intensity;
    float               falloff;
    unsigned int        num_arms;
    unsigned int        frequency;
    bool                mirror_x;
    bool                mirror_y;
    bool                mirror_z;
};

#endif
