// SPDX-License-Identifier: GPL-2.0-only

#include "ActionTimelineWidget.h"
#include "ActionCatalog.h"
#include "Actions/ActionApplier.h"
#include "Actions/ActionMedia.h"
#include "Actions/ActionScript.h"
#include "ZoneManager3D.h"

#include <QColorDialog>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHash>
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
#include <limits>

namespace
{

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor QColorToRgb(const QColor& c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

} // namespace


void ActionTimelineWidget::applyDrag(int mouse_x, int mouse_y)
{
    if(drag_op_ == DragOp::Marquee)
    {
        marquee_current_ = QPoint(mouse_x, mouse_y);
        drag_moved_ = true;
        update();
        return;
    }

    Action::Clip* clip = mutableClip(drag_track_, drag_clip_);
    if(!clip && drag_op_ != DragOp::ScrubHeader)
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

            if(drag_copy_)
            {
                if(drag_copy_source_track_ < 0)
                {
                    drag_copy_source_track_ = drag_track_;
                }
                forkDragGroupCopies();
                drag_copy_ = false;
                clip = mutableClip(drag_track_, drag_clip_);
                if(!clip)
                {
                    break;
                }
            }

            applyDragGroupTimeDelta(start);

            drag_float_y_ = mouse_y;
            if(mouse_y >= header_height_ && drag_group_.size() <= 1)
            {
                const int dest_row = (mouse_y - header_height_) / row_height_;
                if(dest_row >= 0 && dest_row < visible_rows_.size())
                {
                    drag_dest_row_ = dest_row;
                }
            }
            break;
        }
        case DragOp::ResizeStart:
            applyDragGroupResizeStart(std::clamp(t, 0, std::max(0, drag_origin_end_ - min_clip_ms_)));
            break;
        case DragOp::ResizeEnd:
            applyDragGroupResizeEnd(std::clamp(t, drag_origin_start_ + min_clip_ms_, duration_ms_));
            break;
        case DragOp::ResizeVertical:
        {
            if(mouse_y >= header_height_)
            {
                int row = (mouse_y - header_height_) / row_height_;
                row = std::clamp(row, 0, std::max(0, (int)visible_rows_.size() - 1));
                drag_span_hover_row_ = row;
            }
            break;
        }
        case DragOp::ScrubHeader:
            playhead_ms_ = t;
            emit playheadChanged(playhead_ms_);
            break;
        case DragOp::None:
        case DragOp::Marquee:
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

int ActionTimelineWidget::copyClipToTrack(int src_track, int src_clip, int dest_track, int start_ms)
{
    if(!pack_ || src_track < 0 || src_track >= (int)pack_->tracks.size()
       || dest_track < 0 || dest_track >= (int)pack_->tracks.size()
       || src_clip < 0 || src_clip >= (int)pack_->tracks[(size_t)src_track].clips.size())
    {
        return -1;
    }
    Action::Clip copy = pack_->tracks[(size_t)src_track].clips[(size_t)src_clip];
    if(start_ms >= 0)
    {
        const int len = std::max(min_clip_ms_, copy.end_ms - copy.start_ms);
        copy.start_ms = std::clamp(start_ms, 0, std::max(0, duration_ms_ - len));
        copy.end_ms = copy.start_ms + len;
    }
    pack_->tracks[(size_t)dest_track].clips.push_back(std::move(copy));
    return (int)pack_->tracks[(size_t)dest_track].clips.size() - 1;
}

bool ActionTimelineWidget::trackCoversRow(const Action::Track& track, const Row& row) const
{
    if(Action::TargetEquals(track.target, row.target))
    {
        return true;
    }
    if(!row.single_led_row || row.target.kind != Action::TargetKind::Leds
       || row.target.led_indices.empty())
    {
        return false;
    }
    const int led = row.target.led_indices.front();
    // Multi-LED track spans each LED child row it includes.
    if(track.target.kind == Action::TargetKind::Leds
       && track.target.device_name == row.target.device_name
       && track.target.zone_name == row.target.zone_name)
    {
        for(int idx : track.target.led_indices)
        {
            if(idx == led)
            {
                return true;
            }
        }
        return false;
    }
    // Parent zone/device tracks cover their LED child rows when the tree is expanded.
    if(track.target.kind == Action::TargetKind::Zone
       && track.target.device_name == row.target.device_name
       && (track.target.zone_name == row.target.zone_name
           || track.target.zone_name.empty() || row.target.zone_name.empty()))
    {
        return true;
    }
    if(track.target.kind == Action::TargetKind::Device
       && track.target.device_name == row.target.device_name)
    {
        return true;
    }
    if(track.target.kind == Action::TargetKind::All)
    {
        return true;
    }
    return false;
}

bool ActionTimelineWidget::coveredRowSpanForTrack(int track_index, int* out_lo, int* out_hi) const
{
    if(!pack_ || track_index < 0 || track_index >= (int)pack_->tracks.size() || !out_lo || !out_hi)
    {
        return false;
    }
    const Action::Track& track = pack_->tracks[(size_t)track_index];
    int lo = -1;
    int hi = -1;
    for(int row = 0; row < visible_rows_.size(); ++row)
    {
        if(!trackCoversRow(track, visible_rows_[row]))
        {
            continue;
        }
        if(lo < 0)
        {
            lo = row;
        }
        hi = row;
    }
    if(lo < 0)
    {
        return false;
    }
    *out_lo = lo;
    *out_hi = hi;
    return true;
}

bool ActionTimelineWidget::expandClipAcrossRows(int src_track, int src_clip, int row_a, int row_b,
                                                    int* out_track, int* out_clip)
{
    if(!pack_ || src_track < 0 || src_clip < 0
       || src_track >= (int)pack_->tracks.size()
       || src_clip >= (int)pack_->tracks[(size_t)src_track].clips.size())
    {
        return false;
    }
    const int lo = std::min(row_a, row_b);
    const int hi = std::max(row_a, row_b);
    if(lo < 0 || hi >= visible_rows_.size() || lo == hi)
    {
        return false;
    }

    std::vector<int> leds;
    std::string device;
    std::string zone;
    bool have_device = false;
    bool incompatible = false;
    Action::Target broad_target;
    bool have_broad = false;

    for(int row = lo; row <= hi; ++row)
    {
        const Row& r = visible_rows_[row];
        if(r.target.kind == Action::TargetKind::Leds && !r.target.led_indices.empty())
        {
            if(!have_device)
            {
                device = r.target.device_name;
                zone = r.target.zone_name;
                have_device = true;
            }
            else if(device != r.target.device_name || zone != r.target.zone_name)
            {
                incompatible = true;
                break;
            }
            for(int idx : r.target.led_indices)
            {
                leds.push_back(idx);
            }
        }
        else if(r.target.kind == Action::TargetKind::Device
                || r.target.kind == Action::TargetKind::Zone
                || r.target.kind == Action::TargetKind::SceneZone
                || r.target.kind == Action::TargetKind::All)
        {
            if(!have_broad)
            {
                broad_target = r.target;
                have_broad = true;
            }
        }
    }
    if(incompatible)
    {
        return false;
    }

    Action::Target new_target;
    // Vertical resize must stick on the spanned LED rows. Preferring the zone/device
    // parent moved the clip onto that parent track and made it look like it snapped
    // back to a single row after release.
    if(leds.size() >= 2)
    {
        std::sort(leds.begin(), leds.end());
        leds.erase(std::unique(leds.begin(), leds.end()), leds.end());
        new_target.kind = Action::TargetKind::Leds;
        new_target.device_name = device;
        new_target.zone_name = zone;
        new_target.led_indices = std::move(leds);
    }
    else if(leds.size() == 1 && !have_broad)
    {
        new_target.kind = Action::TargetKind::Leds;
        new_target.device_name = device;
        new_target.zone_name = zone;
        new_target.led_indices = std::move(leds);
    }
    else if(have_broad)
    {
        // Span is parent rows only (LEDs collapsed), or one LED + its parent.
        new_target = broad_target;
    }
    else if(leds.size() == 1)
    {
        new_target.kind = Action::TargetKind::Leds;
        new_target.device_name = device;
        new_target.zone_name = zone;
        new_target.led_indices = std::move(leds);
    }
    else
    {
        return false;
    }

    Action::Clip moved = pack_->tracks[(size_t)src_track].clips[(size_t)src_clip];
    // Continuous wipe across the span (not per-LED Sequence loops).
    // All / scene zone → Room; single-device destination → Device from Sequence.
    if(Action::TargetIsMultiDeviceGroup(new_target))
    {
        if(moved.axis_space == Action::AxisSpace::Sequence
           || moved.axis_space == Action::AxisSpace::Device)
        {
            moved.axis_space = Action::AxisSpace::Room;
        }
        if(!Action::IsMediaClip(moved))
        {
            moved.play_bounds = Action::PlayBounds::RoomBox;
        }
    }
    else if(moved.axis_space == Action::AxisSpace::Sequence)
    {
        moved.axis_space = Action::AxisSpace::Device;
        if(Action::IsMediaClip(moved))
        {
            moved.play_bounds = Action::PlayBounds::TrackLights;
        }
    }

    int dest_track = -1;
    for(int i = 0; i < (int)pack_->tracks.size(); ++i)
    {
        if(Action::TargetEquals(pack_->tracks[(size_t)i].target, new_target))
        {
            dest_track = i;
            break;
        }
    }

    auto& src = pack_->tracks[(size_t)src_track];
    if(dest_track < 0 && src.clips.size() == 1)
    {
        src.target = new_target;
        if(new_target.kind == Action::TargetKind::Leds)
        {
            src.name = "LEDs (" + std::to_string(new_target.led_indices.size()) + ")";
        }
        dest_track = src_track;
        src.clips[(size_t)src_clip] = std::move(moved);
    }
    else if(dest_track < 0)
    {
        Action::Track track;
        track.target = new_target;
        track.name = (new_target.kind == Action::TargetKind::Leds)
            ? ("LEDs (" + std::to_string(new_target.led_indices.size()) + ")")
            : src.name;
        track.clips.push_back(std::move(moved));
        pack_->tracks.push_back(std::move(track));
        dest_track = (int)pack_->tracks.size() - 1;
        auto& src_clips = pack_->tracks[(size_t)src_track].clips;
        src_clips.erase(src_clips.begin() + src_clip);
    }
    else if(dest_track != src_track)
    {
        auto& src_clips = pack_->tracks[(size_t)src_track].clips;
        src_clips.erase(src_clips.begin() + src_clip);
        pack_->tracks[(size_t)dest_track].clips.push_back(std::move(moved));
    }
    else
    {
        pack_->tracks[(size_t)dest_track].clips[(size_t)src_clip] = std::move(moved);
    }

    int dest_clip = -1;
    if(dest_track >= 0 && dest_track < (int)pack_->tracks.size())
    {
        dest_clip = (int)pack_->tracks[(size_t)dest_track].clips.size() - 1;
        // If we retargeted in place without moving, keep the same clip index.
        if(dest_track == src_track && pack_->tracks[(size_t)dest_track].clips.size() > (size_t)src_clip)
        {
            // Prefer the clip we just wrote when still present at src_clip.
            if(src_clip >= 0 && src_clip < (int)pack_->tracks[(size_t)dest_track].clips.size())
            {
                dest_clip = src_clip;
            }
        }
    }
    if(dest_clip < 0)
    {
        return false;
    }

    const Action::Clip& keep = pack_->tracks[(size_t)dest_track].clips[(size_t)dest_clip];

    // Remove per-LED clone leftovers now covered by this span.
    for(int ti = (int)pack_->tracks.size() - 1; ti >= 0; --ti)
    {
        if(ti == dest_track)
        {
            continue;
        }
        auto& tr = pack_->tracks[(size_t)ti];
        if(tr.target.kind != Action::TargetKind::Leds || tr.target.led_indices.empty())
        {
            continue;
        }
        bool covered = false;
        if(new_target.kind == Action::TargetKind::Leds)
        {
            if(tr.target.device_name != new_target.device_name
               || tr.target.zone_name != new_target.zone_name)
            {
                continue;
            }
            covered = true;
            for(int idx : tr.target.led_indices)
            {
                if(std::find(new_target.led_indices.begin(), new_target.led_indices.end(), idx)
                   == new_target.led_indices.end())
                {
                    covered = false;
                    break;
                }
            }
        }
        else if(new_target.kind == Action::TargetKind::Zone
                || new_target.kind == Action::TargetKind::SceneZone)
        {
            covered = (tr.target.device_name == new_target.device_name
                       && tr.target.zone_name == new_target.zone_name);
        }
        else if(new_target.kind == Action::TargetKind::Device)
        {
            covered = (tr.target.device_name == new_target.device_name);
        }
        else if(new_target.kind == Action::TargetKind::All)
        {
            covered = true;
        }
        if(!covered)
        {
            continue;
        }
        for(int bi = (int)tr.clips.size() - 1; bi >= 0; --bi)
        {
            const Action::Clip& b = tr.clips[(size_t)bi];
            if(b.start_ms == keep.start_ms && b.end_ms == keep.end_ms && b.effect_id == keep.effect_id)
            {
                tr.clips.erase(tr.clips.begin() + bi);
            }
        }
        if(tr.clips.empty())
        {
            pack_->tracks.erase(pack_->tracks.begin() + ti);
            if(ti < dest_track)
            {
                --dest_track;
            }
        }
    }

    if(out_track)
    {
        *out_track = dest_track;
    }
    if(out_clip)
    {
        *out_clip = (dest_track >= 0 && dest_track < (int)pack_->tracks.size())
            ? std::min(dest_clip, (int)pack_->tracks[(size_t)dest_track].clips.size() - 1)
            : -1;
    }
    return dest_track >= 0;
}

void ActionTimelineWidget::clearClipSelection()
{
    selection_.clear();
    selected_track_ = -1;
    selected_clip_ = -1;
}

void ActionTimelineWidget::setPrimarySelection(int track, int clip)
{
    selection_.clear();
    selected_track_ = track;
    selected_clip_ = clip;
    if(track >= 0 && clip >= 0)
    {
        selection_.push_back({track, clip});
    }
    update();
}

void ActionTimelineWidget::toggleClipSelection(int track, int clip)
{
    if(track < 0 || clip < 0)
    {
        return;
    }
    for(int i = 0; i < selection_.size(); ++i)
    {
        if(selection_[i].track == track && selection_[i].clip == clip)
        {
            selection_.removeAt(i);
            if(selected_track_ == track && selected_clip_ == clip)
            {
                if(selection_.isEmpty())
                {
                    selected_track_ = -1;
                    selected_clip_ = -1;
                }
                else
                {
                    selected_track_ = selection_.last().track;
                    selected_clip_ = selection_.last().clip;
                }
            }
            update();
            return;
        }
    }
    selection_.push_back({track, clip});
    selected_track_ = track;
    selected_clip_ = clip;
    update();
}

bool ActionTimelineWidget::isClipSelected(int track, int clip) const
{
    for(const ClipId& id : selection_)
    {
        if(id.track == track && id.clip == clip)
        {
            return true;
        }
    }
    return false;
}

QRect ActionTimelineWidget::clipAbsRect(int track, int clip_index) const
{
    if(!pack_ || track < 0 || clip_index < 0
       || track >= (int)pack_->tracks.size()
       || clip_index >= (int)pack_->tracks[(size_t)track].clips.size())
    {
        return {};
    }
    const Action::Clip& b = pack_->tracks[(size_t)track].clips[(size_t)clip_index];
    for(int row = 0; row < visible_rows_.size(); ++row)
    {
        if(trackCoversRow(pack_->tracks[(size_t)track], visible_rows_[row]))
        {
            return clipRect(row, b);
        }
    }
    return {};
}

void ActionTimelineWidget::selectClipsInRect(const QRect& rect, bool additive)
{
    if(!additive)
    {
        selection_.clear();
        selected_track_ = -1;
        selected_clip_ = -1;
    }
    if(!pack_ || !rect.isValid())
    {
        return;
    }
    const QRect area = rect.normalized().intersected(
        QRect(gutter_width_, header_height_,
              std::max(0, width() - gutter_width_),
              std::max(0, height() - header_height_)));
    if(area.isEmpty())
    {
        return;
    }
    for(int ti = 0; ti < (int)pack_->tracks.size(); ++ti)
    {
        for(int bi = 0; bi < (int)pack_->tracks[(size_t)ti].clips.size(); ++bi)
        {
            const QRect br = clipAbsRect(ti, bi);
            if(br.isEmpty() || !br.intersects(area))
            {
                continue;
            }
            if(!isClipSelected(ti, bi))
            {
                selection_.push_back({ti, bi});
            }
            selected_track_ = ti;
            selected_clip_ = bi;
        }
    }
}

void ActionTimelineWidget::beginDragGroupFromSelection()
{
    drag_group_.clear();
    if(!pack_)
    {
        return;
    }
    auto add_member = [&](int track, int clip) {
        if(track < 0 || clip < 0
           || track >= (int)pack_->tracks.size()
           || clip >= (int)pack_->tracks[(size_t)track].clips.size())
        {
            return;
        }
        for(const DragMember& m : drag_group_)
        {
            if(m.track == track && m.clip == clip)
            {
                return;
            }
        }
        const Action::Clip& b = pack_->tracks[(size_t)track].clips[(size_t)clip];
        drag_group_.push_back({track, clip, b.start_ms, b.end_ms});
    };
    if(selection_.isEmpty())
    {
        add_member(drag_track_, drag_clip_);
    }
    else
    {
        for(const ClipId& id : selection_)
        {
            add_member(id.track, id.clip);
        }
        add_member(drag_track_, drag_clip_);
    }
}

void ActionTimelineWidget::forkDragGroupCopies()
{
    if(!pack_ || drag_group_.isEmpty())
    {
        return;
    }
    QVector<DragMember> copies;
    selection_.clear();
    for(const DragMember& m : drag_group_)
    {
        const int new_bi = copyClipToTrack(m.track, m.clip, m.track, m.origin_start);
        if(new_bi < 0)
        {
            continue;
        }
        copies.push_back({m.track, new_bi, m.origin_start, m.origin_end});
        selection_.push_back({m.track, new_bi});
        if(m.track == drag_track_ && m.clip == drag_clip_)
        {
            drag_clip_ = new_bi;
            selected_track_ = m.track;
            selected_clip_ = new_bi;
        }
    }
    drag_group_ = copies;
    if(selected_track_ < 0 && !selection_.isEmpty())
    {
        selected_track_ = selection_.front().track;
        selected_clip_ = selection_.front().clip;
        drag_track_ = selected_track_;
        drag_clip_ = selected_clip_;
    }
}

void ActionTimelineWidget::applyDragGroupTimeDelta(int primary_new_start)
{
    if(!pack_ || drag_group_.isEmpty())
    {
        return;
    }
    const int delta = primary_new_start - drag_origin_start_;
    for(const DragMember& m : drag_group_)
    {
        Action::Clip* b = mutableClip(m.track, m.clip);
        if(!b)
        {
            continue;
        }
        const int len = m.origin_end - m.origin_start;
        int start = snapMs(m.origin_start + delta);
        start = std::clamp(start, 0, std::max(0, duration_ms_ - len));
        b->start_ms = start;
        b->end_ms = start + len;
    }
}

void ActionTimelineWidget::applyDragGroupResizeStart(int primary_new_start)
{
    if(!pack_ || drag_group_.isEmpty())
    {
        return;
    }
    const int delta = primary_new_start - drag_origin_start_;
    for(const DragMember& m : drag_group_)
    {
        Action::Clip* b = mutableClip(m.track, m.clip);
        if(!b)
        {
            continue;
        }
        const int max_start = m.origin_end - min_clip_ms_;
        b->start_ms = std::clamp(m.origin_start + delta, 0, std::max(0, max_start));
        b->end_ms = m.origin_end;
        if(b->end_ms - b->start_ms < min_clip_ms_)
        {
            b->start_ms = b->end_ms - min_clip_ms_;
        }
    }
}

void ActionTimelineWidget::applyDragGroupResizeEnd(int primary_new_end)
{
    if(!pack_ || drag_group_.isEmpty())
    {
        return;
    }
    const int delta = primary_new_end - drag_origin_end_;
    for(const DragMember& m : drag_group_)
    {
        Action::Clip* b = mutableClip(m.track, m.clip);
        if(!b)
        {
            continue;
        }
        const int min_end = m.origin_start + min_clip_ms_;
        b->end_ms = std::clamp(m.origin_end + delta, min_end, duration_ms_);
        b->start_ms = m.origin_start;
    }
}

bool ActionTimelineWidget::deleteSelectedClips()
{
    if(!pack_ || selection_.isEmpty())
    {
        return false;
    }
    QVector<ClipId> ordered = selection_;
    std::sort(ordered.begin(), ordered.end(), [](const ClipId& a, const ClipId& b) {
        if(a.track != b.track)
        {
            return a.track > b.track;
        }
        return a.clip > b.clip;
    });
    for(const ClipId& id : ordered)
    {
        if(id.track < 0 || id.track >= (int)pack_->tracks.size())
        {
            continue;
        }
        auto& clips = pack_->tracks[(size_t)id.track].clips;
        if(id.clip < 0 || id.clip >= (int)clips.size())
        {
            continue;
        }
        clips.erase(clips.begin() + id.clip);
        if(clips.empty())
        {
            pack_->tracks.erase(pack_->tracks.begin() + id.track);
        }
    }
    clearClipSelection();
    update();
    emit clipEdited(-1, -1);
    return true;
}

bool ActionTimelineWidget::mutateSelectedClips(const std::function<void(Action::Clip&)>& mutator)
{
    if(!pack_ || selection_.isEmpty() || !mutator)
    {
        return false;
    }
    bool any = false;
    int primary_track = selected_track_;
    int primary_clip = selected_clip_;
    for(const ClipId& id : selection_)
    {
        Action::Clip* b = mutableClip(id.track, id.clip);
        if(!b)
        {
            continue;
        }
        mutator(*b);
        any = true;
        primary_track = id.track;
        primary_clip = id.clip;
    }
    if(!any)
    {
        return false;
    }
    update();
    emit clipEdited(primary_track, primary_clip);
    return true;
}

void ActionTimelineWidget::copySelectedClipToClipboard()
{
    clipboard_has_clip_ = false;
    clipboard_has_multi_ = false;
    clipboard_multi_clips_.clear();
    if(!pack_)
    {
        return;
    }
    if(selection_.size() > 1)
    {
        int min_start = std::numeric_limits<int>::max();
        for(const ClipId& id : selection_)
        {
            if(id.track < 0 || id.track >= (int)pack_->tracks.size()
               || id.clip < 0 || id.clip >= (int)pack_->tracks[(size_t)id.track].clips.size())
            {
                continue;
            }
            const Action::Clip& b = pack_->tracks[(size_t)id.track].clips[(size_t)id.clip];
            min_start = std::min(min_start, b.start_ms);
            clipboard_multi_clips_.push_back(b);
        }
        if(clipboard_multi_clips_.isEmpty())
        {
            return;
        }
        for(Action::Clip& b : clipboard_multi_clips_)
        {
            b.start_ms -= min_start;
            b.end_ms -= min_start;
        }
        clipboard_has_multi_ = true;
        clipboard_has_clip_ = true;
        clipboard_clip_ = clipboard_multi_clips_.front();
        clipboard_has_row_ = false;
        return;
    }
    if(selected_track_ < 0 || selected_clip_ < 0
       || selected_track_ >= (int)pack_->tracks.size()
       || selected_clip_ >= (int)pack_->tracks[(size_t)selected_track_].clips.size())
    {
        return;
    }
    clipboard_clip_ = pack_->tracks[(size_t)selected_track_].clips[(size_t)selected_clip_];
    clipboard_has_clip_ = true;
    clipboard_has_row_ = false;
}

bool ActionTimelineWidget::pasteClipboardClipAt(int row, int ms)
{
    if(!clipboard_has_clip_ || !pack_)
    {
        return false;
    }
    const int dest_track = findOrCreateTrackForRow(row >= 0 ? row : selected_row_);
    if(dest_track < 0)
    {
        return false;
    }
    if(clipboard_has_multi_ && !clipboard_multi_clips_.isEmpty())
    {
        selection_.clear();
        int last = -1;
        for(const Action::Clip& src : clipboard_multi_clips_)
        {
            Action::Clip copy = src;
            const int len = std::max(min_clip_ms_, copy.end_ms - copy.start_ms);
            const int start = snapMs(std::clamp(ms + copy.start_ms, 0, std::max(0, duration_ms_ - len)));
            copy.start_ms = start;
            copy.end_ms = start + len;
            if(Action::TargetIsMultiDeviceGroup(pack_->tracks[(size_t)dest_track].target)
               && (copy.axis_space == Action::AxisSpace::Device
                   || copy.axis_space == Action::AxisSpace::Sequence))
            {
                copy.axis_space = Action::AxisSpace::Room;
                if(!Action::IsMediaClip(copy))
                {
                    copy.play_bounds = Action::PlayBounds::RoomBox;
                }
            }
            pack_->tracks[(size_t)dest_track].clips.push_back(std::move(copy));
            last = (int)pack_->tracks[(size_t)dest_track].clips.size() - 1;
            selection_.push_back({dest_track, last});
        }
        selected_track_ = dest_track;
        selected_clip_ = last;
        selected_row_ = row;
        emit clipCreated(selected_track_, selected_clip_);
        emit clipSelected(selected_track_, selected_clip_);
        update();
        return true;
    }
    Action::Clip copy = clipboard_clip_;
    const int len = std::max(min_clip_ms_, copy.end_ms - copy.start_ms);
    const int start = snapMs(std::clamp(ms, 0, std::max(0, duration_ms_ - len)));
    copy.start_ms = start;
    copy.end_ms = start + len;
    if(dest_track >= 0 && dest_track < (int)pack_->tracks.size()
       && Action::TargetIsMultiDeviceGroup(pack_->tracks[(size_t)dest_track].target)
       && (copy.axis_space == Action::AxisSpace::Device
           || copy.axis_space == Action::AxisSpace::Sequence))
    {
        copy.axis_space = Action::AxisSpace::Room;
        if(!Action::IsMediaClip(copy))
        {
            copy.play_bounds = Action::PlayBounds::RoomBox;
        }
    }
    pack_->tracks[(size_t)dest_track].clips.push_back(std::move(copy));
    selected_track_ = dest_track;
    selected_clip_ = (int)pack_->tracks[(size_t)dest_track].clips.size() - 1;
    selected_row_ = row;
    setPrimarySelection(selected_track_, selected_clip_);
    emit clipCreated(selected_track_, selected_clip_);
    emit clipSelected(selected_track_, selected_clip_);
    update();
    return true;
}

bool ActionTimelineWidget::duplicateSelectedClip()
{
    if(!pack_)
    {
        return false;
    }
    if(selection_.size() > 1)
    {
        beginDragGroupFromSelection();
        // Snapshot origins then place copies after each original.
        QVector<ClipId> created;
        for(const ClipId& id : selection_)
        {
            if(id.track < 0 || id.track >= (int)pack_->tracks.size()
               || id.clip < 0 || id.clip >= (int)pack_->tracks[(size_t)id.track].clips.size())
            {
                continue;
            }
            Action::Clip copy = pack_->tracks[(size_t)id.track].clips[(size_t)id.clip];
            const int len = std::max(min_clip_ms_, copy.end_ms - copy.start_ms);
            copy.start_ms = std::clamp(copy.end_ms, 0, std::max(0, duration_ms_ - len));
            copy.end_ms = copy.start_ms + len;
            if(copy.end_ms > duration_ms_)
            {
                copy.end_ms = duration_ms_;
                copy.start_ms = std::max(0, copy.end_ms - len);
            }
            pack_->tracks[(size_t)id.track].clips.push_back(std::move(copy));
            const int ni = (int)pack_->tracks[(size_t)id.track].clips.size() - 1;
            created.push_back({id.track, ni});
        }
        selection_ = created;
        if(!selection_.isEmpty())
        {
            selected_track_ = selection_.last().track;
            selected_clip_ = selection_.last().clip;
            emit clipCreated(selected_track_, selected_clip_);
            emit clipSelected(selected_track_, selected_clip_);
        }
        drag_group_.clear();
        update();
        return !created.isEmpty();
    }
    if(selected_track_ < 0 || selected_clip_ < 0
       || selected_track_ >= (int)pack_->tracks.size()
       || selected_clip_ >= (int)pack_->tracks[(size_t)selected_track_].clips.size())
    {
        return false;
    }
    Action::Clip copy = pack_->tracks[(size_t)selected_track_].clips[(size_t)selected_clip_];
    const int len = std::max(min_clip_ms_, copy.end_ms - copy.start_ms);
    copy.start_ms = std::clamp(copy.end_ms, 0, std::max(0, duration_ms_ - len));
    copy.end_ms = copy.start_ms + len;
    if(copy.end_ms > duration_ms_)
    {
        copy.end_ms = duration_ms_;
        copy.start_ms = std::max(0, copy.end_ms - len);
    }
    pack_->tracks[(size_t)selected_track_].clips.push_back(std::move(copy));
    selected_clip_ = (int)pack_->tracks[(size_t)selected_track_].clips.size() - 1;
    setPrimarySelection(selected_track_, selected_clip_);
    emit clipCreated(selected_track_, selected_clip_);
    emit clipSelected(selected_track_, selected_clip_);
    update();
    return true;
}

void ActionTimelineWidget::copyRowEffectsToClipboard(int row)
{
    clipboard_has_row_ = false;
    clipboard_row_clips_.clear();
    const int track = trackIndexForRow(row);
    if(!pack_ || track < 0 || track >= (int)pack_->tracks.size())
    {
        return;
    }
    for(const Action::Clip& b : pack_->tracks[(size_t)track].clips)
    {
        clipboard_row_clips_.push_back(b);
    }
    clipboard_has_row_ = !clipboard_row_clips_.isEmpty();
    if(clipboard_has_row_)
    {
        clipboard_has_clip_ = false;
    }
}

bool ActionTimelineWidget::pasteClipboardRowEffectsAt(int row)
{
    if(!clipboard_has_row_ || clipboard_row_clips_.isEmpty() || !pack_)
    {
        return false;
    }
    const int dest_track = findOrCreateTrackForRow(row);
    if(dest_track < 0)
    {
        return false;
    }
    int last = -1;
    for(const Action::Clip& b : clipboard_row_clips_)
    {
        pack_->tracks[(size_t)dest_track].clips.push_back(b);
        last = (int)pack_->tracks[(size_t)dest_track].clips.size() - 1;
    }
    if(last >= 0)
    {
        selected_track_ = dest_track;
        selected_clip_ = last;
        selected_row_ = row;
        emit clipCreated(selected_track_, selected_clip_);
        emit clipSelected(selected_track_, selected_clip_);
    }
    update();
    return true;
}

int ActionTimelineWidget::findOrCreateTrackForRow(int row)
{
    if(!pack_ || row < 0 || row >= visible_rows_.size())
    {
        return -1;
    }
    const int existing = trackIndexForRow(row);
    if(existing >= 0)
    {
        return existing;
    }
    const Row& r = visible_rows_[row];
    Action::Track track;
    track.name = r.label.toStdString();
    track.target = r.target;
    pack_->tracks.push_back(std::move(track));
    return (int)pack_->tracks.size() - 1;
}

bool ActionTimelineWidget::moveDragClipToTrack(int dest_track)
{
    if(!pack_ || dest_track < 0 || dest_track >= (int)pack_->tracks.size()
       || drag_track_ < 0 || drag_track_ >= (int)pack_->tracks.size()
       || drag_clip_ < 0)
    {
        return false;
    }
    if(dest_track == drag_track_)
    {
        return false;
    }

    auto& src_clips = pack_->tracks[(size_t)drag_track_].clips;
    if(drag_clip_ >= (int)src_clips.size())
    {
        return false;
    }

    Action::Clip moved = std::move(src_clips[(size_t)drag_clip_]);
    src_clips.erase(src_clips.begin() + drag_clip_);
    pack_->tracks[(size_t)dest_track].clips.push_back(std::move(moved));

    drag_track_ = dest_track;
    drag_clip_ = (int)pack_->tracks[(size_t)dest_track].clips.size() - 1;
    selected_track_ = drag_track_;
    selected_clip_ = drag_clip_;
    return true;
}

void ActionTimelineWidget::finishDrag()
{
    if(drag_op_ == DragOp::None)
    {
        return;
    }
    const DragOp op = drag_op_;
    int track = drag_track_;
    int clip = drag_clip_;
    const bool moved = drag_moved_;
    const int span_a = drag_span_anchor_row_;
    const int span_b = drag_span_hover_row_;
    const int dest_row = drag_dest_row_;
    const int copy_src = drag_copy_source_track_;
    const int group_n = drag_group_.size();
    const QPoint marquee_a = marquee_origin_;
    const QPoint marquee_b = marquee_current_;
    const bool marquee_add = marquee_additive_;
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

    if(op == DragOp::Marquee)
    {
        const QRect box = QRect(marquee_a, marquee_b).normalized();
        if(box.width() >= 3 || box.height() >= 3)
        {
            selectClipsInRect(box, marquee_add);
            emit clipSelected(selected_track_, selected_clip_);
        }
        update();
        return;
    }

    // Cross-track move only for a single clip; multi-select stays time-shifted on home tracks.
    if(moved && op == DragOp::Move && dest_row >= 0 && track >= 0 && clip >= 0 && group_n <= 1)
    {
        const int dest_track = findOrCreateTrackForRow(dest_row);
        if(dest_track >= 0 && dest_track != track && dest_track != copy_src)
        {
            drag_track_ = track;
            drag_clip_ = clip;
            if(moveDragClipToTrack(dest_track))
            {
                track = drag_track_;
                clip = drag_clip_;
                setPrimarySelection(track, clip);
            }
            drag_track_ = -1;
            drag_clip_ = -1;
        }
        selected_row_ = dest_row;
    }

    if(moved && op == DragOp::ResizeVertical && track >= 0 && clip >= 0
       && span_a >= 0 && span_b >= 0 && span_a != span_b)
    {
        int out_track = track;
        int out_clip = clip;
        if(expandClipAcrossRows(track, clip, span_a, span_b, &out_track, &out_clip))
        {
            setPrimarySelection(out_track, out_clip);
            emit clipSelected(out_track, out_clip);
            emit clipEdited(out_track, out_clip);
        }
        update();
        return;
    }

    if(moved && (op == DragOp::Move || op == DragOp::ResizeStart || op == DragOp::ResizeEnd)
       && track >= 0 && clip >= 0)
    {
        selected_track_ = track;
        selected_clip_ = clip;
        emit clipEdited(track, clip);
        update();
    }
}

void ActionTimelineWidget::updateHoverCursor(int x, int y)
{
    if(drag_op_ != DragOp::None)
    {
        return;
    }
    ClipHit hit = ClipHit::None;
    if(hitTestClip(x, y, nullptr, nullptr, nullptr, &hit))
    {
        if(hit == ClipHit::LeftEdge || hit == ClipHit::RightEdge)
        {
            setCursor(Qt::SizeHorCursor);
            return;
        }
        if(hit == ClipHit::TopEdge || hit == ClipHit::BottomEdge)
        {
            setCursor(Qt::SizeVerCursor);
            return;
        }
        if(hit == ClipHit::Body)
        {
            setCursor(Qt::OpenHandCursor);
            return;
        }
    }
    unsetCursor();
}

void ActionTimelineWidget::populateAddEffectMenu(QMenu* menu, int row, int ms)
{
    if(!menu)
    {
        return;
    }
    const QList<ActionCatalog::Entry> entries = ActionCatalog::LoadEntries(effect_files_dir_);
    QHash<QString, QMenu*> menus;
    for(const QString& section : ActionCatalog::SectionOrder(entries))
    {
        menus.insert(section.toLower(), menu->addMenu(ActionCatalog::SectionLabel(section)));
    }
    for(const ActionCatalog::Entry& e : entries)
    {
        QMenu* dest = menus.value(e.section.toLower());
        if(!dest)
        {
            continue;
        }
        QAction* act = dest->addAction(ActionCatalog::MakeEffectIcon(e), e.name);
        connect(act, &QAction::triggered, this, [this, row, ms, id = e.id]() {
            emit effectAddRequested(row, ms, id);
        });
    }
}

void ActionTimelineWidget::showRowContextMenu(int row, int ms, const QPoint& global_pos)
{
    if(row < 0 || row >= visible_rows_.size())
    {
        return;
    }
    const Row& r = visible_rows_[row];
    QMenu menu(this);
    QAction* preview_track = menu.addAction(QStringLiteral("Preview this track\tShift+Space"));
    QAction* preview_pack = menu.addAction(QStringLiteral("Preview pack\tSpace"));
    QAction* preview_from = menu.addAction(QStringLiteral("Preview from playhead"));
    menu.addSeparator();
    QAction* paste_clip = menu.addAction(QStringLiteral("Paste effect\tCtrl+V"));
    paste_clip->setEnabled(clipboard_has_clip_);
    QAction* copy_row = menu.addAction(QStringLiteral("Copy row effects"));
    copy_row->setEnabled(trackIndexForRow(row) >= 0);
    QAction* paste_row = menu.addAction(QStringLiteral("Paste row effects"));
    paste_row->setEnabled(clipboard_has_row_);
    menu.addSeparator();
    QMenu* add = menu.addMenu(QStringLiteral("Add effect"));
    populateAddEffectMenu(add, row, ms);
    QAction* expand_act = nullptr;
    if(r.expandable)
    {
        expand_act = menu.addAction(r.expanded ? QStringLiteral("Collapse") : QStringLiteral("Expand"));
    }
    QAction* chosen = menu.exec(global_pos);
    if(chosen == preview_track)
    {
        emit previewRowRequested(row, 0);
    }
    else if(chosen == preview_pack)
    {
        emit previewPackRequested(0);
    }
    else if(chosen == preview_from)
    {
        emit previewPackRequested(playhead_ms_);
    }
    else if(chosen == paste_clip)
    {
        pasteClipboardClipAt(row, ms);
    }
    else if(chosen == copy_row)
    {
        copyRowEffectsToClipboard(row);
    }
    else if(chosen == paste_row)
    {
        pasteClipboardRowEffectsAt(row);
    }
    else if(chosen == expand_act && expand_act)
    {
        toggleExpand(r.path);
    }
}

void ActionTimelineWidget::showClipContextMenu(int row, int track, int clip, const QPoint& global_pos)
{
    QMenu menu(this);
    QAction* preview_track = menu.addAction(QStringLiteral("Preview this track\tShift+Space"));
    QAction* play_clip = menu.addAction(QStringLiteral("Play this clip"));
    QAction* preview_pack = menu.addAction(QStringLiteral("Preview pack\tSpace"));
    menu.addSeparator();
    QAction* cut_act = menu.addAction(QStringLiteral("Cut\tCtrl+X"));
    QAction* copy_act = menu.addAction(QStringLiteral("Copy\tCtrl+C"));
    QAction* paste_act = menu.addAction(QStringLiteral("Paste\tCtrl+V"));
    paste_act->setEnabled(clipboard_has_clip_);
    QAction* dup_act = menu.addAction(QStringLiteral("Duplicate\tCtrl+D"));
    menu.addSeparator();
    QMenu* xform = menu.addMenu(QStringLiteral("Transform"));
    QAction* reverse_act = xform->addAction(QStringLiteral("Reverse time"));
    QAction* flip_h_act = xform->addAction(QStringLiteral("Flip horizontal"));
    QAction* flip_v_act = xform->addAction(QStringLiteral("Flip vertical"));
    QAction* rotate_act = xform->addAction(QStringLiteral("Rotate 90° CW"));
    QAction* flip_dir_act = xform->addAction(QStringLiteral("Flip direction"));
    menu.addSeparator();
    QAction* color_act = menu.addAction(QStringLiteral("Change color…"));
    menu.addSeparator();
    QAction* del_act = menu.addAction(QStringLiteral("Delete\tDel"));
    QAction* chosen = menu.exec(global_pos);
    if(chosen == preview_track)
    {
        emit previewTrackRequested(track, 0);
    }
    else if(chosen == play_clip)
    {
        int start_ms = 0;
        if(pack_ && track >= 0 && track < (int)pack_->tracks.size()
           && clip >= 0 && clip < (int)pack_->tracks[(size_t)track].clips.size())
        {
            start_ms = pack_->tracks[(size_t)track].clips[(size_t)clip].start_ms;
        }
        emit previewTrackRequested(track, start_ms);
    }
    else if(chosen == preview_pack)
    {
        emit previewPackRequested(0);
    }
    else if(chosen == cut_act)
    {
        if(!isClipSelected(track, clip))
        {
            setPrimarySelection(track, clip);
        }
        copySelectedClipToClipboard();
        deleteSelectedClips();
    }
    else if(chosen == copy_act)
    {
        if(!isClipSelected(track, clip))
        {
            setPrimarySelection(track, clip);
        }
        copySelectedClipToClipboard();
    }
    else if(chosen == paste_act)
    {
        const int ms = (pack_ && track >= 0 && track < (int)pack_->tracks.size()
                        && clip >= 0 && clip < (int)pack_->tracks[(size_t)track].clips.size())
            ? pack_->tracks[(size_t)track].clips[(size_t)clip].end_ms
            : playhead_ms_;
        pasteClipboardClipAt(row, ms);
    }
    else if(chosen == dup_act)
    {
        if(!isClipSelected(track, clip))
        {
            setPrimarySelection(track, clip);
        }
        duplicateSelectedClip();
    }
    else if(chosen == reverse_act || chosen == flip_h_act || chosen == flip_v_act
            || chosen == rotate_act || chosen == flip_dir_act)
    {
        if(!isClipSelected(track, clip))
        {
            setPrimarySelection(track, clip);
        }
        if(chosen == reverse_act)
        {
            mutateSelectedClips([](Action::Clip& b) { b.reverse = !b.reverse; });
        }
        else if(chosen == flip_h_act)
        {
            mutateSelectedClips([](Action::Clip& b) { b.flip_h = !b.flip_h; });
        }
        else if(chosen == flip_v_act)
        {
            mutateSelectedClips([](Action::Clip& b) { b.flip_v = !b.flip_v; });
        }
        else if(chosen == rotate_act)
        {
            mutateSelectedClips([](Action::Clip& b) {
                b.rotate_quarters = (b.rotate_quarters + 1) % 4;
            });
        }
        else if(chosen == flip_dir_act)
        {
            mutateSelectedClips([](Action::Clip& b) {
                b.direction = Action::OppositeDirection(b.direction);
            });
        }
    }
    else if(chosen == color_act)
    {
        editClipColorAt(track, clip, global_pos);
    }
    else if(chosen == del_act)
    {
        if(!isClipSelected(track, clip))
        {
            setPrimarySelection(track, clip);
        }
        deleteSelectedClips();
    }
}

void ActionTimelineWidget::showHeaderContextMenu(const QPoint& global_pos)
{
    QMenu menu(this);
    QAction* preview_pack = menu.addAction(QStringLiteral("Preview pack\tSpace"));
    QAction* preview_from = menu.addAction(QStringLiteral("Preview from playhead"));
    menu.addSeparator();
    QAction* paste_act = menu.addAction(QStringLiteral("Paste at playhead\tCtrl+V"));
    paste_act->setEnabled(clipboard_has_clip_ && selected_row_ >= 0);
    QAction* chosen = menu.exec(global_pos);
    if(chosen == preview_pack)
    {
        emit previewPackRequested(0);
    }
    else if(chosen == preview_from)
    {
        emit previewPackRequested(playhead_ms_);
    }
    else if(chosen == paste_act)
    {
        pasteClipboardClipAt(selected_row_, playhead_ms_);
    }
}

void ActionTimelineWidget::applyColorToClip(int track, int clip, RGBColor color)
{
    Action::Clip* b = mutableClip(track, clip);
    if(!b)
    {
        return;
    }
    b->color = color;
    b->color_from = color;
    b->color_to = color;
    b->gradient = {{0.0f, color}};
    selected_track_ = track;
    selected_clip_ = clip;
    emit clipSelected(track, clip);
    update();
    emit clipEdited(track, clip);
}

bool ActionTimelineWidget::dropAt(const QPoint& pos, const QMimeData* mime)
{
    if(!mime || !pack_)
    {
        return false;
    }

    const QString effect_id = ActionCatalog::EffectIdFromMime(mime);
    if(!effect_id.isEmpty())
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
        emit effectAddRequested(row, ms, effect_id);
        return true;
    }

    RGBColor color = 0;
    QString preset;
    QString curve;
    const bool has_color = ActionCatalog::ColorFromMime(mime, &color);
    const bool has_grad = ActionCatalog::GradientPresetFromMime(mime, &preset);
    const bool has_curve = ActionCatalog::CurvePresetFromMime(mime, &curve);
    if(!has_color && !has_grad && !has_curve)
    {
        return false;
    }

    int row = -1;
    int track = -1;
    int clip = -1;
    if(!hitTestClip(pos.x(), pos.y(), &row, &track, &clip))
    {
        if(has_curve)
        {
            return false;
        }
        if(pos.y() < header_height_ || pos.x() < gutter_width_)
        {
            return false;
        }
        const int drop_row = (pos.y() - header_height_) / row_height_;
        if(drop_row < 0 || drop_row >= visible_rows_.size())
        {
            return false;
        }
        const int ms = xToTime(pos.x());
        selected_row_ = drop_row;
        emit rowSelected(drop_row);
        if(has_color)
        {
            emit colorDropped(drop_row, ms, color);
            return true;
        }
        emit gradientDropped(drop_row, ms, preset);
        return true;
    }
    if(has_color)
    {
        applyColorToClip(track, clip, color);
        return true;
    }
    if(has_curve)
    {
        emit curvePresetApplied(track, clip, curve);
        return true;
    }
    emit gradientPresetApplied(track, clip, preset);
    return true;
}

void ActionTimelineWidget::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if(mime && (mime->hasFormat(QString::fromUtf8(ActionCatalog::kEffectMimeType))
                || mime->hasFormat(QString::fromUtf8(ActionCatalog::kColorMimeType))
                || mime->hasFormat(QString::fromUtf8(ActionCatalog::kGradientPresetMimeType))
                || mime->hasFormat(QString::fromUtf8(ActionCatalog::kCurvePresetMimeType))
                || mime->hasColor()))
    {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ActionTimelineWidget::dragMoveEvent(QDragMoveEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if(mime && (mime->hasFormat(QString::fromUtf8(ActionCatalog::kEffectMimeType))
                || mime->hasFormat(QString::fromUtf8(ActionCatalog::kColorMimeType))
                || mime->hasFormat(QString::fromUtf8(ActionCatalog::kGradientPresetMimeType))
                || mime->hasFormat(QString::fromUtf8(ActionCatalog::kCurvePresetMimeType))
                || mime->hasColor()))
    {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ActionTimelineWidget::dropEvent(QDropEvent* event)
{
    if(dropAt(event->position().toPoint(), event->mimeData()))
    {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ActionTimelineWidget::editClipColorAt(int track, int clip, const QPoint& /*global_pos*/)
{
    Action::Clip* b = mutableClip(track, clip);
    if(!b)
    {
        return;
    }

    selected_track_ = track;
    selected_clip_ = clip;
    emit clipSelected(track, clip);

    const RGBColor current = Action::EffectColorEnds(Action::ClipFileId(*b)) ? b->color_from : b->color;
    const QColor picked = QColorDialog::getColor(RgbToQColor(current), this, QStringLiteral("Clip color"));
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
    Action::EnsureClipGradient(b);
    update();
    emit clipEdited(track, clip);
}

void ActionTimelineWidget::keyPressEvent(QKeyEvent* event)
{
    const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
    if(ctrl && event->key() == Qt::Key_C)
    {
        copySelectedClipToClipboard();
        event->accept();
        return;
    }
    if(ctrl && event->key() == Qt::Key_X)
    {
        if(!selection_.isEmpty())
        {
            copySelectedClipToClipboard();
            deleteSelectedClips();
        }
        event->accept();
        return;
    }
    if(ctrl && event->key() == Qt::Key_V)
    {
        const int row = selected_row_ >= 0 ? selected_row_ : 0;
        pasteClipboardClipAt(row, playhead_ms_);
        event->accept();
        return;
    }
    if(ctrl && event->key() == Qt::Key_D)
    {
        duplicateSelectedClip();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        if(!selection_.isEmpty())
        {
            deleteSelectedClips();
            event->accept();
            return;
        }
        if(selected_track_ >= 0 && selected_clip_ >= 0)
        {
            emit clipDeleteRequested(selected_track_, selected_clip_);
            event->accept();
            return;
        }
    }
    if(ctrl && event->key() == Qt::Key_A && pack_)
    {
        selection_.clear();
        for(int ti = 0; ti < (int)pack_->tracks.size(); ++ti)
        {
            for(int bi = 0; bi < (int)pack_->tracks[(size_t)ti].clips.size(); ++bi)
            {
                selection_.push_back({ti, bi});
                selected_track_ = ti;
                selected_clip_ = bi;
            }
        }
        emit clipSelected(selected_track_, selected_clip_);
        update();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Space)
    {
        if(event->modifiers() & Qt::ShiftModifier)
        {
            const int row = selected_row_ >= 0 ? selected_row_ : 0;
            if(selected_track_ >= 0)
            {
                emit previewTrackRequested(selected_track_, 0);
            }
            else
            {
                emit previewRowRequested(row, 0);
            }
        }
        else
        {
            emit previewPackRequested(0);
        }
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Escape)
    {
        if(!selection_.isEmpty() || selected_track_ >= 0)
        {
            clearClipSelection();
            emit clipSelected(-1, -1);
            update();
            event->accept();
            return;
        }
        emit stopPreviewRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ActionTimelineWidget::mousePressEvent(QMouseEvent* event)
{
    const int x = event->position().toPoint().x();
    const int y = event->position().toPoint().y();
    setFocus(Qt::MouseFocusReason);
    const int row = (y >= header_height_) ? ((y - header_height_) / row_height_) : -1;

    if(event->button() == Qt::RightButton)
    {
        int hit_row = 0;
        int track = -1;
        int clip = -1;
        if(hitTestClip(x, y, &hit_row, &track, &clip))
        {
            selected_row_ = hit_row;
            if(!isClipSelected(track, clip))
            {
                setPrimarySelection(track, clip);
            }
            else
            {
                selected_track_ = track;
                selected_clip_ = clip;
            }
            emit rowSelected(hit_row);
            emit clipSelected(selected_track_, selected_clip_);
            showClipContextMenu(hit_row, track, clip, event->globalPosition().toPoint());
        }
        else if(y < header_height_)
        {
            showHeaderContextMenu(event->globalPosition().toPoint());
        }
        else if(y >= header_height_ && row >= 0 && row < visible_rows_.size())
        {
            selected_row_ = row;
            emit rowSelected(row);
            const int ms = (x >= gutter_width_) ? xToTime(x) : playhead_ms_;
            if(x >= gutter_width_)
            {
                playhead_ms_ = ms;
                emit playheadChanged(ms);
            }
            showRowContextMenu(row, ms, event->globalPosition().toPoint());
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
        if(r.reorderable && r.transform_index >= 0)
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
    int clip = -1;
    ClipHit hit = ClipHit::None;
    if(hitTestClip(x, y, &hit_row, &track, &clip, &hit))
    {
        selected_row_ = hit_row;
        emit rowSelected(hit_row);

        const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
        if(ctrl && hit == ClipHit::Body)
        {
            // Ctrl+click toggles multi-select like a file manager (no drag).
            toggleClipSelection(track, clip);
            emit clipSelected(selected_track_, selected_clip_);
            update();
            return;
        }

        if(!isClipSelected(track, clip) || selection_.size() <= 1)
        {
            setPrimarySelection(track, clip);
        }
        else
        {
            // Click on an already-selected clip keeps the whole group for group drag.
            selected_track_ = track;
            selected_clip_ = clip;
        }
        emit clipSelected(selected_track_, selected_clip_);

        Action::Clip* b = mutableClip(track, clip);
        if(!b)
        {
            return;
        }
        drag_track_ = track;
        drag_clip_ = clip;
        drag_origin_start_ = b->start_ms;
        drag_origin_end_ = b->end_ms;
        drag_grab_offset_ms_ = xToTime(x) - b->start_ms;
        drag_copy_ = false;
        drag_copy_source_track_ = -1;
        drag_dest_row_ = -1;
        drag_home_row_ = -1;
        drag_float_y_ = y;
        drag_grab_offset_y_ = 0;
        drag_span_anchor_row_ = -1;
        drag_span_hover_row_ = -1;
        beginDragGroupFromSelection();

        if(hit == ClipHit::LeftEdge)
        {
            drag_op_ = DragOp::ResizeStart;
            setCursor(Qt::SizeHorCursor);
        }
        else if(hit == ClipHit::RightEdge)
        {
            drag_op_ = DragOp::ResizeEnd;
            setCursor(Qt::SizeHorCursor);
        }
        else if((hit == ClipHit::TopEdge || hit == ClipHit::BottomEdge) && selection_.size() <= 1)
        {
            drag_op_ = DragOp::ResizeVertical;
            drag_span_anchor_row_ = hit_row;
            drag_span_hover_row_ = hit_row;
            setCursor(Qt::SizeVerCursor);
        }
        else
        {
            drag_op_ = DragOp::Move;
            // Ctrl+drag on a selection copies the group; plain drag moves.
            drag_copy_ = (event->modifiers() & Qt::ControlModifier) != 0;
            drag_dest_row_ = hit_row;
            drag_home_row_ = hit_row;
            const QRect home = clipRect(hit_row, *b);
            drag_grab_offset_y_ = y - home.top();
            drag_float_y_ = y;
            setCursor(Qt::ClosedHandCursor);
        }
        grabMouse();
        update();
        return;
    }

    // Empty timeline click: start a marquee selection box.
    selected_row_ = row;
    emit rowSelected(row);
    if(!(event->modifiers() & Qt::ControlModifier))
    {
        clearClipSelection();
        emit clipSelected(-1, -1);
    }
    const int ms = xToTime(x);
    playhead_ms_ = ms;
    emit playheadChanged(ms);
    drag_op_ = DragOp::Marquee;
    marquee_origin_ = QPoint(x, y);
    marquee_current_ = marquee_origin_;
    marquee_additive_ = (event->modifiers() & Qt::ControlModifier) != 0;
    grabMouse();
    update();
}

void ActionTimelineWidget::mouseMoveEvent(QMouseEvent* event)
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
        applyDrag(pt.x(), pt.y());
        return;
    }
    updateHoverCursor(pt.x(), pt.y());
}

void ActionTimelineWidget::mouseReleaseEvent(QMouseEvent* event)
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
                   && a.scene_zone_name == b.scene_zone_name)
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

void ActionTimelineWidget::leaveEvent(QEvent* event)
{
    if(drag_op_ == DragOp::None)
    {
        unsetCursor();
    }
    QWidget::leaveEvent(event);
}

void ActionTimelineWidget::wheelEvent(QWheelEvent* event)
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
