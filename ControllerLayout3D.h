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
#include "RGBController.h"
#include "LEDPosition3D.h"

class ControllerLayout3D
{
public:
    static std::vector<LEDPosition3D> GenerateCustomGridLayout(RGBController* controller, int grid_x, int grid_y, int grid_z);
    static Vector3D CalculateWorldPosition(Vector3D local_pos, Transform3D transform);

private:
};

#endif