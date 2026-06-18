// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALROOMDEFAULTS_H
#define SPATIALROOMDEFAULTS_H

#include "SpatialRoomTypes.h"

namespace SpatialLighting
{
struct ShadeSettings;
}

namespace SpatialRoom
{

SpatialRoomCapabilities DefaultCapabilitiesForMode(SpatialRoomMode mode);
void ApplyDepthPreset(SpatialRoomCapabilities& caps, SpatialRoomDepthPreset preset);
void ApplyDepthPresetToShadeSettings(SpatialLighting::ShadeSettings& shade, SpatialRoomDepthPreset preset);

/** Suggested effect library category suffix for UI grouping. */
const char* LibraryGroupForMode(SpatialRoomMode mode);

bool ModeUsesSpatialLightingEngine(SpatialRoomMode mode);

} // namespace SpatialRoom

#endif
