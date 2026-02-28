// SPDX-License-Identifier: GPL-2.0-only

#include "VirtualController3D.h"
#include "GridSpaceUtils.h"
#include <cctype>
#include <algorithm>

namespace
{
std::string ToLower(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for(unsigned char c : s)
        out += static_cast<char>(std::tolower(c));
    return out;
}

bool ControllerNameMatches(const std::string& preset_name, const std::string& actual_name)
{
    if(preset_name.empty() || actual_name.empty())
        return false;
    std::string pa = ToLower(preset_name);
    std::string ac = ToLower(actual_name);
    if(pa == ac)
        return true;
    const size_t min_len = 15;
    if(pa.size() >= min_len && ac.size() >= min_len)
    {
        if(pa.find(ac) != std::string::npos || ac.find(pa) != std::string::npos)
            return true;
    }
    return false;
}

std::string ControllerSearchText(RGBController* c)
{
    std::string t;
    std::string name = c->GetName();
    std::string description = c->GetDescription();
    std::string location = c->GetLocation();
    std::string vendor = c->GetVendor();
    std::string serial = c->GetSerial();
    std::string version = c->GetVersion();
    
    if(!name.empty()) t += name + " ";
    if(!description.empty()) t += description + " ";
    if(!location.empty()) t += location + " ";
    if(!vendor.empty()) t += vendor + " ";
    if(!serial.empty()) t += serial + " ";
    if(!version.empty()) t += version + " ";
    t += device_type_to_str(c->GetDeviceType());
    return ToLower(t);
}

bool ControllerMatchesPreset(RGBController* c, const std::string& ctrl_name, const std::string& ctrl_location,
                             const std::string& preset_model, const std::string& preset_brand,
                             const std::string& preset_brand_model, bool match_location)
{
    std::string c_name = c->GetName();
    std::string c_location = c->GetLocation();
    
    if(ControllerNameMatches(ctrl_name, c_name))
        return !match_location || ctrl_location.empty() || c_location == ctrl_location;
    if(match_location && !ctrl_location.empty() && c_location == ctrl_location)
        return true;
    std::string searchText = ControllerSearchText(c);
    if(ctrl_name.size() >= 4 && searchText.find(ToLower(ctrl_name)) != std::string::npos)
        return !match_location || ctrl_location.empty() || c_location == ctrl_location;
    if(preset_brand_model.size() >= 4 && searchText.find(ToLower(preset_brand_model)) != std::string::npos)
        return !match_location || ctrl_location.empty() || c_location == ctrl_location;
    if(preset_model.size() >= 4 && searchText.find(ToLower(preset_model)) != std::string::npos)
        return !match_location || ctrl_location.empty() || c_location == ctrl_location;
    if(preset_brand.size() >= 2 && searchText.find(ToLower(preset_brand)) != std::string::npos)
        return !match_location || ctrl_location.empty() || c_location == ctrl_location;
    return false;
}
}

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

    float scale_x = (spacing_mm_x > 0.001f) ? MMToGridUnits(spacing_mm_x, grid_scale_mm) : 1.0f;
    float scale_y = (spacing_mm_y > 0.001f) ? MMToGridUnits(spacing_mm_y, grid_scale_mm) : 1.0f;
    float scale_z = (spacing_mm_z > 0.001f) ? MMToGridUnits(spacing_mm_z, grid_scale_mm) : 1.0f;

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        if(!led_mappings[i].controller)
        {
            continue;
        }

        LEDPosition3D pos{};
        pos.controller = led_mappings[i].controller;
        pos.zone_idx = led_mappings[i].zone_idx;
        pos.led_idx = led_mappings[i].led_idx;

        if(pos.zone_idx >= pos.controller->GetZoneCount())
        {
            continue;
        }

        pos.local_position.x = (float)led_mappings[i].x * scale_x;
        pos.local_position.y = (float)led_mappings[i].y * scale_y;
        pos.local_position.z = (float)led_mappings[i].z * scale_z;
        pos.world_position = pos.local_position;
        pos.room_position = pos.local_position;
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
        if(led_mappings[i].controller)
        {
            m["controller_name"] = led_mappings[i].controller->GetName();
            m["controller_location"] = led_mappings[i].controller->GetLocation();
        }
        else
        {
            m["controller_name"] = "Unknown (not found on this system)";
            m["controller_location"] = "";
        }
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

    float spacing_x = j.contains("spacing_mm_x") ? j["spacing_mm_x"].get<float>() : 10.0f;
    float spacing_y = j.contains("spacing_mm_y") ? j["spacing_mm_y"].get<float>() : 10.0f;
    float spacing_z = j.contains("spacing_mm_z") ? j["spacing_mm_z"].get<float>() : 10.0f;

    std::vector<GridLEDMapping> mappings;

    std::string preset_model = j.value("model", std::string());
    std::string preset_brand = j.value("brand", std::string());
    std::string preset_brand_model = preset_brand.empty() ? preset_model : preset_brand + " " + preset_model;

    json mappings_json = j["mappings"];
    for(unsigned int i = 0; i < mappings_json.size(); i++)
    {
        std::string ctrl_name = mappings_json[i]["controller_name"];
        std::string ctrl_location = mappings_json[i].value("controller_location", std::string());
        bool match_location = !ctrl_location.empty() && ctrl_location != "1:1";

        RGBController* found_controller = nullptr;
        for(unsigned int c = 0; c < controllers.size(); c++)
        {
            if(!ControllerMatchesPreset(controllers[c], ctrl_name, ctrl_location,
                                        preset_model, preset_brand, preset_brand_model, match_location))
            {
                continue;
            }
            found_controller = controllers[c];
            break;
        }

        GridLEDMapping mapping;
        mapping.x = mappings_json[i]["x"];
        mapping.y = mappings_json[i]["y"];
        mapping.z = mappings_json[i]["z"];
        mapping.controller = found_controller;
        mapping.zone_idx = mappings_json[i]["zone_idx"];
        mapping.led_idx = mappings_json[i]["led_idx"];
        mapping.granularity = mappings_json[i].contains("granularity") ? mappings_json[i]["granularity"].get<int>() : 2;
        mappings.push_back(mapping);
    }

    if(!mappings.empty())
    {
        return std::make_unique<VirtualController3D>(name, width, height, depth, mappings, spacing_x, spacing_y, spacing_z);
    }

    return nullptr;
}

std::unique_ptr<VirtualController3D> VirtualController3D::FromJsonForController(const json& j, RGBController* controller, const std::string& display_name)
{
    if(!controller || !j.contains("mappings"))
    {
        return nullptr;
    }

    int width = j["width"];
    int height = j["height"];
    int depth = j["depth"];
    float spacing_x = j.contains("spacing_mm_x") ? j["spacing_mm_x"].get<float>() : 10.0f;
    float spacing_y = j.contains("spacing_mm_y") ? j["spacing_mm_y"].get<float>() : 10.0f;
    float spacing_z = j.contains("spacing_mm_z") ? j["spacing_mm_z"].get<float>() : 10.0f;

    std::vector<GridLEDMapping> mappings;
    json mappings_json = j["mappings"];
    for(unsigned int i = 0; i < mappings_json.size(); i++)
    {
        GridLEDMapping mapping;
        mapping.x = mappings_json[i]["x"];
        mapping.y = mappings_json[i]["y"];
        mapping.z = mappings_json[i]["z"];
        mapping.controller = controller;
        mapping.zone_idx = mappings_json[i]["zone_idx"];
        mapping.led_idx = mappings_json[i]["led_idx"];
        mapping.granularity = mappings_json[i].contains("granularity") ? mappings_json[i]["granularity"].get<int>() : 2;
        mappings.push_back(mapping);
    }

    if(mappings.empty())
    {
        return nullptr;
    }

    return std::make_unique<VirtualController3D>(display_name, width, height, depth, mappings, spacing_x, spacing_y, spacing_z);
}
