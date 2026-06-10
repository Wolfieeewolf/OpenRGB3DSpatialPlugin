// SPDX-License-Identifier: GPL-2.0-only

#include "VirtualController3D.h"

#include "CustomControllerMappingUtils.h"
#include "GridSpaceUtils.h"

#include <algorithm>
#include <stdexcept>

namespace
{
constexpr const char* kCustomControllerFormat = "OpenRGB3DSpatialCustomController";
constexpr int         kCustomControllerVersion = 1;

void ValidateCustomControllerDocument(const json& j)
{
    if(j.value("format", std::string()) != kCustomControllerFormat)
    {
        throw std::runtime_error("custom controller JSON: invalid or missing format field");
    }
    if(!j.contains("version") || !j["version"].is_number_integer()
       || j["version"].get<int>() != kCustomControllerVersion)
    {
        throw std::runtime_error("custom controller JSON: unsupported version (expected 1)");
    }

    static const char* kRequiredKeys[] = {
        "name", "width", "height", "depth", "spacing_mm_x", "spacing_mm_y", "spacing_mm_z", "mappings"};
    for(const char* key : kRequiredKeys)
    {
        if(!j.contains(key))
        {
            throw std::runtime_error(std::string("custom controller JSON: missing field: ") + key);
        }
    }
    if(!j["mappings"].is_array())
    {
        throw std::runtime_error("custom controller JSON: mappings must be an array");
    }
}

std::string TrimCopy(const std::string& text)
{
    size_t start = 0;
    while(start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
    {
        start++;
    }
    size_t end = text.size();
    while(end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
    {
        end--;
    }
    return text.substr(start, end - start);
}

bool StartsWithInsensitive(const std::string& haystack, const std::string& prefix)
{
    if(prefix.empty() || haystack.size() < prefix.size())
    {
        return false;
    }
    for(size_t i = 0; i < prefix.size(); i++)
    {
        if(std::tolower(static_cast<unsigned char>(haystack[i]))
           != std::tolower(static_cast<unsigned char>(prefix[i])))
        {
            return false;
        }
    }
    return true;
}

std::string DeviceTypeToPresetCategory(device_type type)
{
    switch(type)
    {
    case DEVICE_TYPE_GPU:
        return "graphics_cards";
    case DEVICE_TYPE_COOLER:
        return "cpu_cooler";
    case DEVICE_TYPE_MOUSE:
    case DEVICE_TYPE_MOUSEMAT:
        return "mouses";
    case DEVICE_TYPE_MOTHERBOARD:
        return "motherboard";
    case DEVICE_TYPE_LEDSTRIP:
    case DEVICE_TYPE_LIGHT:
        return "ledstrips";
    case DEVICE_TYPE_KEYBOARD:
    case DEVICE_TYPE_KEYPAD:
        return "keyboards";
    case DEVICE_TYPE_HEADSET:
    case DEVICE_TYPE_HEADSET_STAND:
        return "headsets";
    case DEVICE_TYPE_CASE:
        return "cases";
    case DEVICE_TYPE_MONITOR:
        return "monitors";
    default:
        break;
    }

    std::string slug = device_type_to_str(type);
    for(char& c : slug)
    {
        if(c == ' ')
        {
            c = '_';
        }
        else
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return slug;
}

std::string InferPresetModel(RGBController* controller)
{
    if(!controller)
    {
        return std::string();
    }

    const std::string name        = TrimCopy(controller->GetName());
    const std::string description = TrimCopy(controller->GetDescription());
    const std::string vendor      = TrimCopy(controller->GetVendor());

    if(!description.empty() && description != name)
    {
        return description;
    }

    if(!vendor.empty() && StartsWithInsensitive(name, vendor))
    {
        std::string remainder = TrimCopy(name.substr(vendor.size()));
        if(!remainder.empty() && (remainder[0] == ' ' || remainder[0] == '-' || remainder[0] == ':'))
        {
            remainder = TrimCopy(remainder.substr(1));
        }
        if(!remainder.empty())
        {
            return remainder;
        }
    }

    return name;
}

RGBController* PrimaryMappedController(const std::vector<GridLEDMapping>& mappings)
{
    RGBController* primary = nullptr;
    for(const GridLEDMapping& mapping : mappings)
    {
        if(!mapping.controller)
        {
            continue;
        }
        if(!primary)
        {
            primary = mapping.controller;
        }
        else if(primary != mapping.controller)
        {
            return nullptr;
        }
    }
    return primary;
}
}

VirtualController3D::VirtualController3D(const std::string& name,
                                         int width,
                                         int height,
                                         int depth,
                                         const std::vector<GridLEDMapping>& mappings,
                                         float spacing_x,
                                         float spacing_y,
                                         float spacing_z,
                                         const std::vector<CustomControllerLightBlocker>& blockers)
    : name(name),
      width(width),
      height(height),
      depth(depth),
      spacing_mm_x(spacing_x),
      spacing_mm_y(spacing_y),
      spacing_mm_z(spacing_z),
      led_mappings(mappings),
      light_blockers(blockers)
{
}

VirtualController3D::~VirtualController3D() = default;

std::vector<LEDPosition3D> VirtualController3D::GenerateLEDPositions(float grid_scale_mm,
                                                                     float spacing_x_mm,
                                                                     float spacing_y_mm,
                                                                     float spacing_z_mm)
{
    std::vector<LEDPosition3D> positions;

    const float use_x = spacing_x_mm >= 0.0f ? spacing_x_mm : spacing_mm_x;
    const float use_y = spacing_y_mm >= 0.0f ? spacing_y_mm : spacing_mm_y;
    const float use_z = spacing_z_mm >= 0.0f ? spacing_z_mm : spacing_mm_z;

    float scale_x = (use_x > 0.001f) ? MMToGridUnits(use_x, grid_scale_mm) : 1.0f;
    float scale_y = (use_y > 0.001f) ? MMToGridUnits(use_y, grid_scale_mm) : 1.0f;
    float scale_z = (use_z > 0.001f) ? MMToGridUnits(use_z, grid_scale_mm) : 1.0f;

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

        if(pos.zone_idx >= pos.controller->zones.size())
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

void VirtualController3D::RebindControllerPointers(std::vector<RGBController*>& controllers)
{
    CustomControllerMapping::RebindAll(led_mappings, controllers);
}

json VirtualController3D::ToJson() const
{
    json j;
    j["format"]  = kCustomControllerFormat;
    j["version"] = kCustomControllerVersion;
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
        else if(!led_mappings[i].controller_name.empty())
        {
            m["controller_name"] = led_mappings[i].controller_name;
            m["controller_location"] = led_mappings[i].controller_location;
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

    if(!light_blockers.empty())
    {
        json blockers_array = json::array();
        for(const CustomControllerLightBlocker& blocker : light_blockers)
        {
            json b;
            b["x"] = blocker.x;
            b["y"] = blocker.y;
            b["z"] = blocker.z;
            blockers_array.push_back(b);
        }
        j["light_blockers"] = blockers_array;
    }

    return j;
}

json VirtualController3D::ToPortablePresetJson() const
{
    json j = ToJson();

    for(json& mapping : j["mappings"])
    {
        const std::string ctrl_name = mapping.value("controller_name", std::string());
        if(!ctrl_name.empty() && ctrl_name != "Unknown (not found on this system)")
        {
            mapping["controller_location"] = "1:1";
        }
    }

    if(RGBController* primary = PrimaryMappedController(led_mappings))
    {
        const std::string vendor = TrimCopy(primary->GetVendor());
        const std::string model  = InferPresetModel(primary);
        if(!vendor.empty())
        {
            j["brand"] = vendor;
        }
        if(!model.empty())
        {
            j["model"] = model;
        }
        j["category"] = DeviceTypeToPresetCategory(primary->type);
    }

    return j;
}

std::string VirtualController3D::PresetFilenameSlug(const std::string& layout_name)
{
    std::string slug;
    slug.reserve(layout_name.size());
    bool last_was_sep = false;

    for(unsigned char ch : layout_name)
    {
        if(std::isalnum(ch))
        {
            slug += static_cast<char>(std::tolower(ch));
            last_was_sep = false;
        }
        else if(ch == ' ' || ch == '-' || ch == '_' || ch == '.')
        {
            if(!slug.empty() && !last_was_sep)
            {
                slug += '_';
                last_was_sep = true;
            }
        }
    }

    while(!slug.empty() && slug.back() == '_')
    {
        slug.pop_back();
    }

    return slug.empty() ? std::string("custom_controller") : slug;
}

std::unique_ptr<VirtualController3D> VirtualController3D::FromJson(const json& j, std::vector<RGBController*>& controllers)
{
    ValidateCustomControllerDocument(j);

    const int width  = j["width"].get<int>();
    const int height = j["height"].get<int>();
    const int depth  = j["depth"].get<int>();

    const float spacing_x = j["spacing_mm_x"].get<float>();
    const float spacing_y = j["spacing_mm_y"].get<float>();
    const float spacing_z = j["spacing_mm_z"].get<float>();

    const std::string name = j["name"].get<std::string>();
    const json&         mappings_json = j["mappings"];

    std::vector<GridLEDMapping> mappings;
    mappings.reserve(mappings_json.size());

    for(const json& mapping_json : mappings_json)
    {
        const std::string ctrl_name     = mapping_json.at("controller_name").get<std::string>();
        const std::string ctrl_location = mapping_json.at("controller_location").get<std::string>();

        GridLEDMapping mapping;
        mapping.x           = mapping_json.at("x").get<int>();
        mapping.y           = mapping_json.at("y").get<int>();
        mapping.z           = mapping_json.at("z").get<int>();
        mapping.controller_name     = ctrl_name;
        mapping.controller_location = ctrl_location;
        mapping.controller          = CustomControllerMapping::FindByStoredIdentity(controllers, ctrl_name, ctrl_location);
        mapping.zone_idx            = mapping_json.at("zone_idx").get<unsigned int>();
        mapping.led_idx             = mapping_json.at("led_idx").get<unsigned int>();
        mapping.granularity         = mapping_json.at("granularity").get<int>();
        mappings.push_back(mapping);
    }

    if(mappings.empty())
    {
        return nullptr;
    }

    std::vector<CustomControllerLightBlocker> blockers;
    if(j.contains("light_blockers") && j["light_blockers"].is_array())
    {
        for(const json& blocker_json : j["light_blockers"])
        {
            CustomControllerLightBlocker blocker{};
            blocker.x = blocker_json.at("x").get<int>();
            blocker.y = blocker_json.at("y").get<int>();
            blocker.z = blocker_json.at("z").get<int>();
            blockers.push_back(blocker);
        }
    }

    return std::make_unique<VirtualController3D>(name,
                                               width,
                                               height,
                                               depth,
                                               mappings,
                                               spacing_x,
                                               spacing_y,
                                               spacing_z,
                                               blockers);
}

std::vector<GridLEDMapping> VirtualController3D::ImportMappingsFromCoordinateMapJson(
    const json& map_json,
    LedLayoutCoordinateMap::NormalizationMode mode,
    int grid_w,
    int grid_h,
    int grid_d,
    RGBController* controller,
    unsigned int zone_idx,
    int granularity,
    std::string* out_error)
{
    LedLayoutCoordinateMap::ImportResult r =
        LedLayoutCoordinateMap::ImportCoordinateMapForGrid(map_json, mode, grid_w, grid_h, grid_d, controller, zone_idx, granularity);
    if(r.error)
    {
        if(out_error)
        {
            *out_error = *r.error;
        }
        return {};
    }
    return std::move(r.mappings);
}
