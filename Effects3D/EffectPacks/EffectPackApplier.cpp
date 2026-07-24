// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackApplier.h"
#include "ControllerLayout3D.h"
#include "VirtualController3D.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace EffectPack
{
namespace
{

std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool TryGetGlobalLedIndex(RGBControllerInterface* controller,
                          unsigned int zone_idx,
                          unsigned int led_idx,
                          unsigned int* global_idx)
{
    if(!controller || !global_idx)
    {
        return false;
    }
    if(zone_idx >= controller->GetZoneCount())
    {
        return false;
    }
    if(led_idx >= controller->GetZoneLEDsCount(zone_idx))
    {
        return false;
    }
    *global_idx = controller->GetZoneStartIndex(zone_idx) + led_idx;
    return (*global_idx < controller->GetLEDCount());
}

bool LedIndexInList(const std::vector<int>& indices, int value)
{
    return std::find(indices.begin(), indices.end(), value) != indices.end();
}

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

bool TransformMatchesDevice(ControllerTransform* transform, const std::string& device_name)
{
    if(!transform)
    {
        return false;
    }
    if(device_name.empty())
    {
        return true;
    }
    if(transform->virtual_controller
       && NameMatches(transform->virtual_controller->GetName(), device_name))
    {
        return true;
    }
    if(transform->controller && ControllerMatchesDevice(transform->controller, device_name))
    {
        return true;
    }
    // Fall back: any LED mapping controller name.
    for(const LEDPosition3D& led : transform->led_positions)
    {
        if(led.controller && ControllerMatchesDevice(led.controller, device_name))
        {
            return true;
        }
    }
    return false;
}

bool PackIncludesController(const Pack& pack, RGBControllerInterface* c)
{
    if(!c)
    {
        return false;
    }
    if(pack.devices.empty())
    {
        return true;
    }
    for(const std::string& device : pack.devices)
    {
        if(ControllerMatchesDevice(c, device))
        {
            return true;
        }
    }
    return false;
}

bool LedMatchesTarget(const LEDPosition3D& led,
                      RGBControllerInterface* fallback_controller,
                      const Target& target)
{
    RGBControllerInterface* mapping = led.controller ? led.controller : fallback_controller;
    if(!mapping)
    {
        return false;
    }

    switch(target.kind)
    {
        case TargetKind::All:
            return true;
        case TargetKind::Device:
            // Caller already filtered the transform (device / virtual name).
            return true;
        case TargetKind::Zone:
        {
            if(target.zone_name.empty())
            {
                return true;
            }
            const int zone = FindZoneIndex(mapping, target.zone_name);
            return (zone >= 0 && (unsigned int)zone == led.zone_idx);
        }
        case TargetKind::Leds:
        {
            unsigned int global_idx = 0;
            if(!TryGetGlobalLedIndex(mapping, led.zone_idx, led.led_idx, &global_idx))
            {
                return false;
            }
            return LedIndexInList(target.led_indices, (int)global_idx);
        }
        default:
        {
            const TargetKind unused = target.kind;
            (void)unused;
            return false;
        }
    }
}

void ApplyColorToMappedLed(const LEDPosition3D& led,
                           RGBControllerInterface* fallback_controller,
                           RGBColor color,
                           std::unordered_set<RGBControllerInterface*>* touched)
{
    RGBControllerInterface* mapping = led.controller ? led.controller : fallback_controller;
    if(!mapping || !touched)
    {
        return;
    }
    unsigned int global_idx = 0;
    if(!TryGetGlobalLedIndex(mapping, led.zone_idx, led.led_idx, &global_idx))
    {
        return;
    }
    mapping->SetColor(global_idx, color);
    touched->insert(mapping);
}

int PaintTransformTargetSpatial(ControllerTransform* transform,
                                const Track& track,
                                int local_ms,
                                std::unordered_set<RGBControllerInterface*>* touched)
{
    if(!transform)
    {
        return 0;
    }
    if(transform->world_positions_dirty)
    {
        ControllerLayout3D::UpdateWorldPositions(transform);
    }

    std::vector<LEDPosition3D*> matching;
    matching.reserve(transform->led_positions.size());
    for(LEDPosition3D& led : transform->led_positions)
    {
        if(LedMatchesTarget(led, transform->controller, track.target))
        {
            matching.push_back(&led);
        }
    }
    if(matching.empty())
    {
        return 0;
    }

    const Block* top = FindActiveBlock(track, local_ms);
    float min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
    bool have_bounds = false;
    if(!matching.empty())
    {
        min_x = max_x = matching[0]->world_position.x;
        min_y = max_y = matching[0]->world_position.y;
        min_z = max_z = matching[0]->world_position.z;
        for(LEDPosition3D* led : matching)
        {
            const Vector3D& p = led->world_position;
            min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
            min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
            min_z = std::min(min_z, p.z); max_z = std::max(max_z, p.z);
        }
        have_bounds = true;
    }
    const Direction dir = top ? top->direction : Direction::Right;

    int painted = 0;
    for(int i = 0; i < (int)matching.size(); ++i)
    {
        LEDPosition3D* led = matching[(size_t)i];
        RGBColor color = ToRGBColor(0, 0, 0);
        float intensity = 0.0f;
        bool on = false;
        if(top)
        {
            float axis = 0.0f;
            if(have_bounds)
            {
                axis = WorldAxisPos(dir,
                                    led->world_position.x, led->world_position.y, led->world_position.z,
                                    min_x, max_x, min_y, max_y, min_z, max_z);
            }
            else
            {
                axis = (matching.size() <= 1) ? 0.0f : (float)i / (float)(matching.size() - 1);
                if(dir == Direction::Left || dir == Direction::Up)
                {
                    axis = 1.0f - axis;
                }
            }
            const int seed = (int)(led->zone_idx * 4096u + led->led_idx);
            on = EvaluateBlockAtAxis(*top, local_ms, axis, seed, &color, &intensity);
        }
        if(!on)
        {
            color = ToRGBColor(0, 0, 0);
        }
        led->preview_color = color;
        ApplyColorToMappedLed(*led, transform->controller, color, touched);
        if(on)
        {
            ++painted;
        }
    }
    return painted;
}

} // namespace

bool NameMatches(const std::string& haystack, const std::string& needle)
{
    if(needle.empty())
    {
        return true;
    }
    const std::string h = ToLower(haystack);
    const std::string n = ToLower(needle);
    return h.find(n) != std::string::npos;
}

bool ControllerMatchesDevice(RGBControllerInterface* c, const std::string& device_name)
{
    if(!c)
    {
        return false;
    }
    if(device_name.empty())
    {
        return true;
    }
    return NameMatches(c->GetName(), device_name)
        || NameMatches(c->GetDisplayName(), device_name);
}

int FindZoneIndex(RGBControllerInterface* c, const std::string& zone_name)
{
    if(!c || zone_name.empty())
    {
        return -1;
    }
    const unsigned int zones = c->GetZoneCount();
    for(unsigned int z = 0; z < zones; ++z)
    {
        if(NameMatches(c->GetZoneName(z), zone_name)
           || NameMatches(c->GetZoneDisplayName(z), zone_name))
        {
            return (int)z;
        }
    }
    return -1;
}

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
                          bool force_hw_update)
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
            if(PackIncludesController(pack, c))
            {
                ApplyToControllerAll(c, off, &touched);
            }
        }
    }

    for(const Track& track : pack.tracks)
    {
        if(use_transforms)
        {
            int painted = 0;
            for(std::unique_ptr<ControllerTransform>& transform_ptr : *transforms)
            {
                ControllerTransform* transform = transform_ptr.get();
                if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
                {
                    continue;
                }
                if(track.target.kind != TargetKind::All
                   && !TransformMatchesDevice(transform, track.target.device_name))
                {
                    continue;
                }
                painted += PaintTransformTargetSpatial(transform, track, local_ms, &touched);
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
                    if(PackIncludesController(pack, c))
                    {
                        ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Device:
                for(RGBControllerInterface* c : controllers)
                {
                    if(PackIncludesController(pack, c)
                       && ControllerMatchesDevice(c, track.target.device_name))
                    {
                        ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Zone:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!PackIncludesController(pack, c)
                       || !ControllerMatchesDevice(c, track.target.device_name))
                    {
                        continue;
                    }
                    const int zone = FindZoneIndex(c, track.target.zone_name);
                    if(zone >= 0)
                    {
                        ApplyToZone(c, zone, color, &touched);
                    }
                    else if(track.target.zone_name.empty())
                    {
                        ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Leds:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!PackIncludesController(pack, c)
                       || !ControllerMatchesDevice(c, track.target.device_name))
                    {
                        continue;
                    }
                    ApplyToLeds(c, track.target.led_indices, color, &touched);
                }
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
                ApplyColorToMappedLed(led, transform->controller, off, &touched);
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


bool PackIncludesTransform(const Pack& pack, ControllerTransform* transform)
{
    if(!transform)
    {
        return false;
    }
    if(pack.devices.empty())
    {
        return true;
    }
    for(const std::string& device : pack.devices)
    {
        if(TransformMatchesDevice(transform, device))
        {
            return true;
        }
    }
    return false;
}

bool ComputeLedWorldBounds(const std::vector<LEDPosition3D*>& leds,
                           float* min_x, float* max_x,
                           float* min_y, float* max_y,
                           float* min_z, float* max_z)
{
    if(!min_x || !max_x || !min_y || !max_y || !min_z || !max_z || leds.empty())
    {
        return false;
    }
    *min_x = *max_x = leds.front()->world_position.x;
    *min_y = *max_y = leds.front()->world_position.y;
    *min_z = *max_z = leds.front()->world_position.z;
    for(LEDPosition3D* led : leds)
    {
        const Vector3D& p = led->world_position;
        *min_x = std::min(*min_x, p.x); *max_x = std::max(*max_x, p.x);
        *min_y = std::min(*min_y, p.y); *max_y = std::max(*max_y, p.y);
        *min_z = std::min(*min_z, p.z); *max_z = std::max(*max_z, p.z);
    }
    return true;
}

void BuildSpatialAxesForTarget(const Pack& pack,
                               const Target& target,
                               Direction dir,
                               std::vector<std::unique_ptr<ControllerTransform>>* transforms,
                               std::vector<float>* out_axes,
                               std::vector<int>* out_seeds)
{
    if(!out_axes || !out_seeds)
    {
        return;
    }
    out_axes->clear();
    out_seeds->clear();
    if(!transforms)
    {
        return;
    }

    for(std::unique_ptr<ControllerTransform>& transform_ptr : *transforms)
    {
        ControllerTransform* transform = transform_ptr.get();
        if(!transform || transform->hidden_by_virtual || !PackIncludesTransform(pack, transform))
        {
            continue;
        }
        if(target.kind != TargetKind::All
           && !TransformMatchesDevice(transform, target.device_name)
           && !target.device_name.empty())
        {
            continue;
        }
        if(transform->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(transform);
        }

        std::vector<LEDPosition3D*> matching;
        matching.reserve(transform->led_positions.size());
        for(LEDPosition3D& led : transform->led_positions)
        {
            if(LedMatchesTarget(led, transform->controller, target))
            {
                matching.push_back(&led);
            }
        }
        if(matching.empty())
        {
            continue;
        }

        float min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
        if(!ComputeLedWorldBounds(matching, &min_x, &max_x, &min_y, &max_y, &min_z, &max_z))
        {
            continue;
        }
        for(LEDPosition3D* led : matching)
        {
            out_axes->push_back(WorldAxisPos(dir,
                                             led->world_position.x, led->world_position.y, led->world_position.z,
                                             min_x, max_x, min_y, max_y, min_z, max_z));
            out_seeds->push_back((int)(led->zone_idx * 4096u + led->led_idx));
        }
    }
}

} // namespace EffectPack
