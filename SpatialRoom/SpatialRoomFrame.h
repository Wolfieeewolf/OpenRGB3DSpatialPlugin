// SPDX-License-Identifier: GPL-2.0-only
// Per-frame room evaluation state (overlay sampling).

#ifndef SPATIALROOMFRAME_H
#define SPATIALROOMFRAME_H

#include "SpatialRoomTypes.h"

namespace SpatialRoom
{

void BeginEffectRenderFrame();
void EndEffectRenderFrame();

void BeginRoomGridOverlayPass();
void EndRoomGridOverlayPass();

bool IsRoomGridOverlayPass();

} // namespace SpatialRoom

#endif
