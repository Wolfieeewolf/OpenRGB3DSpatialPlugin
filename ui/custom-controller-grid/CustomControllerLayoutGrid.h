// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERLAYOUTGRID_H
#define CUSTOMCONTROLLERLAYOUTGRID_H

#include <QGraphicsView>
#include <QVector>
#include <set>
#include <utility>

#include "CustomControllerGridCell.h"

class CustomControllerGridItem;
class CustomControllerGridScene;

class CustomControllerLayoutGrid : public QGraphicsView
{
    Q_OBJECT

public:
    explicit CustomControllerLayoutGrid(QWidget* parent = nullptr);

    void SetGridSize(int width, int height);
    void SetCells(const QVector<CustomControllerGridCellVisual>& cells);
    void SetSelectedCells(const std::set<std::pair<int, int>>& cells);
    std::set<std::pair<int, int>> SelectedCells() const;
    void SetSelectionColor(const QColor& color);
    void SetAnchorCell(int column, int row);
    void FitGridInView();
    void RequestFitOnNextResize();
    void ClearSelection();

    bool CellAtViewPos(const QPoint& view_pos, int* column, int* row) const;

signals:
    void cellClicked(int column, int row);
    void cellDoubleClicked(int column, int row);
    void selectionChanged();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool event(QEvent* event) override;

private:
    CustomControllerGridScene* scene_ = nullptr;
    CustomControllerGridItem*  grid_item_ = nullptr;
    std::set<std::pair<int, int>> selected_cells_;
    int anchor_col_ = -1;
    int anchor_row_ = -1;
    bool left_button_pressed_  = false;
    bool rubber_band_active_   = false;
    QPoint rubber_band_origin_;
    QRect rubber_band_rect_;
    bool needs_fit_     = true;
    int  last_grid_w_   = 0;
    int  last_grid_h_   = 0;

    void ApplySelectionToItem();
    void SelectSingleCell(int column, int row);
    void ToggleCellInSelection(int column, int row);
    void SelectRect(int col_a, int row_a, int col_b, int row_b, bool replace);
    void FinishRubberBandSelection(bool add_to_selection);
    void UpdateHoverTooltip(const QPoint& view_pos);
};

#endif
