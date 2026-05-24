// SPDX-License-Identifier: GPL-2.0-only

#include "CustomControllerLayoutGrid.h"

#include "CustomControllerGridItem.h"
#include "CustomControllerGridScene.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace
{
constexpr qreal kFitMarginPx   = 20.0;
constexpr qreal kMinCellPx     = 22.0;
constexpr qreal kMaxCellPx     = 32.0;
constexpr int   kMinViewportPx = 80;
} // namespace

CustomControllerLayoutGrid::CustomControllerLayoutGrid(QWidget* parent)
    : QGraphicsView(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setDragMode(QGraphicsView::NoDrag);
    setRenderHint(QPainter::Antialiasing, true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setInteractive(true);
    setMouseTracking(true);
    setAlignment(Qt::AlignCenter);
    setBackgroundBrush(palette().window());
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);

    scene_ = new CustomControllerGridScene(this);
    setScene(scene_);
    grid_item_ = new CustomControllerGridItem();
    scene_->addItem(grid_item_);
}

void CustomControllerLayoutGrid::SetGridSize(int width, int height)
{
    if(scene_)
    {
        scene_->SetGridSize(width, height);
    }
    if(grid_item_)
    {
        grid_item_->SetGridSize(width, height);
    }

    if(width != last_grid_w_ || height != last_grid_h_)
    {
        needs_fit_   = true;
        last_grid_w_ = width;
        last_grid_h_ = height;
        FitGridInView();
    }
}

void CustomControllerLayoutGrid::RequestFitOnNextResize()
{
    needs_fit_ = true;
}

void CustomControllerLayoutGrid::SetCells(const QVector<CustomControllerGridCellVisual>& cells)
{
    if(grid_item_)
    {
        grid_item_->SetCells(cells);
    }
}

void CustomControllerLayoutGrid::SetSelectedCells(const std::set<std::pair<int, int>>& cells)
{
    selected_cells_ = cells;
    ApplySelectionToItem();
}

std::set<std::pair<int, int>> CustomControllerLayoutGrid::SelectedCells() const
{
    return selected_cells_;
}

void CustomControllerLayoutGrid::SetSelectionColor(const QColor& color)
{
    if(grid_item_)
    {
        grid_item_->SetSelectionColor(color);
    }
}

void CustomControllerLayoutGrid::SetAnchorCell(int column, int row)
{
    anchor_col_ = column;
    anchor_row_ = row;
}

void CustomControllerLayoutGrid::ClearSelection()
{
    selected_cells_.clear();
    ApplySelectionToItem();
    emit selectionChanged();
}

void CustomControllerLayoutGrid::FitGridInView()
{
    if(!grid_item_)
    {
        return;
    }

    const qreal grid_w = grid_item_->GridWidth();
    const qreal grid_h = grid_item_->GridHeight();
    if(grid_w <= 0.0 || grid_h <= 0.0)
    {
        return;
    }

    const QRect viewport_px = viewport()->rect();
    if(viewport_px.width() < kMinViewportPx || viewport_px.height() < kMinViewportPx)
    {
        needs_fit_ = true;
        return;
    }

    const qreal avail_w = std::max(1.0, viewport_px.width() - 2.0 * kFitMarginPx);
    const qreal avail_h = std::max(1.0, viewport_px.height() - 2.0 * kFitMarginPx);
    const qreal fit_scale = std::min(avail_w / grid_w, avail_h / grid_h);

    qreal target_scale = fit_scale;
    const qreal max_dim = std::max(grid_w, grid_h);
    if(max_dim <= 64.0)
    {
        target_scale = std::min(std::max(fit_scale, kMinCellPx), kMaxCellPx);
    }
    else
    {
        target_scale = std::max(fit_scale, kMinCellPx);
    }

    resetTransform();
    scale(target_scale, target_scale);
    centerOn(QPointF(grid_w * 0.5, grid_h * 0.5));
    needs_fit_ = false;
}

bool CustomControllerLayoutGrid::CellAtViewPos(const QPoint& view_pos, int* column, int* row) const
{
    if(!grid_item_)
    {
        return false;
    }
    return grid_item_->CellAtScenePos(mapToScene(view_pos), column, row);
}

void CustomControllerLayoutGrid::ApplySelectionToItem()
{
    if(grid_item_)
    {
        grid_item_->SetSelectedCells(selected_cells_);
    }
}

void CustomControllerLayoutGrid::SelectSingleCell(int column, int row)
{
    selected_cells_.clear();
    selected_cells_.insert(std::make_pair(column, row));
    anchor_col_ = column;
    anchor_row_ = row;
    ApplySelectionToItem();
    emit selectionChanged();
}

void CustomControllerLayoutGrid::ToggleCellInSelection(int column, int row)
{
    const std::pair<int, int> key(column, row);
    if(selected_cells_.count(key) > 0)
    {
        selected_cells_.erase(key);
    }
    else
    {
        selected_cells_.insert(key);
    }
    anchor_col_ = column;
    anchor_row_ = row;
    ApplySelectionToItem();
    emit selectionChanged();
}

void CustomControllerLayoutGrid::SelectRect(int col_a, int row_a, int col_b, int row_b, bool replace)
{
    if(replace)
    {
        selected_cells_.clear();
    }

    const int min_col = std::min(col_a, col_b);
    const int max_col = std::max(col_a, col_b);
    const int min_row = std::min(row_a, row_b);
    const int max_row = std::max(row_a, row_b);

    for(int row = min_row; row <= max_row; row++)
    {
        for(int col = min_col; col <= max_col; col++)
        {
            selected_cells_.insert(std::make_pair(col, row));
        }
    }

    ApplySelectionToItem();
    emit selectionChanged();
}

void CustomControllerLayoutGrid::FinishRubberBandSelection(bool add_to_selection)
{
    if(!grid_item_ || rubber_band_rect_.isNull())
    {
        return;
    }

    int col_a = -1;
    int row_a = -1;
    int col_b = -1;
    int row_b = -1;
    if(!grid_item_->CellAtScenePos(mapToScene(rubber_band_rect_.topLeft()), &col_a, &row_a)
       || !grid_item_->CellAtScenePos(mapToScene(rubber_band_rect_.bottomRight()), &col_b, &row_b))
    {
        return;
    }

    SelectRect(col_a, row_a, col_b, row_b, !add_to_selection);
}

void CustomControllerLayoutGrid::UpdateHoverTooltip(const QPoint& view_pos)
{
    int col = -1;
    int row = -1;
    if(!CellAtViewPos(view_pos, &col, &row) || !grid_item_)
    {
        QToolTip::hideText();
        return;
    }

    const QString tip = grid_item_->TooltipAt(col, row);
    if(tip.isEmpty())
    {
        QToolTip::hideText();
        return;
    }

    QToolTip::showText(mapToGlobal(view_pos), tip, this);
}

void CustomControllerLayoutGrid::wheelEvent(QWheelEvent* event)
{
    const int angle = event->angleDelta().y();
    if(angle == 0)
    {
        QGraphicsView::wheelEvent(event);
        return;
    }

    const qreal factor = (angle > 0)
        ? (event->modifiers() & Qt::ControlModifier ? 1.25 : 1.08)
        : (event->modifiers() & Qt::ControlModifier ? 0.8 : 0.93);
    scale(factor, factor);
    if(grid_item_)
    {
        grid_item_->update();
    }
    event->accept();
}

void CustomControllerLayoutGrid::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    {
        left_button_pressed_ = true;
        rubber_band_origin_  = event->pos();
        rubber_band_active_  = false;
    }

    QGraphicsView::mousePressEvent(event);
}

void CustomControllerLayoutGrid::mouseMoveEvent(QMouseEvent* event)
{
    if(left_button_pressed_
       && (event->pos() - rubber_band_origin_).manhattanLength() > 4)
    {
        rubber_band_active_ = true;
    }

    QGraphicsView::mouseMoveEvent(event);
    UpdateHoverTooltip(event->pos());
}

void CustomControllerLayoutGrid::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && left_button_pressed_)
    {
        left_button_pressed_ = false;

        int col = -1;
        int row = -1;
        const bool has_cell = CellAtViewPos(event->pos(), &col, &row);

        if(rubber_band_active_)
        {
            rubber_band_rect_ = QRect(rubber_band_origin_, event->pos()).normalized();
            FinishRubberBandSelection(event->modifiers() & Qt::ControlModifier);
            rubber_band_active_ = false;
        }
        else if(has_cell)
        {
            if(event->modifiers() & Qt::ControlModifier)
            {
                ToggleCellInSelection(col, row);
            }
            else if(event->modifiers() & Qt::ShiftModifier
                    && anchor_col_ >= 0 && anchor_row_ >= 0)
            {
                SelectRect(anchor_col_, anchor_row_, col, row, true);
            }
            else
            {
                SelectSingleCell(col, row);
            }

            emit cellClicked(col, row);
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void CustomControllerLayoutGrid::mouseDoubleClickEvent(QMouseEvent* event)
{
    int col = -1;
    int row = -1;
    if(event->button() == Qt::LeftButton && CellAtViewPos(event->pos(), &col, &row))
    {
        SelectSingleCell(col, row);
        emit cellDoubleClicked(col, row);
        event->accept();
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

void CustomControllerLayoutGrid::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if(needs_fit_)
    {
        FitGridInView();
    }
}

void CustomControllerLayoutGrid::showEvent(QShowEvent* event)
{
    QGraphicsView::showEvent(event);
    needs_fit_ = true;
    FitGridInView();
}

bool CustomControllerLayoutGrid::event(QEvent* event)
{
    if(event->type() == QEvent::ToolTip)
    {
        const QHelpEvent* help = static_cast<QHelpEvent*>(event);
        int col = -1;
        int row = -1;
        if(CellAtViewPos(help->pos(), &col, &row) && grid_item_)
        {
            const QString tip = grid_item_->TooltipAt(col, row);
            if(!tip.isEmpty())
            {
                QToolTip::showText(help->globalPos(), tip, this);
                return true;
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }

    return QGraphicsView::event(event);
}
