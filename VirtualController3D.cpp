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
#include "GridSpaceUtils.h"

VirtualController3D::VirtualController3D(const std::string& name,
                                         int width, int height, int depth,
                                         const std::vector<GridLEDMapping>& mappings,
                                         float spacing_x, float spacing_y, float spacing_z)
    : name(name),
      width(width),
      height(height),
      depth(depth),
      spacing_mm_x(spacing_x),
      spacing_mm_y(spacing_y),
      spacing_mm_z(spacing_z),
      led_mappings(mappings)
{
}

VirtualController3D::~VirtualController3D()
{
}

std::vector<LEDPosition3D> VirtualController3D::GenerateLEDPositions(float grid_scale_mm)
{
    std::vector<LEDPosition3D> positions;

    // Calculate scale factors based on LED spacing and grid scale
    float scale_x = (spacing_mm_x > 0.001f) ? MMToGridUnits(spacing_mm_x, grid_scale_mm) : 1.0f;
    float scale_y = (spacing_mm_y > 0.001f) ? MMToGridUnits(spacing_mm_y, grid_scale_mm) : 1.0f;
    float scale_z = (spacing_mm_z > 0.001f) ? MMToGridUnits(spacing_mm_z, grid_scale_mm) : 1.0f;

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        LEDPosition3D pos{};
        pos.controller = led_mappings[i].controller;
        pos.zone_idx = led_mappings[i].zone_idx;
        pos.led_idx = led_mappings[i].led_idx;

        if(pos.controller && pos.zone_idx >= pos.controller->zones.size())
        {
            continue;
        }

        // Builder axes: X = horizontal, Y = vertical (height), Z = layers (depth)
        // World axes in viewport: X = width (left-right), Y = height (floor-to-ceiling), Z = depth (front-to-back)
        pos.local_position.x = (float)led_mappings[i].x * scale_x;
        pos.local_position.y = (float)led_mappings[i].y * scale_y; // height/up/down
        pos.local_position.z = (float)led_mappings[i].z * scale_z; // depth/front/back
        pos.world_position = pos.local_position;
        pos.effect_world_position = pos.local_position;
        pos.preview_color = 0x00FFFFFF;
        positions.push_back(pos);
    }

    return positions;
}


json VirtualController3D::ToJson() const
{
    json j;
    j["name"] = name;
    j["width"] = width;
    j["height"] = height;
    j["depth"] = depth;
    j["spacing_mm_x"] = spacing_mm_x;
    j["spacing_mm_y"] = spacing_mm_y;
    j["spacing_mm_z"] = spacing_mm_z;

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
        m["granularity"] = led_mappings[i].granularity;
        mappings_array.push_back(m);
    }
    j["mappings"] = mappings_array;

    return j;
}

std::unique_ptr<VirtualController3D> VirtualController3D::FromJson(const json& j, std::vector<RGBController*>& controllers)
{
    std::string name = j["name"];
    int width = j["width"];
    int height = j["height"];
    int depth = j["depth"];

    // Load spacing (with defaults for older files)
    float spacing_x = j.contains("spacing_mm_x") ? j["spacing_mm_x"].get<float>() : 10.0f;
    float spacing_y = j.contains("spacing_mm_y") ? j["spacing_mm_y"].get<float>() : 10.0f;
    float spacing_z = j.contains("spacing_mm_z") ? j["spacing_mm_z"].get<float>() : 10.0f;

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
            // Load granularity (with default for older files)
            mapping.granularity = mappings_json[i].contains("granularity") ? mappings_json[i]["granularity"].get<int>() : 2;
            mappings.push_back(mapping);
        }
    }

    if(!mappings.empty())
    {
        return std::make_unique<VirtualController3D>(name, width, height, depth, mappings, spacing_x, spacing_y, spacing_z);
    }

    return nullptr;
}
