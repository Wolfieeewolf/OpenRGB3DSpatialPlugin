// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialRoomDefaults.h"

namespace SpatialRoom
{

SpatialRoomCapabilities DefaultCapabilitiesForMode(SpatialRoomMode mode)
{
    SpatialRoomCapabilities caps{};
    switch(mode)
    {
    case SpatialRoomMode::EmissiveRelay:
    case SpatialRoomMode::RoomMappedPattern:
        caps.set(CapSkipSampleWarp);
        break;
    case SpatialRoomMode::OriginField:
    default:
        break;
    }
    return caps;
}

} // namespace SpatialRoom
