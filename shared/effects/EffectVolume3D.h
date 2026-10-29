// SPDX-License-Identifier: GPL-2.0-only

#ifndef EFFECTVOLUME3D_H
#define EFFECTVOLUME3D_H

#include "LEDPosition3D.h"
#include "SpatialEffect3D.h"

/**
 * Controller-like TRS for a stack Spatial effect: room-grid position, Euler
 * rotation (x=pitch, y=yaw, z=roll), and half-extents in grid units (scale).
 * Bidirectionally maps onto SpatialEffect3D offset / rotation / scale fields.
 */
struct EffectVolume3D
{
    Transform3D transform{};
    int         stack_index = -1;

    static EffectVolume3D FromEffect(const SpatialEffect3D& effect,
                                     const GridContext3D& grid,
                                     int stack_index = -1);

    /** Write position/rotation/half-extents back into effect fields. */
    void ApplyToEffect(SpatialEffect3D& effect, const GridContext3D& grid) const;

    /** Half-extents of the effect volume in grid units (from occupancy + axis %). */
    static Vector3D HalfExtentsFromEffect(const SpatialEffect3D& effect,
                                          const GridContext3D& grid);

    /** Eight corners of the oriented box in room/grid space (for wireframe draw). */
    void GetCorners(Vector3D out_corners[8]) const;
};

#endif
