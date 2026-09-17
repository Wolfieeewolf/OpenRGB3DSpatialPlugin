// SPDX-License-Identifier: GPL-2.0-only
#ifndef CUSTOMCONTROLLERGRIDITEM_H
#define CUSTOMCONTROLLERGRIDITEM_H
#include <QGraphicsItem>
#include <QVector>
#include <set>
#include <utility>
#include "CustomControllerGridCell.h"
#include "CustomControllerGridLayoutMath.h"
#include "GridSpaceUtils.h"
class CustomControllerGridItem : public QGraphicsItem
{
public:
    explicit CustomControllerGridItem(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void SetGridSize(int width, int height);
    void SetColumnWidthsMm(const QVector<float>& widths_mm);
    void SetRowHeightsMm(const QVector<float>& heights_mm);
    void SetMmPerSceneUnit(float mm_per_unit);
    float MmPerSceneUnit() const { return mm_per_unit_; }
    void SetCells(const QVector<CustomControllerGridCellVisual>& cells);
    void SetSelectedCells(const std::set<std::pair<int, int>>& cells);
    void SetSelectionColor(const QColor& color);
    int  GridWidth() const { return grid_width_; }
    int  GridHeight() const { return grid_height_; }
    const QVector<float>& ColumnWidthsMm() const { return column_widths_mm_; }
    const QVector<float>& RowHeightsMm() const { return row_heights_mm_; }
    bool CellAtScenePos(const QPointF& scene_pos, int* column, int* row) const;
    int  ColumnResizeIndexAtScenePos(const QPointF& scene_pos, qreal hit_slop_scene) const;
    int  RowResizeIndexAtScenePos(const QPointF& scene_pos, qreal hit_slop_scene) const;
    int  ColumnBorderIndexAtScenePos(const QPointF& scene_pos, qreal hit_slop_scene) const;
    int  RowBorderIndexAtScenePos(const QPointF& scene_pos, qreal hit_slop_scene) const;
    int  ColumnHeaderIndexAtScenePos(const QPointF& scene_pos) const;
    int  RowHeaderIndexAtScenePos(const QPointF& scene_pos) const;
    QRectF CellRectScene(int column, int row) const;
    QString TooltipAt(int column, int row) const;
private:
    int grid_width_  = 0;
    int grid_height_ = 0;
    float mm_per_unit_ = DEFAULT_GRID_SCALE_MM;
    QVector<float> column_widths_mm_;
    QVector<float> row_heights_mm_;
    QVector<CustomControllerGridCellVisual> cells_;
    std::set<std::pair<int, int>> selected_cells_;
    QColor selection_color_;
    void EnsureSizeArrays();
    const CustomControllerGridCellVisual* CellVisual(int column, int row) const;
    bool IsCellSelected(int column, int row) const;
};
#endif
