/*---------------------------------------------------------*\
| RadialWave.h                                              |
|                                                           |
|   Radial wave effect from center point                    |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef RADIALWAVE_H
#define RADIALWAVE_H

#include "SpatialEffect.h"
#include "SpatialEffectRegisterer.h"

class RadialWave : public SpatialEffect
{
public:
    RadialWave();
    ~RadialWave();

    SPATIAL_EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_WAVES, [](){return new RadialWave;});

    static std::string const ClassName() { return "RadialWave"; }
    static std::string const UI_Name() { return "Radial Wave"; }

    SpatialEffectInfo GetEffectInfo() override;
    RGBColor CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params) override;
};

#endif // RADIALWAVE_H