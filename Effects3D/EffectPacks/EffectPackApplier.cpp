// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackApplier.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

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

int PaintViewportLeds(ControllerTransform* transform,
                      const Target& target,
                      RGBColor color)
{
    if(!transform)
    {
        return 0;
    }
    int painted = 0;
    for(LEDPosition3D& led : transform->led_positions)
    {
        RGBControllerInterface* mapping = led.controller ? led.controller : transform->controller;
        if(!mapping)
        {
            continue;
        }

        bool hit = false;
        switch(target.kind)
        {
            case TargetKind::All:
                hit = true;
                break;
            case TargetKind::Device:
                hit = ControllerMatchesDevice(mapping, target.device_name)
                    || (transform->controller && ControllerMatchesDevice(transform->controller, target.device_name));
                break;
            case TargetKind::Zone:
            {
                if(!ControllerMatchesDevice(mapping, target.device_name)
                   && !(transform->controller && ControllerMatchesDevice(transform->controller, target.device_name)))
                {
                    break;
                }
                if(target.zone_name.empty())
                {
                    hit = true;
                    break;
                }
                const int zone = FindZoneIndex(mapping, target.zone_name);
                hit = (zone >= 0 && (unsigned int)zone == led.zone_idx);
                break;
            }
            case TargetKind::Leds:
            {
                if(!ControllerMatchesDevice(mapping, target.device_name)
                   && !(transform->controller && ControllerMatchesDevice(transform->controller, target.device_name)))
                {
                    break;
                }
                unsigned int global_idx = 0;
                if(TryGetGlobalLedIndex(mapping, led.zone_idx, led.led_idx, &global_idx))
                {
                    hit = LedIndexInList(target.led_indices, (int)global_idx);
                }
                break;
            }
            default:
                break;
        }
        if(hit)
        {
            led.preview_color = color;
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
                          std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    ApplyStats stats;
    std::unordered_set<RGBControllerInterface*> touched;

    for(const Track& track : pack.tracks)
    {
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
                    ApplyToControllerAll(c, color, &touched);
                }
                break;
            case TargetKind::Device:
                for(RGBControllerInterface* c : controllers)
                {
                    if(ControllerMatchesDevice(c, track.target.device_name))
                    {
                        ApplyToControllerAll(c, color, &touched);
                    }
                }
                break;
            case TargetKind::Zone:
                for(RGBControllerInterface* c : controllers)
                {
                    if(!ControllerMatchesDevice(c, track.target.device_name))
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
                    if(!ControllerMatchesDevice(c, track.target.device_name))
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

        if(transforms)
        {
            for(std::unique_ptr<ControllerTransform>& transform_ptr : *transforms)
            {
                ControllerTransform* transform = transform_ptr.get();
                if(!transform)
                {
                    continue;
                }
                if(track.target.kind != TargetKind::All && !TransformMatchesDevice(transform, track.target.device_name))
                {
                    continue;
                }
                stats.viewport_leds_painted += PaintViewportLeds(transform, track.target, color);
            }
        }
    }

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
    stats.controllers_touched = (int)touched.size();
    return stats;
}

} // namespace EffectPack
