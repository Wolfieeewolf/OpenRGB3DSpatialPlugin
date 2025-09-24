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
                        const std::vector<GridLEDMapping>& mappings);
    ~VirtualController3D();

    std::string GetName() const { return name; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int GetDepth() const { return depth; }

    const std::vector<GridLEDMapping>& GetMappings() const { return led_mappings; }

    std::vector<LEDPosition3D> GenerateLEDPositions();

    void UpdateColors(std::vector<RGBController*>& controllers);

    json ToJson() const;
    static VirtualController3D* FromJson(const json& j, std::vector<RGBController*>& controllers);

private:
    std::string name;
    int width;
    int height;
    int depth;
    std::vector<GridLEDMapping> led_mappings;
};

#endif