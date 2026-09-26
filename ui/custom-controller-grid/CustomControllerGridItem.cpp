// SPDX-License-Identifier: GPL-2.0-only
#include "CustomControllerGridItem.h"
#include "CustomControllerGridLayoutMath.h"
#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QFontMetricsF>
#include <QStyleOptionGraphicsItem>
#include <QtGlobal>
#include <cmath>
namespace
{
constexpr qreal kCellInsetRatio = 0.06;
constexpr qreal kMinLabelCellPx = 7.0;
constexpr qreal kLabelFillRatio = 0.82;
using namespace CustomControllerGridLayoutMath;
QPen CosmeticPen(const QColor& color, qreal width = 1.0, Qt::PenStyle style = Qt::SolidLine)
{
    QPen pen(color, width);
    pen.setCosmetic(true);
    pen.setStyle(style);
    return pen;
}
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
void DrawHeaderLabel(QPainter* painter,
                     const QRectF& rect,
                     const QString& text,
                     const QColor& color,
                     qreal px_per_scene_unit)
{
    if(text.isEmpty() || px_per_scene_unit < 0.5)
    {
        return;
    }
    const qreal rect_w_px = rect.width() * px_per_scene_unit;
    const qreal rect_h_px = rect.height() * px_per_scene_unit;
    if(rect_w_px < 8.0 || rect_h_px < 8.0)
    {
        return;
    }
    QFont font = painter->font();
    font.setBold(true);
    int font_px = qBound(8, static_cast<int>(std::min(rect_w_px, rect_h_px) * 0.42), 18);
    font.setPixelSize(font_px);
    const QFontMetricsF metrics(font);
    if(metrics.horizontalAdvance(text) > rect_w_px * 0.92)
    {
        font_px = qMax(7, static_cast<int>(font_px * rect_w_px * 0.92 / metrics.horizontalAdvance(text)));
        font.setPixelSize(font_px);
    }
    painter->save();
    painter->translate(rect.center());
    painter->scale(1.0 / px_per_scene_unit, 1.0 / px_per_scene_unit);
    painter->setFont(font);
    painter->setPen(color);
    painter->drawText(QRectF(-rect_w_px * 0.5, -rect_h_px * 0.5, rect_w_px, rect_h_px),
                      Qt::AlignCenter, text);
    painter->restore();
}
QString FormatMmLabel(float mm)
{
    return QString::number(mm, 'f', (mm < 10.0f) ? 1 : 0);
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
    return QRectF(0,
                  0,
                  TotalGridWidthScene(column_widths_mm_, row_heights_mm_, mm_per_unit_),
                  TotalGridHeightScene(row_heights_mm_, column_widths_mm_, mm_per_unit_));
}
void CustomControllerGridItem::EnsureSizeArrays()
{
    while(column_widths_mm_.size() < grid_width_)
    {
        column_widths_mm_.append(kDefaultCellSizeMm);
    }
    while(column_widths_mm_.size() > grid_width_)
    {
        column_widths_mm_.removeLast();
    }
    while(row_heights_mm_.size() < grid_height_)
    {
        row_heights_mm_.append(kDefaultCellSizeMm);
    }
    while(row_heights_mm_.size() > grid_height_)
    {
        row_heights_mm_.removeLast();
    }
}
void CustomControllerGridItem::SetGridSize(int width, int height)
{
    prepareGeometryChange();
    grid_width_  = std::max(0, width);
    grid_height_ = std::max(0, height);
    EnsureSizeArrays();
    cells_.resize(grid_width_ * grid_height_);
    update();
}
void CustomControllerGridItem::SetMmPerSceneUnit(float mm_per_unit)
{
    const float next = CustomControllerGridLayoutMath::MmPerSceneUnit(mm_per_unit);
    if(std::abs(next - mm_per_unit_) < 0.001f)
    {
        return;
    }
    prepareGeometryChange();
    mm_per_unit_ = next;
    update();
}
void CustomControllerGridItem::SetColumnWidthsMm(const QVector<float>& widths_mm)
{
    prepareGeometryChange();
    column_widths_mm_ = widths_mm;
    EnsureSizeArrays();
    update();
}
void CustomControllerGridItem::SetRowHeightsMm(const QVector<float>& heights_mm)
{
    prepareGeometryChange();
    row_heights_mm_ = heights_mm;
    EnsureSizeArrays();
    update();
}
namespace
{
bool CellVisualEqual(const CustomControllerGridCellVisual& a, const CustomControllerGridCellVisual& b)
{
    return a.fill == b.fill
           && a.text == b.text
           && a.label == b.label
           && a.tooltip == b.tooltip
           && a.is_hole == b.is_hole
           && a.is_light_blocker == b.is_light_blocker
           && a.is_empty == b.is_empty;
}

bool CellVisualsEqual(const QVector<CustomControllerGridCellVisual>& a,
                      const QVector<CustomControllerGridCellVisual>& b)
{
    if(a.size() != b.size())
    {
        return false;
    }
    for(int i = 0; i < a.size(); i++)
    {
        if(!CellVisualEqual(a[i], b[i]))
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
void CustomControllerGridItem::SetCellAt(int column, int row, const CustomControllerGridCellVisual& visual)
{
    if(column < 0 || row < 0 || column >= grid_width_ || row >= grid_height_)
    {
        return;
    }
    const int index = row * grid_width_ + column;
    if(index < 0 || index >= cells_.size())
    {
        return;
    }
    if(CellVisualEqual(cells_[index], visual))
    {
        return;
    }
    cells_[index] = visual;
    UpdateCellDirtyRect(column, row);
}
void CustomControllerGridItem::SetSelectedCells(const std::set<std::pair<int, int>>& cells)
{
    if(selected_cells_ == cells)
    {
        return;
    }
    selected_cells_ = cells;
    update();
}

void CustomControllerGridItem::UpdateCellDirtyRect(int column, int row)
{
    if(column < 0 || row < 0)
    {
        return;
    }
    update(CellRectScene(column, row).adjusted(-2.0, -2.0, 2.0, 2.0));
}

void CustomControllerGridItem::SetDragHoverCell(int column, int row)
{
    if(drag_hover_col_ == column && drag_hover_row_ == row)
    {
        return;
    }
    const int prev_col = drag_hover_col_;
    const int prev_row = drag_hover_row_;
    drag_hover_col_ = column;
    drag_hover_row_ = row;
    UpdateCellDirtyRect(prev_col, prev_row);
    UpdateCellDirtyRect(drag_hover_col_, drag_hover_row_);
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
    return CustomControllerGridLayoutMath::CellAtScenePos(column_widths_mm_,
                                                          row_heights_mm_,
                                                          grid_width_,
                                                          grid_height_,
                                                          scene_pos,
                                                          mm_per_unit_,
                                                          column,
                                                          row);
}
int CustomControllerGridItem::ColumnResizeIndexAtScenePos(const QPointF& scene_pos, qreal hit_slop_scene) const
{
    return CustomControllerGridLayoutMath::ColumnResizeIndexAtScenePos(
        column_widths_mm_, row_heights_mm_, grid_width_, scene_pos, hit_slop_scene, mm_per_unit_);
}
int CustomControllerGridItem::RowResizeIndexAtScenePos(const QPointF& scene_pos, qreal hit_slop_scene) const
{
    return CustomControllerGridLayoutMath::RowResizeIndexAtScenePos(
        row_heights_mm_, column_widths_mm_, grid_height_, scene_pos, hit_slop_scene, mm_per_unit_);
}
int CustomControllerGridItem::ColumnBorderIndexAtScenePos(const QPointF& scene_pos, qreal hit_slop_scene) const
{
    return ColumnResizeIndexAtScenePos(scene_pos, hit_slop_scene);
}
int CustomControllerGridItem::RowBorderIndexAtScenePos(const QPointF& scene_pos, qreal hit_slop_scene) const
{
    return RowResizeIndexAtScenePos(scene_pos, hit_slop_scene);
}
int CustomControllerGridItem::ColumnHeaderIndexAtScenePos(const QPointF& scene_pos) const
{
    return CustomControllerGridLayoutMath::ColumnHeaderIndexAtScenePos(
        column_widths_mm_, row_heights_mm_, grid_width_, scene_pos, mm_per_unit_);
}
int CustomControllerGridItem::RowHeaderIndexAtScenePos(const QPointF& scene_pos) const
{
    return CustomControllerGridLayoutMath::RowHeaderIndexAtScenePos(
        row_heights_mm_, column_widths_mm_, grid_height_, scene_pos, mm_per_unit_);
}
QRectF CustomControllerGridItem::CellRectScene(int column, int row) const
{
    if(column < 0 || row < 0 || column >= grid_width_ || row >= grid_height_)
    {
        return QRectF();
    }
    return QRectF(ColumnLeftScene(column_widths_mm_, row_heights_mm_, column, mm_per_unit_),
                  RowTopScene(row_heights_mm_, column_widths_mm_, row, mm_per_unit_),
                  ColumnWidthScene(column_widths_mm_, column, mm_per_unit_),
                  RowHeightScene(row_heights_mm_, row, mm_per_unit_));
}
QString CustomControllerGridItem::TooltipAt(int column, int row) const
{
    const CustomControllerGridCellVisual* cell = CellVisual(column, row);
    return cell ? cell->tooltip : QString();
}
bool CustomControllerGridItem::CellVisualAt(int column, int row, CustomControllerGridCellVisual* out) const
{
    const CustomControllerGridCellVisual* cell = CellVisual(column, row);
    if(!cell || !out)
    {
        return false;
    }
    *out = *cell;
    return true;
}
bool CustomControllerGridItem::IsCellSelected(int column, int row) const
{
    return selected_cells_.count(std::make_pair(column, row)) > 0;
}
void CustomControllerGridItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    if(grid_width_ <= 0 || grid_height_ <= 0)
    {
        return;
    }

    const QRectF exposed = option ? option->exposedRect : boundingRect();

    const QColor header_bg          = QColor(228, 228, 232);
    const QColor header_text        = QColor(16, 16, 18);
    const QColor header_border      = QColor(140, 140, 148);
    const QColor hole_line_color    = QApplication::palette().color(QPalette::Mid);
    const QColor cell_border_color  = QApplication::palette().color(QPalette::Mid);
    const QColor default_text_color = QApplication::palette().color(QPalette::Text);
    const QPen   cell_border_pen    = CosmeticPen(cell_border_color);
    const QPen   hole_line_pen      = CosmeticPen(hole_line_color, 1.0, Qt::DotLine);
    const QPen   selection_pen      = CosmeticPen(selection_color_, 2.0);
    const QPen   header_pen         = CosmeticPen(header_border);
    const qreal px_per_scene_unit = option
        ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform())
        : 1.0;
    const qreal content_w     = SumSceneSizes(column_widths_mm_, mm_per_unit_);
    const qreal content_h     = SumSceneSizes(row_heights_mm_, mm_per_unit_);
    const qreal row_header_w  = RowHeaderWidthScene(row_heights_mm_, mm_per_unit_);
    const qreal col_header_h  = ColHeaderHeightScene(column_widths_mm_, mm_per_unit_);
    const QColor cell_area_bg = QApplication::palette().color(QPalette::Base);

    QVector<qreal> col_left(grid_width_ + 1);
    QVector<qreal> row_top(grid_height_ + 1);
    col_left[0] = row_header_w;
    for(int col = 0; col < grid_width_; ++col)
    {
        col_left[col + 1] = col_left[col] + ColumnWidthScene(column_widths_mm_, col, mm_per_unit_);
    }
    row_top[0] = col_header_h;
    for(int row = 0; row < grid_height_; ++row)
    {
        row_top[row + 1] = row_top[row] + RowHeightScene(row_heights_mm_, row, mm_per_unit_);
    }

    const QRectF corner_rect(0, 0, row_header_w, col_header_h);
    const QRectF row_header_rect(0, col_header_h, row_header_w, content_h);
    const QRectF col_header_rect(row_header_w, 0, content_w, col_header_h);
    const QRectF cell_area_rect(row_header_w, col_header_h, content_w, content_h);

    painter->setPen(header_pen);
    painter->setBrush(header_bg);
    if(exposed.intersects(corner_rect))
    {
        painter->drawRect(corner_rect);
    }
    if(exposed.intersects(row_header_rect))
    {
        painter->drawRect(row_header_rect);
    }
    if(exposed.intersects(col_header_rect))
    {
        painter->drawRect(col_header_rect);
    }
    if(exposed.intersects(cell_area_rect))
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(cell_area_bg);
        painter->drawRect(cell_area_rect);
    }

    if(exposed.intersects(col_header_rect))
    {
        for(int col = 0; col < grid_width_; ++col)
        {
            const QRectF header_rect(col_left[col], 0.0, col_left[col + 1] - col_left[col], col_header_h);
            if(!exposed.intersects(header_rect))
            {
                continue;
            }
            DrawHeaderLabel(painter,
                            header_rect,
                            FormatMmLabel(column_widths_mm_[col]),
                            header_text,
                            px_per_scene_unit);
        }
    }
    if(exposed.intersects(row_header_rect))
    {
        for(int row = 0; row < grid_height_; ++row)
        {
            const QRectF header_rect(0.0, row_top[row], row_header_w, row_top[row + 1] - row_top[row]);
            if(!exposed.intersects(header_rect))
            {
                continue;
            }
            DrawHeaderLabel(painter,
                            header_rect,
                            FormatMmLabel(row_heights_mm_[row]),
                            header_text,
                            px_per_scene_unit);
        }
    }

    QColor hover_fill = selection_color_.isValid() ? selection_color_ : QColor(80, 160, 255);
    hover_fill.setAlpha(60);
    QPen hover_pen = CosmeticPen(selection_color_.isValid()
                                     ? selection_color_.lighter(130)
                                     : QColor(120, 200, 255));
    hover_pen.setWidthF(2.0);

    for(int row = 0; row < grid_height_; ++row)
    {
        const qreal y0 = row_top[row];
        const qreal y1 = row_top[row + 1];
        if(y1 < exposed.top() || y0 > exposed.bottom())
        {
            continue;
        }
        for(int col = 0; col < grid_width_; ++col)
        {
            const qreal x0 = col_left[col];
            const qreal x1 = col_left[col + 1];
            if(x1 < exposed.left() || x0 > exposed.right())
            {
                continue;
            }

            const CustomControllerGridCellVisual* cell = CellVisual(col, row);
            if(!cell)
            {
                continue;
            }

            QRectF tile(x0, y0, x1 - x0, y1 - y0);
            const qreal inset_x = tile.width() * kCellInsetRatio;
            const qreal inset_y = tile.height() * kCellInsetRatio;
            tile = tile.adjusted(inset_x, inset_y, -inset_x, -inset_y);
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
            else if(cell->is_light_blocker)
            {
                painter->setPen(CosmeticPen(QColor(120, 70, 160)));
                painter->setBrush(QColor(55, 35, 75, 200));
                painter->drawRect(tile);
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
                painter->drawRect(tile);
            }
            if(col == drag_hover_col_ && row == drag_hover_row_)
            {
                painter->setPen(hover_pen);
                painter->setBrush(hover_fill);
                painter->drawRect(tile);
            }
        }
    }

    painter->setPen(header_pen);
    painter->setBrush(Qt::NoBrush);
    if(exposed.intersects(cell_area_rect) || exposed.intersects(col_header_rect) || exposed.intersects(row_header_rect))
    {
        for(int col = 0; col <= grid_width_; ++col)
        {
            const qreal x = col_left[col];
            if(x < exposed.left() - 1.0 || x > exposed.right() + 1.0)
            {
                continue;
            }
            painter->drawLine(QPointF(x, col_header_h), QPointF(x, col_header_h + content_h));
        }
        for(int row = 0; row <= grid_height_; ++row)
        {
            const qreal y = row_top[row];
            if(y < exposed.top() - 1.0 || y > exposed.bottom() + 1.0)
            {
                continue;
            }
            painter->drawLine(QPointF(row_header_w, y), QPointF(row_header_w + content_w, y));
        }
    }
}
