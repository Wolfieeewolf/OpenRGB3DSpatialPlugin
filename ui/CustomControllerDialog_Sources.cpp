// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerDialog.h"
#include "CustomControllerDialog_Internal.h"
#include "CustomControllerClipboard.h"
#include "CustomControllerMappingUtils.h"
#include "CustomControllerGridKeys.h"
#include "ControllerDisplayUtils.h"
#include "CustomControllerDeviceList.h"
#include "ControllerLayout3D.h"
#include "SpatialTabLedHelpers.h"
#include "custom-controller-grid/CustomControllerLayoutGrid.h"
#include "custom-controller-grid/CustomControllerGridLayoutMath.h"

#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <map>
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
        removeSourceFromGrid(source);
        refreshDeviceList();
        UpdateCellInfo();
        UpdateSummaryLabel();
        UpdateIdentifyButtonUi();
        UpdateGridColors();
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

QColor CustomControllerDialog::SourceItemColor(const CustomControllerSourceRef& source) const
{
    RGBControllerInterface* controller = controllerForSource(source);
    if(!controller)
    {
        return QColor();
    }
    return GetItemColor(controller, source.granularity, source.item_idx);
}

void CustomControllerDialog::refreshDeviceList(int controller_index)
{
    if(device_list)
    {
        device_list->refreshFromHost(controller_index);
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
            if(!IsItemAssigned(controller, granularity, static_cast<int>(i)))
            {
                const QColor color = GetItemColor(controller, granularity, static_cast<int>(i));
                QPixmap pixmap(16, 16);
                pixmap.fill(color);
                combo->addItem(QIcon(pixmap), QString::fromStdString(controller->GetZoneName(i)),
                               static_cast<int>(i));
            }
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
            if(!IsItemAssigned(controller, granularity, static_cast<int>(i)))
            {
                const QColor color = GetItemColor(controller, granularity, static_cast<int>(i));
                QPixmap pixmap(16, 16);
                pixmap.fill(color);
                combo->addItem(QIcon(pixmap), QString::fromStdString(controller->GetLEDName(i)), static_cast<int>(i));
            }
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

    std::vector<GridLEDMapping> replaced_mappings;
    CollectMappingsAtCell(led_mappings, selected_col, selected_row, current_layer, replaced_mappings);
    RestoreIdentifyForMappings(replaced_mappings);
    RemoveMappingsAtCell(led_mappings, selected_col, selected_row, current_layer);

    PlaceProfileLayout(controller, source.granularity, source.item_idx, selected_col, selected_row);
    UpdateCellInfo();
    UpdateSummaryLabel();
    UpdateIdentifyButtonUi();
    UpdateGridColors();
    refreshDeviceList(source.controller_index);
    syncPreviewLayoutIfVisible();
    return true;
}

bool CustomControllerDialog::PlaceProfileLayout(RGBControllerInterface* controller, int granularity, int item_idx, int start_x, int start_y)
{
    if(!controller || !resource_manager)
    {
        return false;
    }

    std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();

    int grid_w = width_spin->value();
    int grid_h = height_spin->value();

    const std::vector<LEDPosition3D> positions =
        ControllerLayout3D::GenerateCustomGridLayout(controller, grid_w, grid_h, false);

    const ProfileLayoutBounds bounds =
        ComputeProfileLayoutBounds(PositionsForLayoutBounds(positions, granularity, item_idx));

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

    if(granularity == 0)
    {
        int placed = 0;
        int skipped = 0;
        for(unsigned int p = 0; p < positions.size(); p++)
        {
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
            QMessageBox::information(this, tr("Grid too small"),
                                     tr("Placed %1 of %2 LEDs using the device profile (%3 could not fit). Move the anchor cell or use Fit layout.")
                                     .arg(placed).arg(static_cast<int>(positions.size())).arg(skipped));
        }

        if(placed > 0)
        {
            RebuildMatrixHoleMask(controller, start_x, start_y);
        }
    }
    else if(granularity == 1)
    {
        int placed = 0;
        int skipped = 0;
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            if(positions[p].zone_idx != static_cast<unsigned int>(item_idx))
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
            QMessageBox::information(this, tr("Grid too small"),
                                     tr("Placed %1 zone LEDs (%2 could not fit). Move the anchor cell or use Fit layout.")
                                     .arg(placed).arg(skipped));
        }

        if(placed > 0)
        {
            RebuildMatrixHoleMask(controller, start_x, start_y);
        }
    }
    else if(granularity == 2)
    {
        for(unsigned int p = 0; p < positions.size(); p++)
        {
            unsigned int global_led_idx = 0;
            if(!TryGetDialogGlobalLedIndex(controller, positions[p].zone_idx, positions[p].led_idx, &global_led_idx))
            {
                continue;
            }
            if(global_led_idx == static_cast<unsigned int>(item_idx))
            {
                if(IsLedMappedOnLayer(led_mappings, controller, positions[p].zone_idx, positions[p].led_idx,
                                     current_layer, controllers))
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
                return true;
            }
        }
    }

    UpdateGridColors();

    return true;
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
                const int x = static_cast<int>(static_cast<uint32_t>(key >> 32));
                const int y = static_cast<int>(static_cast<uint32_t>(key));
                const int nx = x - min_x;
                const int ny = y - min_y;
                if(nx >= 0 && nx < new_w && ny >= 0 && ny < new_h)
                {
                    shifted_holes.insert(GridCellKey(nx, ny));
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
            const int x = static_cast<int>(static_cast<uint32_t>(key >> 32));
            const int y = static_cast<int>(static_cast<uint32_t>(key));
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
    UpdateSummaryLabel();
    UpdateIdentifyButtonUi();
    syncPreviewLayoutIfVisible();
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

    ClearSelectedCellContents();
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
        const size_t removed_count = led_mappings.size();
        RestoreAllIdentifiedLeds();
        led_mappings.clear();
        matrix_hole_cells.clear();
        light_blocker_cells_.clear();

        QMessageBox::information(this,
                                 tr("Removed"),
                                 tr("Cleared %1 LED assignment(s) and all light blockers.")
                                     .arg(static_cast<int>(removed_count)));

        UpdateGridDisplay();
        UpdateCellInfo();
        refreshDeviceList();
        UpdateIdentifyButtonUi();
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

    for(const std::pair<int, int>& cell : selected_cells)
    {
        light_blocker_cells_.insert(GridCellKey3D(cell.first, cell.second, current_layer));
    }

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
    std::vector<RGBControllerInterface*> controllers;
    if(resource_manager)
    {
        controllers = resource_manager->GetRGBControllers();
    }

    auto mapping_owned_by_controller = [&](const GridLEDMapping& mapping) -> bool
    {
        if(mapping.controller == controller)
        {
            return true;
        }
        if(controllers.empty())
        {
            return false;
        }
        return CustomControllerMapping::MappingOwnedByController(mapping, controller, controllers);
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
    refreshDeviceList();
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

    std::vector<unsigned int> display_numbers;
    display_numbers.reserve(cell_mappings.size());
    for(const GridLEDMapping& mapping : cell_mappings)
    {
        if(MappingHasZoneLed(mapping))
        {
            display_numbers.push_back(MappingDisplayLedNumber(mapping));
        }
    }

    if(display_numbers.empty())
    {
        return QString::number(cell_mappings.size());
    }

    std::sort(display_numbers.begin(), display_numbers.end());
    display_numbers.erase(std::unique(display_numbers.begin(), display_numbers.end()), display_numbers.end());

    if(display_numbers.size() == 1)
    {
        return QString::number(display_numbers.front());
    }

    if(display_numbers.size() <= 2)
    {
        QStringList parts;
        for(unsigned int number : display_numbers)
        {
            parts << QString::number(number);
        }
        return parts.join(QLatin1Char(','));
    }

    return QStringLiteral("%1+%2").arg(display_numbers.front()).arg(display_numbers.size() - 1);
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

