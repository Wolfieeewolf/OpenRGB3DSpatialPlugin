// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"
#include "RGBControllerInterface.h"
#include <string>
#include <vector>

namespace EffectPack
{

struct ApplyStats
{
    int tracks_applied = 0;
    int controllers_touched = 0;
};

/** Match helper: empty needle matches anything; otherwise case-insensitive contains. */
bool NameMatches(const std::string& haystack, const std::string& needle);

/**
 * Apply current local-time colours from a pack onto OpenRGB controllers.
 * Target kinds: all / device / zone / leds.
 */
ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers);

/** Put controllers into Direct/Custom so SetColor sticks during preview. */
void PrepareControllersForPreview(const std::vector<RGBControllerInterface*>& controllers);

} // namespace EffectPack
