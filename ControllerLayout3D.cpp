/*---------------------------------------------------------*\
| ControllerLayout3D.cpp                                    |
|                                                           |
|   Converts OpenRGB controller layouts to 3D positions    |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "ControllerLayout3D.h"
#include "RGBController.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

std::vector<LEDPosition3D> ControllerLayout3D::GenerateCustomGridLayout(RGBController* controller, int grid_x, int grid_y, int grid_z)
{
    (void)grid_z;

    std::vector<LEDPosition3D> positions;

    unsigned int total_leds = 0;
    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        total_leds += controller->zones[zone_idx].leds_count;
    }

    unsigned int global_led_idx = 0;
    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        zone* current_zone = &controller->zones[zone_idx];

        for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
        {
            LEDPosition3D led_pos;
            led_pos.controller = controller;
            led_pos.zone_idx = zone_idx;
            led_pos.led_idx = led_idx;

            unsigned int mapping_idx = led_idx;
            if(current_zone->type != ZONE_TYPE_MATRIX)
            {
                mapping_idx = global_led_idx;
            }

            int x_pos, y_pos, z_pos;

            if(controller->type == DEVICE_TYPE_LEDSTRIP)
            {
                x_pos = mapping_idx;
                y_pos = 0;
                z_pos = 0;
            }
            else if(controller->type == DEVICE_TYPE_KEYBOARD &&
                    current_zone->type == ZONE_TYPE_MATRIX &&
                    current_zone->matrix_map != nullptr)
            {
                int matrix_width = current_zone->matrix_map->width;
                x_pos = led_idx % matrix_width;
                y_pos = led_idx / matrix_width;
                z_pos = 0;
            }
            else
            {
                x_pos = mapping_idx % grid_x;
                y_pos = (mapping_idx / grid_x) % grid_y;
                z_pos = mapping_idx / (grid_x * grid_y);
            }

            led_pos.local_position.x = (float)x_pos;
            led_pos.local_position.y = (float)y_pos;
            led_pos.local_position.z = (float)z_pos;

            led_pos.world_position = led_pos.local_position;

            positions.push_back(led_pos);
            global_led_idx++;
        }
    }

    // Center all positions at 0,0,0
    if(!positions.empty())
    {
        float min_x = positions[0].local_position.x;
        float max_x = positions[0].local_position.x;
        float min_y = positions[0].local_position.y;
        float max_y = positions[0].local_position.y;
        float min_z = positions[0].local_position.z;
        float max_z = positions[0].local_position.z;

        for(unsigned int i = 1; i < positions.size(); i++)
        {
            if(positions[i].local_position.x < min_x) min_x = positions[i].local_position.x;
            if(positions[i].local_position.x > max_x) max_x = positions[i].local_position.x;
            if(positions[i].local_position.y < min_y) min_y = positions[i].local_position.y;
            if(positions[i].local_position.y > max_y) max_y = positions[i].local_position.y;
            if(positions[i].local_position.z < min_z) min_z = positions[i].local_position.z;
            if(positions[i].local_position.z > max_z) max_z = positions[i].local_position.z;
        }

        float center_x = (min_x + max_x) / 2.0f;
        float center_y = (min_y + max_y) / 2.0f;
        float center_z = (min_z + max_z) / 2.0f;

        for(unsigned int i = 0; i < positions.size(); i++)
        {
            positions[i].local_position.x -= center_x;
            positions[i].local_position.y -= center_y;
            positions[i].local_position.z -= center_z;
            positions[i].world_position = positions[i].local_position;
        }
    }

    return positions;
}

std::vector<LEDPosition3D> ControllerLayout3D::GenerateCustomGridLayoutWithSpacing(RGBController* controller, int grid_x, int grid_y, int grid_z, float spacing_mm_x, float spacing_mm_y, float spacing_mm_z, float grid_scale_mm)
{
    // First generate the layout with integer grid positions
    std::vector<LEDPosition3D> positions = GenerateCustomGridLayout(controller, grid_x, grid_y, grid_z);

    // Now scale positions based on LED spacing and grid scale
    // Formula: grid_position = led_spacing_mm / grid_scale_mm
    float scale_x = (spacing_mm_x > 0.001f) ? (spacing_mm_x / grid_scale_mm) : 1.0f;
    float scale_y = (spacing_mm_y > 0.001f) ? (spacing_mm_y / grid_scale_mm) : 1.0f;
    float scale_z = (spacing_mm_z > 0.001f) ? (spacing_mm_z / grid_scale_mm) : 1.0f;

    for(unsigned int i = 0; i < positions.size(); i++)
    {
        positions[i].local_position.x *= scale_x;
        positions[i].local_position.y *= scale_y;
        positions[i].local_position.z *= scale_z;
        positions[i].world_position = positions[i].local_position;
    }

    return positions;
}

Vector3D ControllerLayout3D::CalculateWorldPosition(Vector3D local_pos, Transform3D transform)
{
    float rad_x = transform.rotation.x * (float)M_PI / 180.0f;
    float rad_y = transform.rotation.y * (float)M_PI / 180.0f;
    float rad_z = transform.rotation.z * (float)M_PI / 180.0f;

    Vector3D rotated = local_pos;

    // Apply X rotation
    float cos_x = cosf(rad_x);
    float sin_x = sinf(rad_x);
    float y = rotated.y * cos_x - rotated.z * sin_x;
    float z = rotated.y * sin_x + rotated.z * cos_x;
    rotated.y = y;
    rotated.z = z;

    // Apply Y rotation
    float cos_y = cosf(rad_y);
    float sin_y = sinf(rad_y);
    float x = rotated.x * cos_y + rotated.z * sin_y;
    z = -rotated.x * sin_y + rotated.z * cos_y;
    rotated.x = x;
    rotated.z = z;

    // Apply Z rotation
    float cos_z = cosf(rad_z);
    float sin_z = sinf(rad_z);
    x = rotated.x * cos_z - rotated.y * sin_z;
    y = rotated.x * sin_z + rotated.y * cos_z;
    rotated.x = x;
    rotated.y = y;

    // Apply translation
    Vector3D world_pos;
    world_pos.x = rotated.x + transform.position.x;
    world_pos.y = rotated.y + transform.position.y;
    world_pos.z = rotated.z + transform.position.z;

    return world_pos;
}