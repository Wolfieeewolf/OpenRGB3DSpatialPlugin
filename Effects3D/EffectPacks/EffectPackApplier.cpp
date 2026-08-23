// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackApplier.h"
#include "EffectPackApplierDetail.h"

#include <chrono>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace EffectPack
{

namespace applier_detail
{

void ApplyToControllerAll(RGBControllerInterface* c, RGBColor color, std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c)
    {
        return;
    }
    c->SetAllColors(color);
    touched->insert(c);
}

void ApplyToZone(RGBControllerInterface* c, int zone, RGBColor color, std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c || zone < 0 || (unsigned int)zone >= c->GetZoneCount())
    {
        return;
    }
    c->SetAllZoneColors(zone, color);
    touched->insert(c);
}

void ApplyToLeds(RGBControllerInterface* c, const std::vector<int>& indices, RGBColor color,
                 std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!c)
    {
        return;
    }
    const unsigned int led_count = c->GetLEDCount();
    bool any = false;
    for(int idx : indices)
    {
        if(idx < 0 || (unsigned int)idx >= led_count)
        {
            continue;
        }
        c->SetColor((unsigned int)idx, color);
        any = true;
    }
    if(any)
    {
        touched->insert(c);
    }
}

} // namespace applier_detail

void PrepareControllersForPreview(const std::vector<RGBControllerInterface*>& controllers)
{
    for(RGBControllerInterface* c : controllers)
    {
        if(!c)
        {
            continue;
        }
        try
        {
            c->SetCustomMode();
        }
        catch(...)
        {
        }
    }
}

ApplyStats ApplyPackFrame(const Pack& pack,
                          int local_ms,
                          const std::vector<RGBControllerInterface*>& controllers,
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                          bool force_hw_update,
                          ZoneManager3D* zone_manager)
{
    ApplyStats stats;
    std::unordered_set<RGBControllerInterface*> touched;
    const bool use_transforms = transforms && !transforms->empty();
    const RGBColor off = ToRGBColor(0, 0, 0);

    // Viewport clear only — avoid a full hardware black frame every tick (USB hitch).
    if(use_transforms)
    {
        for(std::unique_ptr<ControllerTransform>& transform_ptr : *transforms)
        {
            ControllerTransform* transform = transform_ptr.get();
            if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
            {
                continue;
            }
            for(LEDPosition3D& led : transform->led_positions)
            {
                led.preview_color = off;
            }
        }
    }
    else
    {
        for(RGBControllerInterface* c : controllers)
        {
            if(applier_detail::PackIncludesController(pack, c))
            {
                applier_detail::ApplyToControllerAll(c, off, &touched);
            }
        }
    }

    for(const Track& track : pack.tracks)
    {
        if(use_transforms)
        {
            const Block* top = FindActiveBlock(track, local_ms);
            applier_detail::SampleBounds shared_bounds;
            applier_detail::SeqAxesMap seq_map;
            const bool want_shared = top && BlockUsesSharedWorldBounds(*top);
            const bool want_sequence = top && BlockUsesSequenceAxis(*top);

            if(want_shared || want_sequence)
            {
                std::vector<std::pair<ControllerTransform*, LEDPosition3D*>> ordered;
                applier_detail::BuildOrderedSequenceLeds(pack, track, transforms, zone_manager, &ordered);
                if(want_sequence)
                {
                    applier_detail::BuildSequenceAxesMap(ordered, &seq_map);
                }
                if(want_shared)
                {
                    for(const auto& pair : ordered)
                    {
                        applier_detail::ExpandBounds(&shared_bounds, pair.second->world_position);
                    }
                    // Never fall back to per-device AABB for Room/Sequence-volume on groups.
                    if(!shared_bounds.valid && TargetIsMultiDeviceGroup(track.target))
                    {
                        continue;
                    }
                }
            }

            int painted = 0;
            for(int ti = 0; ti < (int)transforms->size(); ++ti)
            {
                ControllerTransform* transform = (*transforms)[(size_t)ti].get();
                if(!TrackAppliesToTransform(pack, track, transform, ti, zone_manager))
                {
                    continue;
                }
                const std::vector<float>* seq_axes = nullptr;
                auto it = seq_map.find(transform);
                if(it != seq_map.end())
                {
                    seq_axes = &it->second;
                }
                painted += applier_detail::PaintTransformTargetSpatial(transform, track, local_ms, &touched,
                                                       shared_bounds.valid ? &shared_bounds : nullptr,
                                                       seq_axes);
            }
            if(painted > 0)
            {
                ++stats.tracks_applied;
                stats.viewport_leds_painted += painted;
            }
            continue;
        }

        RGBColor color = ToRGBColor(0, 0, 0);
        float intensity = 0.0f;
        if(!EvaluateTrackColor(track, local_ms, &color, &intensity))
        {
            continue;
        }
        ++stats.tracks_applied;

        switch(track.target.kind)
        {
            case TargetKind::All:
                for(RGBControllerInterface* c : controllers)
                {
                    if(applier_detail::PackIncludesController(pack, c))
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Device:
                for(RGBControllerInterface* c : controllers)
                {
                    if(applier_detail::PackIncludesController(pack, c)
                       && ControllerMatchesDevice(c, track.target.device_name))
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Zone:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!applier_detail::PackIncludesController(pack, c)
                       || !ControllerMatchesDevice(c, track.target.device_name))
                    {
                        continue;
                    }
                    const int zone = FindZoneIndex(c, track.target.zone_name);
                    if(zone >= 0)
                    {
                        applier_detail::ApplyToZone(c, zone, color, &touched);
                    }
                    else if(track.target.zone_name.empty())
                    {
                        applier_detail::ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Leds:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!applier_detail::PackIncludesController(pack, c)
                       || !ControllerMatchesDevice(c, track.target.device_name))
                    {
                        continue;
                    }
                    applier_detail::ApplyToLeds(c, track.target.led_indices, color, &touched);
                }
                break;
            case TargetKind::SceneZone:
                // Without transforms, scene zones cannot resolve controller indices.
                break;
            default:
            {
                const TargetKind unused = track.target.kind;
                (void)unused;
                break;
            }
        }
    }

    // Also push blacks for scoped LEDs that were cleared in viewport but not touched by a track.
    if(use_transforms)
    {
        for(std::unique_ptr<ControllerTransform>& transform_ptr : *transforms)
        {
            ControllerTransform* transform = transform_ptr.get();
            if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
            {
                continue;
            }
            for(LEDPosition3D& led : transform->led_positions)
            {
                if(led.preview_color != off)
                {
                    continue;
                }
                applier_detail::ApplyColorToMappedLed(led, transform->controller, off, &touched);
            }
        }
    }

    // Throttle device I/O — SetColor fills buffers every frame; USB flush ~20 Hz.
    static std::int64_t s_last_hw_ms = 0;
    const std::int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const bool flush_hw = force_hw_update || (now_ms - s_last_hw_ms) >= 45;
    if(flush_hw)
    {
        s_last_hw_ms = now_ms;
        for(RGBControllerInterface* c : touched)
        {
            try
            {
                c->UpdateLEDs();
            }
            catch(...)
            {
            }
        }
    }
    stats.controllers_touched = (int)touched.size();
    return stats;
}


} // namespace EffectPack
