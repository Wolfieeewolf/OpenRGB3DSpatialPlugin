/*---------------------------------------------------------*\
| Plasma.cpp                                                |
|                                                           |
|   Plasma effect with multiple sine waves                  |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Plasma.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_SPATIAL_EFFECT(Plasma);

Plasma::Plasma()
{
}

Plasma::~Plasma()
{
}

SpatialEffectInfo Plasma::GetEffectInfo()
{
    SpatialEffectInfo info;
    info.EffectName = UI_Name();
    info.EffectClassName = ClassName();
    info.EffectDescription = "Plasma effect with flowing colors";
    info.Category = CAT_PLASMA;
    info.IsReversible = false;
    info.SupportsRandom = true;
    info.MaxSpeed = 100;
    info.MinSpeed = 1;
    info.UserColors = 2;
    info.Slider2Name = "Complexity";
    info.MaxSlider2Val = 50;
    info.MinSlider2Val = 1;
    return info;
}

RGBColor Plasma::CalculateColor(Vector3D position, float time_offset, const SpatialEffectParams& params)
{
    float scale = params.scale;
    float t = time_offset * 0.01f;

    float plasma1 = sin(position.x * scale * 0.1f + t);
    float plasma2 = sin(position.y * scale * 0.1f + t * 1.3f);
    float plasma3 = sin((position.x + position.y) * scale * 0.05f + t * 0.8f);
    float plasma4 = sin(sqrt(position.x * position.x + position.y * position.y) * scale * 0.1f + t * 1.7f);

    float plasma = (plasma1 + plasma2 + plasma3 + plasma4) / 4.0f;
    plasma = (plasma + 1.0f) / 2.0f; // Normalize to 0-1

    if(params.use_gradient)
    {
        return LerpColor(params.color_start, params.color_end, plasma);
    }
    else
    {
        // Create rainbow effect when not using gradient
        float hue = Wrap(plasma * 360.0f, 0.0f, 360.0f);

        // Simple HSV to RGB conversion for rainbow effect
        float c = 1.0f;
        float x = c * (1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f));

        float r = 0, g = 0, b = 0;
        if(hue < 60) { r = c; g = x; b = 0; }
        else if(hue < 120) { r = x; g = c; b = 0; }
        else if(hue < 180) { r = 0; g = c; b = x; }
        else if(hue < 240) { r = 0; g = x; b = c; }
        else if(hue < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }

        return ((unsigned char)(r * 255) << 16) |
               ((unsigned char)(g * 255) << 8) |
               (unsigned char)(b * 255);
    }
}