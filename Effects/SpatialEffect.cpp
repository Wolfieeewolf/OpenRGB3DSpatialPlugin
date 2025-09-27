/*---------------------------------------------------------*\
| SpatialEffect.cpp                                         |
|                                                           |
|   Base class for 3D spatial effects                      |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpatialEffect.h"
#include <cmath>

float SpatialEffect::Distance3D(Vector3D a, Vector3D b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrt(dx*dx + dy*dy + dz*dz);
}

RGBColor SpatialEffect::LerpColor(RGBColor start, RGBColor end, float t)
{
    t = Clamp(t, 0.0f, 1.0f);

    unsigned char start_r = (start >> 16) & 0xFF;
    unsigned char start_g = (start >> 8) & 0xFF;
    unsigned char start_b = start & 0xFF;

    unsigned char end_r = (end >> 16) & 0xFF;
    unsigned char end_g = (end >> 8) & 0xFF;
    unsigned char end_b = end & 0xFF;

    unsigned char result_r = (unsigned char)(start_r + t * (end_r - start_r));
    unsigned char result_g = (unsigned char)(start_g + t * (end_g - start_g));
    unsigned char result_b = (unsigned char)(start_b + t * (end_b - start_b));

    return (result_r << 16) | (result_g << 8) | result_b;
}

float SpatialEffect::Clamp(float value, float min, float max)
{
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

float SpatialEffect::Wrap(float value, float min, float max)
{
    float range = max - min;
    if(range <= 0) return min;

    while(value < min) value += range;
    while(value >= max) value -= range;
    return value;
}