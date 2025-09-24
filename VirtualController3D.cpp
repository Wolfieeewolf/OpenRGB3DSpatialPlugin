/*---------------------------------------------------------*\
| VirtualController3D.cpp                                   |
|                                                           |
|   Virtual controller for custom 3D LED layouts           |
|                                                           |
|   Date: 2025-09-24                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "VirtualController3D.h"

VirtualController3D::VirtualController3D(const std::string& name,
                                         int width, int height, int depth,
                                         const std::vector<GridLEDMapping>& mappings)
    : name(name),
      width(width),
      height(height),
      depth(depth),
      led_mappings(mappings)
{
}

VirtualController3D::~VirtualController3D()
{
}

std::vector<LEDPosition3D> VirtualController3D::GenerateLEDPositions()
{
    std::vector<LEDPosition3D> positions;

    for(const auto& mapping : led_mappings)
    {
        if(mapping.controller == nullptr)
        {
            continue;
        }

        if(mapping.zone_idx >= mapping.controller->zones.size())
        {
            continue;
        }

        LEDPosition3D pos;
        pos.controller = mapping.controller;
        pos.zone_idx = mapping.zone_idx;
        pos.led_idx = mapping.led_idx;
        pos.local_position.x = (float)mapping.x;
        pos.local_position.y = (float)mapping.y;
        pos.local_position.z = (float)mapping.z;
        pos.world_position = pos.local_position;
        positions.push_back(pos);
    }

    return positions;
}

void VirtualController3D::UpdateColors(std::vector<RGBController*>& controllers)
{
    for(const auto& mapping : led_mappings)
    {
        bool found = false;
        for(unsigned int i = 0; i < controllers.size(); i++)
        {
            if(controllers[i] == mapping.controller)
            {
                unsigned int led_global_idx = controllers[i]->zones[mapping.zone_idx].start_idx + mapping.led_idx;
                if(led_global_idx < controllers[i]->colors.size())
                {
                    found = true;
                }
                break;
            }
        }
    }
}

json VirtualController3D::ToJson() const
{
    json j;
    j["name"] = name;
    j["width"] = width;
    j["height"] = height;
    j["depth"] = depth;

    json mappings_array = json::array();
    for(const auto& mapping : led_mappings)
    {
        json m;
        m["x"] = mapping.x;
        m["y"] = mapping.y;
        m["z"] = mapping.z;
        m["controller_name"] = mapping.controller->name;
        m["controller_location"] = mapping.controller->location;
        m["zone_idx"] = mapping.zone_idx;
        m["led_idx"] = mapping.led_idx;
        mappings_array.push_back(m);
    }
    j["mappings"] = mappings_array;

    return j;
}

VirtualController3D* VirtualController3D::FromJson(const json& j, std::vector<RGBController*>& controllers)
{
    std::string name = j["name"];
    int width = j["width"];
    int height = j["height"];
    int depth = j["depth"];

    std::vector<GridLEDMapping> mappings;

    for(const auto& m : j["mappings"])
    {
        std::string ctrl_name = m["controller_name"];
        std::string ctrl_location = m["controller_location"];

        RGBController* found_controller = nullptr;
        for(unsigned int i = 0; i < controllers.size(); i++)
        {
            if(controllers[i]->name == ctrl_name && controllers[i]->location == ctrl_location)
            {
                found_controller = controllers[i];
                break;
            }
        }

        if(found_controller)
        {
            GridLEDMapping mapping;
            mapping.x = m["x"];
            mapping.y = m["y"];
            mapping.z = m["z"];
            mapping.controller = found_controller;
            mapping.zone_idx = m["zone_idx"];
            mapping.led_idx = m["led_idx"];
            mappings.push_back(mapping);
        }
    }

    if(!mappings.empty())
    {
        return new VirtualController3D(name, width, height, depth, mappings);
    }

    return nullptr;
}