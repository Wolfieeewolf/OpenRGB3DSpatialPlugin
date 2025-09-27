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
#include <set>

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

    /*---------------------------------------------------------*\
    | Initialize 3D spatial parameters                         |
    \*---------------------------------------------------------*/
    params.scale_3d = {1.0f, 1.0f, 1.0f};
    params.origin = {0.0f, 0.0f, 0.0f};
    params.rotation = {0.0f, 0.0f, 0.0f};
    params.direction = {1.0f, 0.0f, 0.0f};
    params.thickness = 1.0f;
    params.intensity = 1.0f;
    params.falloff = 1.0f;
    params.num_arms = 4;
    params.frequency = 10;
    params.reverse = false;
    params.mirror_x = false;
    params.mirror_y = false;
    params.mirror_z = false;
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

bool SpatialEffects::IsEffectRunning()
{
    return running;
}

void SpatialEffects::UpdateEffectParams(SpatialEffectParams new_params)
{
    QMutexLocker lock(&mutex);

    /*---------------------------------------------------------*\
    | Preserve timing information and running state            |
    \*---------------------------------------------------------*/
    bool was_running = running;
    unsigned int current_time = time_counter;

    /*---------------------------------------------------------*\
    | Update parameters                                        |
    \*---------------------------------------------------------*/
    params = new_params;

    /*---------------------------------------------------------*\
    | Restore timing state                                     |
    \*---------------------------------------------------------*/
    running = was_running;
    time_counter = current_time;
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

            color = SpatialEffectCalculator::CalculateColor(led_pos.world_position, time_offset, params, j);

            if(controller != nullptr)
            {
                unsigned int led_global_idx = controller->zones[led_pos.zone_idx].start_idx + led_pos.led_idx;
                controller->colors[led_global_idx] = color;
            }
            else if(led_pos.controller != nullptr)
            {
                unsigned int led_global_idx = led_pos.controller->zones[led_pos.zone_idx].start_idx + led_pos.led_idx;
                led_pos.controller->colors[led_global_idx] = color;
            }
        }

        if(controller != nullptr)
        {
            controller->SetCustomMode();
            controller->UpdateLEDs();
        }
        else
        {
            std::set<RGBController*> updated_controllers;
            for(unsigned int j = 0; j < ctrl_transform->led_positions.size(); j++)
            {
                if(ctrl_transform->led_positions[j].controller != nullptr)
                {
                    updated_controllers.insert(ctrl_transform->led_positions[j].controller);
                }
            }
            for(std::set<RGBController*>::iterator it = updated_controllers.begin(); it != updated_controllers.end(); ++it)
            {
                (*it)->SetCustomMode();
                (*it)->UpdateLEDs();
            }
        }
    }

    emit EffectUpdated();
}


Vector3D SpatialEffects::RotateVector(Vector3D vec, Rotation3D rot)
{
    float rad_x = rot.x * M_PI / 180.0f;
    float rad_y = rot.y * M_PI / 180.0f;
    float rad_z = rot.z * M_PI / 180.0f;

    Vector3D result = vec;

    // Rotate around X axis
    float cos_x = cos(rad_x);
    float sin_x = sin(rad_x);
    float y = result.y * cos_x - result.z * sin_x;
    float z = result.y * sin_x + result.z * cos_x;
    result.y = y;
    result.z = z;

    // Rotate around Y axis
    float cos_y = cos(rad_y);
    float sin_y = sin(rad_y);
    float x = result.x * cos_y + result.z * sin_y;
    z = -result.x * sin_y + result.z * cos_y;
    result.x = x;
    result.z = z;

    // Rotate around Z axis
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