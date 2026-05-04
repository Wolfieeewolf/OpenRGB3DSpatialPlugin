// SPDX-License-Identifier: GPL-2.0-only

#ifndef LED_LAYOUT_COORDINATE_MAP_H
#define LED_LAYOUT_COORDINATE_MAP_H

#include "ui/CustomControllerDialog.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace LedLayoutCoordinateMap
{

enum class NormalizationMode
{
    Fill,
    Contain
};

struct Float3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline std::optional<std::string> ValidateCoordinateArrayJson(const nlohmann::json& j)
{
    if(!j.is_array())
    {
        return std::string("LED coordinate map JSON must be a top-level array.");
    }
    if(j.empty())
    {
        return std::string("LED coordinate map array is empty.");
    }
    for(size_t i = 0; i < j.size(); i++)
    {
        const nlohmann::json& e = j[i];
        if(!e.is_array())
        {
            return std::string("Each LED entry must be an array of coordinates.");
        }
        if(e.size() != 2 && e.size() != 3)
        {
            return std::string("Each LED entry must have 2 (x,y) or 3 (x,y,z) numbers.");
        }
    }
    return std::nullopt;
}

inline std::vector<Float3> ParseCoordinateArray(const nlohmann::json& j)
{
    std::vector<Float3> out;
    if(auto err = ValidateCoordinateArrayJson(j))
    {
        return out;
    }
    out.reserve(j.size());
    for(const nlohmann::json& e : j)
    {
        Float3 p{};
        p.x = e[0].get<float>();
        p.y = e[1].get<float>();
        p.z = (e.size() >= 3) ? e[2].get<float>() : 0.0f;
        out.push_back(p);
    }
    return out;
}

inline void NormalizeToUnitCube(std::vector<Float3>& pts, NormalizationMode mode)
{
    if(pts.empty())
    {
        return;
    }
    float min_x = std::numeric_limits<float>::infinity();
    float min_y = std::numeric_limits<float>::infinity();
    float min_z = std::numeric_limits<float>::infinity();
    float max_x = -std::numeric_limits<float>::infinity();
    float max_y = -std::numeric_limits<float>::infinity();
    float max_z = -std::numeric_limits<float>::infinity();
    for(const Float3& p : pts)
    {
        min_x = std::min(min_x, p.x);
        min_y = std::min(min_y, p.y);
        min_z = std::min(min_z, p.z);
        max_x = std::max(max_x, p.x);
        max_y = std::max(max_y, p.y);
        max_z = std::max(max_z, p.z);
    }
    float span_x = std::max(1e-9f, max_x - min_x);
    float span_y = std::max(1e-9f, max_y - min_y);
    float span_z = std::max(1e-9f, max_z - min_z);

    if(mode == NormalizationMode::Fill)
    {
        for(Float3& p : pts)
        {
            p.x = (p.x - min_x) / span_x;
            p.y = (p.y - min_y) / span_y;
            p.z = (p.z - min_z) / span_z;
        }
        return;
    }

    float s = std::max(span_x, std::max(span_y, span_z));
    if(s < 1e-9f)
    {
        return;
    }
    for(Float3& p : pts)
    {
        p.x = (p.x - min_x) / s;
        p.y = (p.y - min_y) / s;
        p.z = (p.z - min_z) / s;
    }
}

inline std::vector<Float3> MakeZigzagMatrixPoints2D(int pixel_count, int width)
{
    std::vector<Float3> out;
    if(pixel_count <= 0 || width <= 0)
    {
        return out;
    }
    out.reserve(pixel_count);
    for(int i = 0; i < pixel_count; i++)
    {
        int y = i / width;
        int x = i % width;
        if((y & 1) != 0)
        {
            x = width - 1 - x;
        }
        out.push_back(Float3{(float)x, (float)y, 0.0f});
    }
    return out;
}

inline std::vector<GridLEDMapping> ToGridMappings(const std::vector<Float3>& pts01,
                                                  int grid_w,
                                                  int grid_h,
                                                  int grid_d,
                                                  RGBController* controller,
                                                  unsigned int zone_idx,
                                                  int granularity)
{
    std::vector<GridLEDMapping> mappings;
    if(!controller || grid_w < 1 || grid_h < 1 || grid_d < 1 || pts01.empty())
    {
        return mappings;
    }
    mappings.reserve(pts01.size());
    auto quant = [](float u01, int cells) -> int {
        if(cells <= 1)
        {
            return 0;
        }
        float u = std::clamp(u01, 0.0f, 1.0f);
        int q = (int)std::lround(u * (float)(cells - 1));
        return std::clamp(q, 0, cells - 1);
    };

    for(size_t i = 0; i < pts01.size(); i++)
    {
        const Float3& p = pts01[i];
        GridLEDMapping m{};
        m.x = quant(p.x, grid_w);
        m.y = quant(p.y, grid_h);
        m.z = quant(p.z, grid_d);
        m.controller = controller;
        m.zone_idx = zone_idx;
        m.led_idx = (unsigned int)i;
        m.granularity = granularity;
        mappings.push_back(m);
    }
    return mappings;
}

struct ImportResult
{
    std::vector<Float3> normalized_points;
    std::vector<GridLEDMapping> mappings;
    std::optional<std::string> error;
};

inline ImportResult ImportCoordinateMapForGrid(const nlohmann::json& j,
                                               NormalizationMode mode,
                                               int grid_w,
                                               int grid_h,
                                               int grid_d,
                                               RGBController* controller,
                                               unsigned int zone_idx,
                                               int granularity)
{
    ImportResult r;
    if(auto err = ValidateCoordinateArrayJson(j))
    {
        r.error = *err;
        return r;
    }
    r.normalized_points = ParseCoordinateArray(j);
    if(r.normalized_points.empty())
    {
        r.error = std::string("No coordinates parsed.");
        return r;
    }
    NormalizeToUnitCube(r.normalized_points, mode);
    r.mappings = ToGridMappings(r.normalized_points, grid_w, grid_h, grid_d, controller, zone_idx, granularity);
    return r;
}

}

#endif
