// SPDX-License-Identifier: GPL-2.0-only
// Room evaluation taxonomy: how an effect uses 3D layout (not game probe tinting).

#ifndef SPATIALROOMTYPES_H
#define SPATIALROOMTYPES_H

#include <cstdint>

namespace SpatialRoom
{

/** How an effect interprets room coordinates (library / evaluator family). */
enum class SpatialRoomMode : int
{
    OriginField = 0,
    RoomMappedPattern = 1,
    EmissiveRelay = 2,
};

/** Per-effect coordinate basis (UI on spatial pattern effects). */
enum class SpatialRoomCoordinateMode : int
{
    EffectOrigin = 0,
    RoomMapped = 1,
};

/** Per-effect room output role (UI on spatial pattern effects). */
enum class SpatialRoomOutputRole : int
{
    Direct = 0,
    EmitterRelay = 1,
};

enum SpatialRoomCapability : uint32_t
{
    CapNone = 0,
    /** Do not warp sample XYZ with stack anchor/scale (room-fixed emitters). */
    CapSkipSampleWarp = 1u << 0,
};

struct SpatialRoomCapabilities
{
    uint32_t flags = CapNone;

    bool has(SpatialRoomCapability cap) const { return (flags & cap) != 0; }
    void set(SpatialRoomCapability cap, bool on = true)
    {
        if(on)
        {
            flags |= cap;
        }
        else
        {
            flags &= ~cap;
        }
    }
};

/** Per-frame flags shared by the effect render pass (tab-owned). */
struct SpatialRoomFrameContext
{
    bool room_grid_overlay_pass = false;
};

} // namespace SpatialRoom

#endif
