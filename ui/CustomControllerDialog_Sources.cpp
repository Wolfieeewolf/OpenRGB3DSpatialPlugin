// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerDialog.h"
#include "CustomControllerDialog_Internal.h"
#include "CustomControllerMappingUtils.h"
#include "CustomControllerGridKeys.h"
#include "ControllerDisplayUtils.h"
#include "CustomControllerDeviceList.h"
#include "ControllerLayout3D.h"
#include "MatrixWiringOrder.h"
#include "SpatialTabLedHelpers.h"
#include "custom-controller-grid/CustomControllerLayoutGrid.h"
#include "custom-controller-grid/CustomControllerGridLayoutMath.h"

#include <QColor>
#include <QComboBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

void CustomControllerDialog::sourceSelectionChanged(const CustomControllerSourceRef& source)
{
    Q_UNUSED(source);
    UpdateCellInfo();
}

void CustomControllerDialog::sourceEnableToggled(const CustomControllerSourceRef& source, bool enabled)
{
    if(enabled)
    {
        if(!assignSource(source))
        {
            refreshDeviceList();
        }
    }
    else
    {
        RecordUndoPoint();
        removeSourceFromGrid(source);
        CommitHistoryBaseline();
        UpdateGridDisplay();
        refreshDeviceList();
        UpdateCellInfo();
        UpdateIdentifyButtonUi();
    }
}

bool CustomControllerDialog::selectedGridCellValid() const
{
    return selected_row >= 0 && selected_col >= 0;
}

CustomControllerSourceRef CustomControllerDialog::currentSourceSelection() const
{
    if(device_list)
    {
        return device_list->selectedSource();
    }
    return {};
}

RGBControllerInterface* CustomControllerDialog::controllerForSource(const CustomControllerSourceRef& source) const
{
    if(!source.isValid() || !resource_manager)
    {
        return nullptr;
    }
    return GetControllerByRow(resource_manager, source.controller_index);
}

bool CustomControllerDialog::IsSourceItemAvailable(const CustomControllerSourceRef& source) const
{
    RGBControllerInterface* controller = controllerForSource(source);
    if(!controller)
    {
        return false;
    }

    return !IsItemAssigned(controller, source.granularity, source.item_idx);
}

bool CustomControllerDialog::CanAddSourceToGrid(const CustomControllerSourceRef& source) const
{
    return source.isValid() && selectedGridCellValid() && IsSourceItemAvailable(source);
}

bool CustomControllerDialog::IsSourceItemOnGrid(const CustomControllerSourceRef& source) const
{
    RGBControllerInterface* controller = controllerForSource(source);
    if(!controller)
    {
        return false;
    }
    return IsItemAssigned(controller, source.granularity, source.item_idx);
}

void CustomControllerDialog::refreshDeviceList(int controller_index)
{
    if(device_list)
    {
        device_list->refreshFromHost(controller_index);
    }
}

void CustomControllerDialog::EnsureGridAnchorSelected()
{
    if(selectedGridCellValid())
    {
        return;
    }

    if(!width_spin || !height_spin || width_spin->value() < 1 || height_spin->value() < 1)
    {
        return;
    }

    selected_col = 0;
    selected_row = 0;
    if(layout_grid)
    {
        layout_grid->SelectCellAt(0, 0);
        layout_grid->SetAnchorCell(0, 0);
    }
    if(device_list)
    {
        device_list->refreshEnableButtonsOnly();
    }
}

void CustomControllerDialog::PopulateDeviceItemCombo(int controller_index, int granularity, QComboBox* combo) const
{
    if(!combo || !resource_manager || granularity <= 0)
    {
        return;
    }

    RGBControllerInterface* controller = GetControllerByRow(resource_manager, controller_index);
    if(!controller)
    {
        return;
    }

    if(granularity == 1)
    {
        for(unsigned int i = 0; i < controller->GetZoneCount(); i++)
        {
            const QColor color = GetItemColor(controller, granularity, static_cast<int>(i));
            QPixmap pixmap(16, 16);
            pixmap.fill(color);
            combo->addItem(QIcon(pixmap), QString::fromStdString(controller->GetZoneName(i)),
                           static_cast<int>(i));
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int i = 0; i < controller->GetLEDCount(); i++)
        {
            if(!IsAssignableControllerLed(controller, i))
            {
                continue;
            }
            const QColor color = GetItemColor(controller, granularity, static_cast<int>(i));
            QPixmap pixmap(16, 16);
            pixmap.fill(color);
            combo->addItem(QIcon(pixmap), QString::fromStdString(controller->GetLEDName(i)), static_cast<int>(i));
        }
    }
}

void CustomControllerDialog::removeSourceFromGrid(const CustomControllerSourceRef& source)
{
    RGBControllerInterface* controller = controllerForSource(source);
    if(!controller || !resource_manager)
    {
        return;
    }

    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();
    std::vector<GridLEDMapping> removed_mappings;

    for(auto it = led_mappings.begin(); it != led_mappings.end();)
    {
        const GridLEDMapping& mapping = *it;
        if(!CustomControllerMapping::MappingOwnedByController(mapping, controller, controllers))
        {
            ++it;
            continue;
        }

        bool remove = false;
        if(source.granularity == 0)
        {
            remove = true;
        }
        else if(source.granularity == 1)
        {
            remove = mapping.zone_idx == static_cast<unsigned int>(source.item_idx);
        }
        else if(source.granularity == 2)
        {
            unsigned int global_led_idx = 0;
            if(TryGetDialogGlobalLedIndex(controller, mapping.zone_idx, mapping.led_idx, &global_led_idx)
               && global_led_idx == static_cast<unsigned int>(source.item_idx))
            {
                remove = true;
            }
        }

        if(remove)
        {
            removed_mappings.push_back(mapping);
            it = led_mappings.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if(!matrix_hole_cells.empty() && !removed_mappings.empty())
    {
        std::set<unsigned int> cleared_zones;
        for(const GridLEDMapping& mapping : removed_mappings)
        {
            if(!cleared_zones.insert(mapping.zone_idx).second)
            {
                continue;
            }
            if(!ZoneHasOpenRgbMatrixMap(controller, mapping.zone_idx))
            {
                continue;
            }

            bool zone_still_mapped = false;
            for(const GridLEDMapping& remaining : led_mappings)
            {
                if(CustomControllerMapping::MappingOwnedByController(remaining, controller, controllers)
                   && remaining.zone_idx == mapping.zone_idx)
                {
                    zone_still_mapped = true;
                    break;
                }
            }
            if(zone_still_mapped)
            {
                continue;
            }

            const matrix_map_type map = controller->GetZoneMatrixMap(mapping.zone_idx);
            const int map_w = static_cast<int>(map.width);
            const int map_h = static_cast<int>(map.height);
            zone zone_data = controller->GetZone(mapping.zone_idx);

            int origin_x = mapping.x;
            int origin_y = mapping.y;
            bool found_origin = false;
            for(unsigned int led_y = 0; led_y < map.height && !found_origin; ++led_y)
            {
                for(unsigned int led_x = 0; led_x < map.width; ++led_x)
                {
                    const unsigned int map_idx = led_y * map.width + led_x;
                    if(map.map[map_idx] == kMatrixMapUnused)
                    {
                        continue;
                    }
                    unsigned int zone_led_idx = 0;
                    if(!TryResolveZoneLedFromMatrixValue(&zone_data, map.map[map_idx], &zone_led_idx))
                    {
                        continue;
                    }
                    if(zone_led_idx != mapping.led_idx)
                    {
                        continue;
                    }
                    origin_x = mapping.x - static_cast<int>(led_x);
                    origin_y = mapping.y - static_cast<int>(led_y);
                    found_origin = true;
                    break;
                }
            }

            for(int y = origin_y; y < origin_y + map_h; ++y)
            {
                for(int x = origin_x; x < origin_x + map_w; ++x)
                {
                    matrix_hole_cells.erase(GridCellKey3D(x, y, mapping.z));
                }
            }
        }
    }

    RestoreIdentifyForMappings(removed_mappings);
}

bool CustomControllerDialog::assignSource(const CustomControllerSourceRef& source)
{
    if(!source.isValid())
    {
        QMessageBox::warning(this, tr("Nothing selected"), tr("Select a device, zone, or LED from the list."));
        return false;
    }

    if(selected_row < 0 || selected_col < 0)
    {
        QMessageBox::warning(this, tr("No cell selected"), tr("Please select a canvas cell first."));
        return false;
    }

    RGBControllerInterface* controller = controllerForSource(source);
    if(!controller)
    {
        return false;
    }

    if(!IsSourceItemAvailable(source))
    {
        QMessageBox::information(this, tr("Already on grid"),
                                 tr("That source is already placed. Remove it with − first or pick another."));
        return false;
    }

    PlaceLayoutChoice layout_choice;
    if(source.granularity != 2)
    {
        const bool has_matrix = SourceHasOpenRgbMatrixMap(controller, source.granularity, source.item_idx);
        layout_choice = PromptPlaceLayoutChoice(has_matrix);
        if(layout_choice.cancelled)
        {
            return false;
        }
        if(fill_order_combo)
        {
            fill_order_combo->setCurrentIndex(static_cast<int>(layout_choice.wiring));
        }
    }
    else
    {
        layout_choice.use_openrgb_matrix = false;
        layout_choice.wiring = fill_order_combo
            ? static_cast<MatrixWiringOrder>(std::clamp(fill_order_combo->currentIndex(), 0, (int)MatrixWiringOrder::Count - 1))
            : MatrixWiringOrder::HorizontalTopLeftZigzag;
    }

    std::vector<GridLEDMapping> replaced_mappings;
    CollectMappingsAtCell(led_mappings, selected_col, selected_row, current_layer, replaced_mappings);
    RestoreIdentifyForMappings(replaced_mappings);
    RemoveMappingsAtCell(led_mappings, selected_col, selected_row, current_layer);

    const std::unordered_set<uint64_t> holes_before = matrix_hole_cells;

    RecordUndoPoint();
    const bool placed = PlaceProfileLayout(controller,
                                           source.granularity,
                                           source.item_idx,
                                           selected_col,
                                           selected_row,
                                           layout_choice);
    if(!placed)
    {
        history_.AbandonLastUndoPush();
        matrix_hole_cells = holes_before;
        for(const GridLEDMapping& mapping : replaced_mappings)
        {
            led_mappings.push_back(mapping);
        }
        UpdateGridDisplay();
        UpdateUndoRedoUi();
        return false;
    }

    CommitHistoryBaseline();
    UpdateCellInfo();
    UpdateIdentifyButtonUi();
    UpdateGridDisplay();
    refreshDeviceList(source.controller_index);
    return true;
}

CustomControllerDialog::PlaceLayoutChoice CustomControllerDialog::PromptPlaceLayoutChoice(bool has_openrgb_matrix) const
{
    PlaceLayoutChoice choice;
    choice.wiring = fill_order_combo
        ? static_cast<MatrixWiringOrder>(std::clamp(fill_order_combo->currentIndex(), 0, (int)MatrixWiringOrder::Count - 1))
        : MatrixWiringOrder::HorizontalTopLeftZigzag;
    choice.use_openrgb_matrix = has_openrgb_matrix;

    auto pick_wiring = [this, &choice]() -> bool {
        QStringList items;
        items.reserve((int)MatrixWiringOrder::Count);
        for(int o = 0; o < (int)MatrixWiringOrder::Count; o++)
        {
            items << QString::fromUtf8(MatrixWiringOrderName(static_cast<MatrixWiringOrder>(o)));
        }
        bool ok = false;
        const QString picked = QInputDialog::getItem(
            const_cast<CustomControllerDialog*>(this),
            tr("Strip fill order"),
            tr("Choose how LEDs snake onto the grid\n"
               "(same serpentine presets as OpenRGB's Matrix Map Editor):"),
            items,
            static_cast<int>(choice.wiring),
            false,
            &ok);
        if(!ok)
        {
            return false;
        }
        const int idx = items.indexOf(picked);
        if(idx >= 0)
        {
            choice.wiring = static_cast<MatrixWiringOrder>(idx);
        }
        choice.use_openrgb_matrix = false;
        return true;
    };

    if(!has_openrgb_matrix)
    {
        if(!pick_wiring())
        {
            choice.cancelled = true;
        }
        return choice;
    }

    QMessageBox box(const_cast<CustomControllerDialog*>(this));
    box.setWindowTitle(tr("Import layout"));
    box.setText(tr("OpenRGB already has a matrix map for this source "
                   "(keyboard layout, unused cells, etc.)."));
    box.setInformativeText(tr("Import that map onto the grid, or fill with a strip wiring order instead?"));
    QPushButton* use_map = box.addButton(tr("Use OpenRGB map"), QMessageBox::AcceptRole);
    QPushButton* use_wire = box.addButton(tr("Choose wiring…"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(use_map);
    box.exec();

    if(box.clickedButton() == use_map)
    {
        choice.use_openrgb_matrix = true;
        return choice;
    }
    if(box.clickedButton() == use_wire)
    {
        if(!pick_wiring())
        {
            choice.cancelled = true;
        }
        return choice;
    }

    choice.cancelled = true;
    return choice;
}

bool CustomControllerDialog::EnsureGridFitsFrom(int start_x, int start_y, int span_w, int span_h)
{
    if(!width_spin || !height_spin || span_w <= 0 || span_h <= 0)
    {
        return false;
    }

    const int needed_w = start_x + span_w;
    const int needed_h = start_y + span_h;
    const int cur_w = width_spin->value();
    const int cur_h = height_spin->value();
    if(needed_w <= cur_w && needed_h <= cur_h)
    {
        return true;
    }

    {
        const QSignalBlocker block_w(width_spin);
        const QSignalBlocker block_h(height_spin);
        if(needed_w > cur_w)
        {
            width_spin->setValue(needed_w);
        }
        if(needed_h > cur_h)
        {
            height_spin->setValue(needed_h);
        }
    }
    const bool prev_applying = applying_history_;
    applying_history_ = true;
    dimensionChanged();
    applying_history_ = prev_applying;
    return true;
}

bool CustomControllerDialog::PlaceOpenRgbMatrixMaps(RGBControllerInterface* controller,
                                                    int granularity,
                                                    int item_idx,
                                                    int start_x,
                                                    int start_y)
{
    if(!controller || !resource_manager)
    {
        return false;
    }

    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();

    int cursor_y = start_y;
    int placed = 0;
    int zones_placed = 0;

    auto clear_holes_in_rect = [&](int x0, int y0, int w, int h) {
        for(int y = y0; y < y0 + h; ++y)
        {
            for(int x = x0; x < x0 + w; ++x)
            {
                matrix_hole_cells.erase(GridCellKey3D(x, y, current_layer));
            }
        }
    };

    auto place_led_at = [&](unsigned int zone_idx, unsigned int led_idx, int x, int y) -> bool {
        if(IsLedMappedOnLayer(led_mappings, controller, zone_idx, led_idx, current_layer, controllers))
        {
            return false;
        }
        std::vector<GridLEDMapping> replaced_mappings;
        CollectMappingsAtCell(led_mappings, x, y, current_layer, replaced_mappings);
        RestoreIdentifyForMappings(replaced_mappings);
        RemoveMappingsAtCell(led_mappings, x, y, current_layer);

        GridLEDMapping mapping;
        mapping.x = x;
        mapping.y = y;
        mapping.z = current_layer;
        mapping.controller = controller;
        mapping.zone_idx = zone_idx;
        mapping.led_idx = led_idx;
        mapping.granularity = 2;
        CustomControllerMapping::FinalizeMapping(mapping);
        led_mappings.push_back(mapping);
        return true;
    };

    auto place_matrix_zone = [&](unsigned int zone_idx) -> bool {
        if(!ZoneHasOpenRgbMatrixMap(controller, zone_idx))
        {
            return false;
        }

        const matrix_map_type map = controller->GetZoneMatrixMap(zone_idx);
        const int map_w = static_cast<int>(map.width);
        const int map_h = static_cast<int>(map.height);
        if(!EnsureGridFitsFrom(start_x, cursor_y, map_w, map_h))
        {
            return false;
        }

        clear_holes_in_rect(start_x, cursor_y, map_w, map_h);

        for(unsigned int led_y = 0; led_y < map.height; led_y++)
        {
            for(unsigned int led_x = 0; led_x < map.width; led_x++)
            {
                const unsigned int map_idx = led_y * map.width + led_x;
                const int gx = start_x + static_cast<int>(led_x);
                const int gy = cursor_y + static_cast<int>(led_y);

                if(map.map[map_idx] == kMatrixMapUnused)
                {
                    matrix_hole_cells.insert(GridCellKey3D(gx, gy, current_layer));
                    continue;
                }

                zone zone_data = controller->GetZone(zone_idx);
                unsigned int zone_led_idx = 0;
                if(!TryResolveZoneLedFromMatrixValue(&zone_data, map.map[map_idx], &zone_led_idx))
                {
                    matrix_hole_cells.insert(GridCellKey3D(gx, gy, current_layer));
                    continue;
                }

                unsigned int global_led_idx = 0;
                if(!TryGetDialogGlobalLedIndex(controller, zone_idx, zone_led_idx, &global_led_idx)
                   || !IsAssignableControllerLed(controller, global_led_idx))
                {
                    matrix_hole_cells.insert(GridCellKey3D(gx, gy, current_layer));
                    continue;
                }

                if(place_led_at(zone_idx, zone_led_idx, gx, gy))
                {
                    placed++;
                }
            }
        }

        cursor_y += map_h + 1;
        zones_placed++;
        return true;
    };

    if(granularity == 1)
    {
        if(!place_matrix_zone(static_cast<unsigned int>(item_idx)))
        {
            QMessageBox::warning(this, tr("No matrix map"),
                                 tr("That zone has no OpenRGB matrix map to import."));
            return false;
        }
    }
    else
    {
        for(unsigned int z = 0; z < controller->GetZoneCount(); z++)
        {
            if(ZoneHasOpenRgbMatrixMap(controller, z))
            {
                place_matrix_zone(z);
            }
        }
        if(zones_placed == 0)
        {
            QMessageBox::warning(this, tr("No matrix map"),
                                 tr("This device has no OpenRGB matrix maps to import."));
            return false;
        }
    }

    UpdateGridDisplay();
    return placed > 0 || zones_placed > 0;
}

bool CustomControllerDialog::PlaceLinearWiringLayout(RGBControllerInterface* controller,
                                                     int granularity,
                                                     int item_idx,
                                                     int start_x,
                                                     int start_y,
                                                     MatrixWiringOrder fill_order)
{
    if(!controller || !resource_manager)
    {
        return false;
    }

    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();

    int grid_w = width_spin->value();
    int grid_h = height_spin->value();

    const std::vector<LEDPosition3D> positions =
        ControllerLayout3D::GenerateCustomGridLayout(controller, grid_w, grid_h, false, fill_order, false);

    const ProfileLayoutBounds bounds =
        ComputeProfileLayoutBounds(PositionsForLayoutBounds(positions, granularity, item_idx));

    if(bounds.valid)
    {
        const int span_w = static_cast<int>(std::lround(bounds.max_x - bounds.min_x)) + 1;
        const int span_h = static_cast<int>(std::lround(bounds.max_y - bounds.min_y)) + 1;
        EnsureGridFitsFrom(start_x, start_y, span_w, span_h);
        grid_w = width_spin->value();
        grid_h = height_spin->value();
    }

    auto place_profile_position = [&](const LEDPosition3D& pos) -> bool
    {
        int x = 0;
        int y = 0;
        if(!ProfileCellToGrid(bounds, pos, start_x, start_y, grid_w, grid_h, &x, &y))
        {
            return false;
        }
        if(IsMatrixHoleCell(x, y))
        {
            return false;
        }

        if(IsLedMappedOnLayer(led_mappings, controller, pos.zone_idx, pos.led_idx, current_layer, controllers))
        {
            return false;
        }

        std::vector<GridLEDMapping> replaced_mappings;
        CollectMappingsAtCell(led_mappings, x, y, current_layer, replaced_mappings);
        RestoreIdentifyForMappings(replaced_mappings);
        RemoveMappingsAtCell(led_mappings, x, y, current_layer);

        GridLEDMapping mapping;
        mapping.x = x;
        mapping.y = y;
        mapping.z = current_layer;
        mapping.controller = controller;
        mapping.zone_idx = pos.zone_idx;
        mapping.led_idx = pos.led_idx;
        mapping.granularity = 2;
        CustomControllerMapping::FinalizeMapping(mapping);
        led_mappings.push_back(mapping);
        return true;
    };

    if(granularity == 2)
    {
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            unsigned int global_led_idx = 0;
            if(!TryGetDialogGlobalLedIndex(controller, positions[p].zone_idx, positions[p].led_idx, &global_led_idx))
            {
                continue;
            }
            if(global_led_idx != static_cast<unsigned int>(item_idx))
            {
                continue;
            }
            if(IsLedMappedOnLayer(led_mappings, controller, positions[p].zone_idx, positions[p].led_idx,
                                 current_layer, controllers))
            {
                return false;
            }
            if(IsMatrixHoleCell(start_x, start_y))
            {
                return false;
            }

            std::vector<GridLEDMapping> replaced_mappings;
            CollectMappingsAtCell(led_mappings, start_x, start_y, current_layer, replaced_mappings);
            RestoreIdentifyForMappings(replaced_mappings);
            RemoveMappingsAtCell(led_mappings, start_x, start_y, current_layer);

            GridLEDMapping mapping;
            mapping.x = start_x;
            mapping.y = start_y;
            mapping.z = current_layer;
            mapping.controller = controller;
            mapping.zone_idx = positions[p].zone_idx;
            mapping.led_idx = positions[p].led_idx;
            mapping.granularity = 2;
            CustomControllerMapping::FinalizeMapping(mapping);
            led_mappings.push_back(mapping);
            UpdateGridDisplay();
            return true;
        }
        return false;
    }

    int placed = 0;
    int skipped = 0;
    for(unsigned int p = 0; p < positions.size(); p++)
    {
        if(granularity == 1 && positions[p].zone_idx != static_cast<unsigned int>(item_idx))
        {
            continue;
        }
        if(place_profile_position(positions[p]))
        {
            placed++;
        }
        else
        {
            skipped++;
        }
    }

    if(skipped > 0)
    {
        if(granularity == 0)
        {
            QMessageBox::information(this, tr("Grid too small"),
                                     tr("Placed %1 of %2 LEDs (%3 could not fit). "
                                        "Move the anchor cell or pick another Strip fill order.")
                                     .arg(placed).arg(static_cast<int>(positions.size())).arg(skipped));
        }
        else
        {
            QMessageBox::information(this, tr("Grid too small"),
                                     tr("Placed %1 zone LEDs (%2 could not fit). "
                                        "Move the anchor cell or pick another Strip fill order.")
                                     .arg(placed).arg(skipped));
        }
    }

    UpdateGridDisplay();
    return placed > 0;
}

bool CustomControllerDialog::PlaceProfileLayout(RGBControllerInterface* controller,
                                                int granularity,
                                                int item_idx,
                                                int start_x,
                                                int start_y,
                                                const PlaceLayoutChoice& layout_choice)
{
    if(!controller || layout_choice.cancelled)
    {
        return false;
    }

    if(granularity == 2)
    {
        return PlaceLinearWiringLayout(controller, granularity, item_idx, start_x, start_y, layout_choice.wiring);
    }

    if(layout_choice.use_openrgb_matrix
       && SourceHasOpenRgbMatrixMap(controller, granularity, item_idx))
    {
        if(granularity == 1)
        {
            return PlaceOpenRgbMatrixMaps(controller, granularity, item_idx, start_x, start_y);
        }

        bool any = PlaceOpenRgbMatrixMaps(controller, granularity, item_idx, start_x, start_y);

        int cursor_y = start_y;
        for(const GridLEDMapping& mapping : led_mappings)
        {
            if(mapping.z == current_layer && mapping.controller == controller)
            {
                cursor_y = std::max(cursor_y, mapping.y + 1);
            }
        }
        for(const uint64_t key : matrix_hole_cells)
        {
            int hx = 0;
            int hy = 0;
            int hz = 0;
            DecodeGridCellKey3D(key, &hx, &hy, &hz);
            if(hz == current_layer)
            {
                cursor_y = std::max(cursor_y, hy + 1);
            }
        }

        for(unsigned int z = 0; z < controller->GetZoneCount(); z++)
        {
            if(ZoneHasOpenRgbMatrixMap(controller, z) || controller->GetZoneLEDsCount(z) == 0)
            {
                continue;
            }
            if(PlaceLinearWiringLayout(controller, 1, static_cast<int>(z), start_x, cursor_y + 1, layout_choice.wiring))
            {
                any = true;
                for(const GridLEDMapping& mapping : led_mappings)
                {
                    if(mapping.z == current_layer
                       && mapping.controller == controller
                       && mapping.zone_idx == z)
                    {
                        cursor_y = std::max(cursor_y, mapping.y);
                    }
                }
            }
        }
        return any;
    }

    return PlaceLinearWiringLayout(controller, granularity, item_idx, start_x, start_y, layout_choice.wiring);
}

void CustomControllerDialog::resetGridViewClicked()
{
    if(layout_grid)
    {
        layout_grid->FitGridInView();
    }
}

void CustomControllerDialog::fitDeviceLayoutClicked()
{
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    bool has_mappings = false;

    for(const GridLEDMapping& mapping : led_mappings)
    {
        if(mapping.z != current_layer)
        {
            continue;
        }

        if(!has_mappings)
        {
            min_x = max_x = mapping.x;
            min_y = max_y = mapping.y;
            has_mappings = true;
        }
        else
        {
            min_x = std::min(min_x, mapping.x);
            min_y = std::min(min_y, mapping.y);
            max_x = std::max(max_x, mapping.x);
            max_y = std::max(max_y, mapping.y);
        }
    }

    if(!has_mappings)
    {
        QMessageBox::warning(this, tr("No LEDs placed"),
                             tr("Place LEDs on this layer first, then use Fit layout."));
        return;
    }

    RecordUndoPoint();
    const int new_w = max_x - min_x + 1;
    const int new_h = max_y - min_y + 1;

    if(min_x != 0 || min_y != 0)
    {
        for(GridLEDMapping& mapping : led_mappings)
        {
            if(mapping.z == current_layer)
            {
                mapping.x -= min_x;
                mapping.y -= min_y;
            }
        }

        if(!matrix_hole_cells.empty())
        {
            std::unordered_set<uint64_t> shifted_holes;
            shifted_holes.reserve(matrix_hole_cells.size());
            for(const uint64_t key : matrix_hole_cells)
            {
                int x = 0;
                int y = 0;
                int z = 0;
                DecodeGridCellKey3D(key, &x, &y, &z);
                if(z != current_layer)
                {
                    shifted_holes.insert(key);
                    continue;
                }
                const int nx = x - min_x;
                const int ny = y - min_y;
                if(nx >= 0 && nx < new_w && ny >= 0 && ny < new_h)
                {
                    shifted_holes.insert(GridCellKey3D(nx, ny, z));
                }
            }
            matrix_hole_cells = std::move(shifted_holes);
        }

        if(!light_blocker_cells_.empty())
        {
            std::unordered_set<uint64_t> shifted_blockers;
            shifted_blockers.reserve(light_blocker_cells_.size());
            for(const uint64_t key : light_blocker_cells_)
            {
                int x = 0;
                int y = 0;
                int z = 0;
                DecodeGridCellKey3D(key, &x, &y, &z);
                if(z != current_layer)
                {
                    shifted_blockers.insert(key);
                    continue;
                }
                const int nx = x - min_x;
                const int ny = y - min_y;
                if(nx >= 0 && nx < new_w && ny >= 0 && ny < new_h)
                {
                    shifted_blockers.insert(GridCellKey3D(nx, ny, z));
                }
            }
            light_blocker_cells_ = std::move(shifted_blockers);
        }
    }
    else if(!matrix_hole_cells.empty())
    {
        std::unordered_set<uint64_t> clipped_holes;
        clipped_holes.reserve(matrix_hole_cells.size());
        for(const uint64_t key : matrix_hole_cells)
        {
            int x = 0;
            int y = 0;
            int z = 0;
            DecodeGridCellKey3D(key, &x, &y, &z);
            if(z != current_layer)
            {
                clipped_holes.insert(key);
                continue;
            }
            if(x >= 0 && x < new_w && y >= 0 && y < new_h)
            {
                clipped_holes.insert(key);
            }
        }
        matrix_hole_cells = std::move(clipped_holes);
    }

    width_spin->blockSignals(true);
    height_spin->blockSignals(true);
    width_spin->setValue(std::max(1, new_w));
    height_spin->setValue(std::max(1, new_h));
    width_spin->blockSignals(false);
    height_spin->blockSignals(false);

    selected_col = 0;
    selected_row = 0;
    if(layout_grid)
    {
        layout_grid->SetAnchorCell(0, 0);
        layout_grid->SetSelectedCells({std::make_pair(0, 0)});
    }

    RebuildLayerTabs();
    UpdateGridDisplay();
    UpdateCellInfo();
    refreshDeviceList();
    UpdateIdentifyButtonUi();
    CommitHistoryBaseline();
}

void CustomControllerDialog::RestoreAllIdentifiedLeds()
{
    if(identified_leds.empty())
    {
        return;
    }

    std::set<RGBControllerInterface*> updated_controllers;
    for(const std::pair<const std::pair<RGBControllerInterface*, unsigned int>, RGBColor>& entry : identified_leds)
    {
        RGBControllerInterface* controller = entry.first.first;
        if(!controller)
        {
            continue;
        }

        controller->SetColor(entry.first.second, entry.second);
        updated_controllers.insert(controller);
    }

    identified_leds.clear();

    for(RGBControllerInterface* controller : updated_controllers)
    {
        controller->UpdateLEDs();
    }
}

void CustomControllerDialog::RestoreIdentifyForMappings(const std::vector<GridLEDMapping>& mappings)
{
    if(identified_leds.empty() || mappings.empty())
    {
        return;
    }

    std::set<RGBControllerInterface*> updated_controllers;
    for(const GridLEDMapping& mapping : mappings)
    {
        if(!mapping.controller)
        {
            continue;
        }

        unsigned int global_led_idx = 0;
        if(!TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx))
        {
            continue;
        }

        const auto led_key = std::make_pair(mapping.controller, global_led_idx);
        const auto it = identified_leds.find(led_key);
        if(it == identified_leds.end())
        {
            continue;
        }

        mapping.controller->SetColor(global_led_idx, it->second);
        identified_leds.erase(it);
        updated_controllers.insert(mapping.controller);
    }

    for(RGBControllerInterface* controller : updated_controllers)
    {
        controller->UpdateLEDs();
    }
}

void CustomControllerDialog::SetIdentifyForCells(const std::set<std::pair<int, int>>& cells, bool enabled)
{
    if(cells.empty())
    {
        return;
    }

    std::vector<GridLEDMapping> cell_mappings;
    CollectMappingsForCells(cells, current_layer, led_mappings, cell_mappings);
    if(cell_mappings.empty())
    {
        return;
    }

    std::set<RGBControllerInterface*> updated_controllers;
    std::set<std::pair<RGBControllerInterface*, unsigned int>> seen_leds;

    for(const GridLEDMapping& mapping : cell_mappings)
    {
        unsigned int global_led_idx = 0;
        if(!TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx))
        {
            continue;
        }

        const auto led_key = std::make_pair(mapping.controller, global_led_idx);
        if(!seen_leds.insert(led_key).second)
        {
            continue;
        }

        if(enabled)
        {
            if(identified_leds.count(led_key) > 0)
            {
                continue;
            }

            identified_leds[led_key] = mapping.controller->GetColor(global_led_idx);
            mapping.controller->SetColor(global_led_idx, ToRGBColor(0, 255, 0));
            updated_controllers.insert(mapping.controller);
        }
        else
        {
            const auto it = identified_leds.find(led_key);
            if(it == identified_leds.end())
            {
                continue;
            }

            mapping.controller->SetColor(global_led_idx, it->second);
            identified_leds.erase(it);
            updated_controllers.insert(mapping.controller);
        }
    }

    for(RGBControllerInterface* controller : updated_controllers)
    {
        controller->UpdateLEDs();
    }

    UpdateIdentifyButtonUi();
    UpdateGridColors();
}

void CustomControllerDialog::UpdateIdentifyButtonUi()
{
    if(!identify_button)
    {
        return;
    }

    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    const IdentifyUiState ui_state = EvaluateIdentifyUiState(selected_cells, current_layer, led_mappings, identified_leds);

    switch(ui_state)
    {
    case IdentifyUiState::NoSelection:
        identify_button->setEnabled(false);
        identify_button->setText(tr("Identify · Off"));
        break;
    case IdentifyUiState::NoMapped:
        identify_button->setEnabled(false);
        identify_button->setText(tr("Identify · Off"));
        break;
    case IdentifyUiState::AllOff:
        identify_button->setEnabled(true);
        identify_button->setText(tr("Identify · Off"));
        break;
    case IdentifyUiState::AllOn:
        identify_button->setEnabled(true);
        identify_button->setText(tr("Identify · On"));
        break;
    case IdentifyUiState::Mixed:
        identify_button->setEnabled(true);
        identify_button->setText(tr("Identify"));
        break;
    }
}

void CustomControllerDialog::identifySelectionClicked()
{
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    const IdentifyUiState ui_state = EvaluateIdentifyUiState(selected_cells, current_layer, led_mappings, identified_leds);

    if(ui_state == IdentifyUiState::NoSelection)
    {
        QMessageBox::warning(this, tr("No cell selected"), tr("Select a mapped cell to identify on hardware."));
        return;
    }

    if(ui_state == IdentifyUiState::NoMapped)
    {
        QMessageBox::information(this, tr("Nothing to identify"),
                                 selected_cells.size() > 1
                                     ? tr("None of the selected cells have LED assignments.")
                                     : tr("The selected cell has no LED assignments."));
        return;
    }

    if(ui_state == IdentifyUiState::Mixed)
    {
        SetIdentifyForCells(selected_cells, false);
        return;
    }

    SetIdentifyForCells(selected_cells, ui_state == IdentifyUiState::AllOff);
}

void CustomControllerDialog::clearCellClicked()
{
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    if(selected_cells.empty())
    {
        QMessageBox::warning(this, tr("No selection"), tr("Select one or more cells on the layer grid first."));
        return;
    }

    RecordUndoPoint();
    ClearSelectedCellContents();
    CommitHistoryBaseline();
    UpdateGridDisplay();
    UpdateCellInfo();
    refreshDeviceList();
    UpdateIdentifyButtonUi();
}

void CustomControllerDialog::removeAllLedsClicked()
{
    if(led_mappings.empty() && light_blocker_cells_.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("The grid has no LED assignments or light blockers."));
        return;
    }

    int reply = QMessageBox::question(this,
                                      tr("Clear grid"),
                                      tr("Remove every LED assignment and light blocker from the grid? "
                                         "(%1 assignment(s), %2 blocker cell(s)).")
                                          .arg(led_mappings.size())
                                          .arg(light_blocker_cells_.size()),
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);

    if(reply == QMessageBox::Yes)
    {
        RecordUndoPoint();
        const size_t removed_count = led_mappings.size();
        RestoreAllIdentifiedLeds();
        led_mappings.clear();
        matrix_hole_cells.clear();
        light_blocker_cells_.clear();

        QMessageBox::information(this,
                                 tr("Removed"),
                                 tr("Cleared %1 LED assignment(s) and all light blockers.")
                                     .arg(static_cast<int>(removed_count)));

        EnsureGridAnchorSelected();
        UpdateGridDisplay();
        UpdateCellInfo();
        refreshDeviceList();
        UpdateIdentifyButtonUi();
        CommitHistoryBaseline();
    }
}

void CustomControllerDialog::addLightBlockerClicked()
{
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    if(selected_cells.empty())
    {
        QMessageBox::warning(this, tr("No selection"), tr("Select one or more cells on the layer grid first."));
        return;
    }

    RecordUndoPoint();
    for(const std::pair<int, int>& cell : selected_cells)
    {
        if(IsMatrixHoleCell(cell.first, cell.second))
        {
            continue;
        }
        light_blocker_cells_.insert(GridCellKey3D(cell.first, cell.second, current_layer));
    }
    CommitHistoryBaseline();

    UpdateGridDisplay();
    UpdateCellInfo();
}

void CustomControllerDialog::saveClicked()
{
    if(name_edit->text().isEmpty())
    {
        QMessageBox::warning(this, "No Name", "Please enter a name for the custom controller");
        return;
    }

    int w = width_spin->value();
    int h = height_spin->value();
    int d = depth_spin->value();
    size_t removed = 0;

    for(std::vector<GridLEDMapping>::iterator it = led_mappings.begin(); it != led_mappings.end(); )
    {
        if(it->x < 0 || it->x >= w || it->y < 0 || it->y >= h || it->z < 0 || it->z >= d)
        {
            it = led_mappings.erase(it);
            removed++;
        }
        else
        {
            ++it;
        }
    }
    if(removed > 0)
    {
        QMessageBox::information(this, "Mappings Cleaned",
            QString("Some invalid mappings (outside current grid bounds) were removed."));
    }

    for(auto it = light_blocker_cells_.begin(); it != light_blocker_cells_.end();)
    {
        int x = 0;
        int y = 0;
        int z = 0;
        DecodeGridCellKey3D(*it, &x, &y, &z);
        if(x < 0 || x >= w || y < 0 || y >= h || z < 0 || z >= d)
        {
            it = light_blocker_cells_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if(led_mappings.empty() && light_blocker_cells_.empty())
    {
        QMessageBox::warning(this,
                             tr("Empty layout"),
                             tr("Assign at least one LED or add at least one light blocker cell."));
        return;
    }

    const int unresolved = CustomControllerMapping::UnresolvedCount(led_mappings);
    if(unresolved > 0)
    {
        const QMessageBox::StandardButton reply =
            QMessageBox::warning(this,
                                 tr("Missing devices"),
                                 tr("%1 grid cell(s) reference OpenRGB devices that are not connected. "
                                    "Those cells will not light up until the devices are found again.\n\n"
                                    "Save anyway?")
                                     .arg(unresolved),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No);
        if(reply != QMessageBox::Yes)
        {
            return;
        }
    }

    accept();
}

bool CustomControllerDialog::IsItemAssigned(RGBControllerInterface* controller, int granularity, int item_idx) const
{
    if(!controller)
    {
        return false;
    }

    const std::vector<GridLEDMapping>& mappings = led_mappings;

    auto mapping_owned_by_controller = [&](const GridLEDMapping& mapping) -> bool
    {
        return mapping.controller == controller;
    };

    if(granularity == 0)
    {
        for(unsigned int i = 0; i < mappings.size(); i++)
        {
            if(mapping_owned_by_controller(mappings[i]))
            {
                return true;
            }
        }
    }
    else if(granularity == 1)
    {
        for(unsigned int i = 0; i < mappings.size(); i++)
        {
            if(mapping_owned_by_controller(mappings[i]) && mappings[i].zone_idx == (unsigned int)item_idx)
            {
                return true;
            }
        }
    }
    else if(granularity == 2)
    {
        if(!IsAssignableControllerLed(controller, (unsigned int)item_idx))
        {
            return true;
        }
        for(unsigned int i = 0; i < mappings.size(); i++)
        {
            if(!mapping_owned_by_controller(mappings[i]))
            {
                continue;
            }
            unsigned int global_led_idx = 0;
            if(!TryGetDialogGlobalLedIndex(controller, mappings[i].zone_idx, mappings[i].led_idx, &global_led_idx))
                continue;
            if(global_led_idx == (unsigned int)item_idx)
            {
                return true;
            }
        }
    }
    return false;
}

void CustomControllerDialog::LoadExistingController(const std::string& name,
                                                    int width,
                                                    int height,
                                                    int depth,
                                                    const std::vector<GridLEDMapping>& mappings,
                                                    const std::vector<CustomControllerLightBlocker>& light_blockers,
                                                    const std::vector<float>& column_widths_mm,
                                                    const std::vector<float>& row_heights_mm,
                                                    const std::vector<float>& layer_depths_mm,
                                                    const std::vector<std::string>& layer_names,
                                                    int leds_per_cluster)
{
    setWindowTitle(tr("Edit Custom 3D Controller"));
    name_edit->setText(QString::fromStdString(name));

    const QSignalBlocker width_block(width_spin);
    const QSignalBlocker height_block(height_spin);
    const QSignalBlocker depth_block(depth_spin);
    const QSignalBlocker leds_per_section_block(leds_per_section_combo);
    width_spin->setValue(width);
    height_spin->setValue(height);
    depth_spin->setValue(depth);
    if(leds_per_section_combo)
    {
        leds_per_section_combo->setCurrentIndex((leds_per_cluster > 1) ? 1 : 0);
    }
    led_mappings = mappings;
    if(resource_manager)
    {
        std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();
        CustomControllerMapping::RebindAll(led_mappings, controllers);
    }
    light_blocker_cells_.clear();
    for(const CustomControllerLightBlocker& blocker : light_blockers)
    {
        light_blocker_cells_.insert(GridCellKey3D(blocker.x, blocker.y, blocker.z));
    }

    column_widths_mm_ = column_widths_mm;
    row_heights_mm_   = row_heights_mm;
    layer_depths_mm_  = layer_depths_mm;
    layer_names_      = layer_names;
    EnsureDialogGridSizeArrays();
    EnsureLayerNamesArray();
    RebuildLayerTabs();
    SyncLayerDepthSpinFromCurrentLayer();

    InferMappingGranularity();
    UpdateGridDisplay();
    EnsureGridAnchorSelected();
    refreshDeviceList();
    ResetHistoryFromCurrent();
}

QColor CustomControllerDialog::GetItemColor(RGBControllerInterface* controller, int granularity, int item_idx) const
{
    if(!controller) return QColor(128, 128, 128);

    if(granularity == 0)
    {
        return GetAverageDeviceColor(controller);
    }
    else if(granularity == 1)
    {
        if(item_idx >= 0 && item_idx < (int)controller->GetZoneCount())
        {
            return GetAverageZoneColor(controller, item_idx);
        }
    }
    else if(granularity == 2)
    {
        if(item_idx >= 0 && item_idx < (int)controller->GetLEDCount())
        {
            return RGBToQColor(controller->GetColor(item_idx));
        }
    }
    return QColor(128, 128, 128);
}

QColor CustomControllerDialog::GetAverageZoneColor(RGBControllerInterface* controller, unsigned int zone_idx) const
{
    if(zone_idx >= controller->GetZoneCount()) return QColor(128, 128, 128);

    const zone z = controller->GetZone(zone_idx);
    if(z.leds_count == 0) return QColor(128, 128, 128);

    unsigned int total_r = 0, total_g = 0, total_b = 0;
    unsigned int led_count = 0;

    for(unsigned int i = 0; i < z.leds_count && (z.start_idx + i) < controller->GetLEDCount(); i++)
    {
        unsigned int color = controller->GetColor(z.start_idx + i);
        total_r += (color >> 0) & 0xFF;
        total_g += (color >> 8) & 0xFF;
        total_b += (color >> 16) & 0xFF;
        led_count++;
    }

    if(led_count == 0) return QColor(128, 128, 128);

    return QColor(static_cast<int>(total_r / led_count), static_cast<int>(total_g / led_count), static_cast<int>(total_b / led_count));
}

QColor CustomControllerDialog::GetAverageDeviceColor(RGBControllerInterface* controller) const
{
    if(!controller || controller->GetLEDCount() == 0) return QColor(128, 128, 128);

    unsigned long long total_r = 0, total_g = 0, total_b = 0;

    for(unsigned int i = 0; i < controller->GetLEDCount(); i++)
    {
        const RGBColor c = controller->GetColor(i);
        total_r += (c >> 0) & 0xFF;
        total_g += (c >> 8) & 0xFF;
        total_b += (c >> 16) & 0xFF;
    }

    size_t count = controller->GetLEDCount();
    if(count == 0)
    {
        return QColor(0, 0, 0);
    }
    return QColor(static_cast<int>(total_r / count), static_cast<int>(total_g / count), static_cast<int>(total_b / count));
}

QColor CustomControllerDialog::GetMappingColor(const GridLEDMapping& mapping) const
{
    if(!mapping.controller)
        return QColor(128, 128, 128);

    if(mapping.zone_idx >= mapping.controller->GetZoneCount())
        return QColor(128, 128, 128);

    const zone z = mapping.controller->GetZone(mapping.zone_idx);
    unsigned int global_led_idx = z.start_idx + mapping.led_idx;

    if(global_led_idx >= mapping.controller->GetLEDCount())
        return QColor(128, 128, 128);

    return RGBToQColor(mapping.controller->GetColor(global_led_idx));
}

QString CustomControllerDialog::GetMappingCellLabel(const std::vector<GridLEDMapping>& cell_mappings) const
{
    if(cell_mappings.empty())
    {
        return QString();
    }

    QStringList name_parts;
    name_parts.reserve(static_cast<int>(cell_mappings.size()));
    for(const GridLEDMapping& mapping : cell_mappings)
    {
        if(!MappingHasZoneLed(mapping) || !mapping.controller)
        {
            continue;
        }
        name_parts << ShortOpenRgbLedCellLabel(mapping.controller, mapping.zone_idx, mapping.led_idx);
    }

    if(name_parts.isEmpty())
    {
        return QString::number(cell_mappings.size());
    }

    if(name_parts.size() == 1)
    {
        return name_parts.front();
    }
    if(name_parts.size() <= 2)
    {
        return name_parts.join(QLatin1Char(','));
    }
    return QStringLiteral("%1+%2").arg(name_parts.front()).arg(name_parts.size() - 1);
}

QString CustomControllerDialog::GetMappingTooltip(const GridLEDMapping& mapping) const
{
    if(!mapping.controller)
    {
        return tr("Unknown device (not found on this system)");
    }

    const QString grid_pos = tr("Grid X=%1, Y=%2, Z=%3").arg(mapping.x).arg(mapping.y).arg(mapping.z);
    const QString device_name = ControllerDisplay::FormatRgbControllerTitle(mapping.controller);

    if(!MappingHasZoneLed(mapping))
    {
        if(mapping.granularity == 0)
        {
            return tr("%1\nWhole device\n%2").arg(device_name, grid_pos);
        }
        if(mapping.granularity == 1)
        {
            const QString zone_name = mapping.zone_idx < mapping.controller->GetZoneCount()
                ? QString::fromStdString(mapping.controller->GetZoneName(mapping.zone_idx))
                : tr("Unknown zone");
            return tr("%1\nZone: %2\n%3").arg(device_name, zone_name, grid_pos);
        }
        return tr("%1\n%2").arg(device_name, grid_pos);
    }

    QString led_name = tr("Unknown LED");
    unsigned int global_led_idx = 0;
    const bool has_global = TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx)
                            && global_led_idx < mapping.controller->GetLEDCount();
    if(has_global)
    {
        led_name = QString::fromStdString(mapping.controller->GetLEDName(global_led_idx));
    }

    const QString zone_name = mapping.zone_idx < mapping.controller->GetZoneCount()
        ? QString::fromStdString(mapping.controller->GetZoneName(mapping.zone_idx))
        : tr("Unknown zone");
    const unsigned int display_led = MappingDisplayLedNumber(mapping);

    if(has_global)
    {
        return tr("%1\n%2\nZone LED %3 (global %4)\n%5\n%6")
            .arg(device_name, zone_name)
            .arg(display_led)
            .arg(global_led_idx)
            .arg(led_name, grid_pos);
    }

    return tr("%1\n%2\nZone LED %3\n%4\n%5")
        .arg(device_name, zone_name)
        .arg(display_led)
        .arg(led_name, grid_pos);
}

QString CustomControllerDialog::GetMappingDescription(const GridLEDMapping& mapping) const
{
    if(!mapping.controller)
    {
        return tr("Unknown device (not found on this system)");
    }

    const QString name = ControllerDisplay::FormatRgbControllerTitle(mapping.controller);
    if(MappingHasZoneLed(mapping))
    {
        QString led_name = tr("Unknown LED");
        unsigned int global_led_idx = 0;
        if(TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx) &&
           global_led_idx < mapping.controller->GetLEDCount())
        {
            led_name = QString::fromStdString(mapping.controller->GetLEDName(global_led_idx));
        }

        if(TryGetDialogGlobalLedIndex(mapping.controller, mapping.zone_idx, mapping.led_idx, &global_led_idx))
        {
            return tr("Assigned: %1, LED %3: %2 (global %4)")
                .arg(name, led_name)
                .arg(MappingDisplayLedNumber(mapping))
                .arg(global_led_idx);
        }
        return tr("Assigned: %1, LED %3: %2")
            .arg(name, led_name)
            .arg(MappingDisplayLedNumber(mapping));
    }

    if(mapping.granularity == 0)
    {
        return tr("Assigned: %1 (Whole Device)").arg(name);
    }
    if(mapping.granularity == 1)
    {
        const QString zone_name = mapping.zone_idx < mapping.controller->GetZoneCount()
            ? QString::fromStdString(mapping.controller->GetZoneName(mapping.zone_idx))
            : tr("Unknown Zone");
        return tr("Assigned: %1, Zone: %2").arg(name, zone_name);
    }
    return tr("Assigned: %1").arg(name);
}

void CustomControllerDialog::InferMappingGranularity()
{
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        if(led_mappings[i].granularity < 0 || led_mappings[i].granularity > 2)
        {
            led_mappings[i].granularity = 2;
        }
        if(MappingHasZoneLed(led_mappings[i]))
        {
            led_mappings[i].granularity = 2;
        }
    }
}

QColor CustomControllerDialog::RGBToQColor(unsigned int rgb_value)
{
    unsigned int r = (rgb_value >> 0) & 0xFF;
    unsigned int g = (rgb_value >> 8) & 0xFF;
    unsigned int b = (rgb_value >> 16) & 0xFF;
    return QColor(r, g, b);
}

