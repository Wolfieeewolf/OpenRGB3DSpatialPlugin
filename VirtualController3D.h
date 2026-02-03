/*---------------------------------------------------------*\
| VirtualController3D.h                                     |
|                                                           |
|   Virtual controller for custom 3D LED layouts           |
|                                                           |
|   Date: 2025-09-24                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef VIRTUALCONTROLLER3D_H
#define VIRTUALCONTROLLER3D_H

#include <string>
#include <vector>
#include "RGBController.h"
#include "ui/CustomControllerDialog.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class VirtualController3D
{
public:
    VirtualController3D(const std::string& name,
                        int width, int height, int depth,
                        const std::vector<GridLEDMapping>& mappings,
                        float spacing_x = 10.0f, float spacing_y = 10.0f, float spacing_z = 10.0f);
    ~VirtualController3D();

    std::string GetName() const { return name; }
    void SetName(const std::string& n) { name = n; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int GetDepth() const { return depth; }

    float GetSpacingX() const { return spacing_mm_x; }
    float GetSpacingY() const { return spacing_mm_y; }
    float GetSpacingZ() const { return spacing_mm_z; }

    const std::vector<GridLEDMapping>& GetMappings() const { return led_mappings; }

    std::vector<LEDPosition3D> GenerateLEDPositions(float grid_scale_mm = 10.0f);

    json ToJson() const;
    static std::unique_ptr<VirtualController3D> FromJson(const json& j, std::vector<RGBController*>& controllers);
    /** Build a virtual controller from preset JSON with all mappings bound to the given controller. Used when adding preset for multiple instances (e.g. Fan 1, Fan 2, Fan 3). */
    static std::unique_ptr<VirtualController3D> FromJsonForController(const json& j, RGBController* controller, const std::string& display_name);

private:
    std::string name;
    int width;
    int height;
    int depth;
    float spacing_mm_x;
    float spacing_mm_y;
    float spacing_mm_z;
    std::vector<GridLEDMapping> led_mappings;
};

#endif