/*---------------------------------------------------------*\
| DisplayPlane3D.h                                          |
|                                                           |
|   Virtual display plane definition for ambilight mapping |
|                                                           |
|   Date: 2025-10-22                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef DISPLAYPLANE3D_H
#define DISPLAYPLANE3D_H

#include <string>
#include <memory>
#include "LEDPosition3D.h"
#include <nlohmann/json.hpp>

/**
 * @brief Represents a rectangular display surface placed in 3D space.
 *
 * The plane is described by a Transform3D for position/orientation,
 * physical dimensions in millimetres, and capture identifiers used by
 * screen capture subsystems.
 */
class DisplayPlane3D
{
public:
    explicit DisplayPlane3D(const std::string& name = "Display Plane");

    int                 GetId() const { return id; }
    const std::string&  GetName() const { return name; }
    void                SetName(const std::string& new_name) { name = new_name; }

    Transform3D&        GetTransform() { return transform; }
    const Transform3D&  GetTransform() const { return transform; }

    float               GetWidthMM() const { return width_mm; }
    void                SetWidthMM(float w) { width_mm = (w > 1.0f) ? w : 1.0f; }

    float               GetHeightMM() const { return height_mm; }
    void                SetHeightMM(float h) { height_mm = (h > 1.0f) ? h : 1.0f; }

    bool                IsVisible() const { return visible; }
    void                SetVisible(bool v) { visible = v; }

    const std::string&  GetCaptureSourceId() const { return capture_source_id; }
    void                SetCaptureSourceId(const std::string& id) { capture_source_id = id; }

    const std::string&  GetCaptureLabel() const { return capture_label; }
    void                SetCaptureLabel(const std::string& label) { capture_label = label; }

    const std::string&  GetMonitorPresetId() const { return monitor_preset_id; }
    void                SetMonitorPresetId(const std::string& preset_id) { monitor_preset_id = preset_id; }

    int                 GetReferencePointIndex() const { return reference_point_index; }
    void                SetReferencePointIndex(int index) { reference_point_index = index; }

    nlohmann::json      ToJson() const;
    static std::unique_ptr<DisplayPlane3D> FromJson(const nlohmann::json& j);

private:
    int             id;
    std::string     name;
    Transform3D     transform;
    float           width_mm;
    float           height_mm;
    bool            visible;
    std::string     capture_source_id;
    std::string     capture_label;
    std::string     monitor_preset_id;
    int             reference_point_index;  // Index into reference_points vector (-1 = none)

    static int      next_id;
};

#endif // DISPLAYPLANE3D_H
