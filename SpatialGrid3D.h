/*---------------------------------------------------------*\
| SpatialGrid3D.h                                           |
|                                                           |
|   3D Grid Layout System for LED devices                  |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SPATIALGRID3D_H
#define SPATIALGRID3D_H

#include <vector>
#include <map>
#include <string>
#include "RGBController.h"

struct GridPosition
{
    unsigned int x;
    unsigned int y;
    unsigned int z;
};

struct DeviceGridEntry
{
    RGBController*  controller;
    GridPosition    position;
    bool            enabled;
};

class SpatialGrid3D
{
public:
    SpatialGrid3D(unsigned int width, unsigned int height, unsigned int depth);
    ~SpatialGrid3D();

    void                SetGridDimensions(unsigned int width, unsigned int height, unsigned int depth);
    void                GetGridDimensions(unsigned int* width, unsigned int* height, unsigned int* depth);

    bool                AddDevice(RGBController* controller, GridPosition pos);
    bool                RemoveDevice(RGBController* controller);
    bool                MoveDevice(RGBController* controller, GridPosition pos);

    DeviceGridEntry*    GetDeviceAt(GridPosition pos);
    GridPosition        GetDevicePosition(RGBController* controller);

    bool                IsPositionOccupied(GridPosition pos);
    bool                IsPositionValid(GridPosition pos);

    std::vector<DeviceGridEntry*> GetAllDevices();
    std::vector<DeviceGridEntry*> GetDevicesInRange(GridPosition min, GridPosition max);

private:
    unsigned int                        grid_width;
    unsigned int                        grid_height;
    unsigned int                        grid_depth;

    std::map<RGBController*, DeviceGridEntry*> device_map;
    std::vector<DeviceGridEntry*>       devices;
};

#endif