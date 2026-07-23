// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"
#include "LEDPosition3D.h"
#include "RGBControllerInterface.h"
#include <memory>
#include <string>
#include <vector>

namespace EffectPack
{

struct ApplyStats
{
    int tracks_applied = 0;
    int controllers_touched = 0;
    int viewport_leds_painted = 0;
};

/** Match helper: empty needle matches anything; otherwise case-insensitive contains. */
bool NameMatches(const std::string& haystack, const std::string& needle);

bool ControllerMatchesDevice(RGBControllerInterface* c, const std::string& device_name);
int FindZoneIndex(RGBControllerInterface* c, const std::string& zone_name);

/**
 * Apply current local-time colours from a pack onto OpenRGB controllers.
 * Optionally also paints matching LEDs in the 3D scene (`preview_color`).
 */
ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms = nullptr);

/** Put controllers into Direct/Custom so SetColor sticks during preview. */
void PrepareControllersForPreview(const std::vector<RGBControllerInterface*>& controllers);

} // namespace EffectPack
