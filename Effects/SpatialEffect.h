/*---------------------------------------------------------*\
| SpatialEffect.h                                           |
|                                                           |
|   Base class for 3D spatial effects                      |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALEFFECT_H
#define SPATIALEFFECT_H

#include <string>
#include <vector>
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

#define CAT_WAVES       "Waves"
#define CAT_PARTICLES   "Particles"
#define CAT_GEOMETRIC   "Geometric"
#define CAT_PLASMA      "Plasma"
#define CAT_MOVEMENT    "Movement"
#define CAT_SPECIAL     "Special"

struct SpatialEffectInfo
{
    std::string EffectName = "";
    std::string EffectClassName = "";
    std::string EffectDescription = "";
    std::string Category = "";

    bool IsReversible = false;
    bool SupportsRandom = true;
    int  MaxSpeed = 100;
    int  MinSpeed = 1;

    unsigned int UserColors = 2;  // Default: start and end color

    int         MaxSlider2Val = 100;
    int         MinSlider2Val = 0;
    std::string Slider2Name = "Scale";

    bool HasCustomSettings = false;
    bool ExpandCustomSettings = false;
};

struct SpatialEffectParams
{
    SpatialEffectType type;
    unsigned int speed = 50;
    unsigned int brightness = 100;
    RGBColor color_start = 0xFF0000;
    RGBColor color_end = 0x0000FF;
    bool use_gradient = true;
    float scale = 1.0f;
    Vector3D origin = {0.0f, 0.0f, 0.0f};
    bool reverse = false;

    // Custom parameters that effects can use
    float custom_param1 = 0.0f;
    float custom_param2 = 0.0f;
    float custom_param3 = 0.0f;
};

class SpatialEffect
{
public:
    SpatialEffect() {}
    virtual ~SpatialEffect() {}

    virtual SpatialEffectInfo GetEffectInfo() = 0;
    virtual RGBColor CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params) = 0;

    // Optional: Custom initialization
    virtual void Initialize(const SpatialEffectParams& params) { (void)params; }

    // Optional: Cleanup when effect stops
    virtual void Cleanup() {}

    // Helper functions for common calculations
    static float Distance3D(Vector3D a, Vector3D b);
    static RGBColor LerpColor(RGBColor start, RGBColor end, float t);
    static float Clamp(float value, float min, float max);
    static float Wrap(float value, float min, float max);
};

#endif // SPATIALEFFECT_H