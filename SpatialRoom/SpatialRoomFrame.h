// SPDX-License-Identifier: GPL-2.0-only
// Per-frame room evaluation state (overlay sampling, depth preset).

#ifndef SPATIALROOMFRAME_H
#define SPATIALROOMFRAME_H

#include "SpatialRoomTypes.h"

namespace SpatialRoom
{

const SpatialRoomFrameContext& CurrentFrameContext();

void BeginEffectRenderFrame(std::uint64_t render_sequence,
                            SpatialRoomDepthPreset preset = SpatialRoomDepthPreset::Standard);
void EndEffectRenderFrame();

void BeginRoomGridOverlayPass();
void EndRoomGridOverlayPass();

bool IsRoomGridOverlayPass();
/** @deprecated Use IsRoomGridOverlayPass(). */
bool ShouldUseOverlayFastPreview();

} // namespace SpatialRoom

#endif
