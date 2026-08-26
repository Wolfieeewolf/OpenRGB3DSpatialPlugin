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
    SPATIAL_EFFECT_PLASMA           = 2,
    SPATIAL_EFFECT_SPIRAL           = 3,
    SPATIAL_EFFECT_DNA_HELIX        = 4,
    SPATIAL_EFFECT_BREATHING_SPHERE = 5,
    SPATIAL_EFFECT_PULSE_RING       = 6,
    SPATIAL_EFFECT_STARFIELD        = 7,
    SPATIAL_EFFECT_BUBBLES          = 8,
    SPATIAL_EFFECT_COLOR_WHEEL      = 9,
    SPATIAL_EFFECT_PARTICLE_FIELD   = 10,
    SPATIAL_EFFECT_BOUNCING_BALL    = 11,
    SPATIAL_EFFECT_COMET            = 12,
    SPATIAL_EFFECT_SHADER_FIELD     = 13,
    SPATIAL_EFFECT_TEXTURE_PROJECTION = 14,
    SPATIAL_EFFECT_OMNI_SHAPE_TEXTURE = 15,
    SPATIAL_EFFECT_SHELL_PATTERN = 16,
    SPATIAL_EFFECT_ROTATING_CONE_SPOTLIGHTS = 17,
    SPATIAL_EFFECT_HARMONIC_PULSE = 18,
    SPATIAL_EFFECT_SURFACE_AMBIENT = 19,
    SPATIAL_EFFECT_HEX_LATTICE = 20,
    SPATIAL_EFFECT_SCREEN_MIRROR = 21,
    SPATIAL_EFFECT_NOTE_SPARKLE = 22,
    SPATIAL_EFFECT_DEPTH_TONE = 23,
    SPATIAL_EFFECT_AUDIO_LEVEL = 24,
    SPATIAL_EFFECT_AUDIO_PULSE = 25,
    SPATIAL_EFFECT_BASS_PUNCH = 26,
    SPATIAL_EFFECT_AUDIO_STRIP_VISUALIZER = 27,
    SPATIAL_EFFECT_SPECTRUM_BARS = 28,
    SPATIAL_EFFECT_MC_HEALTH = 29,
    SPATIAL_EFFECT_MC_HUNGER = 30,
    SPATIAL_EFFECT_MC_AIR = 31,
    SPATIAL_EFFECT_MC_DURABILITY = 32,
    SPATIAL_EFFECT_MC_DAMAGE = 33,
    SPATIAL_EFFECT_MC_ROOM_AMBILIGHT = 34,
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
    REF_MODE_WORLD_ORIGIN   = 4,
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

enum class SpatialMappingMode : int
{
    Off = 0,
    SubtleTint = 1,
    CompassPalette = 2,
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

#endif
