/*---------------------------------------------------------*\
| SpatialGrid3D.cpp                                         |
|                                                           |
|   3D Grid Layout System for LED devices                  |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpatialGrid3D.h"

SpatialGrid3D::SpatialGrid3D(unsigned int width, unsigned int height, unsigned int depth)
{
    grid_width  = width;
    grid_height = height;
    grid_depth  = depth;
}

SpatialGrid3D::~SpatialGrid3D()
{
    for(unsigned int i = 0; i < devices.size(); i++)
    {
        delete devices[i];
    }
    devices.clear();
    device_map.clear();
}

void SpatialGrid3D::SetGridDimensions(unsigned int width, unsigned int height, unsigned int depth)
{
    grid_width  = width;
    grid_height = height;
    grid_depth  = depth;
}

void SpatialGrid3D::GetGridDimensions(unsigned int* width, unsigned int* height, unsigned int* depth)
{
    *width  = grid_width;
    *height = grid_height;
    *depth  = grid_depth;
}

bool SpatialGrid3D::AddDevice(RGBController* controller, GridPosition pos)
{
    if(!IsPositionValid(pos))
    {
        return false;
    }

    if(IsPositionOccupied(pos))
    {
        return false;
    }

    if(device_map.find(controller) != device_map.end())
    {
        return false;
    }

    DeviceGridEntry* entry = new DeviceGridEntry();
    entry->controller = controller;
    entry->position = pos;
    entry->enabled = true;

    devices.push_back(entry);
    device_map[controller] = entry;

    return true;
}

bool SpatialGrid3D::RemoveDevice(RGBController* controller)
{
    if(device_map.find(controller) == device_map.end())
    {
        return false;
    }

    DeviceGridEntry* entry = device_map[controller];

    for(unsigned int i = 0; i < devices.size(); i++)
    {
        if(devices[i] == entry)
        {
            devices.erase(devices.begin() + i);
            break;
        }
    }

    device_map.erase(controller);
    delete entry;

    return true;
}

bool SpatialGrid3D::MoveDevice(RGBController* controller, GridPosition pos)
{
    if(!IsPositionValid(pos))
    {
        return false;
    }

    if(device_map.find(controller) == device_map.end())
    {
        return false;
    }

    DeviceGridEntry* existing_at_pos = GetDeviceAt(pos);
    if(existing_at_pos != nullptr && existing_at_pos->controller != controller)
    {
        return false;
    }

    DeviceGridEntry* entry = device_map[controller];
    entry->position = pos;

    return true;
}

DeviceGridEntry* SpatialGrid3D::GetDeviceAt(GridPosition pos)
{
    for(unsigned int i = 0; i < devices.size(); i++)
    {
        if(devices[i]->position.x == pos.x &&
           devices[i]->position.y == pos.y &&
           devices[i]->position.z == pos.z)
        {
            return devices[i];
        }
    }
    return nullptr;
}

GridPosition SpatialGrid3D::GetDevicePosition(RGBController* controller)
{
    GridPosition pos = {0, 0, 0};

    if(device_map.find(controller) != device_map.end())
    {
        pos = device_map[controller]->position;
    }

    return pos;
}

bool SpatialGrid3D::IsPositionOccupied(GridPosition pos)
{
    return GetDeviceAt(pos) != nullptr;
}

bool SpatialGrid3D::IsPositionValid(GridPosition pos)
{
    return (pos.x < grid_width && pos.y < grid_height && pos.z < grid_depth);
}

std::vector<DeviceGridEntry*> SpatialGrid3D::GetAllDevices()
{
    return devices;
}

std::vector<DeviceGridEntry*> SpatialGrid3D::GetDevicesInRange(GridPosition min, GridPosition max)
{
    std::vector<DeviceGridEntry*> range_devices;

    for(unsigned int i = 0; i < devices.size(); i++)
    {
        GridPosition pos = devices[i]->position;

        if(pos.x >= min.x && pos.x <= max.x &&
           pos.y >= min.y && pos.y <= max.y &&
           pos.z >= min.z && pos.z <= max.z)
        {
            range_devices.push_back(devices[i]);
        }
    }

    return range_devices;
}