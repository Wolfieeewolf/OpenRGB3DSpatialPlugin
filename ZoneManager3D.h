/*---------------------------------------------------------*\
| ZoneManager3D.h                                           |
|                                                           |
|   Manages zones for effect targeting                     |
|                                                           |
|   Date: 2025-10-03                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef ZONEMANAGER3D_H
#define ZONEMANAGER3D_H

#include "Zone3D.h"
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

/*---------------------------------------------------------*\
| ZoneManager3D - Manages all zones                        |
\*---------------------------------------------------------*/
class ZoneManager3D
{
public:
    ZoneManager3D();
    ~ZoneManager3D();

    /*---------------------------------------------------------*\
    | Zone Management                                          |
    \*---------------------------------------------------------*/
    Zone3D* CreateZone(const std::string& name);
    void DeleteZone(int zone_idx);
    void DeleteZone(const std::string& name);
    void ClearAllZones();

    /*---------------------------------------------------------*\
    | Access                                                   |
    \*---------------------------------------------------------*/
    int GetZoneCount() const { return (int)zones.size(); }
    Zone3D* GetZone(int idx);
    Zone3D* GetZoneByName(const std::string& name);
    const std::vector<Zone3D*>& GetAllZones() const { return zones; }

    /*---------------------------------------------------------*\
    | Query                                                    |
    \*---------------------------------------------------------*/
    std::vector<int> GetControllersInZone(const std::string& zone_name);
    std::vector<int> GetControllersInZone(int zone_idx);
    bool ZoneExists(const std::string& name) const;

    /*---------------------------------------------------------*\
    | Serialization                                            |
    \*---------------------------------------------------------*/
    nlohmann::json ToJSON() const;
    void FromJSON(const nlohmann::json& json);

private:
    std::vector<Zone3D*> zones;
};

#endif
