// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERGRIDITEM_H
#define CUSTOMCONTROLLERGRIDITEM_H

#include <QGraphicsItem>
#include <QVector>
#include <set>
#include <utility>

#include "CustomControllerGridCell.h"

class CustomControllerGridItem : public QGraphicsItem
{
public:
    explicit CustomControllerGridItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void SetGridSize(int width, int height);
    void SetCells(const QVector<CustomControllerGridCellVisual>& cells);
    void SetSelectedCells(const std::set<std::pair<int, int>>& cells);
    void SetSelectionColor(const QColor& color);

    int  GridWidth() const { return grid_width_; }
    int  GridHeight() const { return grid_height_; }
    bool CellAtScenePos(const QPointF& scene_pos, int* column, int* row) const;
    QString TooltipAt(int column, int row) const;

private:
    int grid_width_  = 0;
    int grid_height_ = 0;
    QVector<CustomControllerGridCellVisual> cells_;
    std::set<std::pair<int, int>> selected_cells_;
    QVector<quint8>               selection_mask_;
    QColor selection_color_;

    const CustomControllerGridCellVisual* CellVisual(int column, int row) const;
    bool IsCellSelected(int column, int row) const;
};

#endif
