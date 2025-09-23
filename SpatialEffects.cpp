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

SpatialEffects::SpatialEffects()
{
    controller_transforms = nullptr;
    running = false;
    time_counter = 0;

    params.type = SPATIAL_EFFECT_WAVE_X;
    params.speed = 50;
    params.brightness = 100;
    params.color_start = 0xFF0000;
    params.color_end = 0x0000FF;
    params.use_gradient = true;
    params.scale = 1.0f;
    params.origin = {0.0f, 0.0f, 0.0f};
}

void SpatialEffects::SetControllerTransforms(std::vector<ControllerTransform*>* transforms)
{
    QMutexLocker lock(&mutex);
    controller_transforms = transforms;
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
        UpdateLEDColors();

        time_counter++;

        msleep(1000 / 60);
    }
}

void SpatialEffects::UpdateLEDColors()
{
    QMutexLocker lock(&mutex);

    if(controller_transforms == nullptr)
    {
        return;
    }

    float time_offset = (float)time_counter * (params.speed / 100.0f);

    for(unsigned int i = 0; i < controller_transforms->size(); i++)
    {
        ControllerTransform* ctrl_transform = (*controller_transforms)[i];
        RGBController* controller = ctrl_transform->controller;

        for(unsigned int j = 0; j < ctrl_transform->led_positions.size(); j++)
        {
            LEDPosition3D& led_pos = ctrl_transform->led_positions[j];

            led_pos.world_position = TransformToWorld(led_pos.local_position, ctrl_transform->transform);

            RGBColor color = 0;

            switch(params.type)
            {
                case SPATIAL_EFFECT_WAVE_X:
                case SPATIAL_EFFECT_WAVE_Y:
                case SPATIAL_EFFECT_WAVE_Z:
                    color = CalculateWaveColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_WAVE_RADIAL:
                    color = CalculateRadialWaveColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_RAIN:
                    color = CalculateRainColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_FIRE:
                    color = CalculateFireColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_PLASMA:
                    color = CalculatePlasmaColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_RIPPLE:
                    color = CalculateRippleColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_SPIRAL:
                    color = CalculateSpiralColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_ORBIT:
                    color = CalculateOrbitColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_SPHERE_PULSE:
                    color = CalculateSpherePulseColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_CUBE_ROTATE:
                    color = CalculateCubeRotateColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_METEOR:
                    color = CalculateMeteorColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_DNA_HELIX:
                    color = CalculateDNAHelixColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_ROOM_SWEEP:
                    color = CalculateRoomSweepColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_CORNERS:
                    color = CalculateCornersColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_VERTICAL_BARS:
                    color = CalculateVerticalBarsColor(led_pos.world_position, time_offset);
                    break;

                case SPATIAL_EFFECT_BREATHING_SPHERE:
                    color = CalculateBreathingSphereColor(led_pos.world_position, time_offset);
                    break;
            }

            unsigned int led_global_idx = controller->zones[led_pos.zone_idx].start_idx + led_pos.led_idx;
            controller->colors[led_global_idx] = color;
        }

        controller->UpdateLEDs();
    }

    emit EffectUpdated();
}

RGBColor SpatialEffects::CalculateWaveColor(Vector3D pos, float time_offset)
{
    float position_val = 0;

    if(params.type == SPATIAL_EFFECT_WAVE_X)
    {
        position_val = pos.x * params.scale;
    }
    else if(params.type == SPATIAL_EFFECT_WAVE_Y)
    {
        position_val = pos.y * params.scale;
    }
    else if(params.type == SPATIAL_EFFECT_WAVE_Z)
    {
        position_val = pos.z * params.scale;
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

RGBColor SpatialEffects::CalculateRadialWaveColor(Vector3D pos, float time_offset)
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

RGBColor SpatialEffects::CalculateRainColor(Vector3D pos, float time_offset)
{
    float y_pos = pos.y + time_offset;
    float intensity = fmod(y_pos * params.scale, 10.0f) / 10.0f;

    return LerpColor(0x000000, params.color_start, intensity);
}

RGBColor SpatialEffects::CalculateFireColor(Vector3D pos, float time_offset)
{
    float base = sin(pos.x * 0.5f + time_offset * 0.1f) * 0.3f;
    float flicker = sin(time_offset * 0.8f + pos.x) * 0.2f;
    float height_factor = 1.0f - (pos.y / 10.0f);

    float intensity = (base + flicker + height_factor) / 2.0f;
    intensity = fmax(0.0f, fmin(1.0f, intensity));

    RGBColor orange = 0xFF4500;
    RGBColor yellow = 0xFFFF00;

    return LerpColor(orange, yellow, intensity);
}

RGBColor SpatialEffects::CalculatePlasmaColor(Vector3D pos, float time_offset)
{
    float v1 = sin((pos.x + time_offset) * 0.1f);
    float v2 = sin((pos.y + time_offset) * 0.1f);
    float v3 = sin((pos.z + time_offset) * 0.1f);

    float value = (v1 + v2 + v3 + 3.0f) / 6.0f;

    return LerpColor(params.color_start, params.color_end, value);
}

RGBColor SpatialEffects::CalculateRippleColor(Vector3D pos, float time_offset)
{
    float dist = Distance3D(pos, params.origin);

    float ripple1 = sin((dist * params.scale - time_offset) / 5.0f);
    float ripple2 = sin((dist * params.scale - time_offset * 1.5f) / 7.0f);

    float intensity = (ripple1 + ripple2 + 2.0f) / 4.0f;

    return LerpColor(params.color_start, params.color_end, intensity);
}

RGBColor SpatialEffects::CalculateSpiralColor(Vector3D pos, float time_offset)
{
    float angle = atan2(pos.y - params.origin.y, pos.x - params.origin.x);
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

    return (b_out << 16) | (g << 8) | r;
}

float SpatialEffects::Distance3D(Vector3D a, Vector3D b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;

    return sqrt(dx * dx + dy * dy + dz * dz);
}

Vector3D SpatialEffects::RotateVector(Vector3D vec, Rotation3D rot)
{
    float rad_x = rot.x * M_PI / 180.0f;
    float rad_y = rot.y * M_PI / 180.0f;
    float rad_z = rot.z * M_PI / 180.0f;

    Vector3D result = vec;

    float cos_x = cos(rad_x);
    float sin_x = sin(rad_x);
    float y = result.y * cos_x - result.z * sin_x;
    float z = result.y * sin_x + result.z * cos_x;
    result.y = y;
    result.z = z;

    float cos_y = cos(rad_y);
    float sin_y = sin(rad_y);
    float x = result.x * cos_y + result.z * sin_y;
    z = -result.x * sin_y + result.z * cos_y;
    result.x = x;
    result.z = z;

    float cos_z = cos(rad_z);
    float sin_z = sin(rad_z);
    x = result.x * cos_z - result.y * sin_z;
    y = result.x * sin_z + result.y * cos_z;
    result.x = x;
    result.y = y;

    return result;
}

Vector3D SpatialEffects::TransformToWorld(Vector3D local_pos, Transform3D transform)
{
    Vector3D rotated = RotateVector(local_pos, transform.rotation);

    Vector3D world;
    world.x = rotated.x * transform.scale.x + transform.position.x;
    world.y = rotated.y * transform.scale.y + transform.position.y;
    world.z = rotated.z * transform.scale.z + transform.position.z;

    return world;
}

RGBColor SpatialEffects::CalculateOrbitColor(Vector3D pos, float time_offset)
{
    float angle = atan2(pos.z, pos.x) + time_offset / 20.0f;
    float radius = sqrt(pos.x * pos.x + pos.z * pos.z);

    float orbit_x = cos(angle) * radius;
    float orbit_z = sin(angle) * radius;

    float dist = sqrt((pos.x - orbit_x) * (pos.x - orbit_x) + (pos.z - orbit_z) * (pos.z - orbit_z));
    float brightness = fmax(0.0f, 1.0f - dist / 5.0f);

    return LerpColor(0x000000, params.color_start, brightness);
}

RGBColor SpatialEffects::CalculateSpherePulseColor(Vector3D pos, float time_offset)
{
    float dist = Distance3D(pos, params.origin);
    float pulse_radius = fmod(time_offset / 5.0f, 50.0f);

    float diff = fabs(dist - pulse_radius);
    float brightness = fmax(0.0f, 1.0f - diff / 3.0f);

    if(params.use_gradient)
    {
        float grad = (sin(time_offset / 20.0f) + 1.0f) / 2.0f;
        return LerpColor(params.color_start, params.color_end, grad * brightness);
    }

    return LerpColor(0x000000, params.color_start, brightness);
}

RGBColor SpatialEffects::CalculateCubeRotateColor(Vector3D pos, float time_offset)
{
    float angle = time_offset / 30.0f;

    float rotated_x = pos.x * cos(angle) - pos.z * sin(angle);
    float rotated_z = pos.x * sin(angle) + pos.z * cos(angle);

    float max_abs = fmax(fmax(fabs(rotated_x), fabs(pos.y)), fabs(rotated_z));

    if(max_abs > 20.0f && max_abs < 25.0f)
    {
        return params.color_start;
    }

    return 0x000000;
}

RGBColor SpatialEffects::CalculateMeteorColor(Vector3D pos, float time_offset)
{
    float meteor_y = 50.0f - fmod(time_offset / 3.0f, 100.0f);
    float meteor_x = time_offset / 5.0f;

    float dist_x = pos.x - meteor_x;
    float dist_y = pos.y - meteor_y;
    float dist_z = pos.z;

    float trail_length = 15.0f;
    float dist = sqrt(dist_x * dist_x + dist_z * dist_z);

    if(dist_y > 0 && dist_y < trail_length && dist < 3.0f)
    {
        float brightness = 1.0f - (dist_y / trail_length);
        return LerpColor(params.color_end, params.color_start, brightness);
    }

    return 0x000000;
}

RGBColor SpatialEffects::CalculateDNAHelixColor(Vector3D pos, float time_offset)
{
    float y_offset = time_offset / 10.0f;

    float angle1 = (pos.y + y_offset) / 5.0f;
    float helix1_x = cos(angle1) * 10.0f;
    float helix1_z = sin(angle1) * 10.0f;

    float angle2 = angle1 + M_PI;
    float helix2_x = cos(angle2) * 10.0f;
    float helix2_z = sin(angle2) * 10.0f;

    float dist1 = sqrt((pos.x - helix1_x) * (pos.x - helix1_x) + (pos.z - helix1_z) * (pos.z - helix1_z));
    float dist2 = sqrt((pos.x - helix2_x) * (pos.x - helix2_x) + (pos.z - helix2_z) * (pos.z - helix2_z));

    if(dist1 < 3.0f)
    {
        return params.color_start;
    }
    else if(dist2 < 3.0f)
    {
        return params.color_end;
    }

    return 0x000000;
}

RGBColor SpatialEffects::CalculateRoomSweepColor(Vector3D pos, float time_offset)
{
    float sweep_pos = fmod(time_offset / 5.0f, 100.0f) - 50.0f;

    float dist = fabs(pos.x - sweep_pos);

    if(dist < 5.0f)
    {
        float brightness = 1.0f - (dist / 5.0f);
        return LerpColor(0x000000, params.color_start, brightness);
    }

    return 0x000000;
}

RGBColor SpatialEffects::CalculateCornersColor(Vector3D pos, float time_offset)
{
    int corner = (int)(time_offset / 30.0f) % 8;

    float target_x = (corner & 1) ? 25.0f : -25.0f;
    float target_y = (corner & 2) ? 25.0f : -25.0f;
    float target_z = (corner & 4) ? 25.0f : -25.0f;

    float dist = sqrt(
        (pos.x - target_x) * (pos.x - target_x) +
        (pos.y - target_y) * (pos.y - target_y) +
        (pos.z - target_z) * (pos.z - target_z)
    );

    if(dist < 10.0f)
    {
        float brightness = 1.0f - (dist / 10.0f);
        return LerpColor(0x000000, params.color_start, brightness);
    }

    return 0x000000;
}

RGBColor SpatialEffects::CalculateVerticalBarsColor(Vector3D pos, float time_offset)
{
    float offset = time_offset / 10.0f;
    float bar_width = 5.0f;
    float spacing = 15.0f;

    float x_mod = fmod(pos.x + offset, spacing);

    if(x_mod < bar_width)
    {
        float t = x_mod / bar_width;
        return LerpColor(params.color_start, params.color_end, t);
    }

    return 0x000000;
}

RGBColor SpatialEffects::CalculateBreathingSphereColor(Vector3D pos, float time_offset)
{
    float breath = (sin(time_offset / 30.0f) + 1.0f) / 2.0f;
    float radius = 15.0f + breath * 15.0f;

    float dist = Distance3D(pos, params.origin);

    if(fabs(dist - radius) < 3.0f)
    {
        float brightness = 1.0f - fabs(dist - radius) / 3.0f;
        return LerpColor(params.color_start, params.color_end, breath * brightness);
    }

    return 0x000000;
}