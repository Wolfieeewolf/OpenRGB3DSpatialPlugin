/*---------------------------------------------------------*\
| VirtualReferencePoint3D.h                                |
|                                                           |
|   Virtual reference point for 3D spatial effect anchors |
|                                                           |
|   Date: 2025-09-30                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef VIRTUALREFERENCEPOINT3D_H
#define VIRTUALREFERENCEPOINT3D_H

#include <string>
#include <vector>
#include "LEDPosition3D.h"
#include "SpatialEffectTypes.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class VirtualReferencePoint3D
{
public:
    VirtualReferencePoint3D(const std::string& name, ReferencePointType type,
                           float x = 0.0f, float y = 0.0f, float z = 0.0f);
    ~VirtualReferencePoint3D();

    /*---------------------------------------------------------*\
    | Basic Properties                                         |
    \*---------------------------------------------------------*/
    int GetId() const { return id; }
    std::string GetName() const { return name; }
    ReferencePointType GetType() const { return type; }
    bool IsVisible() const { return visible; }
    RGBColor GetDisplayColor() const { return display_color; }

    void SetName(const std::string& new_name) { name = new_name; }
    void SetType(ReferencePointType new_type) { type = new_type; }
    void SetVisible(bool vis) { visible = vis; }
    void SetDisplayColor(RGBColor color) { display_color = color; }

    /*---------------------------------------------------------*\
    | Transform Properties (same as controllers)              |
    \*---------------------------------------------------------*/
    Vector3D GetPosition() const { return transform.position; }
    Rotation3D GetRotation() const { return transform.rotation; }
    Vector3D GetScale() const { return transform.scale; }

    void SetPosition(const Vector3D& pos) { transform.position = pos; }
    void SetRotation(const Rotation3D& rot) { transform.rotation = rot; }
    void SetScale(const Vector3D& scale_val) { transform.scale = scale_val; }

    Transform3D GetTransform() const { return transform; }
    void SetTransform(const Transform3D& t) { transform = t; }

    /*---------------------------------------------------------*\
    | Viewport Display                                         |
    \*---------------------------------------------------------*/
    int GetIconType() const;                    // Returns icon based on type
    static const char* GetTypeName(ReferencePointType type);
    static RGBColor GetDefaultColor(ReferencePointType type);

    /*---------------------------------------------------------*\
    | Serialization (same pattern as VirtualController3D)     |
    \*---------------------------------------------------------*/
    json ToJson() const;
    static std::unique_ptr<VirtualReferencePoint3D> FromJson(const json& j);

    /*---------------------------------------------------------*\
    | Static Helper for UI                                     |
    \*---------------------------------------------------------*/
    static std::vector<std::string> GetTypeNames();

private:
    /*---------------------------------------------------------*\
    | Core Properties                                          |
    \*---------------------------------------------------------*/
    int id;                         // Unique ID
    std::string name;              // User-friendly name
    ReferencePointType type;       // Type (monitor, chair, etc.)
    Transform3D transform;         // Position, rotation, scale
    bool visible;                  // Show/hide in viewport
    RGBColor display_color;       // Display color in viewport

    /*---------------------------------------------------------*\
    | Static ID counter (like virtual controllers)            |
    \*---------------------------------------------------------*/
    static int next_id;
};

#endif