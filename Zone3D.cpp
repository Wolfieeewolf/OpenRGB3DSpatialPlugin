// SPDX-License-Identifier: GPL-2.0-only

#include "Zone3D.h"
#include <algorithm>

Zone3D::Zone3D(const std::string& name)
    : zone_name(name)
{
}

Zone3D::~Zone3D()
{
}

void Zone3D::SetName(const std::string& name)
{
    zone_name = name;
}

void Zone3D::AddController(int controller_idx)
{
    if(!ContainsController(controller_idx))
    {
        controller_indices.push_back(controller_idx);
    }
}

void Zone3D::RemoveController(int controller_idx)
{
    std::vector<int>::iterator it = std::find(controller_indices.begin(), controller_indices.end(), controller_idx);
    if(it != controller_indices.end())
    {
        controller_indices.erase(it);
    }
}

void Zone3D::ClearControllers()
{
    controller_indices.clear();
}

bool Zone3D::ContainsController(int controller_idx) const
{
    return std::find(controller_indices.begin(), controller_indices.end(), controller_idx)
           != controller_indices.end();
}

nlohmann::json Zone3D::ToJSON() const
{
    nlohmann::json json;
    json["name"] = zone_name;
    json["controllers"] = controller_indices;
    return json;
}

Zone3D* Zone3D::FromJSON(const nlohmann::json& json)
{
    std::string name = json.value("name", "Unnamed Zone");
    Zone3D* zone = new Zone3D(name);

    if(json.contains("controllers") && json["controllers"].is_array())
    {
        const nlohmann::json& controllers_array = json["controllers"];
        for(size_t i = 0; i < controllers_array.size(); i++)
        {
            zone->AddController(controllers_array[i].get<int>());
        }
    }

    return zone;
}
