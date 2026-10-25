// SPDX-License-Identifier: GPL-2.0-only

#include "ActionTimelineWidget.h"
#include "ActionCatalog.h"
#include "Actions/ActionApplier.h"
#include "ZoneManager3D.h"

#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
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

bool TargetsEqual(const Action::Target& a, const Action::Target& b)
{
    return Action::TargetEquals(a, b);
}

} // namespace

ActionTimelineWidget::ActionTimelineWidget(QWidget* parent)
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

void ActionTimelineWidget::setEffectFilesDir(const filesystem::path& dir)
{
    effect_files_dir_ = dir;
}

void ActionTimelineWidget::clearGpuThumbCache()
{
    gpu_thumb_cache_.clear();
    gpu_thumb_queue_.clear();
    gpu_thumb_queued_keys_.clear();
    gpu_thumb_pump_scheduled_ = false;
}

void ActionTimelineWidget::setPack(Action::Pack* pack)
{
    pack_ = pack;
    clearGpuThumbCache();
    requestContentRepaint();
}

void ActionTimelineWidget::setControllerTransforms(std::vector<std::unique_ptr<ControllerTransform>>* transforms)
{
    transforms_ = transforms;
    clearGpuThumbCache();
    requestContentRepaint();
}

void ActionTimelineWidget::setZoneManager(ZoneManager3D* zone_manager)
{
    zone_manager_ = zone_manager;
    clearGpuThumbCache();
    requestContentRepaint();
}

void ActionTimelineWidget::setRoomWorldBounds(const Action::ApplySpatialContext& room)
{
    room_bounds_ = room;
    clearGpuThumbCache();
    requestContentRepaint();
}

QString ActionTimelineWidget::targetKey(const Action::Target& t) const
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

void ActionTimelineWidget::captureExpandState(QSet<QString>* expanded_keys) const
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

void ActionTimelineWidget::restoreExpandState(const QSet<QString>& expanded_keys)
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

void ActionTimelineWidget::setModel(QVector<Node> roots)
{
    QSet<QString> expanded;
    captureExpandState(&expanded);
    roots_ = std::move(roots);
    restoreExpandState(expanded);
    clearGpuThumbCache();
    rebuildVisibleRows();
}

void ActionTimelineWidget::flattenNode(const Node& node, const QVector<int>& path, int depth,
                                           int led_index, int led_count)
{
    Row row;
    row.label = node.label;
    row.target = node.target;
    row.depth = depth;
    row.path = path;
    row.expandable = !node.children.isEmpty();
    row.expanded = node.expanded;
    row.single_led_row = (node.target.kind == Action::TargetKind::Leds);
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
            && node.children.front().target.kind == Action::TargetKind::Leds;
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

void ActionTimelineWidget::rebuildVisibleRows()
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
    requestContentRepaint();
}

ActionTimelineWidget::Node* ActionTimelineWidget::nodeAtPath(const QVector<int>& path)
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

const ActionTimelineWidget::Node* ActionTimelineWidget::nodeAtPath(const QVector<int>& path) const
{
    return const_cast<ActionTimelineWidget*>(this)->nodeAtPath(path);
}

void ActionTimelineWidget::toggleExpand(const QVector<int>& path)
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

void ActionTimelineWidget::setDurationMs(int duration_ms)
{
    duration_ms_ = std::clamp(duration_ms, 100, Action::kMaxDurationMs);
    updateGeometrySize();
    requestContentRepaint();
}

void ActionTimelineWidget::requestContentRepaint()
{
    content_dirty_ = true;
    update();
}

void ActionTimelineWidget::setPlayheadMs(int ms)
{
    const int old_ms = playhead_ms_;
    playhead_ms_ = std::clamp(ms, 0, duration_ms_);
    if(playhead_ms_ == old_ms)
    {
        return;
    }
    // Playhead motion must not rebuild clip rasters — invalidate only the two strips.
    constexpr int kPad = 3;
    const int old_x = timeToX(old_ms);
    const int new_x = timeToX(playhead_ms_);
    update(QRect(old_x - kPad, 0, kPad * 2 + 1, height()));
    update(QRect(new_x - kPad, 0, kPad * 2 + 1, height()));
}

void ActionTimelineWidget::setPixelsPerSecond(double pps)
{
    pixels_per_second_ = std::clamp(pps, 20.0, 400.0);
    updateGeometrySize();
    requestContentRepaint();
}

void ActionTimelineWidget::setSelectedClip(int track_index, int clip_index)
{
    setPrimarySelection(track_index, clip_index);
}

void ActionTimelineWidget::cancelDrag()
{
    row_reorder_from_ = -1;
    row_reorder_hover_ = -1;
    if(drag_op_ == DragOp::None)
    {
        requestContentRepaint();
        return;
    }
    drag_op_ = DragOp::None;
    drag_track_ = -1;
    drag_clip_ = -1;
    drag_moved_ = false;
    drag_copy_ = false;
    drag_copy_source_track_ = -1;
    drag_dest_row_ = -1;
    drag_home_row_ = -1;
    drag_float_y_ = 0;
    drag_grab_offset_y_ = 0;
    drag_span_anchor_row_ = -1;
    drag_span_hover_row_ = -1;
    drag_group_.clear();
    marquee_origin_ = QPoint();
    marquee_current_ = QPoint();
    releaseMouse();
    unsetCursor();
    requestContentRepaint();
}

int ActionTimelineWidget::trackIndexForRow(int row) const
{
    if(!pack_ || row < 0 || row >= visible_rows_.size())
    {
        return -1;
    }
    const Action::Target& target = visible_rows_[row].target;
    for(int i = 0; i < (int)pack_->tracks.size(); ++i)
    {
        if(TargetsEqual(pack_->tracks[(size_t)i].target, target))
        {
            return i;
        }
    }
    return -1;
}

QVector<Action::Target> ActionTimelineWidget::subtreeTargetsForRow(int row) const
{
    QVector<Action::Target> out;
    if(row < 0 || row >= visible_rows_.size())
    {
        return out;
    }
    const Node* root = nodeAtPath(visible_rows_[row].path);
    if(!root)
    {
        return out;
    }
    std::function<void(const Node&)> walk = [&](const Node& node) {
        out.push_back(node.target);
        for(const Node& child : node.children)
        {
            walk(child);
        }
    };
    walk(*root);
    return out;
}

QVector<ActionTimelineWidget::PaintClip> ActionTimelineWidget::paintClipsForRow(int row) const
{
    QVector<PaintClip> out;
    if(!pack_ || row < 0 || row >= visible_rows_.size())
    {
        return out;
    }
    const Row& r = visible_rows_[row];
    for(int ti = 0; ti < (int)pack_->tracks.size(); ++ti)
    {
        const Action::Track& track = pack_->tracks[(size_t)ti];
        if(!trackCoversRow(track, r))
        {
            continue;
        }
        for(int bi = 0; bi < (int)track.clips.size(); ++bi)
        {
            PaintClip pb;
            pb.clip = &track.clips[(size_t)bi];
            pb.track = ti;
            pb.clip_index = bi;
            pb.single_led_row = r.single_led_row;
            pb.led_index = r.led_index;
            pb.led_count = std::max(1, r.led_count);
            pb.view_target = track.target;
            out.push_back(pb);
        }
    }
    return out;
}

int ActionTimelineWidget::timeToX(int ms) const
{
    return gutter_width_ + (int)std::lround((ms / 1000.0) * pixels_per_second_);
}

int ActionTimelineWidget::xToTime(int x) const
{
    const int local = std::max(0, x - gutter_width_);
    const int ms = (int)std::lround((local / pixels_per_second_) * 1000.0);
    return std::clamp(ms, 0, duration_ms_);
}

int ActionTimelineWidget::snapMs(int ms) const
{
    if(snap_ms_ <= 1)
    {
        return std::clamp(ms, 0, duration_ms_);
    }
    const int snapped = ((ms + snap_ms_ / 2) / snap_ms_) * snap_ms_;
    return std::clamp(snapped, 0, duration_ms_);
}

int ActionTimelineWidget::contentWidth() const
{
    return gutter_width_ + std::max(200, (int)std::lround((duration_ms_ / 1000.0) * pixels_per_second_) + 40);
}

int ActionTimelineWidget::contentHeight() const
{
    return header_height_ + visible_rows_.size() * row_height_ + 8;
}

void ActionTimelineWidget::updateGeometrySize()
{
    setMinimumSize(contentWidth(), contentHeight());
    setMinimumHeight(contentHeight());
    updateGeometry();
}

QSize ActionTimelineWidget::sizeHint() const
{
    return QSize(contentWidth(), contentHeight());
}

QSize ActionTimelineWidget::minimumSizeHint() const
{
    return QSize(480, std::max(120, contentHeight()));
}

QRect ActionTimelineWidget::clipRect(int row, const Action::Clip& clip) const
{
    const int y = header_height_ + row * row_height_;
    const int x0 = timeToX(clip.start_ms);
    const int x1 = std::max(x0 + 4, timeToX(clip.end_ms));
    return QRect(x0, y + 3, x1 - x0, row_height_ - 6);
}

Action::Clip* ActionTimelineWidget::mutableClip(int track, int clip)
{
    if(!pack_ || track < 0 || track >= (int)pack_->tracks.size())
    {
        return nullptr;
    }
    auto& clips = pack_->tracks[(size_t)track].clips;
    if(clip < 0 || clip >= (int)clips.size())
    {
        return nullptr;
    }
    return &clips[(size_t)clip];
}

bool ActionTimelineWidget::hitTestClip(int x, int y, int* out_row, int* out_track, int* out_clip,
                                            ClipHit* out_hit) const
{
    if(out_hit)
    {
        *out_hit = ClipHit::None;
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
    const QVector<PaintClip> clips = paintClipsForRow(row);
    for(int i = clips.size() - 1; i >= 0; --i)
    {
        const PaintClip& pb = clips[i];
        if(!pb.clip)
        {
            continue;
        }
        const QRect br = clipRect(row, *pb.clip);
        const QRect hit = br.adjusted(0, -2, 0, 2);
        if(!hit.contains(x, y))
        {
            continue;
        }
        if(out_row) *out_row = row;
        if(out_track) *out_track = pb.track;
        if(out_clip) *out_clip = pb.clip_index;
        if(out_hit)
        {
            const int edge_x = std::min(edge_hit_px_, std::max(3, br.width() / 3));
            const int edge_y = std::min(6, std::max(3, br.height() / 3));
            // Prefer vertical edges when near top/bottom centre (away from L/R time handles).
            const bool near_top = y <= br.top() + edge_y;
            const bool near_bot = y >= br.bottom() - edge_y;
            const bool near_left = x <= br.left() + edge_x;
            const bool near_right = x >= br.right() - edge_x;
            if(near_top && !near_left && !near_right)
            {
                *out_hit = ClipHit::TopEdge;
            }
            else if(near_bot && !near_left && !near_right)
            {
                *out_hit = ClipHit::BottomEdge;
            }
            else if(near_left)
            {
                *out_hit = ClipHit::LeftEdge;
            }
            else if(near_right)
            {
                *out_hit = ClipHit::RightEdge;
            }
            else
            {
                *out_hit = ClipHit::Body;
            }
        }
        return true;
    }
    return false;
}

