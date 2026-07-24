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

/** True if this scene transform participates in the pack (devices filter). */
bool PackIncludesTransform(const Pack& pack, ControllerTransform* transform);

/** Bounding box of world positions for a LED set. Returns false if empty. */
bool ComputeLedWorldBounds(const std::vector<LEDPosition3D*>& leds,
                           float* min_x, float* max_x,
                           float* min_y, float* max_y,
                           float* min_z, float* max_z);

/**
 * Build per-LED axis samples for timeline thumbnails.
 * Bounds are computed per controller (same as ApplyPackFrame), not one pack-wide box.
 */
void BuildSpatialAxesForTarget(const Pack& pack,
                               const Target& target,
                               Direction dir,
                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                               std::vector<float>* out_axes,
                               std::vector<int>* out_seeds);

/**
 * Apply current local-time colours from a pack onto OpenRGB controllers.
 * Optionally also paints matching LEDs in the 3D scene (`preview_color`).
 * Hardware UpdateLEDs is throttled (~45 ms) unless force_hw_update is set
 * (use on preview stop / final frame so devices are not left one tick behind).
 */
ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms = nullptr,
                          bool force_hw_update = false);

/** Put controllers into Direct/Custom so SetColor sticks during preview. */
void PrepareControllersForPreview(const std::vector<RGBControllerInterface*>& controllers);

} // namespace EffectPack
