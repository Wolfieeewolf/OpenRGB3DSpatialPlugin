/*---------------------------------------------------------*\
| ControllerLayout3D.h                                      |
|                                                           |
|   Converts OpenRGB controller layouts to 3D positions    |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef CONTROLLERLAYOUT3D_H
#define CONTROLLERLAYOUT3D_H

#include <vector>
#include <unordered_map>
#include <memory>
#include "RGBController.h"
#include "LEDPosition3D.h"

// Forward declaration
struct ControllerTransform;

class ControllerLayout3D
{
public:
    static std::vector<LEDPosition3D> GenerateCustomGridLayout(RGBController* controller, int grid_x, int grid_y, int grid_z);
    static std::vector<LEDPosition3D> GenerateCustomGridLayoutWithSpacing(RGBController* controller, int grid_x, int grid_y, int grid_z, float spacing_mm_x, float spacing_mm_y, float spacing_mm_z, float grid_scale_mm);
    static Vector3D CalculateWorldPosition(Vector3D local_pos, Transform3D transform);
    static void UpdateWorldPositions(ControllerTransform* ctrl_transform);
    static void MarkWorldPositionsDirty(ControllerTransform* ctrl_transform);

    /*---------------------------------------------------------*\
    | Spatial Hash for Fast Nearest-Neighbor Queries           |
    \*---------------------------------------------------------*/
    struct SpatialCell
    {
        std::vector<LEDPosition3D*> leds;
    };

    class SpatialHash
    {
    public:
        SpatialHash(float cell_size = 1.0f);
        void Clear();
        void Insert(LEDPosition3D* led_pos);
        void Build(const std::vector<std::unique_ptr<ControllerTransform>>& transforms);
        std::vector<LEDPosition3D*> QueryRadius(float x, float y, float z, float radius);
        LEDPosition3D* FindNearest(float x, float y, float z);

    private:
        int64_t HashCell(int x, int y, int z) const;
        void GetCellCoords(float x, float y, float z, int& cx, int& cy, int& cz) const;
        float DistanceSquared(float x1, float y1, float z1, float x2, float y2, float z2) const;

        float cell_size;
        std::unordered_map<int64_t, SpatialCell> grid;
    };

private:
};

#endif
