// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"
#include "LEDPosition3D.h"
#include "RGBControllerInterface.h"
#include <memory>
#include <string>
#include <vector>

class ZoneManager3D;

namespace EffectPack
{

struct ApplyStats
{
    int tracks_applied = 0;
    int controllers_touched = 0;
    int viewport_leds_painted = 0;
};

bool NameMatches(const std::string& haystack, const std::string& needle);
bool ControllerMatchesDevice(RGBControllerInterface* c, const std::string& device_name);
int FindZoneIndex(RGBControllerInterface* c, const std::string& zone_name);
bool PackIncludesTransform(const Pack& pack, ControllerTransform* transform);
bool TransformMatchesDevice(ControllerTransform* transform, const std::string& device_name);
bool TransformInSceneZone(ControllerTransform* transform,
                          int transform_index,
                          const Target& target,
                          ZoneManager3D* zone_manager);
bool TrackAppliesToTransform(const Pack& pack,
                             const Track& track,
                             ControllerTransform* transform,
                             int transform_index,
                             ZoneManager3D* zone_manager);

void BuildSpatialAxesForTarget(const Pack& pack,
                               const Target& target,
                               const Block& sample,
                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                               std::vector<float>* out_axes,
                               std::vector<int>* out_seeds,
                               std::vector<float>* out_nx = nullptr,
                               std::vector<float>* out_ny = nullptr,
                               std::vector<float>* out_nz = nullptr,
                               ZoneManager3D* zone_manager = nullptr);

ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms = nullptr,
                          bool force_hw_update = false,
                          ZoneManager3D* zone_manager = nullptr);

void PrepareControllersForPreview(const std::vector<RGBControllerInterface*>& controllers);

} // namespace EffectPack
