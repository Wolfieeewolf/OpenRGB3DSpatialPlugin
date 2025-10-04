/*---------------------------------------------------------*\
| SpatialHash3D.cpp                                         |
|                                                           |
|   3D spatial hash for fast spatial queries               |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpatialHash3D.h"
#include <cmath>
#include <limits>

SpatialHash3D::SpatialHash3D(float cell_size)
    : cell_size(cell_size)
{
}

SpatialHash3D::~SpatialHash3D()
{
}

void SpatialHash3D::Clear()
{
    grid.clear();
}

int64_t SpatialHash3D::HashCell(int x, int y, int z) const
{
    // Cantor pairing function for 3D
    // Combine x,y,z into unique 64-bit hash
    int64_t hash = (int64_t)x;
    hash = hash * 73856093;
    hash ^= (int64_t)y * 19349663;
    hash ^= (int64_t)z * 83492791;
    return hash;
}

void SpatialHash3D::GetCellCoords(float x, float y, float z, int& cx, int& cy, int& cz) const
{
    cx = (int)floorf(x / cell_size);
    cy = (int)floorf(y / cell_size);
    cz = (int)floorf(z / cell_size);
}

void SpatialHash3D::Insert(LEDPosition3D* led_pos)
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

void SpatialHash3D::Build(const std::vector<std::unique_ptr<ControllerTransform>>& transforms)
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

float SpatialHash3D::DistanceSquared(float x1, float y1, float z1, float x2, float y2, float z2) const
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return dx*dx + dy*dy + dz*dz;
}

std::vector<LEDPosition3D*> SpatialHash3D::QueryRadius(float x, float y, float z, float radius)
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
                auto it = grid.find(hash);
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

LEDPosition3D* SpatialHash3D::FindNearest(float x, float y, float z)
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
                    auto it = grid.find(hash);
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
