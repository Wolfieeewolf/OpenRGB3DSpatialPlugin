/*---------------------------------------------------------*\
| WaveX.h                                                   |
|                                                           |
|   Wave effect along X axis                                |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef WAVEX_H
#define WAVEX_H

#include "SpatialEffect.h"
#include "SpatialEffectRegisterer.h"

class WaveX : public SpatialEffect
{
public:
    WaveX();
    ~WaveX();

    SPATIAL_EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_WAVES, [](){return new WaveX;});

    static std::string const ClassName() { return "WaveX"; }
    static std::string const UI_Name() { return "Wave X"; }

    SpatialEffectInfo GetEffectInfo() override;
    RGBColor CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params) override;
};

#endif // WAVEX_H