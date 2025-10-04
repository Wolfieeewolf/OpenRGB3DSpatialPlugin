/*---------------------------------------------------------*\
| SpatialHash3D.h                                           |
|                                                           |
|   3D spatial hash for fast spatial queries               |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALHASH3D_H
#define SPATIALHASH3D_H

#include <vector>
#include <unordered_map>
#include "LEDPosition3D.h"

/*---------------------------------------------------------*\
| Spatial Hash Grid for Fast Nearest-Neighbor Queries      |
|                                                           |
| Uses uniform grid cells to partition 3D space            |
| O(1) insertion, O(k) nearest neighbor (k = nearby LEDs)  |
\*---------------------------------------------------------*/
class SpatialHash3D
{
public:
    SpatialHash3D(float cell_size = 1.0f);
    ~SpatialHash3D();

    void Clear();
    void Insert(LEDPosition3D* led_pos);
    void Build(const std::vector<std::unique_ptr<ControllerTransform>>& transforms);

    /*---------------------------------------------------------*\
    | Query nearby LEDs within radius                          |
    \*---------------------------------------------------------*/
    std::vector<LEDPosition3D*> QueryRadius(float x, float y, float z, float radius);

    /*---------------------------------------------------------*\
    | Find single nearest LED                                  |
    \*---------------------------------------------------------*/
    LEDPosition3D* FindNearest(float x, float y, float z);

private:
    struct Cell
    {
        std::vector<LEDPosition3D*> leds;
    };

    /*---------------------------------------------------------*\
    | Hash function for 3D grid cell                          |
    \*---------------------------------------------------------*/
    int64_t HashCell(int x, int y, int z) const;
    void GetCellCoords(float x, float y, float z, int& cx, int& cy, int& cz) const;
    float DistanceSquared(float x1, float y1, float z1, float x2, float y2, float z2) const;

    float cell_size;
    std::unordered_map<int64_t, Cell> grid;
};

#endif
