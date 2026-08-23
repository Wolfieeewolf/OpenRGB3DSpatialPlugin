// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerDialog.h"
#include "CustomControllerDialog_Internal.h"
#include "CustomControllerMappingUtils.h"
#include "CustomControllerPreviewDialog.h"
#include "custom-controller-grid/CustomControllerLayoutGrid.h"
#include "custom-controller-grid/CustomControllerGridLayoutMath.h"

#include <QMessageBox>
#include <QSpinBox>
#include <QString>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

void CustomControllerDialog::UpdateGridColors()
{
    if(!layout_grid)
    {
        return;
    }

    if(!LayoutCellsCacheMatchesGrid())
    {
        RefreshLayoutGridVisuals();
    }
    else
    {
        const LayerCellIndex layer_index = BuildLayerCellIndex(led_mappings, current_layer);
        const std::set<std::pair<int, int>> selected_cells = SelectedGridCells();
        bool changed = false;

        auto update_cell = [&](int col, int row)
        {
            const int cache_index = row * layout_cells_cache_w_ + col;
            if(cache_index < 0 || cache_index >= layout_cells_cache_.size())
            {
                return;
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
            const bool is_selected = selected_cells.count(std::make_pair(col, row)) > 0;
            const bool is_hole     = cell_mappings.empty() && IsMatrixHoleCell(col, row);
            const QColor base_color = ComputeCellBaseColor(cell_mappings, is_hole);

            const QColor previous_fill = visual.fill;
            const QColor previous_text = visual.text;
            ApplyCellFillAndText(visual, base_color, cell_mappings, is_selected);

            if(visual.fill != previous_fill || visual.text != previous_text)
            {
                changed = true;
            }
        };

        for(const std::pair<const uint64_t, std::vector<size_t>>& entry : layer_index)
        {
            const int col = static_cast<int>(entry.first >> 32);
            const int row = static_cast<int>(static_cast<uint32_t>(entry.first));
            update_cell(col, row);
        }

        for(const std::pair<int, int>& cell : selected_cells)
        {
            update_cell(cell.first, cell.second);
        }

        if(changed)
        {
            layout_grid->SetCells(layout_cells_cache_);
        }
    }

    if(preview_dialog && preview_dialog->isVisible())
    {
        preview_dialog->RefreshPreviewColors();
    }
}

void CustomControllerDialog::syncPreviewLayoutIfVisible()
{
    if(preview_dialog && preview_dialog->isVisible())
    {
        preview_dialog->RefreshPreviewFromEditor();
    }
}

void CustomControllerDialog::refresh_colors()
{
    if(shutting_down_ || !isVisible())
    {
        return;
    }

    if(resource_manager)
    {
        const int unresolved_before = CustomControllerMapping::UnresolvedCount(led_mappings);
        std::vector<RGBControllerInterface*> controllers = resource_manager->GetRGBControllers();
        CustomControllerMapping::RebindAll(led_mappings, controllers);
        const int unresolved_after = CustomControllerMapping::UnresolvedCount(led_mappings);
        if(unresolved_before != unresolved_after)
        {
            refreshDeviceList();
        }
    }

    UpdateGridColors();
}

void CustomControllerDialog::showPreview3dClicked()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Nothing to preview"), tr("Assign at least one LED to the grid first."));
        return;
    }

    if(!preview_dialog)
    {
        preview_dialog = new CustomControllerPreviewDialog(this);
    }

    preview_dialog->UpdatePreview(
        this,
        GetControllerName().trimmed().toStdString(),
        GetGridWidth(),
        GetGridHeight(),
        GetGridDepth(),
        GetSpacingX(),
        GetSpacingY(),
        GetSpacingZ(),
        layout_grid_scale_mm,
        GetLEDMappings());

    preview_dialog->show();
    preview_dialog->raise();
    preview_dialog->activateWindow();
}

void CustomControllerDialog::WarnIfMappingCollisions() const
{
    const std::vector<GridLEDMapping>& mappings = led_mappings;
    std::map<std::tuple<int, int, int>, int> cell_counts;

    for(const GridLEDMapping& m : mappings)
    {
        cell_counts[std::make_tuple(m.x, m.y, m.z)]++;
    }

    int collision_cells = 0;
    for(const auto& entry : cell_counts)
    {
        if(entry.second > 1)
        {
            collision_cells++;
        }
    }

    if(collision_cells > 0)
    {
        QMessageBox::warning(
            const_cast<CustomControllerDialog*>(this),
            tr("Overlapping assignments"),
            tr("%1 grid cell(s) have more than one LED mapped. Effects may blend colors in those cells.")
            .arg(collision_cells));
    }
}

void CustomControllerDialog::rotateGrid90()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to rotate."));
        return;
    }

    const int width = width_spin->value();
    const int height = height_spin->value();

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        const int old_x = led_mappings[i].x;
        const int old_y = led_mappings[i].y;
        led_mappings[i].x = old_y;
        led_mappings[i].y = width - 1 - old_x;
    }

    TransformLightBlockerCells([&](int& x, int& y, int& z) {
        (void)z;
        const int old_x = x;
        const int old_y = y;
        x = old_y;
        y = width - 1 - old_x;
    });

    EnsureDialogGridSizeArrays();
    std::vector<float> new_col_widths(static_cast<size_t>(height), CustomControllerGridLayoutMath::kDefaultCellSizeMm);
    std::vector<float> new_row_heights(static_cast<size_t>(width), CustomControllerGridLayoutMath::kDefaultCellSizeMm);
    for(int col = 0; col < height; ++col)
    {
        if(col < static_cast<int>(row_heights_mm_.size()))
        {
            new_col_widths[static_cast<size_t>(col)] = row_heights_mm_[static_cast<size_t>(col)];
        }
    }
    for(int row = 0; row < width; ++row)
    {
        const int old_col = width - 1 - row;
        if(old_col >= 0 && old_col < static_cast<int>(column_widths_mm_.size()))
        {
            new_row_heights[static_cast<size_t>(row)] = column_widths_mm_[static_cast<size_t>(old_col)];
        }
    }
    column_widths_mm_ = std::move(new_col_widths);
    row_heights_mm_   = std::move(new_row_heights);

    width_spin->blockSignals(true);
    height_spin->blockSignals(true);
    width_spin->setValue(height);
    height_spin->setValue(width);
    width_spin->blockSignals(false);
    height_spin->blockSignals(false);

    matrix_hole_cells.clear();
    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

void CustomControllerDialog::rotateGrid180()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to rotate."));
        return;
    }

    const int width = width_spin->value();
    const int height = height_spin->value();

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        led_mappings[i].x = width - 1 - led_mappings[i].x;
        led_mappings[i].y = height - 1 - led_mappings[i].y;
    }

    TransformLightBlockerCells([&](int& x, int& y, int& z) {
        (void)z;
        x = width - 1 - x;
        y = height - 1 - y;
    });

    ReverseFloatVector(&column_widths_mm_);
    ReverseFloatVector(&row_heights_mm_);

    matrix_hole_cells.clear();
    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

void CustomControllerDialog::rotateGrid270()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to rotate."));
        return;
    }

    const int width = width_spin->value();
    const int height = height_spin->value();

    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        const int old_x = led_mappings[i].x;
        const int old_y = led_mappings[i].y;
        led_mappings[i].x = height - 1 - old_y;
        led_mappings[i].y = old_x;
    }

    TransformLightBlockerCells([&](int& x, int& y, int& z) {
        (void)z;
        const int old_x = x;
        const int old_y = y;
        x = height - 1 - old_y;
        y = old_x;
    });

    EnsureDialogGridSizeArrays();
    std::vector<float> new_col_widths(static_cast<size_t>(height), CustomControllerGridLayoutMath::kDefaultCellSizeMm);
    std::vector<float> new_row_heights(static_cast<size_t>(width), CustomControllerGridLayoutMath::kDefaultCellSizeMm);
    for(int col = 0; col < height; ++col)
    {
        const int old_row = height - 1 - col;
        if(old_row >= 0 && old_row < static_cast<int>(row_heights_mm_.size()))
        {
            new_col_widths[static_cast<size_t>(col)] = row_heights_mm_[static_cast<size_t>(old_row)];
        }
    }
    for(int row = 0; row < width; ++row)
    {
        if(row < static_cast<int>(column_widths_mm_.size()))
        {
            new_row_heights[static_cast<size_t>(row)] = column_widths_mm_[static_cast<size_t>(row)];
        }
    }
    column_widths_mm_ = std::move(new_col_widths);
    row_heights_mm_   = std::move(new_row_heights);

    width_spin->blockSignals(true);
    height_spin->blockSignals(true);
    width_spin->setValue(height);
    height_spin->setValue(width);
    width_spin->blockSignals(false);
    height_spin->blockSignals(false);

    matrix_hole_cells.clear();
    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

void CustomControllerDialog::flipGridHorizontal()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to flip."));
        return;
    }

    const int width = width_spin->value();
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        led_mappings[i].x = width - 1 - led_mappings[i].x;
    }

    TransformLightBlockerCells([&](int& x, int& y, int& z) {
        (void)y;
        (void)z;
        x = width - 1 - x;
    });

    ReverseFloatVector(&column_widths_mm_);

    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

void CustomControllerDialog::flipGridVertical()
{
    if(led_mappings.empty())
    {
        QMessageBox::information(this, tr("Grid empty"), tr("No LEDs to flip."));
        return;
    }

    const int height = height_spin->value();
    for(unsigned int i = 0; i < led_mappings.size(); i++)
    {
        led_mappings[i].y = height - 1 - led_mappings[i].y;
    }

    TransformLightBlockerCells([&](int& x, int& y, int& z) {
        (void)x;
        (void)z;
        y = height - 1 - y;
    });

    ReverseFloatVector(&row_heights_mm_);

    UpdateGridDisplay();
    WarnIfMappingCollisions();
}

