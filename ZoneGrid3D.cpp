// SPDX-License-Identifier: GPL-2.0-only

#include "ZoneGrid3D.h"

#include "SpatialEffect3D.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "VirtualController3D.h"
#include "Zone3D.h"
#include "ZoneManager3D.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace
{
struct ZoneGridBounds
{
    GridBounds room;
    GridBounds world;
};

enum class TargetMode
{
    AllControllers,
    SingleController,
    ZoneControllers,
    None
};

struct ZoneTargetSelection
{
    TargetMode mode = TargetMode::None;
    int single_controller_idx = -1;
    std::unordered_set<int> zone_controller_indices;
};

ZoneTargetSelection ResolveZoneTarget(ZoneManager3D* zone_manager,
                                      const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                                      int zone_index)
{
    ZoneTargetSelection selection{};
    if(zone_index == -1)
    {
        selection.mode = TargetMode::AllControllers;
    }
    else if(zone_index <= -1000)
    {
        selection.single_controller_idx = -(zone_index + 1000);
        if(selection.single_controller_idx >= 0 &&
           selection.single_controller_idx < (int)controller_transforms.size())
        {
            selection.mode = TargetMode::SingleController;
        }
    }
    else if(zone_manager && zone_index >= 0 && zone_index < zone_manager->GetZoneCount())
    {
        Zone3D* zone = zone_manager->GetZone(zone_index);
        if(zone)
        {
            const std::vector<int>& controllers = zone->GetControllers();
            selection.zone_controller_indices.insert(controllers.begin(), controllers.end());
            if(!selection.zone_controller_indices.empty())
            {
                selection.mode = TargetMode::ZoneControllers;
            }
        }
    }
    return selection;
}

const std::unordered_set<RGBController*>* ResolveSkipPhysicalControllers(
    const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
    const std::unordered_set<RGBController*>* skip_physical_controllers,
    std::unordered_set<RGBController*>& owned_local)
{
    if(skip_physical_controllers)
    {
        return skip_physical_controllers;
    }
    for(const std::unique_ptr<ControllerTransform>& tp : controller_transforms)
    {
        if(!tp || !tp->virtual_controller)
        {
            continue;
        }
        for(const GridLEDMapping& m : tp->virtual_controller->GetMappings())
        {
            if(m.controller)
            {
                owned_local.insert(m.controller);
            }
        }
    }
    return &owned_local;
}

bool ControllerMatchesTarget(const ZoneTargetSelection& selection,
                             unsigned int ctrl_idx,
                             int single_controller_idx)
{
    switch(selection.mode)
    {
    case TargetMode::AllControllers:
        return true;
    case TargetMode::SingleController:
        return ((int)ctrl_idx == single_controller_idx);
    case TargetMode::ZoneControllers:
        return (selection.zone_controller_indices.find((int)ctrl_idx) != selection.zone_controller_indices.end());
    default:
        return false;
    }
}

struct ZoneLedAggregate
{
    GridBounds room{};
    GridBounds world{};
    double room_cx_sum = 0.0;
    double room_cy_sum = 0.0;
    double room_cz_sum = 0.0;
    double world_cx_sum = 0.0;
    double world_cy_sum = 0.0;
    double world_cz_sum = 0.0;
    std::uint64_t led_count = 0;
    bool have_bounds = false;
};

bool TryAggregateZoneTargetLeds(ZoneManager3D* zone_manager,
                                const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                                int zone_index,
                                const std::unordered_set<RGBController*>* skip_physical_controllers,
                                bool skip_hidden_by_virtual,
                                ZoneLedAggregate& out)
{
    out = ZoneLedAggregate{};
    const ZoneTargetSelection selection = ResolveZoneTarget(zone_manager, controller_transforms, zone_index);
    if(selection.mode == TargetMode::None)
    {
        return false;
    }

    std::unordered_set<RGBController*> owned_local;
    const std::unordered_set<RGBController*>* skip_phys =
        ResolveSkipPhysicalControllers(controller_transforms, skip_physical_controllers, owned_local);

    float room_min_x = std::numeric_limits<float>::max();
    float room_min_y = std::numeric_limits<float>::max();
    float room_min_z = std::numeric_limits<float>::max();
    float room_max_x = std::numeric_limits<float>::lowest();
    float room_max_y = std::numeric_limits<float>::lowest();
    float room_max_z = std::numeric_limits<float>::lowest();
    float world_min_x = std::numeric_limits<float>::max();
    float world_min_y = std::numeric_limits<float>::max();
    float world_min_z = std::numeric_limits<float>::max();
    float world_max_x = std::numeric_limits<float>::lowest();
    float world_max_y = std::numeric_limits<float>::lowest();
    float world_max_z = std::numeric_limits<float>::lowest();

    for(unsigned int ctrl_idx = 0; ctrl_idx < controller_transforms.size(); ctrl_idx++)
    {
        ControllerTransform* transform = controller_transforms[ctrl_idx].get();
        if(!transform)
        {
            continue;
        }
        if(skip_hidden_by_virtual && transform->hidden_by_virtual)
        {
            continue;
        }
        if(transform->controller && skip_phys->find(transform->controller) != skip_phys->end())
        {
            continue;
        }
        if(!ControllerMatchesTarget(selection, ctrl_idx, selection.single_controller_idx))
        {
            continue;
        }

        ControllerLayout3D::UpdateWorldPositions(transform);
        for(const LEDPosition3D& led_pos : transform->led_positions)
        {
            const Vector3D& room = led_pos.room_position;
            const Vector3D& world = led_pos.world_position;
            room_min_x = std::min(room_min_x, room.x);
            room_min_y = std::min(room_min_y, room.y);
            room_min_z = std::min(room_min_z, room.z);
            room_max_x = std::max(room_max_x, room.x);
            room_max_y = std::max(room_max_y, room.y);
            room_max_z = std::max(room_max_z, room.z);
            world_min_x = std::min(world_min_x, world.x);
            world_min_y = std::min(world_min_y, world.y);
            world_min_z = std::min(world_min_z, world.z);
            world_max_x = std::max(world_max_x, world.x);
            world_max_y = std::max(world_max_y, world.y);
            world_max_z = std::max(world_max_z, world.z);
            out.room_cx_sum += room.x;
            out.room_cy_sum += room.y;
            out.room_cz_sum += room.z;
            out.world_cx_sum += world.x;
            out.world_cy_sum += world.y;
            out.world_cz_sum += world.z;
            out.led_count++;
            out.have_bounds = true;
        }
    }

    if(!out.have_bounds)
    {
        return false;
    }

    out.room = GridBounds{room_min_x, room_max_x, room_min_y, room_max_y, room_min_z, room_max_z};
    out.world = GridBounds{world_min_x, world_max_x, world_min_y, world_max_y, world_min_z, world_max_z};
    return true;
}

bool TryComputeZoneGridBounds(ZoneManager3D* zone_manager,
                              const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                              int zone_index,
                              const std::unordered_set<RGBController*>* skip_physical_controllers,
                              bool skip_hidden_by_virtual,
                              ZoneGridBounds& out_bounds)
{
    ZoneLedAggregate aggregate{};
    if(!TryAggregateZoneTargetLeds(zone_manager,
                                   controller_transforms,
                                   zone_index,
                                   skip_physical_controllers,
                                   skip_hidden_by_virtual,
                                   aggregate))
    {
        return false;
    }
    out_bounds.room = aggregate.room;
    out_bounds.world = aggregate.world;
    return true;
}
}

bool TryComputeZoneLedCentroid(ZoneManager3D* zone_manager,
                               const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                               int zone_index,
                               const std::unordered_set<RGBController*>* skip_physical_controllers,
                               bool skip_hidden_by_virtual,
                               bool room_aligned,
                               Vector3D* out_centroid)
{
    if(!out_centroid)
    {
        return false;
    }
    ZoneLedAggregate aggregate{};
    if(!TryAggregateZoneTargetLeds(zone_manager,
                                   controller_transforms,
                                   zone_index,
                                   skip_physical_controllers,
                                   skip_hidden_by_virtual,
                                   aggregate) ||
       aggregate.led_count == 0)
    {
        return false;
    }
    const double inv = 1.0 / static_cast<double>(aggregate.led_count);
    if(room_aligned)
    {
        out_centroid->x = static_cast<float>(aggregate.room_cx_sum * inv);
        out_centroid->y = static_cast<float>(aggregate.room_cy_sum * inv);
        out_centroid->z = static_cast<float>(aggregate.room_cz_sum * inv);
    }
    else
    {
        out_centroid->x = static_cast<float>(aggregate.world_cx_sum * inv);
        out_centroid->y = static_cast<float>(aggregate.world_cy_sum * inv);
        out_centroid->z = static_cast<float>(aggregate.world_cz_sum * inv);
    }
    return true;
}

bool TryComputeZoneAnchorCenter(ZoneManager3D* zone_manager,
                                const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                                int zone_index,
                                const std::unordered_set<RGBController*>* skip_physical_controllers,
                                bool skip_hidden_by_virtual,
                                bool room_aligned,
                                Vector3D* out_center)
{
    if(!out_center)
    {
        return false;
    }
    ZoneGridBounds bounds{};
    if(!TryComputeZoneGridBounds(zone_manager,
                                 controller_transforms,
                                 zone_index,
                                 skip_physical_controllers,
                                 skip_hidden_by_virtual,
                                 bounds))
    {
        return false;
    }
    const GridBounds& b = room_aligned ? bounds.room : bounds.world;
    out_center->x = (b.min_x + b.max_x) * 0.5f;
    out_center->y = (b.min_y + b.max_y) * 0.5f;
    out_center->z = (b.min_z + b.max_z) * 0.5f;
    return true;
}

bool TryMakeZoneGridContextPair(ZoneManager3D* zone_manager,
                                const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                                int zone_index,
                                const std::unordered_set<RGBController*>* skip_physical_controllers,
                                bool skip_hidden_by_virtual,
                                float room_grid_scale_mm,
                                float world_grid_scale_mm,
                                std::unique_ptr<GridContext3D>& out_room_grid,
                                std::unique_ptr<GridContext3D>& out_world_grid)
{
    out_room_grid.reset();
    out_world_grid.reset();
    ZoneLedAggregate aggregate{};
    if(!TryAggregateZoneTargetLeds(zone_manager,
                                   controller_transforms,
                                   zone_index,
                                   skip_physical_controllers,
                                   skip_hidden_by_virtual,
                                   aggregate))
    {
        return false;
    }
    const GridBounds& r = aggregate.room;
    const GridBounds& w = aggregate.world;
    out_room_grid = std::make_unique<GridContext3D>(
        r.min_x, r.max_x, r.min_y, r.max_y, r.min_z, r.max_z, room_grid_scale_mm);
    out_world_grid = std::make_unique<GridContext3D>(
        w.min_x, w.max_x, w.min_y, w.max_y, w.min_z, w.max_z, world_grid_scale_mm);
    if(aggregate.led_count > 0)
    {
        const double inv = 1.0 / static_cast<double>(aggregate.led_count);
        out_room_grid->SetLedCentroid(static_cast<float>(aggregate.room_cx_sum * inv),
                                      static_cast<float>(aggregate.room_cy_sum * inv),
                                      static_cast<float>(aggregate.room_cz_sum * inv));
        out_world_grid->SetLedCentroid(static_cast<float>(aggregate.world_cx_sum * inv),
                                       static_cast<float>(aggregate.world_cy_sum * inv),
                                       static_cast<float>(aggregate.world_cz_sum * inv));
    }
    return true;
}
