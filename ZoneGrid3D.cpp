// SPDX-License-Identifier: GPL-2.0-only

#include "ZoneGrid3D.h"

#include "SpatialEffect3D.h"
#include "GridSpaceUtils.h"
#include "ControllerLayout3D.h"
#include "VirtualController3D.h"
#include "Zone3D.h"
#include "ZoneManager3D.h"

#include <algorithm>
#include <limits>

namespace
{
struct ZoneGridBounds
{
    GridBounds room;
    GridBounds world;
};

bool TryComputeZoneGridBounds(ZoneManager3D* zone_manager,
                              const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                              int zone_index,
                              const std::unordered_set<RGBController*>* skip_physical_controllers,
                              bool skip_hidden_by_virtual,
                              ZoneGridBounds& out_bounds)
{
    enum class TargetMode
    {
        AllControllers,
        SingleController,
        ZoneControllers,
        None
    };

    std::unordered_set<RGBController*> owned_local;
    const std::unordered_set<RGBController*>* skip_phys = skip_physical_controllers;
    if(!skip_phys)
    {
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
        skip_phys = &owned_local;
    }

    TargetMode target_mode = TargetMode::None;
    int single_controller_idx = -1;
    std::unordered_set<int> zone_controller_indices;

    if(zone_index == -1)
    {
        target_mode = TargetMode::AllControllers;
    }
    else if(zone_index <= -1000)
    {
        single_controller_idx = -(zone_index + 1000);
        if(single_controller_idx >= 0 && single_controller_idx < (int)controller_transforms.size())
        {
            target_mode = TargetMode::SingleController;
        }
    }
    else if(zone_manager && zone_index >= 0 && zone_index < zone_manager->GetZoneCount())
    {
        Zone3D* zone = zone_manager->GetZone(zone_index);
        if(zone)
        {
            const std::vector<int>& controllers = zone->GetControllers();
            zone_controller_indices.insert(controllers.begin(), controllers.end());
            if(!zone_controller_indices.empty())
            {
                target_mode = TargetMode::ZoneControllers;
            }
        }
    }

    if(target_mode == TargetMode::None)
    {
        return false;
    }

    bool have_any = false;
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

        bool apply = false;
        switch(target_mode)
        {
        case TargetMode::AllControllers:
            apply = true;
            break;
        case TargetMode::SingleController:
            apply = ((int)ctrl_idx == single_controller_idx);
            break;
        case TargetMode::ZoneControllers:
            apply = (zone_controller_indices.find((int)ctrl_idx) != zone_controller_indices.end());
            break;
        default:
            break;
        }

        if(!apply)
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
            have_any = true;
        }
    }

    if(!have_any)
    {
        return false;
    }

    out_bounds.room = GridBounds{room_min_x, room_max_x, room_min_y, room_max_y, room_min_z, room_max_z};
    out_bounds.world = GridBounds{world_min_x, world_max_x, world_min_y, world_max_y, world_min_z, world_max_z};
    return true;
}
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
    ZoneGridBounds zb{};
    if(!TryComputeZoneGridBounds(zone_manager,
                                 controller_transforms,
                                 zone_index,
                                 skip_physical_controllers,
                                 skip_hidden_by_virtual,
                                 zb))
    {
        return false;
    }
    const GridBounds& r = zb.room;
    const GridBounds& w = zb.world;
    out_room_grid = std::make_unique<GridContext3D>(
        r.min_x, r.max_x, r.min_y, r.max_y, r.min_z, r.max_z, room_grid_scale_mm);
    out_world_grid = std::make_unique<GridContext3D>(
        w.min_x, w.max_x, w.min_y, w.max_y, w.min_z, w.max_z, world_grid_scale_mm);
    return true;
}
