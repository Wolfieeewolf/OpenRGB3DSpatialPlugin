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
    static std::vector<LEDPosition3D> GenerateLEDPositions(RGBController* controller);

private:
    static std::vector<LEDPosition3D> GenerateMatrixLayout(RGBController* controller, unsigned int zone_idx);
    static std::vector<LEDPosition3D> GenerateLinearLayout(RGBController* controller, unsigned int zone_idx);
    static std::vector<LEDPosition3D> GenerateSingleLayout(RGBController* controller, unsigned int zone_idx);

    static Vector3D CalculateWorldPosition(Vector3D local_pos, Transform3D transform);
};

#endif