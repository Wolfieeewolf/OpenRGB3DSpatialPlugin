// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerGridItem.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QFontMetricsF>
#include <QStyleOptionGraphicsItem>
#include <QtGlobal>
#include <cmath>

namespace
{
constexpr qreal kCellInset       = 0.1;
constexpr qreal kCellBorderW     = 0.04;
constexpr qreal kMinLabelCellPx  = 7.0;
constexpr qreal kLabelFillRatio  = 0.82;

void DrawFittedCellLabel(QPainter* painter,
                         const QRectF& tile,
                         const QString& label,
                         const QColor& text_color,
                         qreal px_per_scene_unit)
{
    if(label.isEmpty() || px_per_scene_unit < 1.0)
    {
        return;
    }

    const qreal tile_w_px = tile.width() * px_per_scene_unit;
    const qreal tile_h_px = tile.height() * px_per_scene_unit;
    if(tile_w_px < kMinLabelCellPx || tile_h_px < kMinLabelCellPx)
    {
        return;
    }

    const qreal max_w_px = tile_w_px * kLabelFillRatio;
    const qreal max_h_px = tile_h_px * kLabelFillRatio;

    QFont font = painter->font();
    int font_px = qBound(4, static_cast<int>(max_h_px * 0.72), 14);

    for(int attempt = 0; attempt < 8; attempt++)
    {
        font.setPixelSize(font_px);
        const QFontMetricsF metrics(font);
        const QRectF bounds = metrics.boundingRect(label);
        if(bounds.width() <= max_w_px && bounds.height() <= max_h_px)
        {
            break;
        }
        font_px = qMax(4, font_px - 1);
    }

    painter->save();
    painter->translate(tile.center());
    painter->scale(1.0 / px_per_scene_unit, 1.0 / px_per_scene_unit);
    painter->setFont(font);
    painter->setPen(text_color);
    painter->drawText(QRectF(-tile_w_px * 0.5, -tile_h_px * 0.5, tile_w_px, tile_h_px),
                      Qt::AlignCenter, label);
    painter->restore();
}
} // namespace

CustomControllerGridItem::CustomControllerGridItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
    setAcceptHoverEvents(false);
    setZValue(1.0);
    selection_color_ = QApplication::palette().color(QPalette::Highlight);
}

QRectF CustomControllerGridItem::boundingRect() const
{
    return QRectF(0, 0, grid_width_, grid_height_);
}

void CustomControllerGridItem::SetGridSize(int width, int height)
{
    prepareGeometryChange();
    grid_width_  = std::max(0, width);
    grid_height_ = std::max(0, height);
    cells_.resize(grid_width_ * grid_height_);
    selection_mask_.resize(grid_width_ * grid_height_);
    selection_mask_.fill(0);
    update();
}

namespace
{
bool CellVisualsEqual(const QVector<CustomControllerGridCellVisual>& a,
                      const QVector<CustomControllerGridCellVisual>& b)
{
    if(a.size() != b.size())
    {
        return false;
    }

    for(int i = 0; i < a.size(); i++)
    {
        if(a[i].fill != b[i].fill
           || a[i].text != b[i].text
           || a[i].label != b[i].label
           || a[i].is_hole != b[i].is_hole
           || a[i].is_empty != b[i].is_empty)
        {
            return false;
        }
    }

    return true;
}
} // namespace

void CustomControllerGridItem::SetCells(const QVector<CustomControllerGridCellVisual>& cells)
{
    if(CellVisualsEqual(cells_, cells))
    {
        return;
    }

    cells_ = cells;
    update();
}

void CustomControllerGridItem::SetSelectedCells(const std::set<std::pair<int, int>>& cells)
{
    if(selected_cells_ == cells)
    {
        return;
    }

    selected_cells_ = cells;
    selection_mask_.fill(0);
    for(const std::pair<int, int>& cell : selected_cells_)
    {
        const int index = cell.second * grid_width_ + cell.first;
        if(index >= 0 && index < selection_mask_.size())
        {
            selection_mask_[index] = 1;
        }
    }

    update();
}

void CustomControllerGridItem::SetSelectionColor(const QColor& color)
{
    selection_color_ = color;
    update();
}

const CustomControllerGridCellVisual* CustomControllerGridItem::CellVisual(int column, int row) const
{
    if(column < 0 || row < 0 || column >= grid_width_ || row >= grid_height_)
    {
        return nullptr;
    }
    const int index = row * grid_width_ + column;
    if(index < 0 || index >= cells_.size())
    {
        return nullptr;
    }
    return &cells_[index];
}

bool CustomControllerGridItem::CellAtScenePos(const QPointF& scene_pos, int* column, int* row) const
{
    if(!column || !row || grid_width_ <= 0 || grid_height_ <= 0)
    {
        return false;
    }

    const int col = static_cast<int>(std::floor(scene_pos.x()));
    const int r   = static_cast<int>(std::floor(scene_pos.y()));
    if(col < 0 || r < 0 || col >= grid_width_ || r >= grid_height_)
    {
        return false;
    }

    *column = col;
    *row    = r;
    return true;
}

QString CustomControllerGridItem::TooltipAt(int column, int row) const
{
    const CustomControllerGridCellVisual* cell = CellVisual(column, row);
    return cell ? cell->tooltip : QString();
}

bool CustomControllerGridItem::IsCellSelected(int column, int row) const
{
    const int index = row * grid_width_ + column;
    return index >= 0 && index < selection_mask_.size() && selection_mask_[index] != 0;
}

void CustomControllerGridItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    if(grid_width_ <= 0 || grid_height_ <= 0)
    {
        return;
    }

    const QRectF exposed = option ? option->exposedRect : boundingRect();
    int min_col = static_cast<int>(std::floor(exposed.left()));
    int max_col = static_cast<int>(std::ceil(exposed.right())) - 1;
    int min_row = static_cast<int>(std::floor(exposed.top()));
    int max_row = static_cast<int>(std::ceil(exposed.bottom())) - 1;

    min_col = std::max(0, min_col);
    min_row = std::max(0, min_row);
    max_col = std::min(grid_width_ - 1, max_col);
    max_row = std::min(grid_height_ - 1, max_row);

    if(min_col > max_col || min_row > max_row)
    {
        return;
    }

    const QColor hole_line_color   = QApplication::palette().color(QPalette::Mid);
    const QColor cell_border_color = QApplication::palette().color(QPalette::Mid);
    const QColor default_text_color = QApplication::palette().color(QPalette::Text);
    const QPen   cell_border_pen(cell_border_color, kCellBorderW);
    const QPen   hole_line_pen(hole_line_color, kCellBorderW, Qt::DotLine);
    const QPen   selection_pen(selection_color_, kCellBorderW * 2.5);

    const qreal px_per_scene_unit = option
        ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform())
        : 1.0;

    for(int row = min_row; row <= max_row; row++)
    {
        for(int col = min_col; col <= max_col; col++)
        {
            const CustomControllerGridCellVisual* cell = CellVisual(col, row);
            if(!cell)
            {
                continue;
            }

            const QRectF tile(col + kCellInset, row + kCellInset,
                              1.0 - 2.0 * kCellInset, 1.0 - 2.0 * kCellInset);

            painter->setPen(cell_border_pen);
            painter->setBrush(cell->fill);
            painter->drawRect(tile);

            if(cell->is_hole)
            {
                painter->setPen(hole_line_pen);
                painter->setBrush(Qt::NoBrush);
                painter->drawLine(tile.topLeft(), tile.bottomRight());
                painter->drawLine(tile.topRight(), tile.bottomLeft());
            }

            if(!cell->label.isEmpty())
            {
                const QColor text_color = cell->text.isValid() ? cell->text : default_text_color;
                DrawFittedCellLabel(painter, tile, cell->label, text_color, px_per_scene_unit);
            }

            if(IsCellSelected(col, row))
            {
                painter->setPen(selection_pen);
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(tile.adjusted(-0.03, -0.03, 0.03, 0.03));
            }
        }
    }
}
