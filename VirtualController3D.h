// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIRTUALCONTROLLER3D_H
#define VIRTUALCONTROLLER3D_H

#include <string>
#include <vector>
#include "RGBController.h"
#include "LEDPosition3D.h"
#include "ui/CustomControllerTypes.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class VirtualController3D
{
public:
    VirtualController3D(const std::string& name,
                        int width,
                        int height,
                        int depth,
                        const std::vector<GridLEDMapping>& mappings,
                        float spacing_x = 10.0f,
                        float spacing_y = 10.0f,
                        float spacing_z = 10.0f,
                        const std::vector<CustomControllerLightBlocker>& light_blockers = {},
                        std::vector<float> column_widths_mm = {},
                        std::vector<float> row_heights_mm = {},
                        std::vector<float> layer_depths_mm = {},
                        std::vector<std::string> layer_names = {},
                        int leds_per_cluster_in = 1);
    ~VirtualController3D();

    std::string GetName() const { return name; }
    void SetName(const std::string& n) { name = n; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int GetDepth() const { return depth; }

    void SyncSpacingScalarsFromArrays();

    float GetSpacingX() const { return spacing_mm_x; }
    float GetSpacingY() const { return spacing_mm_y; }
    float GetSpacingZ() const { return spacing_mm_z; }
    void SetSpacing(float spacing_x, float spacing_y, float spacing_z)
    {
        spacing_mm_x = spacing_x;
        spacing_mm_y = spacing_y;
        spacing_mm_z = spacing_z;
    }

    const std::vector<float>& GetColumnWidthsMm() const { return column_widths_mm; }
    const std::vector<float>& GetRowHeightsMm() const { return row_heights_mm; }
    const std::vector<float>& GetLayerDepthsMm() const { return layer_depths_mm; }
    void SetColumnWidthsMm(std::vector<float> widths) { column_widths_mm = std::move(widths); EnsureGridSizeArrays(); }
    void SetRowHeightsMm(std::vector<float> heights) { row_heights_mm = std::move(heights); EnsureGridSizeArrays(); }
    void SetLayerDepthsMm(std::vector<float> depths) { layer_depths_mm = std::move(depths); EnsureGridSizeArrays(); }
    void ApplyUniformCellSizesFromSpacing();

    const std::vector<std::string>& GetLayerNames() const { return layer_names; }
    void SetLayerNames(std::vector<std::string> names) { layer_names = std::move(names); EnsureLayerNamesArray(); }
    std::string LayerDisplayName(int layer) const;

    float ColumnWidthMm(int column) const;
    float RowHeightMm(int row) const;
    float LayerDepthMm(int layer) const;
    Vector3D CellOriginMm(int x, int y, int z) const;
    Vector3D CellGridPointMm(int x, int y, int z) const;
    void CellLocalBoundsMm(int x, int y, int z, Vector3D* out_min, Vector3D* out_max) const;

    const std::vector<GridLEDMapping>& GetMappings() const { return led_mappings; }
    const std::vector<CustomControllerLightBlocker>& GetLightBlockers() const { return light_blockers; }

    int GetLedsPerCluster() const { return leds_per_cluster; }
    void SetLedsPerCluster(int count) { leds_per_cluster = (count > 1) ? 3 : 1; }

    std::vector<LEDPosition3D> GenerateLEDPositions(float grid_scale_mm = 10.0f);

    bool RebindControllerPointers(std::vector<RGBController*>& controllers);

    json ToJson() const;
    json ToPortablePresetJson() const;
    static std::string PresetFilenameSlug(const std::string& layout_name);
    static std::unique_ptr<VirtualController3D> FromJson(const json& j, std::vector<RGBController*>& controllers);

private:
    void EnsureGridSizeArrays();
    void EnsureLayerNamesArray();
    static std::string DefaultLayerName(int layer_index);
    static float GridPointOffsetAlongAxisMm(int index, const std::vector<float>& sizes);
    float GridPointAxisMm(int index, const std::vector<float>& sizes) const;
    std::vector<float> ColumnWidthsSnapshotMm() const;
    std::vector<float> RowHeightsSnapshotMm() const;
    std::vector<float> LayerDepthsSnapshotMm() const;

    std::string name;
    int width;
    int height;
    int depth;
    float spacing_mm_x;
    float spacing_mm_y;
    float spacing_mm_z;
    std::vector<float> column_widths_mm;
    std::vector<float> row_heights_mm;
    std::vector<float> layer_depths_mm;
    std::vector<std::string> layer_names;
    std::vector<GridLEDMapping> led_mappings;
    std::vector<CustomControllerLightBlocker> light_blockers;
    int leds_per_cluster = 1;
};

#endif
