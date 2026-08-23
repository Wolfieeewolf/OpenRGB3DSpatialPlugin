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
    void SetSelectedCells(const std::set<std::pair<int, int>>& cells);
    std::set<std::pair<int, int>> SelectedCells() const;
    void SetSelectionColor(const QColor& color);
    void SetAnchorCell(int column, int row);
    void FitGridInView();
    void RequestFitOnNextResize();
    void ClearSelection();
    void SelectCellAt(int column, int row);
    bool CellAtViewPos(const QPoint& view_pos, int* column, int* row) const;
    bool HeaderAtViewPos(const QPoint& view_pos, int* column_header, int* row_header) const;
signals:
    void cellClicked(int column, int row);
    void cellDoubleClicked(int column, int row);
    void columnHeaderClicked(int column);
    void rowHeaderClicked(int row);
    void selectionChanged();
    void contextMenuRequested(const QPoint& global_pos);
    void columnWidthChanged(int column, float width_mm);
    void rowHeightChanged(int row, float height_mm);
protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
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
    void ApplySelectionToItem();
    void SelectSingleCell(int column, int row);
    void ToggleCellInSelection(int column, int row);
    void SelectRect(int col_a, int row_a, int col_b, int row_b, bool replace);
    void FinishRubberBandSelection(bool add_to_selection);
    void UpdateHoverTooltip(const QPoint& view_pos);
};
#endif
