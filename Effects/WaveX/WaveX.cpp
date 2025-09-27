/*---------------------------------------------------------*\
| WaveX.cpp                                                 |
|                                                           |
|   Wave effect along X axis                                |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "WaveX.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_SPATIAL_EFFECT(WaveX);

WaveX::WaveX()
{
}

WaveX::~WaveX()
{
}

SpatialEffectInfo WaveX::GetEffectInfo()
{
    SpatialEffectInfo info;
    info.EffectName = UI_Name();
    info.EffectClassName = ClassName();
    info.EffectDescription = "Wave effect moving along X axis";
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

RGBColor WaveX::CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params)
{
    float position_val = position.x * params.scale;

    if(params.reverse)
    {
        position_val = -position_val;
    }

    float wave = (sin((position_val + time_offset) / 10.0f) + 1.0f) / 2.0f;

    if(params.use_gradient)
    {
        return LerpColor(params.color_start, params.color_end, wave);
    }
    else
    {
        return params.color_start;
    }
}