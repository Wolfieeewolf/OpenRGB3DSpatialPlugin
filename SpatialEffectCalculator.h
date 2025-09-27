/*---------------------------------------------------------*\
| SpatialEffectCalculator.h                                 |
|                                                           |
|   Calculator for all 3D spatial lighting effects        |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALEFFECTCALCULATOR_H
#define SPATIALEFFECTCALCULATOR_H

#include "LEDPosition3D.h"
#include "RGBController.h"

enum SpatialEffectType
{
    SPATIAL_EFFECT_WAVE_X           = 0,
    SPATIAL_EFFECT_WAVE_Y           = 1,
    SPATIAL_EFFECT_WAVE_Z           = 2,
    SPATIAL_EFFECT_WAVE_RADIAL      = 3,
    SPATIAL_EFFECT_RAIN             = 4,
    SPATIAL_EFFECT_FIRE             = 5,
    SPATIAL_EFFECT_PLASMA           = 6,
    SPATIAL_EFFECT_RIPPLE           = 7,
    SPATIAL_EFFECT_SPIRAL           = 8,
    SPATIAL_EFFECT_ORBIT            = 9,
    SPATIAL_EFFECT_SPHERE_PULSE     = 10,
    SPATIAL_EFFECT_CUBE_ROTATE      = 11,
    SPATIAL_EFFECT_METEOR           = 12,
    SPATIAL_EFFECT_DNA_HELIX        = 13,
    SPATIAL_EFFECT_ROOM_SWEEP       = 14,
    SPATIAL_EFFECT_CORNERS          = 15,
    SPATIAL_EFFECT_VERTICAL_BARS    = 16,
    SPATIAL_EFFECT_BREATHING_SPHERE = 17,
    SPATIAL_EFFECT_EXPLOSION        = 18,
    SPATIAL_EFFECT_WIPE_TOP_BOTTOM  = 19,
    SPATIAL_EFFECT_WIPE_LEFT_RIGHT  = 20,
    SPATIAL_EFFECT_WIPE_FRONT_BACK  = 21,
    SPATIAL_EFFECT_LED_SPARKLE      = 22,
    SPATIAL_EFFECT_LED_CHASE        = 23,
    SPATIAL_EFFECT_LED_TWINKLE      = 24,
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
    Vector3D            origin;             // Center point for effect
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

class SpatialEffectCalculator
{
public:
    static RGBColor CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params, unsigned int led_idx = 0);

private:
    static RGBColor CalculateWaveColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateRadialWaveColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateRainColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateFireColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculatePlasmaColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateRippleColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateSpiralColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateOrbitColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateSpherePulseColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateCubeRotateColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateMeteorColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateDNAHelixColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateRoomSweepColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateCornersColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateVerticalBarsColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateBreathingSphereColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateExplosionColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateWipeTopBottomColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateWipeLeftRightColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateWipeFrontBackColor(Vector3D pos, float time_offset, const SpatialEffectParams& params);
    static RGBColor CalculateLEDSparkleColor(Vector3D pos, float time_offset, const SpatialEffectParams& params, unsigned int led_idx);
    static RGBColor CalculateLEDChaseColor(Vector3D pos, float time_offset, const SpatialEffectParams& params, unsigned int led_idx);
    static RGBColor CalculateLEDTwinkleColor(Vector3D pos, float time_offset, const SpatialEffectParams& params, unsigned int led_idx);

    static RGBColor LerpColor(RGBColor a, RGBColor b, float t, const SpatialEffectParams& params);
    static float Distance3D(Vector3D a, Vector3D b);
};

#endif