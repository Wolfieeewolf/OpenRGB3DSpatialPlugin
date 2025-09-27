/*---------------------------------------------------------*\
| RadialWave.cpp                                            |
|                                                           |
|   Radial wave effect from center point                    |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "RadialWave.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_SPATIAL_EFFECT(RadialWave);

RadialWave::RadialWave()
{
}

RadialWave::~RadialWave()
{
}

SpatialEffectInfo RadialWave::GetEffectInfo()
{
    SpatialEffectInfo info;
    info.EffectName = UI_Name();
    info.EffectClassName = ClassName();
    info.EffectDescription = "Radial wave expanding from center";
    info.Category = CAT_WAVES;
    info.IsReversible = true;
    info.SupportsRandom = true;
    info.MaxSpeed = 100;
    info.MinSpeed = 1;
    info.UserColors = 2;
    info.Slider2Name = "Scale";
    info.MaxSlider2Val = 100;
    info.MinSlider2Val = 1;
    return info;
}

RGBColor RadialWave::CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params)
{
    float dist = Distance3D(position, params.origin);

    if(params.reverse)
    {
        dist = -dist;
    }

    float wave = (sin((dist * params.scale + time_offset) / 10.0f) + 1.0f) / 2.0f;

    if(params.use_gradient)
    {
        return LerpColor(params.color_start, params.color_end, wave);
    }
    else
    {
        return params.color_start;
    }
}