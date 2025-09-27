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
#include "SpatialEffectCalculator.h"

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
    bool                IsEffectRunning();
    void                UpdateEffectParams(SpatialEffectParams params);

    void                SetSpeed(unsigned int speed);
    void                SetBrightness(unsigned int brightness);
    void                SetColors(RGBColor start, RGBColor end, bool gradient);

signals:
    void                EffectUpdated();

protected:
    void                run() override;

private:
    void                UpdateLEDColors();
    Vector3D            RotateVector(Vector3D vec, Rotation3D rot);
    Vector3D            TransformToWorld(Vector3D local_pos, Transform3D transform);

    std::vector<ControllerTransform*>*  controller_transforms;
    SpatialEffectParams                 params;

    bool                running;
    unsigned int        time_counter;

    QMutex              mutex;
};

#endif