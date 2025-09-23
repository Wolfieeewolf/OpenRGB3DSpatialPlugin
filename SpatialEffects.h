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
#include "SpatialGrid3D.h"
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
    GridPosition        origin;
};

class SpatialEffects : public QThread
{
    Q_OBJECT

public:
    SpatialEffects(SpatialGrid3D* grid);
    ~SpatialEffects();

    void                StartEffect(SpatialEffectParams params);
    void                StopEffect();
    bool                IsRunning();

    void                SetSpeed(unsigned int speed);
    void                SetBrightness(unsigned int brightness);
    void                SetColors(RGBColor start, RGBColor end, bool gradient);

protected:
    void                run() override;

private:
    void                UpdateDeviceColors();
    RGBColor            CalculateWaveColor(GridPosition pos, float time_offset);
    RGBColor            CalculateRadialWaveColor(GridPosition pos, float time_offset);
    RGBColor            CalculateRainColor(GridPosition pos, float time_offset);
    RGBColor            CalculateFireColor(GridPosition pos, float time_offset);
    RGBColor            CalculatePlasmaColor(GridPosition pos, float time_offset);
    RGBColor            CalculateRippleColor(GridPosition pos, float time_offset);
    RGBColor            CalculateSpiralColor(GridPosition pos, float time_offset);

    RGBColor            LerpColor(RGBColor a, RGBColor b, float t);
    float               Distance3D(GridPosition a, GridPosition b);

    SpatialGrid3D*      grid;
    SpatialEffectParams params;

    bool                running;
    unsigned int        time_counter;

    QMutex              mutex;
};

#endif