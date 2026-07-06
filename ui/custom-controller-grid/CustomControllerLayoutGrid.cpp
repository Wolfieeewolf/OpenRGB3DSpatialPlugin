// SPDX-License-Identifier: GPL-2.0-only
#include "CustomControllerLayoutGrid.h"
#include "CustomControllerGridItem.h"
#include "CustomControllerGridLayoutMath.h"
#include "CustomControllerGridScene.h"
#include "GridSpaceUtils.h"
#include <QHelpEvent>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QRubberBand>
#include <QResizeEvent>
#include <QShowEvent>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
namespace
{
constexpr qreal kFitMarginPx      = 20.0;
constexpr qreal kMinCellPx        = 22.0;
constexpr qreal kMaxCellPx        = 32.0;
constexpr int   kMinViewportPx    = 80;
constexpr qreal kMinHitSlopScene  = 0.12;
constexpr qreal kMaxHitSlopScene  = 0.55;
using namespace CustomControllerGridLayoutMath;
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
    rubber_band_overlay_ = new QRubberBand(QRubberBand::Rectangle, viewport());
    rubber_band_overlay_->hide();
}
void CustomControllerLayoutGrid::SyncSceneRect()
{
    if(scene_ && grid_item_)
    {
        const QRectF bounds = grid_item_->boundingRect();
        scene_->SetSceneSizeMm(bounds.width(), bounds.height());
    }
}
qreal CustomControllerLayoutGrid::HitSlopScene() const
{
    const QTransform view_to_scene = transform().inverted();
    const QPointF slop_px(6.0, 0.0);
    const QPointF slop_scene = view_to_scene.map(slop_px) - view_to_scene.map(QPointF(0.0, 0.0));
    return std::clamp(std::abs(slop_scene.x()), kMinHitSlopScene, kMaxHitSlopScene);
}
float CustomControllerLayoutGrid::MmPerSceneUnit() const
{
    return grid_item_ ? grid_item_->MmPerSceneUnit() : DEFAULT_GRID_SCALE_MM;
}
void CustomControllerLayoutGrid::SetMmPerSceneUnit(float mm_per_unit)
{
    if(grid_item_)
    {
        grid_item_->SetMmPerSceneUnit(mm_per_unit);
    }
    SyncSceneRect();
    if(isVisible())
    {
        FitGridInView();
    }
}
void CustomControllerLayoutGrid::ShowResizeSizeTooltip(const QPoint& view_pos, const QString& text)
{
    if(text.isEmpty())
    {
        QToolTip::hideText();
        return;
    }
    QToolTip::showText(mapToGlobal(view_pos), text, this);
}
void CustomControllerLayoutGrid::SetGridSize(int width, int height)
{
    if(grid_item_)
    {
        grid_item_->SetGridSize(width, height);
    }
    SyncSceneRect();
    if(width != last_grid_w_ || height != last_grid_h_)
    {
        needs_fit_   = true;
        last_grid_w_ = width;
        last_grid_h_ = height;
        FitGridInView();
    }
}
void CustomControllerLayoutGrid::SetColumnWidthsMm(const QVector<float>& widths_mm)
{
    if(grid_item_)
    {
        grid_item_->SetColumnWidthsMm(widths_mm);
    }
    SyncSceneRect();
}
void CustomControllerLayoutGrid::SetRowHeightsMm(const QVector<float>& heights_mm)
{
    if(grid_item_)
    {
        grid_item_->SetRowHeightsMm(heights_mm);
    }
    SyncSceneRect();
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
void CustomControllerLayoutGrid::SelectCellAt(int column, int row)
{
    SelectSingleCell(column, row);
}
void CustomControllerLayoutGrid::FitGridInView()
{
    if(!grid_item_)
    {
        return;
    }
    const QRectF bounds = grid_item_->boundingRect();
    const qreal grid_w = bounds.width();
    const qreal grid_h = bounds.height();
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
    const qreal row_header_w = RowHeaderWidthScene(grid_item_->RowHeightsMm(), MmPerSceneUnit());
    const qreal col_header_h = ColHeaderHeightScene(grid_item_->ColumnWidthsMm(), MmPerSceneUnit());
    const qreal content_w    = std::max(0.001, grid_w - row_header_w);
    const qreal content_h    = std::max(0.001, grid_h - col_header_h);
    const int   gw        = grid_item_->GridWidth();
    const int   gh        = grid_item_->GridHeight();
    qreal avg_cell_scene  = 1.0;
    if(gw > 0 && gh > 0)
    {
        avg_cell_scene = std::min(content_w / gw, content_h / gh);
    }
    const qreal min_scale = kMinCellPx / avg_cell_scene;
    const qreal max_scale = kMaxCellPx / avg_cell_scene;
    qreal target_scale    = fit_scale;
    const qreal max_dim   = std::max(grid_w, grid_h);
    if(max_dim <= 64.0)
    {
        target_scale = std::min(std::max(fit_scale, min_scale), max_scale);
    }
    else
    {
        target_scale = std::max(fit_scale, min_scale);
    }
    resetTransform();
    scale(target_scale, target_scale);
    centerOn(bounds.center());
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
bool CustomControllerLayoutGrid::HeaderAtViewPos(const QPoint& view_pos, int* column_header, int* row_header) const
{
    if(column_header)
    {
        *column_header = -1;
    }
    if(row_header)
    {
        *row_header = -1;
    }
    if(!grid_item_)
    {
        return false;
    }
    const QPointF scene_pos   = mapToScene(view_pos);
    const int     col_hdr_idx = grid_item_->ColumnHeaderIndexAtScenePos(scene_pos);
    const int     row_hdr_idx = grid_item_->RowHeaderIndexAtScenePos(scene_pos);
    if(col_hdr_idx >= 0 && column_header)
    {
        *column_header = col_hdr_idx;
    }
    if(row_hdr_idx >= 0 && row_header)
    {
        *row_header = row_hdr_idx;
    }
    return col_hdr_idx >= 0 || row_hdr_idx >= 0;
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
void CustomControllerLayoutGrid::UpdateResizeCursor(const QPoint& view_pos)
{
    if(!grid_item_)
    {
        unsetCursor();
        return;
    }
    const QPointF scene_pos = mapToScene(view_pos);
    const qreal slop        = HitSlopScene();
    if(grid_item_->ColumnBorderIndexAtScenePos(scene_pos, slop) >= 0)
    {
        setCursor(Qt::SplitHCursor);
        return;
    }
    if(grid_item_->RowBorderIndexAtScenePos(scene_pos, slop) >= 0)
    {
        setCursor(Qt::SplitVCursor);
        return;
    }
    if(grid_item_->ColumnHeaderIndexAtScenePos(scene_pos) >= 0
       || grid_item_->RowHeaderIndexAtScenePos(scene_pos) >= 0)
    {
        setCursor(Qt::PointingHandCursor);
        return;
    }
    unsetCursor();
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
    if(event->button() == Qt::LeftButton && grid_item_)
    {
        const QPointF scene_pos = mapToScene(event->pos());
        const qreal slop        = HitSlopScene();
        const int col_resize    = grid_item_->ColumnResizeIndexAtScenePos(scene_pos, slop);
        const int row_resize    = grid_item_->RowResizeIndexAtScenePos(scene_pos, slop);
        if(col_resize >= 0)
        {
            resize_mode_            = ResizeMode::Column;
            resize_index_           = col_resize;
            resize_start_size_      = grid_item_->ColumnWidthsMm()[col_resize];
            resize_start_scene_pos_ = scene_pos.x();
            left_button_pressed_    = true;
            event->accept();
            return;
        }
        if(row_resize >= 0)
        {
            resize_mode_            = ResizeMode::Row;
            resize_index_           = row_resize;
            resize_start_size_      = grid_item_->RowHeightsMm()[row_resize];
            resize_start_scene_pos_ = scene_pos.y();
            left_button_pressed_    = true;
            pressed_header_col_     = -1;
            pressed_header_row_     = -1;
            event->accept();
            return;
        }
        const int col_header = grid_item_->ColumnHeaderIndexAtScenePos(scene_pos);
        const int row_header = grid_item_->RowHeaderIndexAtScenePos(scene_pos);
        if(col_header >= 0 || row_header >= 0)
        {
            pressed_header_col_  = col_header;
            pressed_header_row_  = row_header;
            left_button_pressed_ = true;
            event->accept();
            return;
        }
        pressed_header_col_  = -1;
        pressed_header_row_  = -1;
        left_button_pressed_ = true;
        rubber_band_origin_  = event->pos();
        rubber_band_active_  = false;
        if(rubber_band_overlay_)
        {
            rubber_band_overlay_->hide();
        }
    }
    QGraphicsView::mousePressEvent(event);
}
void CustomControllerLayoutGrid::mouseMoveEvent(QMouseEvent* event)
{
    if(left_button_pressed_ && resize_mode_ != ResizeMode::None && grid_item_)
    {
        const QPointF scene_pos = mapToScene(event->pos());
        if(resize_mode_ == ResizeMode::Column && resize_index_ >= 0)
        {
            const float delta_mm = SceneToMm(scene_pos.x() - resize_start_scene_pos_, MmPerSceneUnit());
            const float size     = std::max(kMinCellSizeMm, resize_start_size_ + delta_mm);
            QVector<float> widths = grid_item_->ColumnWidthsMm();
            if(resize_index_ < widths.size())
            {
                widths[resize_index_] = size;
                SetColumnWidthsMm(widths);
                emit columnWidthChanged(resize_index_, size);
                ShowResizeSizeTooltip(event->pos(),
                                      tr("Column %1: %2 mm")
                                          .arg(resize_index_ + 1)
                                          .arg(static_cast<double>(size), 0, 'f', 1));
            }
        }
        else if(resize_mode_ == ResizeMode::Row && resize_index_ >= 0)
        {
            const float delta_mm = SceneToMm(scene_pos.y() - resize_start_scene_pos_, MmPerSceneUnit());
            const float size     = std::max(kMinCellSizeMm, resize_start_size_ + delta_mm);
            QVector<float> heights = grid_item_->RowHeightsMm();
            if(resize_index_ < heights.size())
            {
                heights[resize_index_] = size;
                SetRowHeightsMm(heights);
                emit rowHeightChanged(resize_index_, size);
                ShowResizeSizeTooltip(event->pos(),
                                      tr("Row %1: %2 mm")
                                          .arg(resize_index_ + 1)
                                          .arg(static_cast<double>(size), 0, 'f', 1));
            }
        }
        event->accept();
        return;
    }
    if(left_button_pressed_
       && (event->pos() - rubber_band_origin_).manhattanLength() > 4)
    {
        rubber_band_active_ = true;
    }
    if(rubber_band_active_ && rubber_band_overlay_)
    {
        rubber_band_overlay_->setGeometry(QRect(rubber_band_origin_, event->pos()).normalized());
        rubber_band_overlay_->show();
        QToolTip::hideText();
    }
    else if(!left_button_pressed_)
    {
        UpdateResizeCursor(event->pos());
        UpdateHoverTooltip(event->pos());
    }
    QGraphicsView::mouseMoveEvent(event);
}
void CustomControllerLayoutGrid::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && left_button_pressed_)
    {
        if(resize_mode_ != ResizeMode::None)
        {
            resize_mode_          = ResizeMode::None;
            resize_index_         = -1;
            QToolTip::hideText();
            left_button_pressed_  = false;
            unsetCursor();
            event->accept();
            return;
        }
        left_button_pressed_ = false;
        const bool header_click = !rubber_band_active_
                                  && (event->pos() - rubber_band_origin_).manhattanLength() <= 4;
        if(header_click && pressed_header_col_ >= 0)
        {
            emit columnHeaderClicked(pressed_header_col_);
            pressed_header_col_ = -1;
            pressed_header_row_ = -1;
            event->accept();
            return;
        }
        if(header_click && pressed_header_row_ >= 0)
        {
            emit rowHeaderClicked(pressed_header_row_);
            pressed_header_col_ = -1;
            pressed_header_row_ = -1;
            event->accept();
            return;
        }
        pressed_header_col_ = -1;
        pressed_header_row_ = -1;
        int col = -1;
        int row = -1;
        const bool has_cell = CellAtViewPos(event->pos(), &col, &row);
        if(rubber_band_active_)
        {
            if(rubber_band_overlay_)
            {
                rubber_band_overlay_->hide();
            }
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
    if(event->button() == Qt::LeftButton && grid_item_)
    {
        const QPointF scene_pos = mapToScene(event->pos());
        const int col_header    = grid_item_->ColumnHeaderIndexAtScenePos(scene_pos);
        const int row_header    = grid_item_->RowHeaderIndexAtScenePos(scene_pos);
        if(col_header >= 0)
        {
            emit columnHeaderClicked(col_header);
            event->accept();
            return;
        }
        if(row_header >= 0)
        {
            emit rowHeaderClicked(row_header);
            event->accept();
            return;
        }
    }
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
void CustomControllerLayoutGrid::contextMenuEvent(QContextMenuEvent* event)
{
    if(event)
    {
        emit contextMenuRequested(event->globalPos());
        event->accept();
        return;
    }
    QGraphicsView::contextMenuEvent(event);
}
bool CustomControllerLayoutGrid::event(QEvent* event)
{
    if(event->type() == QEvent::ToolTip)
    {
        const QHelpEvent* help = static_cast<QHelpEvent*>(event);
        int col_header = -1;
        int row_header = -1;
        if(HeaderAtViewPos(help->pos(), &col_header, &row_header) && grid_item_)
        {
            QString tip;
            if(col_header >= 0)
            {
                tip = tr("Column %1 — %2 mm\nClick to change width")
                          .arg(col_header + 1)
                          .arg(static_cast<double>(grid_item_->ColumnWidthsMm()[col_header]), 0, 'f', 1);
            }
            else if(row_header >= 0)
            {
                tip = tr("Row %1 — %2 mm\nClick to change height")
                          .arg(row_header + 1)
                          .arg(static_cast<double>(grid_item_->RowHeightsMm()[row_header]), 0, 'f', 1);
            }
            if(!tip.isEmpty())
            {
                QToolTip::showText(help->globalPos(), tip, this);
                return true;
            }
        }
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
