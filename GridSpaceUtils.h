#ifndef GRIDSPACEUTILS_H
#define GRIDSPACEUTILS_H

#include <memory>
#include <vector>

#include "LEDPosition3D.h"

struct ManualRoomSettings
{
    bool  use_manual;
    float width_mm;
    float height_mm;
    float depth_mm;
};

struct GridBounds
{
    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float min_z;
    float max_z;
};

struct GridDimensionDefaults
{
    int grid_x;
    int grid_y;
    int grid_z;
};

struct GridExtents
{
    float width_units;
    float height_units;
    float depth_units;
};

inline ManualRoomSettings MakeManualRoomSettings(bool use_manual,
                                                 float width_mm,
                                                 float height_mm,
                                                 float depth_mm)
{
    ManualRoomSettings settings{use_manual, width_mm, height_mm, depth_mm};
    return settings;
}

inline GridDimensionDefaults MakeGridDefaults(int grid_x, int grid_y, int grid_z)
{
    GridDimensionDefaults defaults{grid_x, grid_y, grid_z};
    return defaults;
}

float MMToGridUnits(float mm, float grid_scale_mm);
float GridUnitsToMM(float units, float grid_scale_mm);

GridExtents ResolveGridExtents(const ManualRoomSettings& settings,
                               float grid_scale_mm,
                               const GridDimensionDefaults& defaults);

GridExtents BoundsToExtents(const GridBounds& bounds);

GridBounds ComputeGridBounds(const ManualRoomSettings& settings,
                             float grid_scale_mm,
                             const std::vector<std::unique_ptr<ControllerTransform>>& transforms);

GridBounds ComputeRoomAlignedBounds(const ManualRoomSettings& settings,
                                    float grid_scale_mm,
                                    const std::vector<std::unique_ptr<ControllerTransform>>& transforms);

#endif // GRIDSPACEUTILS_H
