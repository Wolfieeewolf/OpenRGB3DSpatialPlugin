/*---------------------------------------------------------*\
| SpatialEffects.h                                          |
|                                                           |
|   3D Spatial lighting effects system                     |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALEFFECTS_H
#define SPATIALEFFECTS_H

#include <QThread>
#include <QMutex>
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

struct SpatialEffectParams
{
    SpatialEffectType   type;
    unsigned int        speed;
    unsigned int        brightness;
    RGBColor            color_start;
    RGBColor            color_end;
    bool                use_gradient;
    float               scale;
    Vector3D            origin;
};

class SpatialEffects : public QThread
{
    Q_OBJECT

public:
    SpatialEffects();
    ~SpatialEffects();

    void                SetControllerTransforms(std::vector<ControllerTransform*>* transforms);

    void                StartEffect(SpatialEffectParams params);
    void                StopEffect();
    bool                IsRunning();

    void                SetSpeed(unsigned int speed);
    void                SetBrightness(unsigned int brightness);
    void                SetColors(RGBColor start, RGBColor end, bool gradient);

signals:
    void                EffectUpdated();

protected:
    void                run() override;

private:
    void                UpdateLEDColors();
    RGBColor            CalculateWaveColor(Vector3D pos, float time_offset);
    RGBColor            CalculateRadialWaveColor(Vector3D pos, float time_offset);
    RGBColor            CalculateRainColor(Vector3D pos, float time_offset);
    RGBColor            CalculateFireColor(Vector3D pos, float time_offset);
    RGBColor            CalculatePlasmaColor(Vector3D pos, float time_offset);
    RGBColor            CalculateRippleColor(Vector3D pos, float time_offset);
    RGBColor            CalculateSpiralColor(Vector3D pos, float time_offset);
    RGBColor            CalculateOrbitColor(Vector3D pos, float time_offset);
    RGBColor            CalculateSpherePulseColor(Vector3D pos, float time_offset);
    RGBColor            CalculateCubeRotateColor(Vector3D pos, float time_offset);
    RGBColor            CalculateMeteorColor(Vector3D pos, float time_offset);
    RGBColor            CalculateDNAHelixColor(Vector3D pos, float time_offset);
    RGBColor            CalculateRoomSweepColor(Vector3D pos, float time_offset);
    RGBColor            CalculateCornersColor(Vector3D pos, float time_offset);
    RGBColor            CalculateVerticalBarsColor(Vector3D pos, float time_offset);
    RGBColor            CalculateBreathingSphereColor(Vector3D pos, float time_offset);
    RGBColor            CalculateExplosionColor(Vector3D pos, float time_offset);
    RGBColor            CalculateWipeTopBottomColor(Vector3D pos, float time_offset);
    RGBColor            CalculateWipeLeftRightColor(Vector3D pos, float time_offset);
    RGBColor            CalculateWipeFrontBackColor(Vector3D pos, float time_offset);
    RGBColor            CalculateLEDSparkleColor(Vector3D pos, float time_offset, unsigned int led_idx);
    RGBColor            CalculateLEDChaseColor(Vector3D pos, float time_offset, unsigned int led_idx);
    RGBColor            CalculateLEDTwinkleColor(Vector3D pos, float time_offset, unsigned int led_idx);

    RGBColor            LerpColor(RGBColor a, RGBColor b, float t);
    float               Distance3D(Vector3D a, Vector3D b);
    Vector3D            RotateVector(Vector3D vec, Rotation3D rot);
    Vector3D            TransformToWorld(Vector3D local_pos, Transform3D transform);

    std::vector<ControllerTransform*>*  controller_transforms;
    SpatialEffectParams                 params;

    bool                running;
    unsigned int        time_counter;

    QMutex              mutex;
};

#endif