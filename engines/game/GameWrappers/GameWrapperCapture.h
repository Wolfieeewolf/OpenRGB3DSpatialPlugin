// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMEWRAPPERCAPTURE_H
#define GAMEWRAPPERCAPTURE_H

#include "Game/GameLightingBridge.h"
#include "RGBController.h"

struct GridContext3D;

namespace GameWrapperCapture
{

struct Settings
{
    float room_heading_offset_deg = 0.0f;
    float compass_offset_deg = -45.0f;
    bool discrete_compass_zones = false;
};

RGBColor RenderColor(const GameLightingBridge::LightingSnapshot& snap,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid,
                     const Settings& settings);

} // namespace GameWrapperCapture

#endif
