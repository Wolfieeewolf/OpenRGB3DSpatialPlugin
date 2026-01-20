#include "GridSpaceUtils.h"

#include "ControllerLayout3D.h"
#include <algorithm>

namespace
{
    constexpr float kDefaultRoomSizeMM = 1000.0f;

    enum class BoundsSpace
    {
        World,
        RoomAligned
    };

    GridBounds ComputeBoundsInternal(const ManualRoomSettings& settings,
                                     float grid_scale_mm,
                                     const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
                                     BoundsSpace space)
    {
        GridBounds bounds{};

        if(settings.use_manual)
        {
            bounds.min_x = 0.0f;
            bounds.max_x = MMToGridUnits(settings.width_mm, grid_scale_mm);
            bounds.min_y = 0.0f;
            bounds.max_y = MMToGridUnits(settings.height_mm, grid_scale_mm);
            bounds.min_z = 0.0f;
            bounds.max_z = MMToGridUnits(settings.depth_mm, grid_scale_mm);
            return bounds;
        }

        bool has_leds = false;

        for(const std::unique_ptr<ControllerTransform>& transform_ptr : transforms)
        {
            ControllerTransform* transform = transform_ptr.get();
            if(!transform || transform->hidden_by_virtual)
            {
                continue;
            }

            if(transform->world_positions_dirty)
            {
                ControllerLayout3D::UpdateWorldPositions(transform);
            }

            for(const LEDPosition3D& led : transform->led_positions)
            {
                const Vector3D& pos = (space == BoundsSpace::RoomAligned) ? led.room_position
                                                                          : led.world_position;

                if(!has_leds)
                {
                    bounds.min_x = bounds.max_x = pos.x;
                    bounds.min_y = bounds.max_y = pos.y;
                    bounds.min_z = bounds.max_z = pos.z;
                    has_leds = true;
                }
                else
                {
                    if(pos.x < bounds.min_x) bounds.min_x = pos.x;
                    if(pos.x > bounds.max_x) bounds.max_x = pos.x;
                    if(pos.y < bounds.min_y) bounds.min_y = pos.y;
                    if(pos.y > bounds.max_y) bounds.max_y = pos.y;
                    if(pos.z < bounds.min_z) bounds.min_z = pos.z;
                    if(pos.z > bounds.max_z) bounds.max_z = pos.z;
                }
            }
        }

        if(!has_leds)
        {
            float default_units = MMToGridUnits(kDefaultRoomSizeMM, grid_scale_mm);
            bounds.min_x = 0.0f;
            bounds.max_x = default_units;
            bounds.min_y = 0.0f;
            bounds.max_y = default_units;
            bounds.min_z = 0.0f;
            bounds.max_z = default_units;
        }

        return bounds;
    }
}

float MMToGridUnits(float mm, float grid_scale_mm)
{
    return (grid_scale_mm > 0.001f) ? (mm / grid_scale_mm) : mm;
}

float GridUnitsToMM(float units, float grid_scale_mm)
{
    return (grid_scale_mm > 0.001f) ? (units * grid_scale_mm) : units;
}

GridExtents ResolveGridExtents(const ManualRoomSettings& settings,
                               float grid_scale_mm,
                               const GridDimensionDefaults& defaults)
{
    GridExtents extents{};

    const int safe_grid_x = std::max(defaults.grid_x, 0);
    const int safe_grid_y = std::max(defaults.grid_y, 0);
    const int safe_grid_z = std::max(defaults.grid_z, 0);

    if(settings.use_manual)
    {
        extents.width_units  = MMToGridUnits(settings.width_mm, grid_scale_mm);
        extents.height_units = MMToGridUnits(settings.height_mm, grid_scale_mm);
        extents.depth_units  = MMToGridUnits(settings.depth_mm, grid_scale_mm);
    }
    else
    {
        extents.width_units  = static_cast<float>(safe_grid_x);
        extents.height_units = static_cast<float>(safe_grid_y);
        extents.depth_units  = static_cast<float>(safe_grid_z);
    }

    return extents;
}

GridExtents BoundsToExtents(const GridBounds& bounds)
{
    GridExtents extents{};
    extents.width_units  = std::max(0.0f, bounds.max_x - bounds.min_x);
    extents.height_units = std::max(0.0f, bounds.max_y - bounds.min_y);
    extents.depth_units  = std::max(0.0f, bounds.max_z - bounds.min_z);
    return extents;
}

GridBounds ComputeGridBounds(const ManualRoomSettings& settings,
                             float grid_scale_mm,
                             const std::vector<std::unique_ptr<ControllerTransform>>& transforms)
{
    return ComputeBoundsInternal(settings, grid_scale_mm, transforms, BoundsSpace::World);
}

GridBounds ComputeRoomAlignedBounds(const ManualRoomSettings& settings,
                                    float grid_scale_mm,
                                    const std::vector<std::unique_ptr<ControllerTransform>>& transforms)
{
    return ComputeBoundsInternal(settings, grid_scale_mm, transforms, BoundsSpace::RoomAligned);
}
