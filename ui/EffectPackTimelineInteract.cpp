// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackTimelineWidget.h"
#include "EffectPackCatalog.h"
#include "EffectPacks/EffectPackApplier.h"
#include "ZoneManager3D.h"

#include <QColorDialog>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

namespace
{

bool TargetsEqual(const EffectPack::Target& a, const EffectPack::Target& b)
{
    if(a.kind != b.kind || a.flatten_leds != b.flatten_leds)
    {
        return false;
    }
    switch(a.kind)
    {
        case EffectPack::TargetKind::All:
            return true;
        case EffectPack::TargetKind::Device:
            return a.device_name == b.device_name;
        case EffectPack::TargetKind::Zone:
            return a.device_name == b.device_name && a.zone_name == b.zone_name;
        case EffectPack::TargetKind::Leds:
            return a.device_name == b.device_name
                && a.zone_name == b.zone_name
                && a.led_indices == b.led_indices;
        case EffectPack::TargetKind::SceneZone:
            return a.scene_zone_name == b.scene_zone_name
                && a.flatten_leds == b.flatten_leds;
    }
    return false;
}

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor QColorToRgb(const QColor& c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

} // namespace


void EffectPackTimelineWidget::applyDrag(int mouse_x)
{
    EffectPack::Block* block = mutableBlock(drag_track_, drag_block_);
    if(!block)
    {
        return;
    }

    const int t = snapMs(xToTime(mouse_x));
    const int length = drag_origin_end_ - drag_origin_start_;

    switch(drag_op_)
    {
        case DragOp::Move:
        {
            int start = snapMs(t - drag_grab_offset_ms_);
            start = std::clamp(start, 0, std::max(0, duration_ms_ - length));
            block->start_ms = start;
            block->end_ms = start + length;
            break;
        }
        case DragOp::ResizeStart:
        {
            const int max_start = drag_origin_end_ - min_block_ms_;
            block->start_ms = std::clamp(t, 0, std::max(0, max_start));
            block->end_ms = drag_origin_end_;
            if(block->end_ms - block->start_ms < min_block_ms_)
            {
                block->start_ms = block->end_ms - min_block_ms_;
            }
            break;
        }
        case DragOp::ResizeEnd:
        {
            const int min_end = drag_origin_start_ + min_block_ms_;
            block->end_ms = std::clamp(t, min_end, duration_ms_);
            block->start_ms = drag_origin_start_;
            break;
        }
        case DragOp::ScrubHeader:
            playhead_ms_ = t;
            emit playheadChanged(playhead_ms_);
            break;
        case DragOp::None:
            break;
        default:
        {
            const DragOp unused = drag_op_;
            (void)unused;
            break;
        }
    }
    drag_moved_ = true;
    update();
}

void EffectPackTimelineWidget::finishDrag()
{
    if(drag_op_ == DragOp::None)
    {
        return;
    }
    const DragOp op = drag_op_;
    const int track = drag_track_;
    const int block = drag_block_;
    drag_op_ = DragOp::None;
    drag_track_ = -1;
    drag_block_ = -1;
    unsetCursor();

    if(drag_moved_ && (op == DragOp::Move || op == DragOp::ResizeStart || op == DragOp::ResizeEnd)
       && track >= 0 && block >= 0)
    {
        emit blockEdited(track, block);
    }
    drag_moved_ = false;
}

void EffectPackTimelineWidget::updateHoverCursor(int x, int y)
{
    if(drag_op_ != DragOp::None)
    {
        return;
    }
    BlockHit hit = BlockHit::None;
    if(hitTestBlock(x, y, nullptr, nullptr, nullptr, &hit))
    {
        if(hit == BlockHit::LeftEdge || hit == BlockHit::RightEdge)
        {
            setCursor(Qt::SizeHorCursor);
            return;
        }
        if(hit == BlockHit::Body)
        {
            setCursor(Qt::OpenHandCursor);
            return;
        }
    }
    unsetCursor();
}

void EffectPackTimelineWidget::populateAddEffectMenu(QMenu* menu, int row, int ms)
{
    if(!menu)
    {
        return;
    }
    QMenu* basic = menu->addMenu(EffectPackCatalog::CategoryLabel(EffectPackCatalog::Category::Basic));
    QMenu* pixel = menu->addMenu(EffectPackCatalog::CategoryLabel(EffectPackCatalog::Category::Pixel));
    QMenu* volume = menu->addMenu(EffectPackCatalog::CategoryLabel(EffectPackCatalog::Category::Volume));
    for(const EffectPackCatalog::Entry& e : EffectPackCatalog::AllEntries())
    {
        QMenu* dest = basic;
        if(e.category == EffectPackCatalog::Category::Pixel)
        {
            dest = pixel;
        }
        else if(e.category == EffectPackCatalog::Category::Volume)
        {
            dest = volume;
        }
        QAction* act = dest->addAction(EffectPackCatalog::MakeEffectIcon(e), QString::fromUtf8(e.name));
        connect(act, &QAction::triggered, this, [this, row, ms, type = e.type]() {
            emit effectAddRequested(row, ms, (int)type);
        });
    }
}

void EffectPackTimelineWidget::showAddEffectMenu(int row, int ms, const QPoint& global_pos)
{
    QMenu menu(this);
    QMenu* add = menu.addMenu(QStringLiteral("Add effect"));
    populateAddEffectMenu(add, row, ms);
    menu.exec(global_pos);
}

void EffectPackTimelineWidget::applyColorToBlock(int track, int block, RGBColor color)
{
    EffectPack::Block* b = mutableBlock(track, block);
    if(!b)
    {
        return;
    }
    b->color = color;
    b->color_from = color;
    if(!b->gradient.empty())
    {
        b->gradient.front().color = color;
    }
    EffectPack::EnsureBlockGradient(b);
    selected_track_ = track;
    selected_block_ = block;
    emit blockSelected(track, block);
    update();
    emit blockEdited(track, block);
}

bool EffectPackTimelineWidget::dropAt(const QPoint& pos, const QMimeData* mime)
{
    if(!mime || !pack_)
    {
        return false;
    }

    EffectPack::BlockType type = EffectPack::BlockType::Solid;
    if(EffectPackCatalog::EffectTypeFromMime(mime, &type))
    {
        if(pos.y() < header_height_ || pos.x() < gutter_width_)
        {
            return false;
        }
        const int row = (pos.y() - header_height_) / row_height_;
        if(row < 0 || row >= visible_rows_.size())
        {
            return false;
        }
        const int ms = xToTime(pos.x());
        selected_row_ = row;
        emit rowSelected(row);
        emit effectAddRequested(row, ms, (int)type);
        return true;
    }

    RGBColor color = 0;
    QString preset;
    QString curve;
    const bool has_color = EffectPackCatalog::ColorFromMime(mime, &color);
    const bool has_grad = EffectPackCatalog::GradientPresetFromMime(mime, &preset);
    const bool has_curve = EffectPackCatalog::CurvePresetFromMime(mime, &curve);
    if(!has_color && !has_grad && !has_curve)
    {
        return false;
    }

    int row = -1;
    int track = -1;
    int block = -1;
    if(!hitTestBlock(pos.x(), pos.y(), &row, &track, &block))
    {
        // Prefer currently selected block when dropping on empty space of its row.
        if(selected_track_ >= 0 && selected_block_ >= 0)
        {
            track = selected_track_;
            block = selected_block_;
        }
        else
        {
            return false;
        }
    }
    if(has_color)
    {
        applyColorToBlock(track, block, color);
        return true;
    }
    if(has_curve)
    {
        emit curvePresetApplied(track, block, curve);
        return true;
    }
    emit gradientPresetApplied(track, block, preset);
    return true;
}

void EffectPackTimelineWidget::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if(mime && (mime->hasFormat(QString::fromUtf8(EffectPackCatalog::kEffectMimeType))
                || mime->hasFormat(QString::fromUtf8(EffectPackCatalog::kColorMimeType))
                || mime->hasFormat(QString::fromUtf8(EffectPackCatalog::kGradientPresetMimeType))
                || mime->hasFormat(QString::fromUtf8(EffectPackCatalog::kCurvePresetMimeType))
                || mime->hasColor()))
    {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void EffectPackTimelineWidget::dragMoveEvent(QDragMoveEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if(mime && (mime->hasFormat(QString::fromUtf8(EffectPackCatalog::kEffectMimeType))
                || mime->hasFormat(QString::fromUtf8(EffectPackCatalog::kColorMimeType))
                || mime->hasFormat(QString::fromUtf8(EffectPackCatalog::kGradientPresetMimeType))
                || mime->hasFormat(QString::fromUtf8(EffectPackCatalog::kCurvePresetMimeType))
                || mime->hasColor()))
    {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void EffectPackTimelineWidget::dropEvent(QDropEvent* event)
{
    if(dropAt(event->position().toPoint(), event->mimeData()))
    {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void EffectPackTimelineWidget::editBlockColorAt(int track, int block, const QPoint& /*global_pos*/)
{
    EffectPack::Block* b = mutableBlock(track, block);
    if(!b)
    {
        return;
    }

    selected_track_ = track;
    selected_block_ = block;
    emit blockSelected(track, block);

    const RGBColor current = (b->type == EffectPack::BlockType::Fade) ? b->color_from : b->color;
    const QColor picked = QColorDialog::getColor(RgbToQColor(current), this, QStringLiteral("Block color"));
    if(!picked.isValid())
    {
        return;
    }
    const RGBColor rgb = QColorToRgb(picked);
    b->color = rgb;
    b->color_from = rgb;
    if(!b->gradient.empty())
    {
        b->gradient.front().color = rgb;
    }
    EffectPack::EnsureBlockGradient(b);
    update();
    emit blockEdited(track, block);
}

void EffectPackTimelineWidget::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        if(selected_track_ >= 0 && selected_block_ >= 0)
        {
            emit blockDeleteRequested(selected_track_, selected_block_);
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void EffectPackTimelineWidget::mousePressEvent(QMouseEvent* event)
{
    const int x = event->position().toPoint().x();
    const int y = event->position().toPoint().y();
    setFocus(Qt::MouseFocusReason);
    const int row = (y >= header_height_) ? ((y - header_height_) / row_height_) : -1;

    if(event->button() == Qt::RightButton)
    {
        int hit_row = 0;
        int track = -1;
        int block = -1;
        if(hitTestBlock(x, y, &hit_row, &track, &block))
        {
            selected_row_ = hit_row;
            selected_track_ = track;
            selected_block_ = block;
            emit rowSelected(hit_row);
            emit blockSelected(track, block);
            QMenu menu(this);
            QAction* color_act = menu.addAction(QStringLiteral("Change color…"));
            menu.addSeparator();
            QAction* del_act = menu.addAction(QStringLiteral("Delete"));
            QAction* chosen = menu.exec(event->globalPosition().toPoint());
            if(chosen == color_act)
            {
                editBlockColorAt(track, block, event->globalPosition().toPoint());
            }
            else if(chosen == del_act)
            {
                emit blockDeleteRequested(track, block);
            }
        }
        else if(y >= header_height_ && row >= 0 && row < visible_rows_.size() && x >= gutter_width_)
        {
            selected_row_ = row;
            emit rowSelected(row);
            const int ms = xToTime(x);
            playhead_ms_ = ms;
            emit playheadChanged(ms);
            showAddEffectMenu(row, ms, event->globalPosition().toPoint());
            update();
        }
        return;
    }

    if(event->button() != Qt::LeftButton)
    {
        return;
    }

    drag_moved_ = false;

    if(y < header_height_)
    {
        if(x >= gutter_width_)
        {
            drag_op_ = DragOp::ScrubHeader;
            playhead_ms_ = xToTime(x);
            emit playheadChanged(playhead_ms_);
            update();
        }
        return;
    }

    if(row < 0 || row >= visible_rows_.size())
    {
        return;
    }

    if(x < gutter_width_)
    {
        const Row& r = visible_rows_[row];
        selected_row_ = row;
        emit rowSelected(row);
        if(r.expandable)
        {
            const int indent = 6 + r.depth * 14;
            const int row_y = header_height_ + row * row_height_;
            const QRect plus(indent, row_y + (row_height_ - expand_hit_) / 2, expand_hit_, expand_hit_);
            if(plus.contains(x, y))
            {
                toggleExpand(r.path);
                return;
            }
        }
        if(r.reorderable && !r.scene_zone_name.isEmpty() && r.transform_index >= 0)
        {
            row_reorder_from_ = row;
            row_reorder_hover_ = row;
            setCursor(Qt::ClosedHandCursor);
        }
        update();
        return;
    }

    int hit_row = 0;
    int track = -1;
    int block = -1;
    BlockHit hit = BlockHit::None;
    if(hitTestBlock(x, y, &hit_row, &track, &block, &hit))
    {
        selected_row_ = hit_row;
        selected_track_ = track;
        selected_block_ = block;
        emit rowSelected(hit_row);
        emit blockSelected(track, block);

        EffectPack::Block* b = mutableBlock(track, block);
        if(!b)
        {
            return;
        }
        drag_track_ = track;
        drag_block_ = block;
        drag_origin_start_ = b->start_ms;
        drag_origin_end_ = b->end_ms;
        drag_grab_offset_ms_ = xToTime(x) - b->start_ms;

        if(hit == BlockHit::LeftEdge)
        {
            drag_op_ = DragOp::ResizeStart;
            setCursor(Qt::SizeHorCursor);
        }
        else if(hit == BlockHit::RightEdge)
        {
            drag_op_ = DragOp::ResizeEnd;
            setCursor(Qt::SizeHorCursor);
        }
        else
        {
            drag_op_ = DragOp::Move;
            setCursor(Qt::ClosedHandCursor);
        }
        update();
        return;
    }

    // Empty cell: select row + move playhead; clear block selection.
    selected_row_ = row;
    selected_track_ = -1;
    selected_block_ = -1;
    emit rowSelected(row);
    emit blockSelected(-1, -1);
    const int ms = xToTime(x);
    playhead_ms_ = ms;
    emit playheadChanged(ms);
    update();
}

void EffectPackTimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pt = event->position().toPoint();
    if(row_reorder_from_ >= 0 && (event->buttons() & Qt::LeftButton))
    {
        const int y = pt.y();
        const int row = (y >= header_height_) ? ((y - header_height_) / row_height_) : -1;
        if(row >= 0 && row < visible_rows_.size())
        {
            const Row& from = visible_rows_[row_reorder_from_];
            const Row& hover = visible_rows_[row];
            if(hover.reorderable
               && hover.scene_zone_name == from.scene_zone_name
               && hover.transform_index >= 0)
            {
                row_reorder_hover_ = row;
            }
        }
        update();
        return;
    }
    if(drag_op_ != DragOp::None && (event->buttons() & Qt::LeftButton))
    {
        applyDrag(pt.x());
        return;
    }
    updateHoverCursor(pt.x(), pt.y());
}

void EffectPackTimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    {
        if(row_reorder_from_ >= 0)
        {
            const int from = row_reorder_from_;
            const int to = row_reorder_hover_;
            row_reorder_from_ = -1;
            row_reorder_hover_ = -1;
            unsetCursor();
            if(from >= 0 && to >= 0 && from != to
               && from < visible_rows_.size() && to < visible_rows_.size())
            {
                const Row& a = visible_rows_[from];
                const Row& b = visible_rows_[to];
                if(a.reorderable && b.reorderable
                   && a.scene_zone_name == b.scene_zone_name
                   && !a.scene_zone_name.isEmpty())
                {
                    QVector<int> order;
                    for(const Row& r : visible_rows_)
                    {
                        if(r.reorderable && r.scene_zone_name == a.scene_zone_name
                           && r.transform_index >= 0)
                        {
                            order.push_back(r.transform_index);
                        }
                    }
                    const int ai = order.indexOf(a.transform_index);
                    const int bi = order.indexOf(b.transform_index);
                    if(ai >= 0 && bi >= 0)
                    {
                        order.move(ai, bi);
                        emit sceneZoneControllersReordered(a.scene_zone_name, order);
                    }
                }
            }
            update();
            return;
        }
        finishDrag();
    }
}

void EffectPackTimelineWidget::leaveEvent(QEvent* event)
{
    if(drag_op_ == DragOp::None)
    {
        unsetCursor();
    }
    QWidget::leaveEvent(event);
}

void EffectPackTimelineWidget::wheelEvent(QWheelEvent* event)
{
    if(event->modifiers() & Qt::ControlModifier)
    {
        const double factor = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
        setPixelsPerSecond(pixels_per_second_ * factor);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}
