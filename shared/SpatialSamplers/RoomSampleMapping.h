// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ROOMSAMPLEMAPPING_H
#define ROOMSAMPLEMAPPING_H

#include "Game/GameTelemetryBridge.h"
#include "RGBController.h"

namespace RoomSampleMapping
{

RGBColor SampleAtRoomGrid(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                          float grid_x,
                          float grid_y,
                          float grid_z,
                          bool* out_got_sample);

}

#endif
