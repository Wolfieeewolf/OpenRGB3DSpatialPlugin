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
#include <cmath>

std::vector<LEDPosition3D> ControllerLayout3D::GenerateLEDPositions(RGBController* controller)
{
    std::vector<LEDPosition3D> all_positions;

    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        std::vector<LEDPosition3D> zone_positions;

        switch(controller->zones[zone_idx].type)
        {
            case ZONE_TYPE_MATRIX:
                zone_positions = GenerateMatrixLayout(controller, zone_idx);
                break;

            case ZONE_TYPE_LINEAR:
                zone_positions = GenerateLinearLayout(controller, zone_idx);
                break;

            case ZONE_TYPE_SINGLE:
                zone_positions = GenerateSingleLayout(controller, zone_idx);
                break;
        }

        for(unsigned int i = 0; i < zone_positions.size(); i++)
        {
            all_positions.push_back(zone_positions[i]);
        }
    }

    return all_positions;
}

std::vector<LEDPosition3D> ControllerLayout3D::GenerateMatrixLayout(RGBController* controller, unsigned int zone_idx)
{
    std::vector<LEDPosition3D> positions;

    zone* current_zone = &controller->zones[zone_idx];

    if(current_zone->matrix_map == nullptr)
    {
        return GenerateLinearLayout(controller, zone_idx);
    }

    unsigned int width = current_zone->matrix_map->width;
    unsigned int height = current_zone->matrix_map->height;
    unsigned int* map = current_zone->matrix_map->map;

    float led_spacing = 1.0f;
    float center_x = (width - 1) * led_spacing / 2.0f;
    float center_y = (height - 1) * led_spacing / 2.0f;

    for(unsigned int y = 0; y < height; y++)
    {
        for(unsigned int x = 0; x < width; x++)
        {
            unsigned int led_idx = map[y * width + x];

            if(led_idx != 0xFFFFFFFF && led_idx < current_zone->leds_count)
            {
                LEDPosition3D led_pos;
                led_pos.controller = controller;
                led_pos.zone_idx = zone_idx;
                led_pos.led_idx = led_idx;

                led_pos.local_position.x = x * led_spacing - center_x;
                led_pos.local_position.y = -(y * led_spacing - center_y);
                led_pos.local_position.z = 0.0f;

                led_pos.world_position = led_pos.local_position;

                positions.push_back(led_pos);
            }
        }
    }

    return positions;
}

std::vector<LEDPosition3D> ControllerLayout3D::GenerateLinearLayout(RGBController* controller, unsigned int zone_idx)
{
    std::vector<LEDPosition3D> positions;

    zone* current_zone = &controller->zones[zone_idx];

    float led_spacing = 1.0f;
    unsigned int led_count = current_zone->leds_count;
    float total_width = (led_count - 1) * led_spacing;
    float center_x = total_width / 2.0f;

    for(unsigned int led_idx = 0; led_idx < led_count; led_idx++)
    {
        LEDPosition3D led_pos;
        led_pos.controller = controller;
        led_pos.zone_idx = zone_idx;
        led_pos.led_idx = led_idx;

        led_pos.local_position.x = led_idx * led_spacing - center_x;
        led_pos.local_position.y = 0.0f;
        led_pos.local_position.z = 0.0f;

        led_pos.world_position = led_pos.local_position;

        positions.push_back(led_pos);
    }

    return positions;
}

std::vector<LEDPosition3D> ControllerLayout3D::GenerateSingleLayout(RGBController* controller, unsigned int zone_idx)
{
    std::vector<LEDPosition3D> positions;

    zone* current_zone = &controller->zones[zone_idx];

    for(unsigned int led_idx = 0; led_idx < current_zone->leds_count; led_idx++)
    {
        LEDPosition3D led_pos;
        led_pos.controller = controller;
        led_pos.zone_idx = zone_idx;
        led_pos.led_idx = led_idx;

        led_pos.local_position.x = 0.0f;
        led_pos.local_position.y = 0.0f;
        led_pos.local_position.z = 0.0f;

        led_pos.world_position = led_pos.local_position;

        positions.push_back(led_pos);
    }

    return positions;
}

Vector3D ControllerLayout3D::CalculateWorldPosition(Vector3D local_pos, Transform3D transform)
{
    float qx = transform.rotation.x;
    float qy = transform.rotation.y;
    float qz = transform.rotation.z;
    float qw = transform.rotation.w;

    float x = local_pos.x * transform.scale.x;
    float y = local_pos.y * transform.scale.y;
    float z = local_pos.z * transform.scale.z;

    float rx = x * (1 - 2*qy*qy - 2*qz*qz) + y * (2*qx*qy - 2*qz*qw) + z * (2*qx*qz + 2*qy*qw);
    float ry = x * (2*qx*qy + 2*qz*qw) + y * (1 - 2*qx*qx - 2*qz*qz) + z * (2*qy*qz - 2*qx*qw);
    float rz = x * (2*qx*qz - 2*qy*qw) + y * (2*qy*qz + 2*qx*qw) + z * (1 - 2*qx*qx - 2*qy*qy);

    Vector3D world_pos;
    world_pos.x = rx + transform.position.x;
    world_pos.y = ry + transform.position.y;
    world_pos.z = rz + transform.position.z;

    return world_pos;
}