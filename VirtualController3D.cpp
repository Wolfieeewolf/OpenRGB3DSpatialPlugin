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
                                         const std::vector<CustomControllerLightBlocker>& blockers,
                                         std::vector<float> column_widths,
                                         std::vector<float> row_heights,
                                         std::vector<float> layer_depths,
                                         std::vector<std::string> layer_names_in,
                                         int leds_per_cluster_in)
    : name(name),
      width(width),
      height(height),
      depth(depth),
      spacing_mm_x(spacing_x),
      spacing_mm_y(spacing_y),
      spacing_mm_z(spacing_z),
      column_widths_mm(std::move(column_widths)),
      row_heights_mm(std::move(row_heights)),
      layer_depths_mm(std::move(layer_depths)),
      layer_names(std::move(layer_names_in)),
      led_mappings(mappings),
      light_blockers(blockers),
      leds_per_cluster((leds_per_cluster_in > 1) ? 3 : 1)
{
    EnsureGridSizeArrays();
    EnsureLayerNamesArray();
    SyncSpacingScalarsFromArrays();
}

std::string VirtualController3D::DefaultLayerName(int layer_index)
{
    return "Layer " + std::to_string(layer_index + 1);
}

void VirtualController3D::EnsureLayerNamesArray()
{
    while(static_cast<int>(layer_names.size()) < depth)
    {
        layer_names.push_back(DefaultLayerName(static_cast<int>(layer_names.size())));
    }
    while(static_cast<int>(layer_names.size()) > depth)
    {
        layer_names.pop_back();
    }
}

std::string VirtualController3D::LayerDisplayName(int layer) const
{
    if(layer >= 0 && layer < static_cast<int>(layer_names.size()) && !layer_names[static_cast<size_t>(layer)].empty())
    {
        return layer_names[static_cast<size_t>(layer)];
    }
    return DefaultLayerName(layer);
}

void VirtualController3D::EnsureGridSizeArrays()
{
    while(static_cast<int>(column_widths_mm.size()) < width)
    {
        column_widths_mm.push_back(spacing_mm_x);
    }
    while(static_cast<int>(column_widths_mm.size()) > width)
    {
        column_widths_mm.pop_back();
    }

    while(static_cast<int>(row_heights_mm.size()) < height)
    {
        row_heights_mm.push_back(spacing_mm_y);
    }
    while(static_cast<int>(row_heights_mm.size()) > height)
    {
        row_heights_mm.pop_back();
    }

    while(static_cast<int>(layer_depths_mm.size()) < depth)
    {
        layer_depths_mm.push_back(spacing_mm_z);
    }
    while(static_cast<int>(layer_depths_mm.size()) > depth)
    {
        layer_depths_mm.pop_back();
    }
}

void VirtualController3D::ApplyUniformCellSizesFromSpacing()
{
    EnsureGridSizeArrays();
    for(float& value : column_widths_mm)
    {
        value = spacing_mm_x;
    }
    for(float& value : row_heights_mm)
    {
        value = spacing_mm_y;
    }
    for(float& value : layer_depths_mm)
    {
        value = spacing_mm_z;
    }
}

float VirtualController3D::ColumnWidthMm(int column) const
{
    if(column < 0 || column >= static_cast<int>(column_widths_mm.size()))
    {
        return spacing_mm_x;
    }
    return column_widths_mm[static_cast<size_t>(column)];
}

float VirtualController3D::RowHeightMm(int row) const
{
    if(row < 0 || row >= static_cast<int>(row_heights_mm.size()))
    {
        return spacing_mm_y;
    }
    return row_heights_mm[static_cast<size_t>(row)];
}

float VirtualController3D::LayerDepthMm(int layer) const
{
    if(layer < 0 || layer >= static_cast<int>(layer_depths_mm.size()))
    {
        return spacing_mm_z;
    }
    return layer_depths_mm[static_cast<size_t>(layer)];
}

Vector3D VirtualController3D::CellOriginMm(int x, int y, int z) const
{
    Vector3D origin{};
    for(int col = 0; col < x; ++col)
    {
        origin.x += ColumnWidthMm(col);
    }
    for(int row = 0; row < y; ++row)
    {
        origin.y += RowHeightMm(row);
    }
    for(int layer = 0; layer < z; ++layer)
    {
        origin.z += LayerDepthMm(layer);
    }
    return origin;
}

void VirtualController3D::CellLocalBoundsMm(int x, int y, int z, Vector3D* out_min, Vector3D* out_max) const
{
    if(!out_min || !out_max)
    {
        return;
    }

    *out_min = CellOriginMm(x, y, z);
    out_max->x = out_min->x + ColumnWidthMm(x);
    out_max->y = out_min->y + RowHeightMm(y);
    out_max->z = out_min->z + LayerDepthMm(z);
}

float VirtualController3D::GridPointOffsetAlongAxisMm(int index, const std::vector<float>& sizes)
{
    const int count = static_cast<int>(sizes.size());
    if(count <= 0 || index < 0 || index >= count)
    {
        return 0.0f;
    }

    const float w       = sizes[static_cast<size_t>(index)];
    const float w_left  = (index > 0) ? sizes[static_cast<size_t>(index - 1)] : w;
    const float w_right = (index + 1 < count) ? sizes[static_cast<size_t>(index + 1)] : w;

    if(w < w_left && w < w_right)
    {
        return w * 0.5f;
    }

    if(w_right < w)
    {
        return w - w_right * 0.5f;
    }

    if(w_left < w)
    {
        return w_left * 0.5f;
    }

    return w * 0.5f;
}

float VirtualController3D::GridPointAxisMm(int index, const std::vector<float>& sizes) const
{
    const int count = static_cast<int>(sizes.size());
    if(count <= 0 || index < 0 || index >= count)
    {
        return 0.0f;
    }

    if(index == 0)
    {
        return GridPointOffsetAlongAxisMm(0, sizes);
    }

    float origin = 0.0f;
    for(int i = 0; i < index; ++i)
    {
        origin += sizes[static_cast<size_t>(i)];
    }

    const float local = origin + GridPointOffsetAlongAxisMm(index, sizes);
    const float chained =
        GridPointAxisMm(index - 1, sizes)
        + std::min(sizes[static_cast<size_t>(index - 1)], sizes[static_cast<size_t>(index)]);

    const float w       = sizes[static_cast<size_t>(index)];
    const float w_left  = sizes[static_cast<size_t>(index - 1)];
    const float w_right = (index + 1 < count) ? sizes[static_cast<size_t>(index + 1)] : w;

    if(w_left < w && w_left < w_right)
    {
        return chained;
    }

    return local;
}

std::vector<float> VirtualController3D::ColumnWidthsSnapshotMm() const
{
    std::vector<float> sizes;
    sizes.reserve(static_cast<size_t>(width));
    for(int col = 0; col < width; ++col)
    {
        sizes.push_back(ColumnWidthMm(col));
    }
    return sizes;
}

std::vector<float> VirtualController3D::RowHeightsSnapshotMm() const
{
    std::vector<float> sizes;
    sizes.reserve(static_cast<size_t>(height));
    for(int row = 0; row < height; ++row)
    {
        sizes.push_back(RowHeightMm(row));
    }
    return sizes;
}

std::vector<float> VirtualController3D::LayerDepthsSnapshotMm() const
{
    std::vector<float> sizes;
    sizes.reserve(static_cast<size_t>(depth));
    for(int layer = 0; layer < depth; ++layer)
    {
        sizes.push_back(LayerDepthMm(layer));
    }
    return sizes;
}

Vector3D VirtualController3D::CellGridPointMm(int x, int y, int z) const
{
    const std::vector<float> column_sizes = ColumnWidthsSnapshotMm();
    const std::vector<float> row_sizes    = RowHeightsSnapshotMm();
    const std::vector<float> layer_sizes  = LayerDepthsSnapshotMm();

    return {
        GridPointAxisMm(x, column_sizes),
        GridPointAxisMm(y, row_sizes),
        GridPointAxisMm(z, layer_sizes),
    };
}

VirtualController3D::~VirtualController3D() = default;

std::vector<LEDPosition3D> VirtualController3D::GenerateLEDPositions(float grid_scale_mm)
{
    EnsureGridSizeArrays();

    std::vector<LEDPosition3D> positions;

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

        const Vector3D grid_point_mm = CellGridPointMm(led_mappings[i].x, led_mappings[i].y, led_mappings[i].z);
        pos.local_position.x = MMToGridUnits(grid_point_mm.x, grid_scale_mm);
        pos.local_position.y = MMToGridUnits(grid_point_mm.y, grid_scale_mm);
        pos.local_position.z = MMToGridUnits(grid_point_mm.z, grid_scale_mm);
        pos.world_position = pos.local_position;
        pos.room_position = pos.local_position;
        pos.preview_color = 0x00FFFFFF;
        positions.push_back(pos);
    }

    return positions;
}

bool VirtualController3D::RebindControllerPointers(std::vector<RGBController*>& controllers)
{
    return CustomControllerMapping::RebindAll(led_mappings, controllers);
}

void VirtualController3D::SyncSpacingScalarsFromArrays()
{
    EnsureGridSizeArrays();
    if(!column_widths_mm.empty())
    {
        spacing_mm_x = column_widths_mm.front();
    }
    if(!row_heights_mm.empty())
    {
        spacing_mm_y = row_heights_mm.front();
    }
    if(!layer_depths_mm.empty())
    {
        spacing_mm_z = layer_depths_mm.front();
    }
}

json VirtualController3D::ToJson() const
{
    const_cast<VirtualController3D*>(this)->SyncSpacingScalarsFromArrays();

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
    j["column_widths_mm"] = column_widths_mm;
    j["row_heights_mm"] = row_heights_mm;
    j["layer_depths_mm"] = layer_depths_mm;

    json layer_names_json = json::array();
    for(const std::string& layer_name : layer_names)
    {
        layer_names_json.push_back(layer_name);
    }
    j["layer_names"] = layer_names_json;
    j["leds_per_cluster"] = leds_per_cluster;

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

    std::vector<float> column_widths;
    std::vector<float> row_heights;
    std::vector<float> layer_depths;
    if(j.contains("column_widths_mm") && j["column_widths_mm"].is_array())
    {
        for(const json& value_json : j["column_widths_mm"])
        {
            column_widths.push_back(value_json.get<float>());
        }
    }
    if(j.contains("row_heights_mm") && j["row_heights_mm"].is_array())
    {
        for(const json& value_json : j["row_heights_mm"])
        {
            row_heights.push_back(value_json.get<float>());
        }
    }
    if(j.contains("layer_depths_mm") && j["layer_depths_mm"].is_array())
    {
        for(const json& value_json : j["layer_depths_mm"])
        {
            layer_depths.push_back(value_json.get<float>());
        }
    }

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
        mapping.controller          = nullptr;
        mapping.zone_idx            = mapping_json.at("zone_idx").get<unsigned int>();
        mapping.led_idx             = mapping_json.at("led_idx").get<unsigned int>();
        mapping.granularity         = mapping_json.at("granularity").get<int>();
        mappings.push_back(mapping);
    }

    if(mappings.empty())
    {
        return nullptr;
    }

    CustomControllerMapping::RebindAll(mappings, controllers);

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

    std::vector<std::string> layer_names;
    if(j.contains("layer_names") && j["layer_names"].is_array())
    {
        for(const json& name_json : j["layer_names"])
        {
            layer_names.push_back(name_json.get<std::string>());
        }
    }

    int leds_per_cluster = 1;
    if(j.contains("leds_per_cluster"))
    {
        leds_per_cluster = (j["leds_per_cluster"].get<int>() > 1) ? 3 : 1;
    }

    return std::make_unique<VirtualController3D>(name,
                                               width,
                                               height,
                                               depth,
                                               mappings,
                                               spacing_x,
                                               spacing_y,
                                               spacing_z,
                                               blockers,
                                               std::move(column_widths),
                                               std::move(row_heights),
                                               std::move(layer_depths),
                                               std::move(layer_names),
                                               leds_per_cluster);
}
