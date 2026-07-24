// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackTimelineWidget.h"

#include <QColorDialog>
#include <QCursor>
#include <QEvent>
#include <QMenu>
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
    if(a.kind != b.kind)
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
            return a.device_name == b.device_name && a.led_indices == b.led_indices;
    }
    return false;
}

QColor BlockFillColor(const EffectPack::Block& block)
{
    RGBColor c = block.color;
    if(block.type == EffectPack::BlockType::Fade)
    {
        c = block.color_from;
    }
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor QColorToRgb(const QColor& c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

} // namespace

EffectPackTimelineWidget::EffectPackTimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(32, 32, 36));
    setPalette(pal);
    updateGeometrySize();
}

void EffectPackTimelineWidget::setPack(EffectPack::Pack* pack)
{
    pack_ = pack;
    update();
}

QString EffectPackTimelineWidget::targetKey(const EffectPack::Target& t) const
{
    QString key = QString::number((int)t.kind) + QLatin1Char('|')
        + QString::fromStdString(t.device_name) + QLatin1Char('|')
        + QString::fromStdString(t.zone_name);
    for(int led : t.led_indices)
    {
        key += QLatin1Char(',') + QString::number(led);
    }
    return key;
}

void EffectPackTimelineWidget::captureExpandState(QSet<QString>* expanded_keys) const
{
    if(!expanded_keys)
    {
        return;
    }
    std::function<void(const Node&)> walk = [&](const Node& n) {
        if(n.expanded && !n.children.isEmpty())
        {
            expanded_keys->insert(targetKey(n.target));
        }
        for(const Node& c : n.children)
        {
            walk(c);
        }
    };
    for(const Node& r : roots_)
    {
        walk(r);
    }
}

void EffectPackTimelineWidget::restoreExpandState(const QSet<QString>& expanded_keys)
{
    std::function<void(Node&)> walk = [&](Node& n) {
        if(!n.children.isEmpty() && expanded_keys.contains(targetKey(n.target)))
        {
            n.expanded = true;
        }
        for(Node& c : n.children)
        {
            walk(c);
        }
    };
    for(Node& r : roots_)
    {
        walk(r);
    }
}

void EffectPackTimelineWidget::setModel(QVector<Node> roots)
{
    QSet<QString> expanded;
    captureExpandState(&expanded);
    roots_ = std::move(roots);
    restoreExpandState(expanded);
    rebuildVisibleRows();
}

void EffectPackTimelineWidget::flattenNode(const Node& node, const QVector<int>& path, int depth)
{
    Row row;
    row.label = node.label;
    row.target = node.target;
    row.depth = depth;
    row.path = path;
    row.expandable = !node.children.isEmpty();
    row.expanded = node.expanded;
    visible_rows_.push_back(row);

    if(node.expanded)
    {
        for(int i = 0; i < node.children.size(); ++i)
        {
            QVector<int> child_path = path;
            child_path.push_back(i);
            flattenNode(node.children[i], child_path, depth + 1);
        }
    }
}

void EffectPackTimelineWidget::rebuildVisibleRows()
{
    visible_rows_.clear();
    for(int i = 0; i < roots_.size(); ++i)
    {
        flattenNode(roots_[i], QVector<int>{i}, 0);
    }
    updateGeometrySize();
    emit contentHeightChanged(contentHeight());
    update();
}

EffectPackTimelineWidget::Node* EffectPackTimelineWidget::nodeAtPath(const QVector<int>& path)
{
    if(path.isEmpty() || path[0] < 0 || path[0] >= roots_.size())
    {
        return nullptr;
    }
    Node* n = &roots_[path[0]];
    for(int i = 1; i < path.size(); ++i)
    {
        if(path[i] < 0 || path[i] >= n->children.size())
        {
            return nullptr;
        }
        n = &n->children[path[i]];
    }
    return n;
}

const EffectPackTimelineWidget::Node* EffectPackTimelineWidget::nodeAtPath(const QVector<int>& path) const
{
    return const_cast<EffectPackTimelineWidget*>(this)->nodeAtPath(path);
}

void EffectPackTimelineWidget::toggleExpand(const QVector<int>& path)
{
    Node* n = nodeAtPath(path);
    if(!n || n->children.isEmpty())
    {
        return;
    }
    n->expanded = !n->expanded;
    rebuildVisibleRows();
    emit modelExpandedChanged();
}

void EffectPackTimelineWidget::setDurationMs(int duration_ms)
{
    duration_ms_ = std::clamp(duration_ms, 100, EffectPack::kMaxDurationMs);
    updateGeometrySize();
    update();
}

void EffectPackTimelineWidget::setPlayheadMs(int ms)
{
    playhead_ms_ = std::clamp(ms, 0, duration_ms_);
    update();
}

void EffectPackTimelineWidget::setPixelsPerSecond(double pps)
{
    pixels_per_second_ = std::clamp(pps, 20.0, 400.0);
    updateGeometrySize();
    update();
}

void EffectPackTimelineWidget::setSelectedBlock(int track_index, int block_index)
{
    selected_track_ = track_index;
    selected_block_ = block_index;
    update();
}

int EffectPackTimelineWidget::trackIndexForRow(int row) const
{
    if(!pack_ || row < 0 || row >= visible_rows_.size())
    {
        return -1;
    }
    const EffectPack::Target& target = visible_rows_[row].target;
    for(int i = 0; i < (int)pack_->tracks.size(); ++i)
    {
        if(TargetsEqual(pack_->tracks[(size_t)i].target, target))
        {
            return i;
        }
    }
    return -1;
}

int EffectPackTimelineWidget::timeToX(int ms) const
{
    return gutter_width_ + (int)std::lround((ms / 1000.0) * pixels_per_second_);
}

int EffectPackTimelineWidget::xToTime(int x) const
{
    const int local = std::max(0, x - gutter_width_);
    const int ms = (int)std::lround((local / pixels_per_second_) * 1000.0);
    return std::clamp(ms, 0, duration_ms_);
}

int EffectPackTimelineWidget::snapMs(int ms) const
{
    if(snap_ms_ <= 1)
    {
        return std::clamp(ms, 0, duration_ms_);
    }
    const int snapped = ((ms + snap_ms_ / 2) / snap_ms_) * snap_ms_;
    return std::clamp(snapped, 0, duration_ms_);
}

int EffectPackTimelineWidget::contentWidth() const
{
    return gutter_width_ + std::max(200, (int)std::lround((duration_ms_ / 1000.0) * pixels_per_second_) + 40);
}

int EffectPackTimelineWidget::contentHeight() const
{
    return header_height_ + visible_rows_.size() * row_height_ + 8;
}

void EffectPackTimelineWidget::updateGeometrySize()
{
    setMinimumSize(contentWidth(), contentHeight());
    setMinimumHeight(contentHeight());
    updateGeometry();
}

QSize EffectPackTimelineWidget::sizeHint() const
{
    return QSize(contentWidth(), contentHeight());
}

QSize EffectPackTimelineWidget::minimumSizeHint() const
{
    return QSize(480, std::max(120, contentHeight()));
}

QRect EffectPackTimelineWidget::blockRect(int row, const EffectPack::Block& block) const
{
    const int y = header_height_ + row * row_height_;
    const int x0 = timeToX(block.start_ms);
    const int x1 = std::max(x0 + 4, timeToX(block.end_ms));
    return QRect(x0, y + 3, x1 - x0, row_height_ - 6);
}

EffectPack::Block* EffectPackTimelineWidget::mutableBlock(int track, int block)
{
    if(!pack_ || track < 0 || track >= (int)pack_->tracks.size())
    {
        return nullptr;
    }
    auto& blocks = pack_->tracks[(size_t)track].blocks;
    if(block < 0 || block >= (int)blocks.size())
    {
        return nullptr;
    }
    return &blocks[(size_t)block];
}

bool EffectPackTimelineWidget::hitTestBlock(int x, int y, int* out_row, int* out_track, int* out_block, BlockHit* out_hit) const
{
    if(out_hit)
    {
        *out_hit = BlockHit::None;
    }
    if(y < header_height_ || x < gutter_width_ || !pack_)
    {
        return false;
    }
    const int row = (y - header_height_) / row_height_;
    if(row < 0 || row >= visible_rows_.size())
    {
        return false;
    }
    const int track = trackIndexForRow(row);
    if(track < 0 || track >= (int)pack_->tracks.size())
    {
        return false;
    }
    const auto& blocks = pack_->tracks[(size_t)track].blocks;
    for(int b = (int)blocks.size() - 1; b >= 0; --b)
    {
        const EffectPack::Block& block = blocks[(size_t)b];
        const QRect br = blockRect(row, block);
        // Slightly taller hit so edges are easy to grab.
        const QRect hit = br.adjusted(0, -2, 0, 2);
        if(!hit.contains(x, y))
        {
            continue;
        }
        if(out_row) *out_row = row;
        if(out_track) *out_track = track;
        if(out_block) *out_block = b;
        if(out_hit)
        {
            const int edge = std::min(edge_hit_px_, std::max(3, br.width() / 3));
            if(x <= br.left() + edge)
            {
                *out_hit = BlockHit::LeftEdge;
            }
            else if(x >= br.right() - edge)
            {
                *out_hit = BlockHit::RightEdge;
            }
            else
            {
                *out_hit = BlockHit::Body;
            }
        }
        return true;
    }
    return false;
}

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

void EffectPackTimelineWidget::editBlockColorAt(int track, int block, const QPoint& global_pos)
{
    EffectPack::Block* b = mutableBlock(track, block);
    if(!b)
    {
        return;
    }

    selected_track_ = track;
    selected_block_ = block;
    emit blockSelected(track, block);

    QMenu menu(this);
    QAction* color_act = menu.addAction(QStringLiteral("Change color…"));
    QAction* end_act = nullptr;
    if(b->type == EffectPack::BlockType::Fade)
    {
        end_act = menu.addAction(QStringLiteral("Change end color…"));
    }
    QAction* chosen = menu.exec(global_pos);
    if(!chosen)
    {
        return;
    }

    if(chosen == color_act)
    {
        const RGBColor current = (b->type == EffectPack::BlockType::Fade) ? b->color_from : b->color;
        const QColor picked = QColorDialog::getColor(RgbToQColor(current), this, QStringLiteral("Block color"));
        if(!picked.isValid())
        {
            return;
        }
        const RGBColor rgb = QColorToRgb(picked);
        b->color = rgb;
        b->color_from = rgb;
    }
    else if(chosen == end_act)
    {
        const QColor picked = QColorDialog::getColor(RgbToQColor(b->color_to), this, QStringLiteral("Fade end color"));
        if(!picked.isValid())
        {
            return;
        }
        b->color_to = QColorToRgb(picked);
    }
    else
    {
        return;
    }

    update();
    emit blockEdited(track, block);
}

void EffectPackTimelineWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(32, 32, 36));

    // Gutter + header
    p.fillRect(0, 0, gutter_width_, height(), QColor(40, 40, 44));
    p.fillRect(gutter_width_, 0, width() - gutter_width_, header_height_, QColor(45, 45, 50));
    p.setPen(QColor(70, 70, 78));
    p.drawLine(gutter_width_, 0, gutter_width_, height());

    p.setPen(QColor(160, 160, 170));
    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);
    for(int sec = 0; sec * 1000 <= duration_ms_; ++sec)
    {
        const int x = timeToX(sec * 1000);
        p.drawLine(x, header_height_ - 8, x, header_height_);
        p.drawText(x + 2, header_height_ - 10, QString::number(sec) + QStringLiteral("s"));
    }
    p.drawText(8, header_height_ - 8, QStringLiteral("Controllers"));

    for(int row = 0; row < visible_rows_.size(); ++row)
    {
        const Row& r = visible_rows_[row];
        const int y = header_height_ + row * row_height_;
        const QColor bg = (row % 2 == 0) ? QColor(38, 38, 42) : QColor(34, 34, 38);
        const QColor gutter_bg = (selected_row_ == row) ? QColor(55, 55, 70) : bg;
        p.fillRect(0, y, gutter_width_, row_height_, gutter_bg);
        p.fillRect(gutter_width_, y, width() - gutter_width_, row_height_, bg);
        p.setPen(QColor(55, 55, 60));
        p.drawLine(0, y + row_height_ - 1, width(), y + row_height_ - 1);

        if(r.depth == 0)
        {
            p.fillRect(0, y, 3, row_height_, QColor(90, 140, 220));
        }

        const int indent = 6 + r.depth * 14;
        if(r.expandable)
        {
            const QRect plus(indent, y + (row_height_ - expand_hit_) / 2, expand_hit_, expand_hit_);
            p.setPen(QColor(200, 200, 210));
            p.setBrush(QColor(55, 55, 62));
            p.drawRect(plus);
            p.drawText(plus, Qt::AlignCenter, r.expanded ? QStringLiteral("−") : QStringLiteral("+"));
        }

        p.setPen(r.depth == 0 ? QColor(230, 230, 235) : QColor(180, 180, 190));
        const int text_x = indent + (r.expandable ? expand_hit_ + 4 : 4);
        p.drawText(text_x, y, gutter_width_ - text_x - 4, row_height_,
                   Qt::AlignVCenter | Qt::AlignLeft, r.label);

        const int track = trackIndexForRow(row);
        if(pack_ && track >= 0 && track < (int)pack_->tracks.size())
        {
            const auto& blocks = pack_->tracks[(size_t)track].blocks;
            for(int b = 0; b < (int)blocks.size(); ++b)
            {
                const EffectPack::Block& block = blocks[(size_t)b];
                const QRect br = blockRect(row, block);
                QColor fill = BlockFillColor(block);
                fill.setAlpha(200);
                p.fillRect(br, fill);

                // Edge grips
                const int grip = std::min(4, std::max(2, br.width() / 8));
                p.fillRect(br.left(), br.top(), grip, br.height(), QColor(255, 255, 255, 50));
                p.fillRect(br.right() - grip + 1, br.top(), grip, br.height(), QColor(255, 255, 255, 50));

                const bool selected = (track == selected_track_ && b == selected_block_);
                p.setPen(selected ? QColor(255, 220, 80) : QColor(20, 20, 20));
                p.drawRect(br.adjusted(0, 0, -1, -1));
            }
        }
    }

    const int px = timeToX(playhead_ms_);
    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(px, 0, px, height());
}

void EffectPackTimelineWidget::mousePressEvent(QMouseEvent* event)
{
    const int x = event->position().toPoint().x();
    const int y = event->position().toPoint().y();

    if(event->button() == Qt::RightButton)
    {
        int hit_row = 0;
        int track = -1;
        int block = -1;
        if(hitTestBlock(x, y, &hit_row, &track, &block))
        {
            selected_row_ = hit_row;
            emit rowSelected(hit_row);
            editBlockColorAt(track, block, event->globalPosition().toPoint());
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

    const int row = (y - header_height_) / row_height_;
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
            const QRect label_hit(indent, row_y, gutter_width_ - indent, row_height_);
            if(plus.contains(x, y) || label_hit.contains(x, y))
            {
                toggleExpand(r.path);
                return;
            }
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

    selected_row_ = row;
    emit rowSelected(row);
    const int ms = xToTime(x);
    playhead_ms_ = ms;
    emit playheadChanged(ms);
    emit emptyCellClicked(row, ms);
    update();
}

void EffectPackTimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pt = event->position().toPoint();
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
