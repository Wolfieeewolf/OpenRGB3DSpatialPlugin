// SPDX-License-Identifier: GPL-2.0-only

#ifndef ZONEGRID3D_H
#define ZONEGRID3D_H

// Zone grid: AABB for the effect target (zone/controller), alongside room/world grids (GridSpaceUtils + GridContext3D).

#include <memory>
#include <unordered_set>
#include <vector>

#include "LEDPosition3D.h"

class RGBController;
class ZoneManager3D;

struct GridContext3D;

bool TryMakeZoneGridContextPair(ZoneManager3D* zone_manager,
                                const std::vector<std::unique_ptr<ControllerTransform>>& controller_transforms,
                                int zone_index,
                                const std::unordered_set<RGBController*>* skip_physical_controllers,
                                bool skip_hidden_by_virtual,
                                float room_grid_scale_mm,
                                float world_grid_scale_mm,
                                std::unique_ptr<GridContext3D>& out_room_grid,
                                std::unique_ptr<GridContext3D>& out_world_grid);

#endif
