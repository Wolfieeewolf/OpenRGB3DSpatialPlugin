/*---------------------------------------------------------*\
| ZoneManager3D.cpp                                         |
|                                                           |
|   Manages zones for effect targeting                     |
|                                                           |
|   Date: 2025-10-03                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "ZoneManager3D.h"
#include <algorithm>

ZoneManager3D::ZoneManager3D()
{
}

ZoneManager3D::~ZoneManager3D()
{
    ClearAllZones();
}

Zone3D* ZoneManager3D::CreateZone(const std::string& name)
{
    Zone3D* zone = new Zone3D(name);
    zones.push_back(zone);
    return zone;
}

void ZoneManager3D::DeleteZone(int zone_idx)
{
    if(zone_idx >= 0 && zone_idx < (int)zones.size())
    {
        delete zones[zone_idx];
        zones.erase(zones.begin() + zone_idx);
    }
}

void ZoneManager3D::DeleteZone(const std::string& name)
{
    for(size_t i = 0; i < zones.size(); i++)
    {
        if(zones[i]->GetName() == name)
        {
            delete zones[i];
            zones.erase(zones.begin() + i);
            return;
        }
    }
}

void ZoneManager3D::ClearAllZones()
{
    for(size_t i = 0; i < zones.size(); i++)
    {
        delete zones[i];
    }
    zones.clear();
}

Zone3D* ZoneManager3D::GetZone(int idx)
{
    if(idx >= 0 && idx < (int)zones.size())
    {
        return zones[idx];
    }
    return nullptr;
}

Zone3D* ZoneManager3D::GetZoneByName(const std::string& name)
{
    for(size_t i = 0; i < zones.size(); i++)
    {
        if(zones[i]->GetName() == name)
        {
            return zones[i];
        }
    }
    return nullptr;
}

std::vector<int> ZoneManager3D::GetControllersInZone(const std::string& zone_name)
{
    Zone3D* zone = GetZoneByName(zone_name);
    if(zone)
    {
        return zone->GetControllers();
    }
    return std::vector<int>();
}

std::vector<int> ZoneManager3D::GetControllersInZone(int zone_idx)
{
    Zone3D* zone = GetZone(zone_idx);
    if(zone)
    {
        return zone->GetControllers();
    }
    return std::vector<int>();
}

bool ZoneManager3D::ZoneExists(const std::string& name) const
{
    for(size_t i = 0; i < zones.size(); i++)
    {
        if(zones[i]->GetName() == name)
        {
            return true;
        }
    }
    return false;
}

nlohmann::json ZoneManager3D::ToJSON() const
{
    nlohmann::json json;
    nlohmann::json zones_array = nlohmann::json::array();

    for(size_t i = 0; i < zones.size(); i++)
    {
        zones_array.push_back(zones[i]->ToJSON());
    }

    json["zones"] = zones_array;
    return json;
}

void ZoneManager3D::FromJSON(const nlohmann::json& json)
{
    ClearAllZones();

    if(json.contains("zones") && json["zones"].is_array())
    {
        const nlohmann::json& zones_array = json["zones"];
        for(size_t i = 0; i < zones_array.size(); i++)
        {
            Zone3D* zone = Zone3D::FromJSON(zones_array[i]);
            zones.push_back(zone);
        }
    }
}
