// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ActionApplier.h"
#include "LEDPosition3D.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Action
{
namespace applier_detail
{

std::string ToLower(std::string s);

bool TryGetGlobalLedIndex(RGBControllerInterface* controller,
                          unsigned int zone_idx,
                          unsigned int led_idx,
                          unsigned int* global_idx);

bool LedIndexInList(const std::vector<int>& indices, int value);

bool PackIncludesController(const Pack& pack, RGBControllerInterface* c);

bool LedMatchesTarget(const LEDPosition3D& led,
                      RGBControllerInterface* fallback_controller,
                      const Target& target);

void ApplyToControllerAll(RGBControllerInterface* c,
                          RGBColor color,
                          std::unordered_set<RGBControllerInterface*>* touched);

void ApplyToZone(RGBControllerInterface* c,
                 int zone,
                 RGBColor color,
                 std::unordered_set<RGBControllerInterface*>* touched);

void ApplyToLeds(RGBControllerInterface* c,
                 const std::vector<int>& indices,
                 RGBColor color,
                 std::unordered_set<RGBControllerInterface*>* touched);

void ApplyColorToMappedLed(const LEDPosition3D& led,
                           RGBControllerInterface* fallback_controller,
                           RGBColor color,
                           std::unordered_set<RGBControllerInterface*>* touched);

/** Write one clip's colour for an LED: blend over what lower tracks painted this frame,
    record its afterglow, and push the result to the mapped controller. */
void PaintClipLed(LEDPosition3D& led,
                  RGBControllerInterface* fallback_controller,
                  const Clip& clip,
                  RGBColor color,
                  int local_ms,
                  std::unordered_set<RGBControllerInterface*>* touched);

/** Afterglow left on an LED at local_ms (black when none). Clears expired or rewound trails. */
RGBColor TrailGlowAt(LEDPosition3D& led, int local_ms);

struct SampleBounds
{
    float min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
    bool valid = false;
    /** Set for RoomBox bounds: lets field clips use the room-grid origin-local mapping. */
    const ApplySpatialContext* room_ctx = nullptr;
};

void ExpandBounds(SampleBounds* b, const Vector3D& p);

using SeqAxesMap = std::unordered_map<ControllerTransform*, std::vector<float>>;

int PaintTransformTargetSpatial(ControllerTransform* transform,
                                const Track& track,
                                int local_ms,
                                std::unordered_set<RGBControllerInterface*>* touched,
                                const SampleBounds* shared_bounds,
                                const std::vector<float>* sequence_axes);

void BuildOrderedSequenceLeds(const Pack& pack,
                              const Track& track,
                              std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                              ZoneManager3D* zone_manager,
                              std::vector<std::pair<ControllerTransform*, LEDPosition3D*>>* ordered);

void BuildSequenceAxesMap(const std::vector<std::pair<ControllerTransform*, LEDPosition3D*>>& ordered,
                          SeqAxesMap* out);

} // namespace applier_detail
} // namespace Action
