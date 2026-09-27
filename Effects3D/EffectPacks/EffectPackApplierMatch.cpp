// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackApplier.h"
#include "EffectPackApplierDetail.h"
#include "VirtualController3D.h"
#include "ZoneManager3D.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace EffectPack
{
namespace applier_detail
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
        case TargetKind::Device:
        case TargetKind::SceneZone:
            return true;
        case TargetKind::Zone:
        {
            if(target.zone_name.empty())
            {
                return true;
            }
            // Match THIS LED's zone by name — do not FindZoneIndex then compare indices.
            // Timeline rows often store display / "Zone N" labels; FindZoneIndex can miss
            // those or return the wrong first substring hit, which made Zone tracks no-ops.
            const unsigned int z = led.zone_idx;
            if(z < mapping->GetZoneCount())
            {
                const std::string zn = mapping->GetZoneName(z);
                const std::string zd = mapping->GetZoneDisplayName(z);
                if(NameMatches(zn, target.zone_name) || NameMatches(zd, target.zone_name)
                   || NameMatches(target.zone_name, zn) || NameMatches(target.zone_name, zd))
                {
                    return true;
                }
            }
            const std::string synthetic = "Zone " + std::to_string(z);
            if(NameMatches(synthetic, target.zone_name) || NameMatches(target.zone_name, synthetic))
            {
                return true;
            }
            // Null-rgb timeline buckets used a generic "LEDs" label for the only strip.
            return target.zone_name == "LEDs" && mapping->GetZoneCount() <= 1;
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

} // namespace applier_detail

bool NameMatches(const std::string& haystack, const std::string& needle)
{
    if(needle.empty())
    {
        return true;
    }
    const std::string h = applier_detail::ToLower(haystack);
    const std::string n = applier_detail::ToLower(needle);
    return h.find(n) != std::string::npos;
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
    for(const LEDPosition3D& led : transform->led_positions)
    {
        if(led.controller && ControllerMatchesDevice(led.controller, device_name))
        {
            return true;
        }
    }
    return false;
}

bool TransformInSceneZone(ControllerTransform* transform,
                          int transform_index,
                          const Target& target,
                          ZoneManager3D* zone_manager)
{
    (void)transform;
    if(target.kind != TargetKind::SceneZone || !zone_manager || target.scene_zone_name.empty())
    {
        return false;
    }
    Zone3D* zone = zone_manager->GetZoneByName(target.scene_zone_name);
    return zone && zone->ContainsController(transform_index);
}

bool TrackAppliesToTransform(const Pack& pack,
                             const Track& track,
                             ControllerTransform* transform,
                             int transform_index,
                             ZoneManager3D* zone_manager)
{
    if(!transform || !PackIncludesTransform(pack, transform))
    {
        return false;
    }
    switch(track.target.kind)
    {
        case TargetKind::All:
            return true;
        case TargetKind::Device:
        case TargetKind::Zone:
        case TargetKind::Leds:
            return TransformMatchesDevice(transform, track.target.device_name);
        case TargetKind::SceneZone:
            return TransformInSceneZone(transform, transform_index, track.target, zone_manager);
        default:
        {
            const TargetKind unused = track.target.kind;
            (void)unused;
            return false;
        }
    }
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
    auto equals_ci = [](const std::string& a, const std::string& b) {
        return applier_detail::ToLower(a) == applier_detail::ToLower(b);
    };
    // Prefer exact name / display match so short substring hits cannot steal the wrong zone.
    for(unsigned int z = 0; z < zones; ++z)
    {
        if(equals_ci(c->GetZoneName(z), zone_name)
           || equals_ci(c->GetZoneDisplayName(z), zone_name))
        {
            return (int)z;
        }
    }
    for(unsigned int z = 0; z < zones; ++z)
    {
        if(NameMatches(c->GetZoneName(z), zone_name)
           || NameMatches(c->GetZoneDisplayName(z), zone_name)
           || NameMatches(zone_name, c->GetZoneName(z))
           || NameMatches(zone_name, c->GetZoneDisplayName(z)))
        {
            return (int)z;
        }
    }
    // Synthetic labels from the timeline ("Zone 0", …).
    if(zone_name.size() > 5)
    {
        const std::string lower = applier_detail::ToLower(zone_name);
        if(lower.rfind("zone ", 0) == 0)
        {
            try
            {
                const int idx = std::stoi(zone_name.substr(5));
                if(idx >= 0 && (unsigned int)idx < zones)
                {
                    return idx;
                }
            }
            catch(...)
            {
            }
        }
    }
    if(zone_name == "LEDs" && zones == 1)
    {
        return 0;
    }
    return -1;
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

} // namespace EffectPack
