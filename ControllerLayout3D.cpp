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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

std::vector<LEDPosition3D> ControllerLayout3D::GenerateCustomGridLayout(RGBController* controller, int grid_x, int grid_y, int grid_z)
{
    (void)grid_z; // Parameter reserved for future 3D grid functionality

    std::vector<LEDPosition3D> positions;

    // Calculate total LEDs across all zones
    unsigned int total_leds = 0;
    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        total_leds += controller->zones[zone_idx].leds_count;
    }

    // Map LEDs zone by zone to handle different zone types appropriately
    unsigned int global_led_idx = 0;
    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        zone* current_zone = &controller->zones[zone_idx];

        // Always use user-specified grid dimensions for 1:1 mapping
        // This ensures LEDs fit within the specified grid bounds

        for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
        {
            LEDPosition3D led_pos;
            led_pos.controller = controller;
            led_pos.zone_idx = zone_idx;
            led_pos.led_idx = led_idx;

            // For matrix zones, use LED position within the zone
            // For other zones, use global LED index
            unsigned int mapping_idx = led_idx;
            if(current_zone->type != ZONE_TYPE_MATRIX)
            {
                mapping_idx = global_led_idx;
            }

            // Calculate position using a reasonable automatic layout
            // For LED strips, use linear layout; for others, use grid layout
            int x_pos, y_pos, z_pos;

            if(controller->type == DEVICE_TYPE_LEDSTRIP)
            {
                // LED strips: linear layout
                x_pos = mapping_idx;
                y_pos = 0;
                z_pos = 0;
            }
            else if(controller->type == DEVICE_TYPE_KEYBOARD &&
                    current_zone->type == ZONE_TYPE_MATRIX &&
                    current_zone->matrix_map != nullptr)
            {
                // Keyboards: use actual matrix dimensions
                int matrix_width = current_zone->matrix_map->width;
                // matrix_height available but not needed for current mapping
                x_pos = led_idx % matrix_width;
                y_pos = led_idx / matrix_width;
                z_pos = 0; // Keyboards are flat
            }
            else
            {
                // Other devices: use grid layout
                x_pos = mapping_idx % grid_x;
                y_pos = (mapping_idx / grid_x) % grid_y;
                z_pos = mapping_idx / (grid_x * grid_y);
            }

            // Convert to centered INTEGER coordinates for 1x1x1 cube alignment
            led_pos.local_position.x = (float)(x_pos - (x_pos / 2)); // Center around 0
            led_pos.local_position.y = (float)y_pos; // Start from 0
            led_pos.local_position.z = (float)(z_pos - (z_pos / 2)); // Center around 0

            led_pos.world_position = led_pos.local_position;

            positions.push_back(led_pos);
            global_led_idx++;
        }
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