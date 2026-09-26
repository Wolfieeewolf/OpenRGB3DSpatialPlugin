// SPDX-License-Identifier: GPL-2.0-only
#ifndef CUSTOMCONTROLLERLAYOUTGRID_H
#define CUSTOMCONTROLLERLAYOUTGRID_H
#include <QContextMenuEvent>
#include <QGraphicsView>
#include <QVector>
#include <set>
#include <utility>
#include "CustomControllerGridCell.h"
class CustomControllerGridItem;
class CustomControllerGridScene;
class QFocusEvent;
class QKeyEvent;
class QLabel;
class QRubberBand;
class CustomControllerLayoutGrid : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CustomControllerLayoutGrid(QWidget* parent = nullptr);
    void SetGridSize(int width, int height);
    void SetColumnWidthsMm(const QVector<float>& widths_mm);
    void SetRowHeightsMm(const QVector<float>& heights_mm);
    void SetMmPerSceneUnit(float mm_per_unit);
    void SetCells(const QVector<CustomControllerGridCellVisual>& cells);
    void SetCellAt(int column, int row, const CustomControllerGridCellVisual& visual);
    void SetSelectedCells(const std::set<std::pair<int, int>>& cells);
    std::set<std::pair<int, int>> SelectedCells() const;
    void SetSelectionColor(const QColor& color);
    void SetAnchorCell(int column, int row);
    void SetDraggableCells(const std::set<std::pair<int, int>>& cells);
    void FitGridInView();
    void ClearSelection();
    void SelectCellAt(int column, int row);
    bool CellAtViewPos(const QPoint& view_pos, int* column, int* row) const;
    bool HeaderAtViewPos(const QPoint& view_pos, int* column_header, int* row_header) const;
    bool InteractionInProgress() const;
signals:
    void cellClicked(int column, int row);
    void cellDoubleClicked(int column, int row);
    void columnHeaderClicked(int column);
    void rowHeaderClicked(int row);
    void selectionChanged();
    void contextMenuRequested(const QPoint& global_pos);
    void columnWidthChanged(int column, float width_mm);
    void rowHeightChanged(int row, float height_mm);
    void gridLineResizeEnded();
    void cellContentsDropped(int from_column, int from_row, int to_column, int to_row);
    void cellContentsNudged(int delta_column, int delta_row);
protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool event(QEvent* event) override;
private:
    enum class ResizeMode
    {
        None,
        Column,
        Row,
    };
    CustomControllerGridScene* scene_ = nullptr;
    CustomControllerGridItem*  grid_item_ = nullptr;
    QRubberBand*               rubber_band_overlay_ = nullptr;
    QLabel*                    drag_ghost_ = nullptr;
    std::set<std::pair<int, int>> selected_cells_;
    std::set<std::pair<int, int>> draggable_cells_;
    int anchor_col_ = -1;
    int anchor_row_ = -1;
    bool left_button_pressed_  = false;
    bool rubber_band_active_   = false;
    bool cell_drag_active_     = false;
    int  pressed_cell_col_     = -1;
    int  pressed_cell_row_     = -1;
    int  cell_drag_hover_col_  = -1;
    int  cell_drag_hover_row_  = -1;
    QPoint rubber_band_origin_;
    QRect rubber_band_rect_;
    bool needs_fit_     = true;
    int  last_grid_w_   = 0;
    int  last_grid_h_   = 0;
    ResizeMode resize_mode_            = ResizeMode::None;
    int        resize_index_           = -1;
    float      resize_start_size_      = 0.0f;
    qreal      resize_start_scene_pos_ = 0.0;
    int        pressed_header_col_     = -1;
    int        pressed_header_row_     = -1;
    void SyncSceneRect();
    qreal HitSlopScene() const;
    float MmPerSceneUnit() const;
    void ShowResizeSizeTooltip(const QPoint& view_pos, const QString& text);
    void UpdateResizeCursor(const QPoint& view_pos);
    void EndGridLineResize(bool emit_ended);
    void EndPointerGesture();
    void EndCellContentDrag(bool commit_drop, int drop_col, int drop_row);
    void PreviewCellDragHover(int column, int row);
    bool CellIsDraggable(int column, int row) const;
    void ApplySelectionToItem();
    void SelectSingleCell(int column, int row);
    void ToggleCellInSelection(int column, int row);
    void SelectRect(int col_a, int row_a, int col_b, int row_b, bool replace);
    void FinishRubberBandSelection(bool add_to_selection);
    void UpdateHoverTooltip(const QPoint& view_pos);
    void GrabPointer();
    void ReleasePointer();
    void ShowDragGhost(const QPoint& view_pos);
    void MoveDragGhost(const QPoint& view_pos);
    void HideDragGhost();
};
#endif
