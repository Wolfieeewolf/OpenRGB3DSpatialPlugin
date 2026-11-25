// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionTimelineWidget.h"
#include "ActionDeviceView.h"
#include "ActionCatalog.h"
#include "ActionApplier.h"
#include "ActionScript.h"
#include "ZoneManager3D.h"

#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QGuiApplication>
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
    effect_names_.clear();
    effect_names_loaded_ = false;
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
    // Do not wipe GPU thumbs here — add/remove keep the same pack* and keys already
    // hash clip look; loadIntoUi clears when content is replaced wholesale.
    pack_ = pack;
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
    layoutRows();
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
    layoutRows();
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
    // Pad covers the ruler handle (10 px wide plus anti-aliasing), not just the 2 px line.
    constexpr int kPad = 8;
    const int old_x = timeToX(old_ms);
    const int new_x = timeToX(playhead_ms_);
    update(QRect(old_x - kPad, 0, kPad * 2 + 1, height()));
    update(QRect(new_x - kPad, 0, kPad * 2 + 1, height()));
}

void ActionTimelineWidget::setPixelsPerSecond(double pps)
{
    const double clamped = std::clamp(pps, 20.0, 400.0);
    if(clamped == pixels_per_second_)
    {
        return;
    }
    pixels_per_second_ = clamped;
    updateGeometrySize();
    requestContentRepaint();
    emit zoomChanged(pixels_per_second_);
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
    snap_guide_ms_ = -1;
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

bool ActionTimelineWidget::snapBypassed() const
{
    return !snap_enabled_ || (QGuiApplication::keyboardModifiers() & Qt::AltModifier);
}

QVector<int> ActionTimelineWidget::snapCandidates(bool include_playhead) const
{
    QVector<int> out{0, duration_ms_};
    if(include_playhead)
    {
        out.push_back(playhead_ms_);
    }
    if(!pack_)
    {
        return out;
    }
    for(const Action::Marker& m : pack_->markers)
    {
        out.push_back(m.ms);
    }
    auto dragged = [&](int track, int clip) {
        if(track == drag_track_ && clip == drag_clip_)
        {
            return true;
        }
        for(const DragMember& m : drag_group_)
        {
            if(m.track == track && m.clip == clip)
            {
                return true;
            }
        }
        return false;
    };
    for(int ti = 0; ti < (int)pack_->tracks.size(); ++ti)
    {
        const Action::Track& track = pack_->tracks[(size_t)ti];
        for(int bi = 0; bi < (int)track.clips.size(); ++bi)
        {
            if(dragged(ti, bi))
            {
                continue;
            }
            out.push_back(track.clips[(size_t)bi].start_ms);
            out.push_back(track.clips[(size_t)bi].end_ms);
        }
    }
    return out;
}

int ActionTimelineWidget::snapEdgeMs(int ms, bool include_playhead, int* out_guide_ms) const
{
    if(out_guide_ms)
    {
        *out_guide_ms = -1;
    }
    if(snapBypassed())
    {
        return snapMs(ms);
    }
    const int reach = std::max(1, (int)std::lround(snap_reach_px_ * 1000.0 / pixels_per_second_));
    int best = -1;
    int best_dist = reach + 1;
    for(int c : snapCandidates(include_playhead))
    {
        const int d = std::abs(c - ms);
        if(d < best_dist)
        {
            best_dist = d;
            best = c;
        }
    }
    if(best >= 0)
    {
        if(out_guide_ms)
        {
            *out_guide_ms = best;
        }
        return std::clamp(best, 0, duration_ms_);
    }
    return snapMs(ms);
}

int ActionTimelineWidget::snapGroupDelta(int raw_delta, int* out_guide_ms) const
{
    if(out_guide_ms)
    {
        *out_guide_ms = -1;
    }
    const int grid_delta = snapMs(std::max(0, drag_origin_start_ + raw_delta)) - drag_origin_start_;
    if(snapBypassed())
    {
        return grid_delta;
    }
    const int reach = std::max(1, (int)std::lround(snap_reach_px_ * 1000.0 / pixels_per_second_));
    const QVector<int> candidates = snapCandidates(true);
    int best_adjust = 0;
    int best_dist = reach + 1;
    int best_c = -1;
    auto consider = [&](int edge) {
        for(int c : candidates)
        {
            const int d = c - edge;
            if(std::abs(d) < best_dist)
            {
                best_dist = std::abs(d);
                best_adjust = d;
                best_c = c;
            }
        }
    };
    if(drag_group_.isEmpty())
    {
        consider(drag_origin_start_ + raw_delta);
        consider(drag_origin_end_ + raw_delta);
    }
    for(const DragMember& m : drag_group_)
    {
        consider(m.origin_start + raw_delta);
        consider(m.origin_end + raw_delta);
    }
    if(best_c < 0)
    {
        return grid_delta;
    }
    if(out_guide_ms)
    {
        *out_guide_ms = best_c;
    }
    return raw_delta + best_adjust;
}

void ActionTimelineWidget::addMarkerAt(int ms)
{
    if(!pack_ || pack_->markers.size() >= Action::kMaxMarkers)
    {
        return;
    }
    ms = std::clamp(ms, 0, duration_ms_);
    for(const Action::Marker& m : pack_->markers)
    {
        if(std::abs(m.ms - ms) < 5)
        {
            return; // one there already
        }
    }
    Action::Marker marker;
    marker.ms = ms;
    auto it = std::upper_bound(pack_->markers.begin(), pack_->markers.end(), marker,
                               [](const Action::Marker& a, const Action::Marker& b) { return a.ms < b.ms; });
    pack_->markers.insert(it, marker);
    requestContentRepaint();
    emit markersChanged();
}

int ActionTimelineWidget::previousMarkerMs(int ms) const
{
    int best = -1;
    if(pack_)
    {
        for(const Action::Marker& m : pack_->markers)
        {
            if(m.ms < ms - 1)
            {
                best = m.ms;
            }
        }
    }
    return best;
}

int ActionTimelineWidget::nextMarkerMs(int ms) const
{
    if(pack_)
    {
        for(const Action::Marker& m : pack_->markers)
        {
            if(m.ms > ms + 1)
            {
                return m.ms;
            }
        }
    }
    return -1;
}

int ActionTimelineWidget::markerNear(int x) const
{
    if(!pack_)
    {
        return -1;
    }
    int best = -1;
    int best_dist = 7;
    for(int i = 0; i < (int)pack_->markers.size(); ++i)
    {
        const int d = std::abs(timeToX(pack_->markers[(size_t)i].ms) - x);
        if(d < best_dist)
        {
            best_dist = d;
            best = i;
        }
    }
    return best;
}

void ActionTimelineWidget::fitClipBetweenMarkers(int track, int clip)
{
    Action::Clip* b = mutableClip(track, clip);
    if(!b)
    {
        return;
    }
    const int middle = (b->start_ms + b->end_ms) / 2;
    int lo = 0;
    int hi = duration_ms_;
    for(const Action::Marker& m : pack_->markers)
    {
        if(m.ms <= middle)
        {
            lo = m.ms;
        }
        else
        {
            hi = m.ms;
            break;
        }
    }
    if(hi - lo < min_clip_ms_)
    {
        return;
    }
    b->start_ms = lo;
    b->end_ms = hi;
    requestContentRepaint();
    emit clipEdited(track, clip);
}

QString ActionTimelineWidget::effectDisplayName(const Action::Clip& clip) const
{
    if(!effect_names_loaded_)
    {
        for(const ActionCatalog::Entry& e : ActionCatalog::LoadEntries(effect_files_dir_))
        {
            effect_names_.insert(e.id.toLower(), e.name.isEmpty() ? e.id : e.name);
        }
        effect_names_loaded_ = true;
    }
    const QString id = QString::fromStdString(Action::ClipFileId(clip));
    return effect_names_.value(id.toLower(), id);
}

int ActionTimelineWidget::rulerStepMs() const
{
    const double raw = 80.0 * 1000.0 / std::max(1.0, pixels_per_second_);
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    const double norm = raw / magnitude;
    const double mult = norm >= 5.0 ? 10.0 : (norm >= 2.0 ? 5.0 : (norm >= 1.0 ? 2.0 : 1.0));
    return std::max(10, (int)std::lround(mult * magnitude));
}

int ActionTimelineWidget::contentWidth() const
{
    return gutter_width_ + std::max(200, (int)std::lround((duration_ms_ / 1000.0) * pixels_per_second_) + 40);
}

int ActionTimelineWidget::contentHeight() const
{
    return rowTop(visible_rows_.size()) + 8;
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
    const int y = rowTop(row);
    const int x0 = timeToX(clip.start_ms);
    const int x1 = std::max(x0 + 4, timeToX(clip.end_ms));
    return QRect(x0, y + 3, x1 - x0, rowHeightAt(row) - 6);
}

QRect ActionTimelineWidget::LaneBand(const QRect& full, int lanes, int lane)
{
    if(lanes <= 1)
    {
        return full;
    }
    lane = std::clamp(lane, 0, lanes - 1);
    const int y0 = full.top() + (full.height() * lane) / lanes;
    const int y1 = full.top() + (full.height() * (lane + 1)) / lanes - (lane < lanes - 1 ? 1 : 0);
    return QRect(full.left(), y0, full.width(), std::max(2, y1 - y0));
}

QRect ActionTimelineWidget::clipRectOnTrack(int row, int track, const Action::Clip& clip) const
{
    return LaneBand(clipRect(row, clip), laneCountForTrack(track), clip.layer);
}

void ActionTimelineWidget::layoutRows()
{
    const int old_total = row_tops_.isEmpty() ? -1 : row_tops_.back();
    const int n = visible_rows_.size();
    row_tops_.resize(n + 1);
    int y = 0;
    for(int row = 0; row < n; ++row)
    {
        row_tops_[row] = y;
        int h = row_height_;
        const int t = trackIndexForRow(row);
        const int lanes = laneCountForTrack(t);
        if(t >= 0 && lanes > 1 && pack_->tracks[(size_t)t].layers_open)
        {
            h = 6 + lanes * open_lane_height_;
        }
        y += h;
    }
    row_tops_[n] = y;
    if(y != old_total)
    {
        updateGeometrySize();
        emit contentHeightChanged(contentHeight());
    }
}

int ActionTimelineWidget::rowTop(int row) const
{
    const int n = visible_rows_.size();
    if(row_tops_.size() == n + 1 && row >= 0 && row <= n)
    {
        return header_height_ + row_tops_[row];
    }
    return header_height_ + row * row_height_;
}

int ActionTimelineWidget::rowHeightAt(int row) const
{
    const int n = visible_rows_.size();
    if(row_tops_.size() == n + 1 && row >= 0 && row < n)
    {
        return row_tops_[row + 1] - row_tops_[row];
    }
    return row_height_;
}

int ActionTimelineWidget::rowAtY(int y) const
{
    if(y < header_height_)
    {
        return -1;
    }
    const int rel = y - header_height_;
    const int n = visible_rows_.size();
    if(row_tops_.size() != n + 1)
    {
        return rel / row_height_;
    }
    if(rel >= row_tops_[n])
    {
        return n;
    }
    const auto it = std::upper_bound(row_tops_.begin(), row_tops_.end(), rel);
    return std::max(0, (int)(it - row_tops_.begin()) - 1);
}

int ActionTimelineWidget::laneCountForTrack(int track) const
{
    if(!pack_ || track < 0 || track >= (int)pack_->tracks.size())
    {
        return 1;
    }
    return Action::TrackLayerCount(pack_->tracks[(size_t)track]);
}

int ActionTimelineWidget::laneAtY(int row, int y) const
{
    if(row < 0 || row >= visible_rows_.size())
    {
        return 0;
    }
    const int lanes = laneCountForTrack(trackIndexForRow(row));
    if(lanes <= 1)
    {
        return 0;
    }
    const int top = rowTop(row) + 3;
    const int h = std::max(1, rowHeightAt(row) - 6);
    return std::clamp(((y - top) * lanes) / h, 0, lanes - 1);
}

QRect ActionTimelineWidget::laneFoldRect(int row) const
{
    if(row < 0 || row >= visible_rows_.size())
    {
        return {};
    }
    const int t = trackIndexForRow(row);
    if(laneCountForTrack(t) <= 1)
    {
        return {};
    }
    const int y = rowTop(row);
    if(pack_->tracks[(size_t)t].layers_open)
    {
        const Row& r = visible_rows_[row];
        const int indent = 6 + r.depth * 14;
        const int text_x = indent + (r.expandable ? expand_hit_ + 4 : 4);
        return QRect(text_x, y + row_height_ - 4, 40, 16);
    }
    return QRect(gutter_width_ - 58, y + (row_height_ - 16) / 2, 54, 16);
}

QRect ActionTimelineWidget::laneNameRect(int row, int lane) const
{
    const int lanes = laneCountForTrack(trackIndexForRow(row));
    const QRect band = LaneBand(QRect(0, rowTop(row) + 3, gutter_width_, rowHeightAt(row) - 6), lanes, lane);
    return QRect(gutter_width_ - 100, band.top(), 56, band.height());
}

QRect ActionTimelineWidget::laneMuteRect(int row, int lane) const
{
    const QRect name = laneNameRect(row, lane);
    return QRect(gutter_width_ - 41, name.center().y() - 7, 17, 14);
}

QRect ActionTimelineWidget::laneSoloRect(int row, int lane) const
{
    const QRect name = laneNameRect(row, lane);
    return QRect(gutter_width_ - 21, name.center().y() - 7, 17, 14);
}

bool ActionTimelineWidget::rowHasDeviceView(int row) const
{
    if(!transforms_ || row < 0 || row >= visible_rows_.size())
    {
        return false;
    }
    const Row& r = visible_rows_[row];
    if(r.single_led_row)
    {
        return false;
    }
    const Action::TargetKind k = r.target.kind;
    return k == Action::TargetKind::Device || k == Action::TargetKind::Zone || k == Action::TargetKind::SceneZone;
}

QRect ActionTimelineWidget::eyeRect(int row) const
{
    if(!rowHasDeviceView(row))
    {
        return {};
    }
    const int t = trackIndexForRow(row);
    const int lanes = laneCountForTrack(t);
    int right = gutter_width_ - 4;
    if(lanes > 1)
    {
        right = pack_->tracks[(size_t)t].layers_open ? gutter_width_ - 104 : gutter_width_ - 62;
    }
    return QRect(right - 20, rowTop(row) + (row_height_ - 14) / 2, 20, 14);
}

void ActionTimelineWidget::openDeviceView(int row)
{
    if(!rowHasDeviceView(row))
    {
        return;
    }
    const Row& r = visible_rows_[row];
    const QString key = targetKey(r.target);
    QPointer<QWidget> existing = device_views_.value(key);
    if(existing)
    {
        existing->showNormal();
        existing->raise();
        existing->activateWindow();
        return;
    }
    auto* view = new ActionDeviceViewWindow(r.target, r.label, transforms_, zone_manager_, window());
    device_views_.insert(key, view);
    view->show();
}

bool ActionTimelineWidget::handleLaneGutterClick(int row, int x, int y)
{
    const int t = trackIndexForRow(row);
    const int lanes = laneCountForTrack(t);
    if(t < 0 || lanes <= 1)
    {
        return false;
    }
    Action::Track& track = pack_->tracks[(size_t)t];
    if(laneFoldRect(row).contains(x, y))
    {
        track.layers_open = !track.layers_open;
        requestContentRepaint();
        emit layersChanged();
        return true;
    }
    if(!track.layers_open)
    {
        return false;
    }
    for(int lane = 0; lane < lanes; ++lane)
    {
        const bool mute = laneMuteRect(row, lane).contains(x, y);
        const bool solo = laneSoloRect(row, lane).contains(x, y);
        if(!mute && !solo)
        {
            continue;
        }
        if((int)track.layers.size() < lanes)
        {
            track.layers.resize((size_t)lanes);
        }
        Action::TrackLayer& layer = track.layers[(size_t)lane];
        if(mute)
        {
            layer.muted = !layer.muted;
        }
        else
        {
            layer.solo = !layer.solo;
        }
        requestContentRepaint();
        emit layersChanged();
        return true;
    }
    return false;
}

void ActionTimelineWidget::compactTrackLayers(int track_index)
{
    if(!pack_ || track_index < 0 || track_index >= (int)pack_->tracks.size())
    {
        return;
    }
    Action::Track& track = pack_->tracks[(size_t)track_index];
    const int lanes = Action::TrackLayerCount(track);
    std::vector<int> remap((size_t)lanes, -1);
    for(const Action::Clip& c : track.clips)
    {
        remap[(size_t)std::clamp(c.layer, 0, lanes - 1)] = 0;
    }
    std::vector<Action::TrackLayer> kept;
    int next = 0;
    for(int lane = 0; lane < lanes; ++lane)
    {
        if(remap[(size_t)lane] < 0)
        {
            continue;
        }
        remap[(size_t)lane] = next++;
        kept.push_back(lane < (int)track.layers.size() ? track.layers[(size_t)lane] : Action::TrackLayer{});
    }
    for(Action::Clip& c : track.clips)
    {
        c.layer = std::max(0, remap[(size_t)std::clamp(c.layer, 0, lanes - 1)]);
    }
    track.layers = kept.size() > 1 ? kept : std::vector<Action::TrackLayer>{};
    if(Action::TrackLayerCount(track) <= 1)
    {
        track.layers_open = false;
    }
    requestContentRepaint();
    emit layersChanged();
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
    const int row = rowAtY(y);
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
        const QRect br = clipRectOnTrack(row, pb.track, *pb.clip);
        const int lanes = laneCountForTrack(pb.track);
        const int pad = lanes > 1 ? 0 : 2;
        const QRect hit = br.adjusted(0, -pad, 0, pad);
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
            /* Stretching a clip across rows only from the outer edges of a layered row,
               so thin lanes stay easy to grab and move. */
            const bool near_top = y <= br.top() + edge_y && (lanes <= 1 || pb.clip->layer == 0);
            const bool near_bot = y >= br.bottom() - edge_y && (lanes <= 1 || pb.clip->layer >= lanes - 1);
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

