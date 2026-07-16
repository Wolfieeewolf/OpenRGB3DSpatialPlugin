// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERDIALOG_INTERNAL_H
#define CUSTOMCONTROLLERDIALOG_INTERNAL_H

#include "CustomControllerTypes.h"
#include "CustomControllerMappingUtils.h"
#include "LEDPosition3D.h"
#include "ResourceManagerInterface.h"
#include "RGBController.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr int kMaxSelectionFillTintUpdates = 128;

inline void ReverseFloatVector(std::vector<float>* values)
{
    if(!values)
    {
        return;
    }
    std::reverse(values->begin(), values->end());
}

inline bool ComputeSelectionBounds(const std::set<std::pair<int, int>>& cells,
                                   int* min_col,
                                   int* min_row,
                                   int* max_col,
                                   int* max_row)
{
    if(cells.empty() || !min_col || !min_row || !max_col || !max_row)
    {
        return false;
    }

    *min_col = cells.begin()->first;
    *max_col = cells.begin()->first;
    *min_row = cells.begin()->second;
    *max_row = cells.begin()->second;
    for(const std::pair<int, int>& cell : cells)
    {
        *min_col = std::min(*min_col, cell.first);
        *max_col = std::max(*max_col, cell.first);
        *min_row = std::min(*min_row, cell.second);
        *max_row = std::max(*max_row, cell.second);
    }
    return true;
}

inline bool TryGetDialogGlobalLedIndex(RGBController* controller,
                                       unsigned int zone_idx,
                                       unsigned int led_idx,
                                       unsigned int* global_led_idx)
{
    if(!controller || !global_led_idx)
    {
        return false;
    }
    if(zone_idx >= controller->zones.size())
    {
        return false;
    }
    if(led_idx >= controller->zones[zone_idx].leds_count)
    {
        return false;
    }

    *global_led_idx = controller->zones[zone_idx].start_idx + led_idx;
    return true;
}

inline RGBController* GetControllerByRow(ResourceManagerInterface* resource_manager, int row)
{
    if(!resource_manager || row < 0)
    {
        return nullptr;
    }
    std::vector<RGBController*>& controllers = resource_manager->GetRGBControllers();
    if(row < 0 || row >= (int)controllers.size())
    {
        return nullptr;
    }
    return controllers[row];
}

struct ProfileLayoutBounds
{
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    bool valid;
};

inline ProfileLayoutBounds ComputeProfileLayoutBounds(const std::vector<LEDPosition3D>& positions)
{
    ProfileLayoutBounds bounds{0.0f, 0.0f, 0.0f, 0.0f, false};
    if(positions.empty())
    {
        return bounds;
    }

    bounds.min_x = bounds.max_x = positions[0].local_position.x;
    bounds.min_y = bounds.max_y = positions[0].local_position.y;
    bounds.valid = true;

    for(unsigned int i = 1; i < positions.size(); i++)
    {
        if(positions[i].local_position.x < bounds.min_x) bounds.min_x = positions[i].local_position.x;
        if(positions[i].local_position.x > bounds.max_x) bounds.max_x = positions[i].local_position.x;
        if(positions[i].local_position.y < bounds.min_y) bounds.min_y = positions[i].local_position.y;
        if(positions[i].local_position.y > bounds.max_y) bounds.max_y = positions[i].local_position.y;
    }

    return bounds;
}

inline std::vector<LEDPosition3D> PositionsForLayoutBounds(const std::vector<LEDPosition3D>& positions,
                                                         int                               granularity,
                                                         int                               item_idx)
{
    if(granularity == 0)
    {
        return positions;
    }

    if(granularity == 1)
    {
        std::vector<LEDPosition3D> zone_positions;
        zone_positions.reserve(positions.size());
        const unsigned int zone_idx = static_cast<unsigned int>(item_idx);
        for(const LEDPosition3D& pos : positions)
        {
            if(pos.zone_idx == zone_idx)
            {
                zone_positions.push_back(pos);
            }
        }
        return zone_positions;
    }

    return {};
}

inline bool IsLedMappedOnLayer(const std::vector<GridLEDMapping>& mappings,
                              RGBController*              controller,
                              unsigned int                zone_idx,
                              unsigned int                led_idx,
                              int                         layer,
                              const std::vector<RGBController*>& controllers)
{
    for(const GridLEDMapping& mapping : mappings)
    {
        if(!CustomControllerMapping::MappingOwnedByController(mapping, controller, controllers))
        {
            continue;
        }
        if(mapping.zone_idx == zone_idx && mapping.led_idx == led_idx && mapping.z == layer)
        {
            return true;
        }
    }
    return false;
}

inline bool ProfileCellToGrid(const ProfileLayoutBounds& bounds,
                              const LEDPosition3D& pos,
                              int start_x,
                              int start_y,
                              int grid_w,
                              int grid_h,
                              int* out_x,
                              int* out_y)
{
    if(!bounds.valid || !out_x || !out_y)
    {
        return false;
    }

    const int x = start_x + (int)std::lround(pos.local_position.x - bounds.min_x);
    const int y = start_y + (int)std::lround(pos.local_position.y - bounds.min_y);
    if(x < 0 || x >= grid_w || y < 0 || y >= grid_h)
    {
        return false;
    }

    *out_x = x;
    *out_y = y;
    return true;
}

inline void CollectMappingsAtCell(const std::vector<GridLEDMapping>& mappings,
                                  int x,
                                  int y,
                                  int z,
                                  std::vector<GridLEDMapping>& out)
{
    out.clear();
    for(const GridLEDMapping& mapping : mappings)
    {
        if(mapping.x == x && mapping.y == y && mapping.z == z)
        {
            out.push_back(mapping);
        }
    }
}

inline void RemoveMappingsAtCell(std::vector<GridLEDMapping>& mappings, int x, int y, int z)
{
    for(std::vector<GridLEDMapping>::iterator it = mappings.begin(); it != mappings.end();)
    {
        if(it->x == x && it->y == y && it->z == z)
        {
            it = mappings.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

inline uint64_t GridCellKey(int x, int y)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
}

using LayerCellIndex = std::unordered_map<uint64_t, std::vector<size_t>>;

inline LayerCellIndex BuildLayerCellIndex(const std::vector<GridLEDMapping>& mappings, int layer)
{
    LayerCellIndex index;
    for(size_t mapping_index = 0; mapping_index < mappings.size(); mapping_index++)
    {
        const GridLEDMapping& mapping = mappings[mapping_index];
        if(mapping.z != layer || !mapping.controller)
        {
            continue;
        }

        index[GridCellKey(mapping.x, mapping.y)].push_back(mapping_index);
    }

    return index;
}

inline void CollectMappingsForCells(const std::set<std::pair<int, int>>& cells,
                                    int layer,
                                    const std::vector<GridLEDMapping>& all_mappings,
                                    std::vector<GridLEDMapping>& out)
{
    out.clear();
    for(const GridLEDMapping& m : all_mappings)
    {
        if(m.z != layer || !m.controller)
        {
            continue;
        }
        if(cells.count(std::make_pair(m.x, m.y)) > 0)
        {
            out.push_back(m);
        }
    }
}

enum class CellIdentifyState
{
    Empty,
    Off,
    On,
    Partial
};

enum class IdentifyUiState
{
    NoSelection,
    NoMapped,
    AllOff,
    AllOn,
    Mixed
};

inline CellIdentifyState GetCellIdentifyState(int x,
                                             int y,
                                             int layer,
                                             const std::vector<GridLEDMapping>& mappings,
                                             const std::map<std::pair<RGBController*, unsigned int>, RGBColor>& identified)
{
    unsigned int mapped_count = 0;
    unsigned int identified_count = 0;

    for(const GridLEDMapping& mapping : mappings)
    {
        if(mapping.x != x || mapping.y != y || mapping.z != layer || !mapping.controller)
        {
            continue;
        }

        unsigned int global_led_idx = 0;
        if(!TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx))
        {
            continue;
        }

        mapped_count++;
        if(identified.count(std::make_pair(mapping.controller, global_led_idx)) > 0)
        {
            identified_count++;
        }
    }

    if(mapped_count == 0)
    {
        return CellIdentifyState::Empty;
    }
    if(identified_count == 0)
    {
        return CellIdentifyState::Off;
    }
    if(identified_count == mapped_count)
    {
        return CellIdentifyState::On;
    }
    return CellIdentifyState::Partial;
}

inline IdentifyUiState EvaluateIdentifyUiState(const std::set<std::pair<int, int>>& cells,
                                               int layer,
                                               const std::vector<GridLEDMapping>& mappings,
                                               const std::map<std::pair<RGBController*, unsigned int>, RGBColor>& identified)
{
    if(cells.empty())
    {
        return IdentifyUiState::NoSelection;
    }

    bool saw_empty = false;
    bool saw_off = false;
    bool saw_on = false;

    for(const std::pair<int, int>& cell : cells)
    {
        switch(GetCellIdentifyState(cell.first, cell.second, layer, mappings, identified))
        {
        case CellIdentifyState::Empty:
            saw_empty = true;
            break;
        case CellIdentifyState::Off:
            saw_off = true;
            break;
        case CellIdentifyState::On:
            saw_on = true;
            break;
        case CellIdentifyState::Partial:
            return IdentifyUiState::Mixed;
        }
    }

    if(saw_on && (saw_off || saw_empty))
    {
        return IdentifyUiState::Mixed;
    }
    if(saw_off && saw_empty)
    {
        return IdentifyUiState::Mixed;
    }
    if(saw_on)
    {
        return IdentifyUiState::AllOn;
    }
    if(saw_off)
    {
        return IdentifyUiState::AllOff;
    }
    return IdentifyUiState::NoMapped;
}

inline bool MappingHasZoneLed(const GridLEDMapping& mapping)
{
    if(!mapping.controller || mapping.zone_idx >= mapping.controller->zones.size())
    {
        return false;
    }

    return mapping.led_idx < mapping.controller->zones[mapping.zone_idx].leds_count;
}

inline unsigned int MappingDisplayLedNumber(const GridLEDMapping& mapping)
{
    return mapping.led_idx + 1;
}

#endif // CUSTOMCONTROLLERDIALOG_INTERNAL_H
