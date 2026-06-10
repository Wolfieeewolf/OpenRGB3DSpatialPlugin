// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerMappingUtils.h"

#include "RGBController.h"

#include <cctype>
#include <string>

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

} // namespace

bool IsControllerRegistered(RGBController* controller, const std::vector<RGBController*>& controllers)
{
    if(!controller)
    {
        return false;
    }
    for(RGBController* candidate : controllers)
    {
        if(candidate == controller)
        {
            return true;
        }
    }
    return false;
}

RGBController* FindByStoredIdentity(const std::vector<RGBController*>& controllers,
                                    const std::string& controller_name,
                                    const std::string& controller_location)
{
    if(IsUnknownStoredName(controller_name))
    {
        return nullptr;
    }

    const bool portable = controller_location.empty() || controller_location == "1:1";
    for(RGBController* controller : controllers)
    {
        if(!controller || !StringsEqualCaseInsensitive(controller_name, controller->GetName()))
        {
            continue;
        }
        if(portable)
        {
            return controller;
        }
        if(controller->GetLocation() == controller_location)
        {
            return controller;
        }
    }
    return nullptr;
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

void RebindAll(std::vector<GridLEDMapping>& mappings, std::vector<RGBController*>& controllers)
{
    for(GridLEDMapping& mapping : mappings)
    {
        if(mapping.controller && IsControllerRegistered(mapping.controller, controllers))
        {
            SyncIdentity(mapping);
            continue;
        }

        mapping.controller = nullptr;
        if(!IsUnknownStoredName(mapping.controller_name))
        {
            mapping.controller =
                FindByStoredIdentity(controllers, mapping.controller_name, mapping.controller_location);
        }

        if(mapping.controller)
        {
            SyncIdentity(mapping);
        }
    }
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
