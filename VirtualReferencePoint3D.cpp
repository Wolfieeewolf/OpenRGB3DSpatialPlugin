/*---------------------------------------------------------*\
| VirtualReferencePoint3D.cpp                              |
|                                                           |
|   Virtual reference point for 3D spatial effect anchors |
|                                                           |
|   Date: 2025-09-30                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "VirtualReferencePoint3D.h"

int VirtualReferencePoint3D::next_id = 1;

VirtualReferencePoint3D::VirtualReferencePoint3D(const std::string& name, ReferencePointType type, float x, float y, float z)
    : id(next_id++), name(name), type(type), visible(true)
{
    // Initialize transform
    transform.position = {x, y, z};
    transform.rotation = {0.0f, 0.0f, 0.0f};
    transform.scale = {1.0f, 1.0f, 1.0f};

    // Set default color based on type
    display_color = GetDefaultColor(type);
}

VirtualReferencePoint3D::~VirtualReferencePoint3D()
{
}

/*---------------------------------------------------------*\
| Get icon type for viewport rendering                     |
\*---------------------------------------------------------*/
int VirtualReferencePoint3D::GetIconType() const
{
    return (int)type;  // Icon type matches reference point type
}

/*---------------------------------------------------------*\
| Get display name for reference point type               |
\*---------------------------------------------------------*/
const char* VirtualReferencePoint3D::GetTypeName(ReferencePointType type)
{
    switch(type)
    {
        case REF_POINT_USER:         return "User";
        case REF_POINT_MONITOR:      return "Monitor";
        case REF_POINT_CHAIR:        return "Chair";
        case REF_POINT_DESK:         return "Desk";
        case REF_POINT_SPEAKER_LEFT: return "Left Speaker";
        case REF_POINT_SPEAKER_RIGHT: return "Right Speaker";
        case REF_POINT_DOOR:         return "Door";
        case REF_POINT_WINDOW:       return "Window";
        case REF_POINT_BED:          return "Bed";
        case REF_POINT_TV:           return "TV";
        case REF_POINT_CUSTOM:
        default:                     return "Custom";
    }
}

/*---------------------------------------------------------*\
| Get default colors for different reference point types  |
\*---------------------------------------------------------*/
RGBColor VirtualReferencePoint3D::GetDefaultColor(ReferencePointType type)
{
    switch(type)
    {
        case REF_POINT_USER:         return 0x0000FF00;  // Green (user stick figure)
        case REF_POINT_MONITOR:      return 0x000080FF;  // Blue (screen)
        case REF_POINT_CHAIR:        return 0x00804000;  // Brown (chair)
        case REF_POINT_DESK:         return 0x00A0522D;  // Wood brown (desk)
        case REF_POINT_SPEAKER_LEFT: return 0x00FF4500;  // Orange red (left)
        case REF_POINT_SPEAKER_RIGHT: return 0x00FF6500; // Orange (right)
        case REF_POINT_DOOR:         return 0x00FFFFFF;  // White (door)
        case REF_POINT_WINDOW:       return 0x0087CEEB;  // Sky blue (window)
        case REF_POINT_BED:          return 0x00DDA0DD;  // Plum (bed)
        case REF_POINT_TV:           return 0x00000000;  // Black (TV)
        case REF_POINT_CUSTOM:
        default:                     return 0x00808080;  // Gray (custom)
    }
}

/*---------------------------------------------------------*\
| Get all type names for UI dropdown                      |
\*---------------------------------------------------------*/
std::vector<std::string> VirtualReferencePoint3D::GetTypeNames()
{
    return {
        "User Position",
        "Monitor",
        "Chair",
        "Desk",
        "Left Speaker",
        "Right Speaker",
        "Door",
        "Window",
        "Bed",
        "TV",
        "Custom"
    };
}

/*---------------------------------------------------------*\
| Serialize to JSON (same pattern as VirtualController3D) |
\*---------------------------------------------------------*/
json VirtualReferencePoint3D::ToJson() const
{
    json j;
    j["id"] = id;
    j["name"] = name;
    j["type"] = (int)type;
    j["visible"] = visible;
    j["display_color"] = (unsigned int)display_color;

    j["transform"]["position"]["x"] = transform.position.x;
    j["transform"]["position"]["y"] = transform.position.y;
    j["transform"]["position"]["z"] = transform.position.z;
    j["transform"]["rotation"]["x"] = transform.rotation.x;
    j["transform"]["rotation"]["y"] = transform.rotation.y;
    j["transform"]["rotation"]["z"] = transform.rotation.z;
    j["transform"]["scale"]["x"] = transform.scale.x;
    j["transform"]["scale"]["y"] = transform.scale.y;
    j["transform"]["scale"]["z"] = transform.scale.z;

    return j;
}

/*---------------------------------------------------------*\
| Deserialize from JSON                                    |
\*---------------------------------------------------------*/
std::unique_ptr<VirtualReferencePoint3D> VirtualReferencePoint3D::FromJson(const json& j)
{
    if(!j.contains("name") || !j.contains("type") || !j.contains("transform"))
    {
        return nullptr;
    }

    std::string name = j["name"];
    ReferencePointType type = (ReferencePointType)j["type"];

    nlohmann::json pos = j["transform"]["position"];
    auto ref_point = std::make_unique<VirtualReferencePoint3D>(name, type, pos["x"], pos["y"], pos["z"]);

    // Restore other properties
    if(j.contains("id")) ref_point->id = j["id"];
    if(j.contains("visible")) ref_point->visible = j["visible"];
    if(j.contains("display_color")) ref_point->display_color = j["display_color"];

    // Restore full transform
    if(j["transform"].contains("rotation"))
    {
        nlohmann::json rot = j["transform"]["rotation"];
        ref_point->transform.rotation = {rot["x"], rot["y"], rot["z"]};
    }
    if(j["transform"].contains("scale"))
    {
        nlohmann::json scale = j["transform"]["scale"];
        ref_point->transform.scale = {scale["x"], scale["y"], scale["z"]};
    }

    return ref_point;
}