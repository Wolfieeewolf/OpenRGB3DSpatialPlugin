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
                          const std::vector<RGBControllerInterface*>& controllers)
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
