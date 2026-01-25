/*---------------------------------------------------------*\
| DisplayPlane3D.cpp                                        |
|                                                           |
|   Virtual display plane definition for ambilight mapping |
|                                                           |
|   Date: 2025-10-22                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "DisplayPlane3D.h"
#include <cmath>

int DisplayPlane3D::next_id = 1;

DisplayPlane3D::DisplayPlane3D(const std::string& name_value) :
    id(next_id++),
    name(name_value),
    width_mm(1000.0f),
    height_mm(600.0f),
    visible(true),
    reference_point_index(-1)
{
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = {0.0f, 0.0f, 0.0f};
    transform.scale    = {1.0f, 1.0f, 1.0f};
    monitor_preset_id.clear();
}

nlohmann::json DisplayPlane3D::ToJson() const
{
    nlohmann::json j;
    j["id"]            = id;
    j["name"]          = name;
    j["width_mm"]      = width_mm;
    j["height_mm"]     = height_mm;
    j["visible"]       = visible;
    j["capture_id"]    = capture_source_id;
    j["capture_label"] = capture_label;
    if(!monitor_preset_id.empty())
    {
        j["monitor_preset_id"] = monitor_preset_id;
    }
    if(reference_point_index >= 0)
    {
        j["reference_point_index"] = reference_point_index;
    }

    nlohmann::json t;
    t["position"] = { transform.position.x, transform.position.y, transform.position.z };
    t["rotation"] = { transform.rotation.x, transform.rotation.y, transform.rotation.z };
    t["scale"]    = { transform.scale.x, transform.scale.y, transform.scale.z };
    j["transform"] = t;

    return j;
}

std::unique_ptr<DisplayPlane3D> DisplayPlane3D::FromJson(const nlohmann::json& j)
{
    if(j.is_null())
    {
        return nullptr;
    }

    std::string name_value = j.value("name", "Display Plane");
    std::unique_ptr<DisplayPlane3D> plane = std::make_unique<DisplayPlane3D>(name_value);

    if(j.contains("id"))
    {
        plane->id = j["id"].get<int>();
        if(plane->id >= next_id)
        {
            next_id = plane->id + 1;
        }
    }

    plane->width_mm  = j.value("width_mm", 1000.0f);
    plane->height_mm = j.value("height_mm", 600.0f);
    plane->visible   = j.value("visible", true);
    plane->capture_source_id = j.value("capture_id", std::string());
    plane->capture_label     = j.value("capture_label", std::string());
    plane->monitor_preset_id = j.value("monitor_preset_id", std::string());
    plane->reference_point_index = j.value("reference_point_index", -1);

    if(j.contains("transform"))
    {
        const nlohmann::json& t = j["transform"];
        if(t.contains("position") && t["position"].is_array() && t["position"].size() == 3)
        {
            plane->transform.position.x = t["position"][0].get<float>();
            plane->transform.position.y = t["position"][1].get<float>();
            plane->transform.position.z = t["position"][2].get<float>();
        }
        if(t.contains("rotation") && t["rotation"].is_array() && t["rotation"].size() == 3)
        {
            plane->transform.rotation.x = t["rotation"][0].get<float>();
            plane->transform.rotation.y = t["rotation"][1].get<float>();
            plane->transform.rotation.z = t["rotation"][2].get<float>();
        }
        if(t.contains("scale") && t["scale"].is_array() && t["scale"].size() == 3)
        {
            plane->transform.scale.x = t["scale"][0].get<float>();
            plane->transform.scale.y = t["scale"][1].get<float>();
            plane->transform.scale.z = t["scale"][2].get<float>();
        }
    }

    return plane;
}
