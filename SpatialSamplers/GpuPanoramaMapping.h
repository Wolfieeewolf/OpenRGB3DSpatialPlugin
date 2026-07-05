// SPDX-License-Identifier: GPL-2.0-only

#ifndef GPUPANORAMAMAPPING_H
#define GPUPANORAMAMAPPING_H

#include "Game/GameTelemetryBridge.h"
#include "RGBController.h"

namespace GpuPanoramaMapping
{

RGBColor SampleCubemapDirection(const GameTelemetryBridge::GpuPanoramaFrameChannel& pano,
                                float dir_x,
                                float dir_y,
                                float dir_z,
                                bool* out_got_sample);

float EstimateForwardFaceLuminance(const GameTelemetryBridge::GpuPanoramaFrameChannel& pano);

RGBColor SampleRoomPoint(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                         float room_to_world_scale,
                         float heading_offset_deg,
                         float room_x,
                         float room_y,
                         float room_z,
                         float effect_origin_x,
                         float effect_origin_y,
                         float effect_origin_z,
                         float pos_offset_forward_blocks,
                         float pos_offset_right_blocks,
                         float pos_offset_up_blocks,
                         bool* out_got_sample);

}

#endif
