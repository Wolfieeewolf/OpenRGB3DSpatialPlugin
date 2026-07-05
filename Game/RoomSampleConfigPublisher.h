// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMSAMPLECONFIGPUBLISHER_H
#define ROOMSAMPLECONFIGPUBLISHER_H

#include "RoomSampleFrameProtocol.h"

struct GridContext3D;

namespace MinecraftGame
{
struct Settings;
}

namespace RoomSampleConfigPublisher
{

void PublishIfNeeded(const GridContext3D& grid,
                     const MinecraftGame::Settings& settings,
                     float effect_origin_x,
                     float effect_origin_y,
                     float effect_origin_z,
                     float room_to_world_scale);

void Disable();

bool GetLastPublishedConfig(RoomSampleFrameProtocol::ConfigHeader& out);

}

#endif
