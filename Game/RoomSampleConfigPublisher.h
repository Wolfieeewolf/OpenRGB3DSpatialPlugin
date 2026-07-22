// SPDX-License-Identifier: GPL-2.0-only

#ifndef ROOMSAMPLECONFIGPUBLISHER_H
#define ROOMSAMPLECONFIGPUBLISHER_H

#include "RoomSampleFrameProtocol.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct GridContext3D;

namespace MinecraftGame
{
struct Settings;
}

namespace RoomSampleConfigPublisher
{

/**
 * Global room grid used for Room VR sizing / LED→cubemap mapping.
 * Must match LED room_position space (not a zone-local effect grid).
 */
void SetPublishRoomGrid(const GridContext3D& room_grid);

/** xyz triplets in room space for every LED that Room VR should cover this frame. */
void SetFrameLedRoomPositions(const float* xyz_triplets, std::size_t triplet_count);

void PublishIfNeeded(const GridContext3D& grid,
                     const MinecraftGame::Settings& settings,
                     float effect_origin_x,
                     float effect_origin_y,
                     float effect_origin_z,
                     float room_to_world_scale);

void Disable();

bool GetLastPublishedConfig(RoomSampleFrameProtocol::ConfigHeader& out);

const std::vector<std::uint32_t>& GetLastImportantCells();

}

#endif
