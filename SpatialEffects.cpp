/*---------------------------------------------------------*\
| SpatialEffects.cpp                                        |
|                                                           |
|   3D Spatial lighting effects system                     |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpatialEffects.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SpatialEffects::SpatialEffects(SpatialGrid3D* g) : grid(g)
{
    running = false;
    time_counter = 0;

    params.type = SPATIAL_EFFECT_WAVE_X;
    params.speed = 50;
    params.brightness = 100;
    params.color_start = 0xFF0000;
    params.color_end = 0x0000FF;
    params.use_gradient = true;
    params.scale = 1.0f;
    params.origin = {0, 0, 0};
}

SpatialEffects::~SpatialEffects()
{
    StopEffect();
}

void SpatialEffects::StartEffect(SpatialEffectParams p)
{
    QMutexLocker lock(&mutex);

    params = p;
    time_counter = 0;
    running = true;

    start();
}

void SpatialEffects::StopEffect()
{
    running = false;
    wait();
}

bool SpatialEffects::IsRunning()
{
    return running;
}

void SpatialEffects::SetSpeed(unsigned int speed)
{
    QMutexLocker lock(&mutex);
    params.speed = speed;
}

void SpatialEffects::SetBrightness(unsigned int brightness)
{
    QMutexLocker lock(&mutex);
    params.brightness = brightness;
}

void SpatialEffects::SetColors(RGBColor start, RGBColor end, bool gradient)
{
    QMutexLocker lock(&mutex);
    params.color_start = start;
    params.color_end = end;
    params.use_gradient = gradient;
}

void SpatialEffects::run()
{
    while(running)
    {
        UpdateDeviceColors();

        time_counter++;

        msleep(1000 / 60);
    }
}

void SpatialEffects::UpdateDeviceColors()
{
    QMutexLocker lock(&mutex);

    std::vector<DeviceGridEntry*> devices = grid->GetAllDevices();

    float time_offset = (float)time_counter * (params.speed / 100.0f);

    for(unsigned int i = 0; i < devices.size(); i++)
    {
        if(!devices[i]->enabled)
        {
            continue;
        }

        RGBColor color = 0;

        switch(params.type)
        {
            case SPATIAL_EFFECT_WAVE_X:
            case SPATIAL_EFFECT_WAVE_Y:
            case SPATIAL_EFFECT_WAVE_Z:
                color = CalculateWaveColor(devices[i]->position, time_offset);
                break;

            case SPATIAL_EFFECT_WAVE_RADIAL:
                color = CalculateRadialWaveColor(devices[i]->position, time_offset);
                break;

            case SPATIAL_EFFECT_RAIN:
                color = CalculateRainColor(devices[i]->position, time_offset);
                break;

            case SPATIAL_EFFECT_FIRE:
                color = CalculateFireColor(devices[i]->position, time_offset);
                break;

            case SPATIAL_EFFECT_PLASMA:
                color = CalculatePlasmaColor(devices[i]->position, time_offset);
                break;

            case SPATIAL_EFFECT_RIPPLE:
                color = CalculateRippleColor(devices[i]->position, time_offset);
                break;

            case SPATIAL_EFFECT_SPIRAL:
                color = CalculateSpiralColor(devices[i]->position, time_offset);
                break;
        }

        RGBController* controller = devices[i]->controller;

        for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
        {
            for(unsigned int led_idx = 0; led_idx < controller->zones[zone_idx].leds_count; led_idx++)
            {
                controller->colors[controller->zones[zone_idx].start_idx + led_idx] = color;
            }
        }

        controller->UpdateLEDs();
    }
}

RGBColor SpatialEffects::CalculateWaveColor(GridPosition pos, float time_offset)
{
    float position_val = 0;

    if(params.type == SPATIAL_EFFECT_WAVE_X)
    {
        position_val = (float)pos.x * params.scale;
    }
    else if(params.type == SPATIAL_EFFECT_WAVE_Y)
    {
        position_val = (float)pos.y * params.scale;
    }
    else if(params.type == SPATIAL_EFFECT_WAVE_Z)
    {
        position_val = (float)pos.z * params.scale;
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

RGBColor SpatialEffects::CalculateRadialWaveColor(GridPosition pos, float time_offset)
{
    float dist = Distance3D(pos, params.origin);

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

RGBColor SpatialEffects::CalculateRainColor(GridPosition pos, float time_offset)
{
    float y_pos = (float)pos.y + time_offset;
    float intensity = fmod(y_pos * params.scale, 10.0f) / 10.0f;

    return LerpColor(0x000000, params.color_start, intensity);
}

RGBColor SpatialEffects::CalculateFireColor(GridPosition pos, float time_offset)
{
    float base = sin((float)pos.x * 0.5f + time_offset * 0.1f) * 0.3f;
    float flicker = sin(time_offset * 0.8f + (float)pos.x) * 0.2f;
    float height_factor = 1.0f - ((float)pos.y / 10.0f);

    float intensity = (base + flicker + height_factor) / 2.0f;
    intensity = fmax(0.0f, fmin(1.0f, intensity));

    RGBColor orange = 0xFF4500;
    RGBColor yellow = 0xFFFF00;

    return LerpColor(orange, yellow, intensity);
}

RGBColor SpatialEffects::CalculatePlasmaColor(GridPosition pos, float time_offset)
{
    float v1 = sin(((float)pos.x + time_offset) * 0.1f);
    float v2 = sin(((float)pos.y + time_offset) * 0.1f);
    float v3 = sin(((float)pos.z + time_offset) * 0.1f);

    float value = (v1 + v2 + v3 + 3.0f) / 6.0f;

    return LerpColor(params.color_start, params.color_end, value);
}

RGBColor SpatialEffects::CalculateRippleColor(GridPosition pos, float time_offset)
{
    float dist = Distance3D(pos, params.origin);

    float ripple1 = sin((dist * params.scale - time_offset) / 5.0f);
    float ripple2 = sin((dist * params.scale - time_offset * 1.5f) / 7.0f);

    float intensity = (ripple1 + ripple2 + 2.0f) / 4.0f;

    return LerpColor(params.color_start, params.color_end, intensity);
}

RGBColor SpatialEffects::CalculateSpiralColor(GridPosition pos, float time_offset)
{
    float angle = atan2((float)(pos.y - params.origin.y), (float)(pos.x - params.origin.x));
    float dist = Distance3D(pos, params.origin);

    float spiral = angle + (dist * params.scale / 5.0f) - (time_offset / 10.0f);
    float value = (sin(spiral * 2.0f * M_PI) + 1.0f) / 2.0f;

    return LerpColor(params.color_start, params.color_end, value);
}

RGBColor SpatialEffects::LerpColor(RGBColor a, RGBColor b, float t)
{
    t = fmax(0.0f, fmin(1.0f, t));

    unsigned char r_a = (a >> 16) & 0xFF;
    unsigned char g_a = (a >> 8) & 0xFF;
    unsigned char b_a = a & 0xFF;

    unsigned char r_b = (b >> 16) & 0xFF;
    unsigned char g_b = (b >> 8) & 0xFF;
    unsigned char b_b = b & 0xFF;

    unsigned char r = (unsigned char)(r_a + (r_b - r_a) * t);
    unsigned char g = (unsigned char)(g_a + (g_b - g_a) * t);
    unsigned char b_out = (unsigned char)(b_a + (b_b - b_a) * t);

    float brightness_scale = params.brightness / 100.0f;
    r = (unsigned char)(r * brightness_scale);
    g = (unsigned char)(g * brightness_scale);
    b_out = (unsigned char)(b_out * brightness_scale);

    return (r << 16) | (g << 8) | b_out;
}

float SpatialEffects::Distance3D(GridPosition a, GridPosition b)
{
    float dx = (float)(a.x - b.x);
    float dy = (float)(a.y - b.y);
    float dz = (float)(a.z - b.z);

    return sqrt(dx * dx + dy * dy + dz * dz);
}