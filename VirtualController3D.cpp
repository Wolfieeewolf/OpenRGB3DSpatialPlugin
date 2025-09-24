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

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        if(led_mappings[i].controller == nullptr)
        {
            continue;
        }

        if(led_mappings[i].zone_idx >= led_mappings[i].controller->zones.size())
        {
            continue;
        }

        LEDPosition3D pos;
        pos.controller = led_mappings[i].controller;
        pos.zone_idx = led_mappings[i].zone_idx;
        pos.led_idx = led_mappings[i].led_idx;
        pos.local_position.x = (float)led_mappings[i].x;
        pos.local_position.y = (float)led_mappings[i].y;
        pos.local_position.z = (float)led_mappings[i].z;
        pos.world_position = pos.local_position;
        positions.push_back(pos);
    }

    return positions;
}

void VirtualController3D::UpdateColors(std::vector<RGBController*>& controllers)
{
    for(unsigned int m = 0; m < led_mappings.size(); m++)
    {
        for(unsigned int i = 0; i < controllers.size(); i++)
        {
            if(controllers[i] == led_mappings[m].controller)
            {
                unsigned int led_global_idx = controllers[i]->zones[led_mappings[m].zone_idx].start_idx + led_mappings[m].led_idx;
                if(led_global_idx < controllers[i]->colors.size())
                {
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
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        json m;
        m["x"] = led_mappings[i].x;
        m["y"] = led_mappings[i].y;
        m["z"] = led_mappings[i].z;
        m["controller_name"] = led_mappings[i].controller->name;
        m["controller_location"] = led_mappings[i].controller->location;
        m["zone_idx"] = led_mappings[i].zone_idx;
        m["led_idx"] = led_mappings[i].led_idx;
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

    json mappings_json = j["mappings"];
    for(unsigned int i = 0; i < mappings_json.size(); i++)
    {
        std::string ctrl_name = mappings_json[i]["controller_name"];
        std::string ctrl_location = mappings_json[i]["controller_location"];

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
            mapping.x = mappings_json[i]["x"];
            mapping.y = mappings_json[i]["y"];
            mapping.z = mappings_json[i]["z"];
            mapping.controller = found_controller;
            mapping.zone_idx = mappings_json[i]["zone_idx"];
            mapping.led_idx = mappings_json[i]["led_idx"];
            mappings.push_back(mapping);
        }
    }

    if(!mappings.empty())
    {
        return new VirtualController3D(name, width, height, depth, mappings);
    }

    return nullptr;
}