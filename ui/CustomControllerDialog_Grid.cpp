// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerDialog.h"
#include "CustomControllerDialog_Internal.h"
#include "CustomControllerClipboard.h"
#include "CustomControllerGridKeys.h"
#include "ControllerDisplayUtils.h"
#include "custom-controller-grid/CustomControllerLayoutGrid.h"
#include "custom-controller-grid/CustomControllerGridCell.h"
#include "custom-controller-grid/CustomControllerGridLayoutMath.h"
#include "PluginUiUtils.h"

#include <QColor>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVector>
#include <QAction>
#include <QPoint>

#include <algorithm>
#include <functional>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

void CustomControllerDialog::gridCellClicked(int column, int row)
{
    selected_col = column;
    selected_row = row;
    UpdateCellInfo();
    UpdateIdentifyButtonUi();
}

void CustomControllerDialog::gridCellDoubleClicked(int column, int row)
{
    selected_col = column;
    selected_row = row;
    UpdateCellInfo();

    const CustomControllerSourceRef source = currentSourceSelection();
    if(!CanAddSourceToGrid(source))
    {
        return;
    }

    assignSource(source);
}

void CustomControllerDialog::gridSelectionChanged()
{
    if(layout_grid)
    {
        const std::set<std::pair<int, int>> cells = layout_grid->SelectedCells();
        if(!cells.empty())
        {
            const std::pair<int, int> first = *cells.begin();
            selected_col = first.first;
            selected_row = first.second;
            layout_grid->SetAnchorCell(selected_col, selected_row);
        }
    }

    UpdateCellInfo();
    RefreshSelectionFillTints();
    UpdateIdentifyButtonUi();
}

std::set<std::pair<int, int>> CustomControllerDialog::SelectedGridCells() const
{
    if(layout_grid)
    {
        std::set<std::pair<int, int>> cells = layout_grid->SelectedCells();
        if(!cells.empty())
        {
            return cells;
        }
    }

    if(selected_col >= 0 && selected_row >= 0)
    {
        return {std::make_pair(selected_col, selected_row)};
    }

    return {};
}

bool CustomControllerDialog::LayoutCellsCacheMatchesGrid() const
{
    if(!layout_grid || !width_spin || !height_spin)
    {
        return false;
    }

    return layout_cells_cache_w_ == width_spin->value()
        && layout_cells_cache_h_ == height_spin->value()
        && layout_cells_cache_layer_ == current_layer
        && layout_cells_cache_.size() == layout_cells_cache_w_ * layout_cells_cache_h_;
}

QColor CustomControllerDialog::ComputeCellBaseColor(const std::vector<GridLEDMapping>& cell_mappings,
                                                    bool is_hole) const
{
    if(!layout_grid)
    {
        return QColor();
    }

    if(is_hole)
    {
        return PluginUiGridHoleCellColor(layout_grid);
    }

    if(cell_mappings.empty())
    {
        return PluginUiGridEmptyCellColor(layout_grid);
    }

    if(cell_mappings.size() == 1)
    {
        return GetMappingColor(cell_mappings[0]);
    }

    unsigned int total_r = 0;
    unsigned int total_g = 0;
    unsigned int total_b = 0;

    for(const GridLEDMapping& mapping : cell_mappings)
    {
        const QColor led_color = GetMappingColor(mapping);
        total_r += led_color.red();
        total_g += led_color.green();
        total_b += led_color.blue();
    }

    return QColor(static_cast<int>(total_r / cell_mappings.size()),
                  static_cast<int>(total_g / cell_mappings.size()),
                  static_cast<int>(total_b / cell_mappings.size()));
}

void CustomControllerDialog::ApplyCellFillAndText(CustomControllerGridCellVisual& visual,
                                                  const QColor& base_color,
                                                  const std::vector<GridLEDMapping>& cell_mappings,
                                                  bool is_selected) const
{
    if(!layout_grid)
    {
        return;
    }

    if(is_selected)
    {
        const QColor selection_color = PluginUiGridSelectionColor(layout_grid);
        visual.fill = cell_mappings.empty()
            ? selection_color
            : PluginUiBlendColors(base_color, selection_color, 0.7f);
    }
    else
    {
        visual.fill = base_color;
    }

    if(visual.is_light_blocker && cell_mappings.empty())
    {
        visual.fill = is_selected ? PluginUiBlendColors(QColor(70, 45, 95), PluginUiGridSelectionColor(layout_grid), 0.65f)
                                  : QColor(55, 35, 75);
    }

    if(!cell_mappings.empty() || is_selected || visual.is_light_blocker)
    {
        visual.text = PluginUiReadableTextOn(visual.fill, layout_grid);
    }
    else
    {
        visual.text = QColor();
    }
}

void CustomControllerDialog::PopulateCellVisual(int col,
                                                int row,
                                                const std::vector<GridLEDMapping>& cell_mappings,
                                                CustomControllerGridCellVisual& visual,
                                                bool is_selected) const
{
    visual.is_empty         = cell_mappings.empty();
    visual.is_hole          = visual.is_empty && IsMatrixHoleCell(col, row);
    visual.is_light_blocker = IsLightBlockerCell(col, row, current_layer);

    if(!cell_mappings.empty())
    {
        if(cell_mappings.size() == 1)
        {
            visual.tooltip = GetMappingTooltip(cell_mappings[0]);
        }
        else
        {
            visual.tooltip = tr("Multiple LEDs (%1):\n").arg(cell_mappings.size());
            for(size_t m = 0; m < cell_mappings.size() && m < 5; m++)
            {
                visual.tooltip += QString(QChar(0x2022)) + QLatin1Char(' ')
                    + GetMappingTooltip(cell_mappings[m]) + QLatin1Char('\n');
            }
            if(cell_mappings.size() > 5)
            {
                visual.tooltip += tr("... and %1 more").arg(cell_mappings.size() - 5);
            }
        }

        visual.label = GetMappingCellLabel(cell_mappings);
    }
    else if(visual.is_hole)
    {
        visual.tooltip = tr("Matrix gap (no LED on this device)");
        visual.label.clear();
    }
    else if(visual.is_light_blocker)
    {
        visual.tooltip = tr("Light blocker — spatial lighting cannot pass through this cell");
        if(cell_mappings.empty())
        {
            visual.label = QStringLiteral("B");
        }
    }
    else
    {
        visual.tooltip = tr("Empty — click to assign");
        visual.label.clear();
    }

    const QColor base_color = ComputeCellBaseColor(cell_mappings, visual.is_hole);
    ApplyCellFillAndText(visual, base_color, cell_mappings, is_selected);
}

void CustomControllerDialog::RefreshLayoutGridVisuals()
{
    if(!layout_grid)
    {
        return;
    }

    const int grid_w = width_spin->value();
    const int grid_h = height_spin->value();
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();

    layout_grid->SetGridSize(grid_w, grid_h);
    layout_grid->SetMmPerSceneUnit(layout_grid_scale_mm);
    layout_grid->SetColumnWidthsMm(ColumnWidthsQVector());
    layout_grid->SetRowHeightsMm(RowHeightsQVector());
    layout_grid->SetSelectionColor(PluginUiGridSelectionColor(layout_grid));

    const LayerCellIndex layer_index = BuildLayerCellIndex(led_mappings, current_layer);

    layout_cells_cache_.resize(grid_w * grid_h);
    for(int row = 0; row < grid_h; row++)
    {
        for(int col = 0; col < grid_w; col++)
        {
            const int index = row * grid_w + col;
            CustomControllerGridCellVisual& visual = layout_cells_cache_[index];

            std::vector<GridLEDMapping> cell_mappings;
            const auto cell_it = layer_index.find(GridCellKey(col, row));
            if(cell_it != layer_index.end())
            {
                cell_mappings.reserve(cell_it->second.size());
                for(size_t mapping_index : cell_it->second)
                {
                    cell_mappings.push_back(led_mappings[mapping_index]);
                }
            }

            const bool is_selected = selected_cells.count(std::make_pair(col, row)) > 0;
            PopulateCellVisual(col, row, cell_mappings, visual, is_selected);
        }
    }

    layout_cells_cache_w_     = grid_w;
    layout_cells_cache_h_     = grid_h;
    layout_cells_cache_layer_ = current_layer;

    layout_grid->SetCells(layout_cells_cache_);
    layout_grid->SetSelectedCells(selected_cells);
    layout_grid->updateGeometry();
    selection_fill_cache_ = selected_cells;
}

void CustomControllerDialog::RefreshSelectionFillTints()
{
    if(!layout_grid)
    {
        return;
    }

    const std::set<std::pair<int, int>> new_selection = SelectedGridCells();
    if(new_selection == selection_fill_cache_)
    {
        return;
    }

    if(!LayoutCellsCacheMatchesGrid())
    {
        selection_fill_cache_ = new_selection;
        RefreshLayoutGridVisuals();
        return;
    }

    std::set<std::pair<int, int>> affected;
    for(const std::pair<int, int>& cell : new_selection)
    {
        affected.insert(cell);
    }
    for(const std::pair<int, int>& cell : selection_fill_cache_)
    {
        affected.insert(cell);
    }
    selection_fill_cache_ = new_selection;

    if(affected.empty() || static_cast<int>(affected.size()) > kMaxSelectionFillTintUpdates)
    {
        return;
    }

    const LayerCellIndex layer_index = BuildLayerCellIndex(led_mappings, current_layer);
    bool changed = false;

    for(const std::pair<int, int>& cell : affected)
    {
        const int col = cell.first;
        const int row = cell.second;
        const int cache_index = row * layout_cells_cache_w_ + col;
        if(cache_index < 0 || cache_index >= layout_cells_cache_.size())
        {
            continue;
        }

        std::vector<GridLEDMapping> cell_mappings;
        const auto cell_it = layer_index.find(GridCellKey(col, row));
        if(cell_it != layer_index.end())
        {
            cell_mappings.reserve(cell_it->second.size());
            for(size_t mapping_index : cell_it->second)
            {
                cell_mappings.push_back(led_mappings[mapping_index]);
            }
        }

        CustomControllerGridCellVisual& visual = layout_cells_cache_[cache_index];
        const bool is_selected = new_selection.count(cell) > 0;
        const QColor previous_fill = visual.fill;
        const QColor previous_text = visual.text;
        const QColor base_color = ComputeCellBaseColor(cell_mappings, visual.is_hole);
        ApplyCellFillAndText(visual, base_color, cell_mappings, is_selected);

        if(visual.fill != previous_fill || visual.text != previous_text)
        {
            changed = true;
        }
    }

    if(changed)
    {
        layout_grid->SetCells(layout_cells_cache_);
    }
}

std::pair<int, int> CustomControllerDialog::SelectionPasteAnchor() const
{
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    if(selected_cells.empty())
    {
        return {selected_col, selected_row};
    }

    int min_col = 0;
    int min_row = 0;
    int max_col = 0;
    int max_row = 0;
    if(!ComputeSelectionBounds(selected_cells, &min_col, &min_row, &max_col, &max_row))
    {
        return {selected_col, selected_row};
    }

    return {min_col, min_row};
}

bool CustomControllerDialog::BuildClipboardFromSelection(CustomControllerClipboardRegion& out) const
{
    out = CustomControllerClipboardRegion{};
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    if(selected_cells.empty())
    {
        return false;
    }

    if(!ComputeSelectionBounds(selected_cells,
                              &out.min_col,
                              &out.min_row,
                              &out.max_col,
                              &out.max_row))
    {
        return false;
    }

    out.source_layer = current_layer;
    out.valid        = true;

    for(const GridLEDMapping& mapping : led_mappings)
    {
        if(mapping.z != current_layer || !mapping.controller)
        {
            continue;
        }
        if(selected_cells.count(std::make_pair(mapping.x, mapping.y)) == 0)
        {
            continue;
        }

        GridLEDMapping relative = mapping;
        relative.x -= out.min_col;
        relative.y -= out.min_row;
        out.mappings.push_back(relative);
    }

    for(const std::pair<int, int>& cell : selected_cells)
    {
        if(IsLightBlockerCell(cell.first, cell.second, current_layer))
        {
            out.blocker_offsets.emplace_back(cell.first - out.min_col, cell.second - out.min_row);
        }
    }

    return true;
}

bool CustomControllerDialog::PasteClipboardRegion(const CustomControllerClipboardRegion& region)
{
    if(!region.valid)
    {
        return false;
    }

    const int grid_w = width_spin->value();
    const int grid_h = height_spin->value();
    const std::pair<int, int> anchor = SelectionPasteAnchor();
    const int anchor_col = anchor.first;
    const int anchor_row = anchor.second;

    if(anchor_col < 0 || anchor_row < 0)
    {
        QMessageBox::warning(this, tr("No anchor"), tr("Select a cell to paste into."));
        return false;
    }

    size_t pasted_mappings = 0;
    size_t skipped_mappings = 0;
    size_t pasted_blockers = 0;

    for(const GridLEDMapping& relative : region.mappings)
    {
        const int x = anchor_col + relative.x;
        const int y = anchor_row + relative.y;
        if(x < 0 || x >= grid_w || y < 0 || y >= grid_h)
        {
            skipped_mappings++;
            continue;
        }
        if(IsMatrixHoleCell(x, y))
        {
            skipped_mappings++;
            continue;
        }

        std::vector<GridLEDMapping> replaced_mappings;
        CollectMappingsAtCell(led_mappings, x, y, current_layer, replaced_mappings);
        RestoreIdentifyForMappings(replaced_mappings);
        RemoveMappingsAtCell(led_mappings, x, y, current_layer);

        GridLEDMapping mapping = relative;
        mapping.x = x;
        mapping.y = y;
        mapping.z = current_layer;
        CustomControllerMapping::FinalizeMapping(mapping);
        led_mappings.push_back(mapping);
        pasted_mappings++;
    }

    for(const std::pair<int, int>& offset : region.blocker_offsets)
    {
        const int x = anchor_col + offset.first;
        const int y = anchor_row + offset.second;
        if(x < 0 || x >= grid_w || y < 0 || y >= grid_h)
        {
            continue;
        }
        light_blocker_cells_.insert(GridCellKey3D(x, y, current_layer));
        pasted_blockers++;
    }

    if(pasted_mappings == 0 && pasted_blockers == 0)
    {
        QMessageBox::information(this,
                               tr("Paste"),
                               tr("Nothing could be pasted at the current selection (out of bounds or blocked)."));
        return false;
    }

    if(skipped_mappings > 0)
    {
        QMessageBox::information(this,
                               tr("Paste"),
                               tr("Pasted %1 LED assignment(s) and %2 blocker cell(s); %3 assignment(s) skipped.")
                                   .arg(static_cast<qlonglong>(pasted_mappings))
                                   .arg(static_cast<qlonglong>(pasted_blockers))
                                   .arg(static_cast<qlonglong>(skipped_mappings)));
    }

    UpdateGridDisplay();
    UpdateCellInfo();
    refreshDeviceList();
    UpdateIdentifyButtonUi();
    return true;
}

void CustomControllerDialog::ClearSelectedCellContents()
{
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    if(selected_cells.empty())
    {
        return;
    }

    std::vector<GridLEDMapping> removed_mappings;
    for(const GridLEDMapping& mapping : led_mappings)
    {
        if(mapping.z == current_layer
           && selected_cells.count(std::make_pair(mapping.x, mapping.y)) > 0)
        {
            removed_mappings.push_back(mapping);
        }
    }

    led_mappings.erase(std::remove_if(led_mappings.begin(),
                                      led_mappings.end(),
                                      [&](const GridLEDMapping& mapping) {
                                          return mapping.z == current_layer
                                                 && selected_cells.count(std::make_pair(mapping.x, mapping.y)) > 0;
                                      }),
                       led_mappings.end());

    RestoreIdentifyForMappings(removed_mappings);

    for(const std::pair<int, int>& cell : selected_cells)
    {
        light_blocker_cells_.erase(GridCellKey3D(cell.first, cell.second, current_layer));
    }
}

void CustomControllerDialog::copySelectionClicked()
{
    if(!BuildClipboardFromSelection(clipboard_region_))
    {
        QMessageBox::warning(this, tr("Copy"), tr("Select one or more cells first."));
        return;
    }

    if(paste_button)
    {
        paste_button->setEnabled(true);
    }
}

void CustomControllerDialog::cutSelectionClicked()
{
    if(!BuildClipboardFromSelection(clipboard_region_))
    {
        QMessageBox::warning(this, tr("Cut"), tr("Select one or more cells first."));
        return;
    }

    ClearSelectedCellContents();
    if(paste_button)
    {
        paste_button->setEnabled(true);
    }

    UpdateGridDisplay();
    UpdateCellInfo();
    refreshDeviceList();
    UpdateIdentifyButtonUi();
}

void CustomControllerDialog::pasteSelectionClicked()
{
    PasteClipboardRegion(clipboard_region_);
}

void CustomControllerDialog::ShowGridContextMenu(const QPoint& global_pos)
{
    QMenu menu(this);
    QAction* copy_action = menu.addAction(tr("Copy"));
    QAction* cut_action  = menu.addAction(tr("Cut"));
    QAction* paste_action = menu.addAction(tr("Paste"));
    menu.addSeparator();
    QAction* set_col_sel_action = menu.addAction(tr("Set column width for selection…"));
    QAction* set_row_sel_action = menu.addAction(tr("Set row height for selection…"));
    menu.addSeparator();
    QAction* set_all_cols_action = menu.addAction(tr("Set all column widths…"));
    QAction* set_all_rows_action = menu.addAction(tr("Set all row heights…"));
    menu.addSeparator();
    QAction* clear_action = menu.addAction(tr("Clear selection"));
    QAction* add_blocker_action = menu.addAction(tr("Add light blocker"));

    const bool has_selection = !SelectedGridCells().empty();
    copy_action->setEnabled(has_selection);
    cut_action->setEnabled(has_selection);
    set_col_sel_action->setEnabled(has_selection);
    set_row_sel_action->setEnabled(has_selection);
    clear_action->setEnabled(has_selection);
    add_blocker_action->setEnabled(has_selection);
    paste_action->setEnabled(clipboard_region_.valid);

    QAction* chosen = menu.exec(global_pos);
    if(!chosen)
    {
        return;
    }

    if(chosen == copy_action)
    {
        copySelectionClicked();
    }
    else if(chosen == cut_action)
    {
        cutSelectionClicked();
    }
    else if(chosen == paste_action)
    {
        pasteSelectionClicked();
    }
    else if(chosen == set_col_sel_action)
    {
        float width_mm = GetSpacingX();
        if(PromptGridDimensionMm(tr("Column width"),
                                 tr("Width (mm) for each selected column:"),
                                 width_mm,
                                 &width_mm))
        {
            ApplyColumnWidthsForSelection(width_mm);
        }
    }
    else if(chosen == set_row_sel_action)
    {
        float height_mm = GetSpacingY();
        if(PromptGridDimensionMm(tr("Row height"),
                                 tr("Height (mm) for each selected row:"),
                                 height_mm,
                                 &height_mm))
        {
            ApplyRowHeightsForSelection(height_mm);
        }
    }
    else if(chosen == set_all_cols_action)
    {
        float width_mm = GetSpacingX();
        if(PromptGridDimensionMm(tr("All column widths"),
                                 tr("Width (mm) for every column:"),
                                 width_mm,
                                 &width_mm))
        {
            ApplyAllColumnWidths(width_mm);
        }
    }
    else if(chosen == set_all_rows_action)
    {
        float height_mm = GetSpacingY();
        if(PromptGridDimensionMm(tr("All row heights"),
                                 tr("Height (mm) for every row:"),
                                 height_mm,
                                 &height_mm))
        {
            ApplyAllRowHeights(height_mm);
        }
    }
    else if(chosen == clear_action)
    {
        clearCellClicked();
    }
    else if(chosen == add_blocker_action)
    {
        addLightBlockerClicked();
    }
}

void CustomControllerDialog::ShowGridHeaderContextMenu(const QPoint& global_pos,
                                                       int column_header,
                                                       int row_header)
{
    QMenu menu(this);
    QAction* chosen = nullptr;
    if(column_header >= 0)
    {
        QAction* set_col_action = menu.addAction(tr("Set column %1 width…").arg(column_header + 1));
        QAction* set_all_cols   = menu.addAction(tr("Set all column widths…"));
        chosen                  = menu.exec(global_pos);
        if(!chosen)
        {
            return;
        }
        if(chosen == set_col_action)
        {
            PromptAndApplyColumnWidth(column_header);
        }
        else if(chosen == set_all_cols)
        {
            float width_mm = GetSpacingX();
            if(PromptGridDimensionMm(tr("All column widths"),
                                     tr("Width (mm) for every column:"),
                                     width_mm,
                                     &width_mm))
            {
                ApplyAllColumnWidths(width_mm);
            }
        }
        return;
    }

    if(row_header >= 0)
    {
        QAction* set_row_action = menu.addAction(tr("Set row %1 height…").arg(row_header + 1));
        QAction* set_all_rows     = menu.addAction(tr("Set all row heights…"));
        chosen                    = menu.exec(global_pos);
        if(!chosen)
        {
            return;
        }
        if(chosen == set_row_action)
        {
            PromptAndApplyRowHeight(row_header);
        }
        else if(chosen == set_all_rows)
        {
            float height_mm = GetSpacingY();
            if(PromptGridDimensionMm(tr("All row heights"),
                                     tr("Height (mm) for every row:"),
                                     height_mm,
                                     &height_mm))
            {
                ApplyAllRowHeights(height_mm);
            }
        }
    }
}

bool CustomControllerDialog::PromptGridDimensionMm(const QString& title,
                                                     const QString& prompt,
                                                     float current_mm,
                                                     float* out_mm) const
{
    if(!out_mm)
    {
        return false;
    }

    bool ok    = false;
    const double value = QInputDialog::getDouble(
        const_cast<CustomControllerDialog*>(this),
        title,
        prompt,
        current_mm,
        CustomControllerGridLayoutMath::kMinCellSizeMm,
        100000.0,
        1,
        &ok);
    if(ok)
    {
        *out_mm = static_cast<float>(value);
    }
    return ok;
}

void CustomControllerDialog::ApplyColumnWidthMm(int column, float width_mm)
{
    EnsureDialogGridSizeArrays();
    if(column < 0 || column >= static_cast<int>(column_widths_mm_.size()))
    {
        return;
    }

    column_widths_mm_[static_cast<size_t>(column)] =
        std::max(CustomControllerGridLayoutMath::kMinCellSizeMm, width_mm);
    if(layout_grid)
    {
        layout_grid->SetColumnWidthsMm(ColumnWidthsQVector());
    }
    UpdateGridDisplay();
}

void CustomControllerDialog::ApplyRowHeightMm(int row, float height_mm)
{
    EnsureDialogGridSizeArrays();
    if(row < 0 || row >= static_cast<int>(row_heights_mm_.size()))
    {
        return;
    }

    row_heights_mm_[static_cast<size_t>(row)] =
        std::max(CustomControllerGridLayoutMath::kMinCellSizeMm, height_mm);
    if(layout_grid)
    {
        layout_grid->SetRowHeightsMm(RowHeightsQVector());
    }
    UpdateGridDisplay();
}

void CustomControllerDialog::ApplyColumnWidthsForColumns(const std::set<int>& columns, float width_mm)
{
    EnsureDialogGridSizeArrays();
    const float clamped = std::max(CustomControllerGridLayoutMath::kMinCellSizeMm, width_mm);
    for(int column : columns)
    {
        if(column >= 0 && column < static_cast<int>(column_widths_mm_.size()))
        {
            column_widths_mm_[static_cast<size_t>(column)] = clamped;
        }
    }
    if(layout_grid)
    {
        layout_grid->SetColumnWidthsMm(ColumnWidthsQVector());
    }
    UpdateGridDisplay();
}

void CustomControllerDialog::ApplyRowHeightsForRows(const std::set<int>& rows, float height_mm)
{
    EnsureDialogGridSizeArrays();
    const float clamped = std::max(CustomControllerGridLayoutMath::kMinCellSizeMm, height_mm);
    for(int row : rows)
    {
        if(row >= 0 && row < static_cast<int>(row_heights_mm_.size()))
        {
            row_heights_mm_[static_cast<size_t>(row)] = clamped;
        }
    }
    if(layout_grid)
    {
        layout_grid->SetRowHeightsMm(RowHeightsQVector());
    }
    UpdateGridDisplay();
}

void CustomControllerDialog::ApplyColumnWidthsForSelection(float width_mm)
{
    std::set<int> columns;
    for(const std::pair<int, int>& cell : SelectedGridCells())
    {
        columns.insert(cell.first);
    }
    ApplyColumnWidthsForColumns(columns, width_mm);
}

void CustomControllerDialog::ApplyRowHeightsForSelection(float height_mm)
{
    std::set<int> rows;
    for(const std::pair<int, int>& cell : SelectedGridCells())
    {
        rows.insert(cell.second);
    }
    ApplyRowHeightsForRows(rows, height_mm);
}

void CustomControllerDialog::ApplyAllColumnWidths(float width_mm)
{
    EnsureDialogGridSizeArrays();
    const float clamped = std::max(CustomControllerGridLayoutMath::kMinCellSizeMm, width_mm);
    for(float& value : column_widths_mm_)
    {
        value = clamped;
    }
    if(layout_grid)
    {
        layout_grid->SetColumnWidthsMm(ColumnWidthsQVector());
    }
    UpdateGridDisplay();
}

void CustomControllerDialog::ApplyAllRowHeights(float height_mm)
{
    EnsureDialogGridSizeArrays();
    const float clamped = std::max(CustomControllerGridLayoutMath::kMinCellSizeMm, height_mm);
    for(float& value : row_heights_mm_)
    {
        value = clamped;
    }
    if(layout_grid)
    {
        layout_grid->SetRowHeightsMm(RowHeightsQVector());
    }
    UpdateGridDisplay();
}

void CustomControllerDialog::PromptAndApplyColumnWidth(int column)
{
    EnsureDialogGridSizeArrays();
    if(column < 0 || column >= static_cast<int>(column_widths_mm_.size()))
    {
        return;
    }

    float width_mm = column_widths_mm_[static_cast<size_t>(column)];
    if(PromptGridDimensionMm(tr("Column width"),
                             tr("Column %1 width (mm):").arg(column + 1),
                             width_mm,
                             &width_mm))
    {
        ApplyColumnWidthMm(column, width_mm);
    }
}

void CustomControllerDialog::PromptAndApplyRowHeight(int row)
{
    EnsureDialogGridSizeArrays();
    if(row < 0 || row >= static_cast<int>(row_heights_mm_.size()))
    {
        return;
    }

    float height_mm = row_heights_mm_[static_cast<size_t>(row)];
    if(PromptGridDimensionMm(tr("Row height"),
                             tr("Row %1 height (mm):").arg(row + 1),
                             height_mm,
                             &height_mm))
    {
        ApplyRowHeightMm(row, height_mm);
    }
}

void CustomControllerDialog::gridColumnHeaderClicked(int column)
{
    PromptAndApplyColumnWidth(column);
}

void CustomControllerDialog::gridRowHeaderClicked(int row)
{
    PromptAndApplyRowHeight(row);
}

void CustomControllerDialog::gridContextMenuRequested(const QPoint& global_pos)
{
    if(layout_grid)
    {
        const QPoint local_pos = layout_grid->mapFromGlobal(global_pos);
        int col_header = -1;
        int row_header = -1;
        if(layout_grid->HeaderAtViewPos(local_pos, &col_header, &row_header))
        {
            ShowGridHeaderContextMenu(global_pos, col_header, row_header);
            return;
        }

        int col = -1;
        int row = -1;
        if(layout_grid->CellAtViewPos(local_pos, &col, &row))
        {
            const std::pair<int, int> cell(col, row);
            if(layout_grid->SelectedCells().count(cell) == 0)
            {
                layout_grid->SelectCellAt(col, row);
            }
        }
    }

    ShowGridContextMenu(global_pos);
}

void CustomControllerDialog::UpdateGridDisplay()
{
    RefreshLayoutGridVisuals();
    UpdateSummaryLabel();
    syncPreviewLayoutIfVisible();
}

bool CustomControllerDialog::IsLightBlockerCell(int x, int y, int layer) const
{
    return light_blocker_cells_.find(GridCellKey3D(x, y, layer)) != light_blocker_cells_.end();
}

void CustomControllerDialog::TransformLightBlockerCells(const std::function<void(int& x, int& y, int& z)>& transform_fn)
{
    if(!transform_fn || light_blocker_cells_.empty())
    {
        return;
    }

    std::unordered_set<uint64_t> transformed;
    transformed.reserve(light_blocker_cells_.size());
    for(const uint64_t key : light_blocker_cells_)
    {
        int x = 0;
        int y = 0;
        int z = 0;
        DecodeGridCellKey3D(key, &x, &y, &z);
        transform_fn(x, y, z);
        transformed.insert(GridCellKey3D(x, y, z));
    }
    light_blocker_cells_ = std::move(transformed);
}

void CustomControllerDialog::TrimLightBlockerCells(int max_width, int max_height, int max_depth)
{
    for(auto it = light_blocker_cells_.begin(); it != light_blocker_cells_.end();)
    {
        int x = 0;
        int y = 0;
        int z = 0;
        DecodeGridCellKey3D(*it, &x, &y, &z);
        if(x < 0 || x >= max_width || y < 0 || y >= max_height || z < 0 || z >= max_depth)
        {
            it = light_blocker_cells_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CustomControllerDialog::UpdateSummaryLabel()
{
    const std::vector<GridLEDMapping>& mappings_ref = led_mappings;
    std::set<std::tuple<int, int, int>> cells;
    for(const GridLEDMapping& m : mappings_ref)
    {
        cells.insert(std::make_tuple(m.x, m.y, m.z));
    }

    int blockers_on_layer = 0;
    for(const uint64_t key : light_blocker_cells_)
    {
        int z = 0;
        DecodeGridCellKey3D(key, nullptr, nullptr, &z);
        if(z == current_layer)
        {
            ++blockers_on_layer;
        }
    }

    summary_label->setText(tr("Assigned: %1 cells · Blockers: %2 · %3")
                               .arg(cells.size())
                               .arg(blockers_on_layer)
                               .arg(LayerTabLabel(current_layer)));

    if(transform_group)
    {
        transform_group->setVisible(!mappings_ref.empty());
    }
}

bool CustomControllerDialog::IsMatrixHoleCell(int x, int y) const
{
    return matrix_hole_cells.find(GridCellKey(x, y)) != matrix_hole_cells.end();
}

void CustomControllerDialog::RebuildMatrixHoleMask(RGBController* controller, int anchor_x, int anchor_y)
{
    matrix_hole_cells.clear();
    if(!controller)
    {
        return;
    }

    const int grid_w = width_spin->value();
    const int grid_h = height_spin->value();

    for(unsigned int zone_idx = 0; zone_idx < controller->zones.size(); zone_idx++)
    {
        zone* current_zone = &controller->zones[zone_idx];
        if(current_zone->type != ZONE_TYPE_MATRIX || current_zone->matrix_map == nullptr)
        {
            continue;
        }

        matrix_map_type* map = current_zone->matrix_map;
        for(unsigned int led_y = 0; led_y < map->height; led_y++)
        {
            for(unsigned int led_x = 0; led_x < map->width; led_x++)
            {
                const unsigned int map_idx = led_y * map->width + led_x;
                if(map->map[map_idx] == 0xFFFFFFFFu)
                {
                    const int gx = anchor_x + static_cast<int>(led_x);
                    const int gy = anchor_y + static_cast<int>(led_y);
                    if(gx >= 0 && gx < grid_w && gy >= 0 && gy < grid_h)
                    {
                        matrix_hole_cells.insert(GridCellKey(gx, gy));
                    }
                }
            }
        }
    }
}

void CustomControllerDialog::UpdateCellInfo()
{
    const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
    if(selected_cells.empty())
    {
        cell_info_label->setText("Click a cell to select it");
        return;
    }

    if(selected_cells.size() > 1)
    {
        std::vector<GridLEDMapping> cell_mappings;
        CollectMappingsForCells(selected_cells, current_layer, led_mappings, cell_mappings);
        QString info = tr("Selected %1 cells on layer %2").arg(selected_cells.size()).arg(current_layer);
        if(cell_mappings.empty())
        {
            info += tr(" — empty");
        }
        else
        {
            info += tr(" — %1 LED(s)").arg(cell_mappings.size());
        }
        cell_info_label->setText(info);
        return;
    }

    const std::pair<int, int> cell = *selected_cells.begin();
    const int col = cell.first;
    const int row = cell.second;

    QString info = QString("Selected: X=%1, Y=%2, Z=%3")
                    .arg(col)
                    .arg(row)
                    .arg(current_layer);

    const std::vector<GridLEDMapping>& mappings_ref = led_mappings;
    std::vector<GridLEDMapping> cell_mappings;
    for(unsigned int i = 0; i < mappings_ref.size(); i++)
    {
        if(mappings_ref[i].x == col && mappings_ref[i].y == row && mappings_ref[i].z == current_layer)
        {
            cell_mappings.push_back(mappings_ref[i]);
        }
    }

    if(cell_mappings.empty())
    {
        info += " - Empty";
    }
    else if(cell_mappings.size() == 1)
    {
        info += " - " + GetMappingDescription(cell_mappings[0]);
    }
    else
    {
        info += tr(" - Multiple LEDs (%1)").arg(cell_mappings.size());
    }

    cell_info_label->setText(info);
}

