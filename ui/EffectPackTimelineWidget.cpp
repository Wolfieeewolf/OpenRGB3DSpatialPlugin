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

EffectPackTimelineWidget::EffectPackTimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
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

void EffectPackTimelineWidget::setControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    transforms_ = transforms;
    update();
}

void EffectPackTimelineWidget::setZoneManager(ZoneManager3D* zone_manager)
{
    zone_manager_ = zone_manager;
    update();
}

QString EffectPackTimelineWidget::targetKey(const EffectPack::Target& t) const
{
    QString key = QString::number((int)t.kind) + QLatin1Char('|')
        + QString::fromStdString(t.device_name) + QLatin1Char('|')
        + QString::fromStdString(t.zone_name) + QLatin1Char('|')
        + QString::fromStdString(t.scene_zone_name) + QLatin1Char('|')
        + (t.flatten_leds ? QLatin1Char('1') : QLatin1Char('0'));
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

void EffectPackTimelineWidget::flattenNode(const Node& node, const QVector<int>& path, int depth,
                                           int led_index, int led_count)
{
    Row row;
    row.label = node.label;
    row.target = node.target;
    row.depth = depth;
    row.path = path;
    row.expandable = !node.children.isEmpty();
    row.expanded = node.expanded;
    row.single_led_row = (node.target.kind == EffectPack::TargetKind::Leds);
    row.transform_index = node.transform_index;
    row.scene_zone_name = node.scene_zone_name;
    row.reorderable = node.reorderable;
    if(row.single_led_row)
    {
        row.led_index = led_index;
        row.led_count = std::max(1, led_count);
    }
    else
    {
        row.led_index = 0;
        row.led_count = std::max(1, node.led_count);
    }
    visible_rows_.push_back(row);

    if(node.expanded)
    {
        const bool children_are_leds = !node.children.isEmpty()
            && node.children.front().target.kind == EffectPack::TargetKind::Leds;
        const int child_led_count = children_are_leds
            ? std::max(node.led_count, (int)node.children.size())
            : 1;
        for(int i = 0; i < node.children.size(); ++i)
        {
            QVector<int> child_path = path;
            child_path.push_back(i);
            const int child_led_index = children_are_leds ? i : 0;
            const int child_count = children_are_leds ? child_led_count : 1;
            flattenNode(node.children[i], child_path, depth + 1, child_led_index, child_count);
        }
    }
}

void EffectPackTimelineWidget::rebuildVisibleRows()
{
    visible_rows_.clear();
    for(int i = 0; i < roots_.size(); ++i)
    {
        flattenNode(roots_[i], QVector<int>{i}, 0, 0, 1);
    }
    if(selected_row_ >= visible_rows_.size())
    {
        selected_row_ = visible_rows_.isEmpty() ? -1 : visible_rows_.size() - 1;
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

void EffectPackTimelineWidget::cancelDrag()
{
    row_reorder_from_ = -1;
    row_reorder_hover_ = -1;
    if(drag_op_ == DragOp::None)
    {
        update();
        return;
    }
    drag_op_ = DragOp::None;
    drag_track_ = -1;
    drag_block_ = -1;
    drag_moved_ = false;
    unsetCursor();
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

QVector<EffectPackTimelineWidget::PaintBlock> EffectPackTimelineWidget::paintBlocksForRow(int row) const
{
    QVector<PaintBlock> out;
    if(!pack_ || row < 0 || row >= visible_rows_.size())
    {
        return out;
    }
    const Row& r = visible_rows_[row];
    // Exact target only — no ghosting onto child zone/LED rows.
    for(int ti = 0; ti < (int)pack_->tracks.size(); ++ti)
    {
        const EffectPack::Track& track = pack_->tracks[(size_t)ti];
        if(!TargetsEqual(track.target, r.target))
        {
            continue;
        }
        for(int bi = 0; bi < (int)track.blocks.size(); ++bi)
        {
            PaintBlock pb;
            pb.block = &track.blocks[(size_t)bi];
            pb.track = ti;
            pb.block_index = bi;
            pb.single_led_row = r.single_led_row;
            pb.led_index = r.led_index;
            pb.led_count = std::max(1, r.led_count);
            pb.view_target = track.target;
            out.push_back(pb);
        }
    }
    return out;
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

bool EffectPackTimelineWidget::hitTestBlock(int x, int y, int* out_row, int* out_track, int* out_block,
                                            BlockHit* out_hit) const
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
    const QVector<PaintBlock> blocks = paintBlocksForRow(row);
    for(int i = blocks.size() - 1; i >= 0; --i)
    {
        const PaintBlock& pb = blocks[i];
        if(!pb.block)
        {
            continue;
        }
        const QRect br = blockRect(row, *pb.block);
        const QRect hit = br.adjusted(0, -2, 0, 2);
        if(!hit.contains(x, y))
        {
            continue;
        }
        if(out_row) *out_row = row;
        if(out_track) *out_track = pb.track;
        if(out_block) *out_block = pb.block_index;
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

