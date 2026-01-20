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
#include "GridSpaceUtils.h"
#include "Geometry3DUtils.h"
#include <cmath>
#include <limits>

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
            led_pos.room_position = led_pos.local_position;
            led_pos.preview_color = 0x00FFFFFF;

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
            positions[i].room_position = positions[i].local_position;
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
    float scale_x = (spacing_mm_x > 0.001f) ? MMToGridUnits(spacing_mm_x, grid_scale_mm) : 1.0f;
    float scale_y = (spacing_mm_y > 0.001f) ? MMToGridUnits(spacing_mm_y, grid_scale_mm) : 1.0f;
    float scale_z = (spacing_mm_z > 0.001f) ? MMToGridUnits(spacing_mm_z, grid_scale_mm) : 1.0f;

    for(unsigned int i = 0; i < positions.size(); i++)
    {
        positions[i].local_position.x *= scale_x;
        positions[i].local_position.y *= scale_y;
        positions[i].local_position.z *= scale_z;
        positions[i].world_position = positions[i].local_position;
        positions[i].preview_color = 0x00FFFFFF;
    }

    return positions;
}

Vector3D ControllerLayout3D::CalculateWorldPosition(Vector3D local_pos, Transform3D transform)
{
    float rotation_matrix[9];
    Geometry3D::ComputeRotationMatrix(transform.rotation, rotation_matrix);
    Vector3D rotated = Geometry3D::RotateVector(local_pos, rotation_matrix);

    Vector3D world_pos;
    world_pos.x = rotated.x + transform.position.x;
    world_pos.y = rotated.y + transform.position.y;
    world_pos.z = rotated.z + transform.position.z;

    return world_pos;
}

void ControllerLayout3D::UpdateWorldPositions(ControllerTransform* ctrl_transform)
{
    if(!ctrl_transform)
    {
        return;
    }

    // Compute local-space bounding box center so world positions match viewport centering
    // The viewport renders controllers centered on their local bounds before applying the
    // controller transform. To ensure effects use the same world positions, we center here too.
    Vector3D local_min = {0.0f, 0.0f, 0.0f};
    Vector3D local_max = {0.0f, 0.0f, 0.0f};
    bool have_bounds = false;
    for(unsigned int i = 0; i < ctrl_transform->led_positions.size(); i++)
    {
        const Vector3D& p = ctrl_transform->led_positions[i].local_position;
        if(!have_bounds)
        {
            local_min = p;
            local_max = p;
            have_bounds = true;
        }
        else
        {
            if(p.x < local_min.x) local_min.x = p.x;
            if(p.y < local_min.y) local_min.y = p.y;
            if(p.z < local_min.z) local_min.z = p.z;
            if(p.x > local_max.x) local_max.x = p.x;
            if(p.y > local_max.y) local_max.y = p.y;
            if(p.z > local_max.z) local_max.z = p.z;
        }
    }
    Vector3D local_center = {0.0f, 0.0f, 0.0f};
    if(have_bounds)
    {
        local_center.x = (local_min.x + local_max.x) * 0.5f;
        local_center.y = (local_min.y + local_max.y) * 0.5f;
        local_center.z = (local_min.z + local_max.z) * 0.5f;
    }

    // Pre-compute rotation matrix once per controller
    float rotation_matrix[9];
    Geometry3D::ComputeRotationMatrix(ctrl_transform->transform.rotation, rotation_matrix);

    // Update world position for each LED
    for(unsigned int i = 0; i < ctrl_transform->led_positions.size(); i++)
    {
        LEDPosition3D* led_pos = &ctrl_transform->led_positions[i];
        // Center local position so rotation/translation match viewport rendering
        Vector3D local = {
            led_pos->local_position.x - local_center.x,
            led_pos->local_position.y - local_center.y,
            led_pos->local_position.z - local_center.z
        };

        Vector3D rotated = Geometry3D::RotateVector(local, rotation_matrix);

        // Apply translation for display/world coordinates (used by viewport, ambilight, etc.)
        led_pos->world_position.x = rotated.x + ctrl_transform->transform.position.x;
        led_pos->world_position.y = rotated.y + ctrl_transform->transform.position.y;
        led_pos->world_position.z = rotated.z + ctrl_transform->transform.position.z;

        // Store a room-aligned position that ignores controller rotation so
        // global room effects (wipes, waves, etc.) remain axis-locked.
        led_pos->room_position.x = local.x + ctrl_transform->transform.position.x;
        led_pos->room_position.y = local.y + ctrl_transform->transform.position.y;
        led_pos->room_position.z = local.z + ctrl_transform->transform.position.z;
    }

    ctrl_transform->world_positions_dirty = false;
}

void ControllerLayout3D::MarkWorldPositionsDirty(ControllerTransform* ctrl_transform)
{
    if(!ctrl_transform)
    {
        return;
    }

    ctrl_transform->world_positions_dirty = true;
}

/*---------------------------------------------------------*\
| SpatialHash Implementation                               |
\*---------------------------------------------------------*/

ControllerLayout3D::SpatialHash::SpatialHash(float cell_size)
    : cell_size(cell_size)
{
}

void ControllerLayout3D::SpatialHash::Clear()
{
    grid.clear();
}

int64_t ControllerLayout3D::SpatialHash::HashCell(int x, int y, int z) const
{
    // Cantor pairing function for 3D
    // Combine x,y,z into unique 64-bit hash
    int64_t hash = (int64_t)x;
    hash = hash * 73856093;
    hash ^= (int64_t)y * 19349663;
    hash ^= (int64_t)z * 83492791;
    return hash;
}

void ControllerLayout3D::SpatialHash::GetCellCoords(float x, float y, float z, int& cx, int& cy, int& cz) const
{
    cx = (int)floorf(x / cell_size);
    cy = (int)floorf(y / cell_size);
    cz = (int)floorf(z / cell_size);
}

void ControllerLayout3D::SpatialHash::Insert(LEDPosition3D* led_pos)
{
    if(!led_pos) return;

    int cx, cy, cz;
    GetCellCoords(led_pos->world_position.x,
                  led_pos->world_position.y,
                  led_pos->world_position.z,
                  cx, cy, cz);

    int64_t hash = HashCell(cx, cy, cz);
    grid[hash].leds.push_back(led_pos);
}

void ControllerLayout3D::SpatialHash::Build(const std::vector<std::unique_ptr<ControllerTransform>>& transforms)
{
    Clear();

    for(unsigned int i = 0; i < transforms.size(); i++)
    {
        ControllerTransform* transform = transforms[i].get();
        if(!transform) continue;

        for(unsigned int j = 0; j < transform->led_positions.size(); j++)
        {
            Insert(&transform->led_positions[j]);
        }
    }
}

float ControllerLayout3D::SpatialHash::DistanceSquared(float x1, float y1, float z1, float x2, float y2, float z2) const
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return dx*dx + dy*dy + dz*dz;
}

std::vector<LEDPosition3D*> ControllerLayout3D::SpatialHash::QueryRadius(float x, float y, float z, float radius)
{
    std::vector<LEDPosition3D*> results;

    // Calculate cell range to check
    int min_cx = (int)floorf((x - radius) / cell_size);
    int max_cx = (int)floorf((x + radius) / cell_size);
    int min_cy = (int)floorf((y - radius) / cell_size);
    int max_cy = (int)floorf((y + radius) / cell_size);
    int min_cz = (int)floorf((z - radius) / cell_size);
    int max_cz = (int)floorf((z + radius) / cell_size);

    float radius_sq = radius * radius;

    // Check all cells within range
    for(int cx = min_cx; cx <= max_cx; cx++)
    {
        for(int cy = min_cy; cy <= max_cy; cy++)
        {
            for(int cz = min_cz; cz <= max_cz; cz++)
            {
                int64_t hash = HashCell(cx, cy, cz);
                std::unordered_map<int64_t, SpatialCell>::iterator it = grid.find(hash);
                if(it == grid.end()) continue;

                // Check each LED in cell
                for(unsigned int i = 0; i < it->second.leds.size(); i++)
                {
                    LEDPosition3D* led = it->second.leds[i];
                    float dist_sq = DistanceSquared(x, y, z,
                                                    led->world_position.x,
                                                    led->world_position.y,
                                                    led->world_position.z);
                    if(dist_sq <= radius_sq)
                    {
                        results.push_back(led);
                    }
                }
            }
        }
    }

    return results;
}

LEDPosition3D* ControllerLayout3D::SpatialHash::FindNearest(float x, float y, float z)
{
    LEDPosition3D* nearest = nullptr;
    float min_dist_sq = std::numeric_limits<float>::max();

    int cx, cy, cz;
    GetCellCoords(x, y, z, cx, cy, cz);

    // Start with current cell and expand if needed
    int search_radius = 0;
    const int max_search_radius = 10; // Limit search to avoid infinite loop

    while(!nearest && search_radius <= max_search_radius)
    {
        for(int dx = -search_radius; dx <= search_radius; dx++)
        {
            for(int dy = -search_radius; dy <= search_radius; dy++)
            {
                for(int dz = -search_radius; dz <= search_radius; dz++)
                {
                    // Only check cells on the current shell (not already checked)
                    if(search_radius > 0 &&
                       abs(dx) != search_radius &&
                       abs(dy) != search_radius &&
                       abs(dz) != search_radius)
                    {
                        continue;
                    }

                    int64_t hash = HashCell(cx + dx, cy + dy, cz + dz);
                    std::unordered_map<int64_t, SpatialCell>::iterator it = grid.find(hash);
                    if(it == grid.end()) continue;

                    // Check each LED in cell
                    for(unsigned int i = 0; i < it->second.leds.size(); i++)
                    {
                        LEDPosition3D* led = it->second.leds[i];
                        float dist_sq = DistanceSquared(x, y, z,
                                                        led->world_position.x,
                                                        led->world_position.y,
                                                        led->world_position.z);
                        if(dist_sq < min_dist_sq)
                        {
                            min_dist_sq = dist_sq;
                            nearest = led;
                        }
                    }
                }
            }
        }

        search_radius++;
    }

    return nearest;
}
