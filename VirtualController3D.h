// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIRTUALCONTROLLER3D_H
#define VIRTUALCONTROLLER3D_H

#include <string>
#include <vector>
#include "RGBController.h"
#include "ui/CustomControllerDialog.h"
#include "Game/LedLayoutCoordinateMap.h"
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
    void SetSpacing(float spacing_x, float spacing_y, float spacing_z)
    {
        spacing_mm_x = spacing_x;
        spacing_mm_y = spacing_y;
        spacing_mm_z = spacing_z;
    }

    const std::vector<GridLEDMapping>& GetMappings() const { return led_mappings; }

    std::vector<LEDPosition3D> GenerateLEDPositions(float grid_scale_mm = 10.0f);

    json ToJson() const;
    static std::unique_ptr<VirtualController3D> FromJson(const json& j, std::vector<RGBController*>& controllers);
    static std::unique_ptr<VirtualController3D> FromJsonForController(const json& j, RGBController* controller, const std::string& display_name);

    static std::vector<GridLEDMapping> ImportMappingsFromCoordinateMapJson(
        const json& map_json,
        LedLayoutCoordinateMap::NormalizationMode mode,
        int grid_w,
        int grid_h,
        int grid_d,
        RGBController* controller,
        unsigned int zone_idx,
        int granularity = 1,
        std::string* out_error = nullptr);

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
