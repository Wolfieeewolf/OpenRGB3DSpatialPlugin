// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerMappingUtils.h"

#include "RGBController.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace CustomControllerMapping
{
namespace
{

std::string ToLower(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for(unsigned char c : s)
    {
        out += static_cast<char>(std::tolower(c));
    }
    return out;
}

bool StringsEqualCaseInsensitive(const std::string& a, const std::string& b)
{
    return ToLower(a) == ToLower(b);
}

bool IsUnknownStoredName(const std::string& name)
{
    return name.empty() || name == "Unknown (not found on this system)";
}

bool IsPortableLocation(const std::string& location)
{
    return location.empty() || location == "1:1";
}

bool LooksLikeNetworkLocation(const std::string& location)
{
    return location.find('.') != std::string::npos || location.find(':') != std::string::npos;
}

void CollectNameMatches(const std::vector<RGBControllerInterface*>& controllers,
                        const std::string& controller_name,
                        std::vector<RGBControllerInterface*>& out)
{
    out.clear();
    for(RGBControllerInterface* controller : controllers)
    {
        if(controller && StringsEqualCaseInsensitive(controller_name, controller->GetName()))
        {
            out.push_back(controller);
        }
    }
}

bool MappingStructureValid(const GridLEDMapping& mapping, RGBControllerInterface* controller)
{
    if(!controller)
    {
        return false;
    }
    if(mapping.granularity == 0)
    {
        return true;
    }
    if(mapping.zone_idx >= controller->GetZoneCount())
    {
        return false;
    }
    const zone z = controller->GetZone(mapping.zone_idx);
    if(mapping.granularity == 1)
    {
        return true;
    }
    return mapping.led_idx < z.leds_count;
}

bool ControllerSupportsMappings(RGBControllerInterface* controller,
                                const std::vector<const GridLEDMapping*>& mappings)
{
    if(!controller)
    {
        return false;
    }
    for(const GridLEDMapping* mapping : mappings)
    {
        if(!mapping || !MappingStructureValid(*mapping, controller))
        {
            return false;
        }
    }
    return true;
}

unsigned int MappingMaxGlobalLedIndex(const GridLEDMapping& mapping, RGBControllerInterface* controller)
{
    if(!MappingStructureValid(mapping, controller))
    {
        return 0;
    }
    if(mapping.granularity == 0)
    {
        return (controller->GetLEDCount() == 0) ? 0U : (controller->GetLEDCount() - 1);
    }
    const zone z = controller->GetZone(mapping.zone_idx);
    return z.start_idx + mapping.led_idx;
}

unsigned int GroupMaxGlobalLedIndex(RGBControllerInterface* controller,
                                    const std::vector<const GridLEDMapping*>& mappings)
{
    unsigned int max_idx = 0;
    for(const GridLEDMapping* mapping : mappings)
    {
        if(!mapping)
        {
            continue;
        }
        max_idx = std::max(max_idx, MappingMaxGlobalLedIndex(*mapping, controller));
    }
    return max_idx;
}

RGBControllerInterface* PickBestStructureMatch(const std::vector<RGBControllerInterface*>& candidates,
                                      const std::vector<const GridLEDMapping*>& mappings)
{
    std::vector<RGBControllerInterface*> viable;
    viable.reserve(candidates.size());
    for(RGBControllerInterface* controller : candidates)
    {
        if(ControllerSupportsMappings(controller, mappings))
        {
            viable.push_back(controller);
        }
    }

    if(viable.empty())
    {
        return nullptr;
    }
    if(viable.size() == 1)
    {
        return viable.front();
    }

    RGBControllerInterface* best         = nullptr;
    int            best_distance = -1;
    for(RGBControllerInterface* controller : viable)
    {
        const unsigned int max_used = GroupMaxGlobalLedIndex(controller, mappings);
        const int distance =
            std::abs(static_cast<int>(controller->GetLEDCount()) - static_cast<int>(max_used + 1));
        if(best == nullptr || distance < best_distance)
        {
            best          = controller;
            best_distance = distance;
        }
    }

    int tie_count = 0;
    for(RGBControllerInterface* controller : viable)
    {
        const unsigned int max_used = GroupMaxGlobalLedIndex(controller, mappings);
        const int distance =
            std::abs(static_cast<int>(controller->GetLEDCount()) - static_cast<int>(max_used + 1));
        if(distance == best_distance)
        {
            ++tie_count;
        }
    }

    return tie_count == 1 ? best : nullptr;
}

RGBControllerInterface* UniqueNetworkNameMatch(const std::vector<RGBControllerInterface*>& name_matches,
                                      const std::string& stored_location)
{
    if(!LooksLikeNetworkLocation(stored_location))
    {
        return nullptr;
    }

    RGBControllerInterface* network_match = nullptr;
    for(RGBControllerInterface* controller : name_matches)
    {
        if(!controller || !LooksLikeNetworkLocation(controller->GetLocation()))
        {
            continue;
        }
        if(network_match)
        {
            return nullptr;
        }
        network_match = controller;
    }
    return network_match;
}

std::string GroupStoredLocation(const std::vector<const GridLEDMapping*>& mappings)
{
    for(const GridLEDMapping* mapping : mappings)
    {
        if(mapping && IsPortableLocation(mapping->controller_location))
        {
            return "1:1";
        }
    }

    for(const GridLEDMapping* mapping : mappings)
    {
        if(mapping && !IsPortableLocation(mapping->controller_location))
        {
            return mapping->controller_location;
        }
    }

    return "1:1";
}

} // namespace

bool IsControllerRegistered(RGBControllerInterface* controller, const std::vector<RGBControllerInterface*>& controllers)
{
    if(!controller)
    {
        return false;
    }
    for(RGBControllerInterface* candidate : controllers)
    {
        if(candidate == controller)
        {
            return true;
        }
    }
    return false;
}

RGBControllerInterface* FindControllerForMappings(const std::vector<RGBControllerInterface*>& controllers,
                                           const std::string& controller_name,
                                           const std::string& controller_location,
                                           const std::vector<const GridLEDMapping*>& mappings)
{
    if(IsUnknownStoredName(controller_name))
    {
        return nullptr;
    }

    const bool portable = IsPortableLocation(controller_location);

    std::vector<RGBControllerInterface*> name_matches;
    CollectNameMatches(controllers, controller_name, name_matches);
    if(name_matches.empty())
    {
        return nullptr;
    }

    if(!portable)
    {
        for(RGBControllerInterface* controller : name_matches)
        {
            if(controller->GetLocation() == controller_location)
            {
                if(mappings.empty() || ControllerSupportsMappings(controller, mappings))
                {
                    return controller;
                }
            }
        }
    }

    if(!mappings.empty())
    {
        if(RGBControllerInterface* structure_match = PickBestStructureMatch(name_matches, mappings))
        {
            return structure_match;
        }
    }
    else if(name_matches.size() == 1)
    {
        return name_matches.front();
    }

    if(!portable)
    {
        if(RGBControllerInterface* network_match = UniqueNetworkNameMatch(name_matches, controller_location))
        {
            if(mappings.empty() || ControllerSupportsMappings(network_match, mappings))
            {
                return network_match;
            }
        }
    }

    if(portable && name_matches.size() == 1)
    {
        return name_matches.front();
    }

    return nullptr;
}

RGBControllerInterface* FindByStoredIdentity(const std::vector<RGBControllerInterface*>& controllers,
                                    const std::string& controller_name,
                                    const std::string& controller_location)
{
    return FindControllerForMappings(controllers, controller_name, controller_location, {});
}

bool MappingOwnedByController(const GridLEDMapping& mapping,
                              RGBControllerInterface* controller,
                              const std::vector<RGBControllerInterface*>& controllers)
{
    if(!controller)
    {
        return false;
    }
    if(mapping.controller == controller)
    {
        return true;
    }
    if(mapping.controller)
    {
        return false;
    }

    const std::vector<const GridLEDMapping*> group = {&mapping};
    return FindControllerForMappings(controllers, mapping.controller_name, mapping.controller_location, group)
           == controller;
}

void SyncIdentity(GridLEDMapping& mapping)
{
    if(!mapping.controller)
    {
        return;
    }
    mapping.controller_name = mapping.controller->GetName();
    mapping.controller_location = mapping.controller->GetLocation();
}

void FinalizeMapping(GridLEDMapping& mapping)
{
    SyncIdentity(mapping);
}

bool RebindAll(std::vector<GridLEDMapping>& mappings, std::vector<RGBControllerInterface*>& controllers)
{
    bool changed = false;

    for(GridLEDMapping& mapping : mappings)
    {
        if(mapping.controller && IsControllerRegistered(mapping.controller, controllers))
        {
            const std::string previous_location = mapping.controller_location;
            SyncIdentity(mapping);
            if(mapping.controller_location != previous_location)
            {
                changed = true;
            }
        }
        else
        {
            if(mapping.controller)
            {
                changed = true;
            }
            mapping.controller = nullptr;
        }
    }

    std::unordered_map<std::string, std::vector<size_t>> pending_by_name;
    pending_by_name.reserve(mappings.size());
    for(size_t i = 0; i < mappings.size(); ++i)
    {
        if(mappings[i].controller || IsUnknownStoredName(mappings[i].controller_name))
        {
            continue;
        }
        pending_by_name[ToLower(mappings[i].controller_name)].push_back(i);
    }

    for(const auto& entry : pending_by_name)
    {
        std::vector<const GridLEDMapping*> group_ptrs;
        group_ptrs.reserve(entry.second.size());
        for(size_t index : entry.second)
        {
            group_ptrs.push_back(&mappings[index]);
        }

        const GridLEDMapping& anchor = mappings[entry.second.front()];
        RGBControllerInterface* resolved =
            FindControllerForMappings(controllers,
                                    anchor.controller_name,
                                    GroupStoredLocation(group_ptrs),
                                    group_ptrs);
        if(!resolved)
        {
            continue;
        }

        for(size_t index : entry.second)
        {
            mappings[index].controller = resolved;
            SyncIdentity(mappings[index]);
            changed                    = true;
        }
    }

    return changed;
}

int UnresolvedCount(const std::vector<GridLEDMapping>& mappings)
{
    int count = 0;
    for(const GridLEDMapping& mapping : mappings)
    {
        if(!mapping.controller)
        {
            ++count;
        }
    }
    return count;
}

} // namespace CustomControllerMapping
