// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackTimelineWidget.h"
#include "EffectPackCatalog.h"
#include "EffectPacks/EffectPackApplier.h"
#include "EffectPacks/EffectScript.h"
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


void EffectPackTimelineWidget::applyDrag(int mouse_x, int mouse_y)
{
    if(drag_op_ == DragOp::Marquee)
    {
        marquee_current_ = QPoint(mouse_x, mouse_y);
        drag_moved_ = true;
        update();
        return;
    }

    EffectPack::Block* block = mutableBlock(drag_track_, drag_block_);
    if(!block && drag_op_ != DragOp::ScrubHeader)
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
                block = mutableBlock(drag_track_, drag_block_);
                if(!block)
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
            applyDragGroupResizeStart(std::clamp(t, 0, std::max(0, drag_origin_end_ - min_block_ms_)));
            break;
        case DragOp::ResizeEnd:
            applyDragGroupResizeEnd(std::clamp(t, drag_origin_start_ + min_block_ms_, duration_ms_));
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

int EffectPackTimelineWidget::copyBlockToTrack(int src_track, int src_block, int dest_track, int start_ms)
{
    if(!pack_ || src_track < 0 || src_track >= (int)pack_->tracks.size()
       || dest_track < 0 || dest_track >= (int)pack_->tracks.size()
       || src_block < 0 || src_block >= (int)pack_->tracks[(size_t)src_track].blocks.size())
    {
        return -1;
    }
    EffectPack::Block copy = pack_->tracks[(size_t)src_track].blocks[(size_t)src_block];
    if(start_ms >= 0)
    {
        const int len = std::max(min_block_ms_, copy.end_ms - copy.start_ms);
        copy.start_ms = std::clamp(start_ms, 0, std::max(0, duration_ms_ - len));
        copy.end_ms = copy.start_ms + len;
    }
    pack_->tracks[(size_t)dest_track].blocks.push_back(std::move(copy));
    return (int)pack_->tracks[(size_t)dest_track].blocks.size() - 1;
}

bool EffectPackTimelineWidget::trackCoversRow(const EffectPack::Track& track, const Row& row) const
{
    if(EffectPack::TargetEquals(track.target, row.target))
    {
        return true;
    }
    if(!row.single_led_row || row.target.kind != EffectPack::TargetKind::Leds
       || row.target.led_indices.empty())
    {
        return false;
    }
    const int led = row.target.led_indices.front();
    // Multi-LED track spans each LED child row it includes.
    if(track.target.kind == EffectPack::TargetKind::Leds
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
    if(track.target.kind == EffectPack::TargetKind::Zone
       && track.target.device_name == row.target.device_name
       && (track.target.zone_name == row.target.zone_name
           || track.target.zone_name.empty() || row.target.zone_name.empty()))
    {
        return true;
    }
    if(track.target.kind == EffectPack::TargetKind::Device
       && track.target.device_name == row.target.device_name)
    {
        return true;
    }
    if(track.target.kind == EffectPack::TargetKind::All)
    {
        return true;
    }
    return false;
}

bool EffectPackTimelineWidget::coveredRowSpanForTrack(int track_index, int* out_lo, int* out_hi) const
{
    if(!pack_ || track_index < 0 || track_index >= (int)pack_->tracks.size() || !out_lo || !out_hi)
    {
        return false;
    }
    const EffectPack::Track& track = pack_->tracks[(size_t)track_index];
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

bool EffectPackTimelineWidget::expandBlockAcrossRows(int src_track, int src_block, int row_a, int row_b,
                                                    int* out_track, int* out_block)
{
    if(!pack_ || src_track < 0 || src_block < 0
       || src_track >= (int)pack_->tracks.size()
       || src_block >= (int)pack_->tracks[(size_t)src_track].blocks.size())
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
    EffectPack::Target broad_target;
    bool have_broad = false;

    for(int row = lo; row <= hi; ++row)
    {
        const Row& r = visible_rows_[row];
        if(r.target.kind == EffectPack::TargetKind::Leds && !r.target.led_indices.empty())
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
        else if(r.target.kind == EffectPack::TargetKind::Device
                || r.target.kind == EffectPack::TargetKind::Zone
                || r.target.kind == EffectPack::TargetKind::SceneZone
                || r.target.kind == EffectPack::TargetKind::All)
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

    EffectPack::Target new_target;
    // Vertical resize must stick on the spanned LED rows. Preferring the zone/device
    // parent moved the block onto that parent track and made it look like it snapped
    // back to a single row after release.
    if(leds.size() >= 2)
    {
        std::sort(leds.begin(), leds.end());
        leds.erase(std::unique(leds.begin(), leds.end()), leds.end());
        new_target.kind = EffectPack::TargetKind::Leds;
        new_target.device_name = device;
        new_target.zone_name = zone;
        new_target.led_indices = std::move(leds);
    }
    else if(leds.size() == 1 && !have_broad)
    {
        new_target.kind = EffectPack::TargetKind::Leds;
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
        new_target.kind = EffectPack::TargetKind::Leds;
        new_target.device_name = device;
        new_target.zone_name = zone;
        new_target.led_indices = std::move(leds);
    }
    else
    {
        return false;
    }

    EffectPack::Block moved = pack_->tracks[(size_t)src_track].blocks[(size_t)src_block];
    // Continuous wipe across the span (not per-LED Sequence loops).
    if(moved.axis_space == EffectPack::AxisSpace::Sequence)
    {
        moved.axis_space = EffectPack::AxisSpace::Device;
    }

    int dest_track = -1;
    for(int i = 0; i < (int)pack_->tracks.size(); ++i)
    {
        if(EffectPack::TargetEquals(pack_->tracks[(size_t)i].target, new_target))
        {
            dest_track = i;
            break;
        }
    }

    auto& src = pack_->tracks[(size_t)src_track];
    if(dest_track < 0 && src.blocks.size() == 1)
    {
        src.target = new_target;
        if(new_target.kind == EffectPack::TargetKind::Leds)
        {
            src.name = "LEDs (" + std::to_string(new_target.led_indices.size()) + ")";
        }
        dest_track = src_track;
        src.blocks[(size_t)src_block] = std::move(moved);
    }
    else if(dest_track < 0)
    {
        EffectPack::Track track;
        track.target = new_target;
        track.name = (new_target.kind == EffectPack::TargetKind::Leds)
            ? ("LEDs (" + std::to_string(new_target.led_indices.size()) + ")")
            : src.name;
        track.blocks.push_back(std::move(moved));
        pack_->tracks.push_back(std::move(track));
        dest_track = (int)pack_->tracks.size() - 1;
        auto& src_blocks = pack_->tracks[(size_t)src_track].blocks;
        src_blocks.erase(src_blocks.begin() + src_block);
    }
    else if(dest_track != src_track)
    {
        auto& src_blocks = pack_->tracks[(size_t)src_track].blocks;
        src_blocks.erase(src_blocks.begin() + src_block);
        pack_->tracks[(size_t)dest_track].blocks.push_back(std::move(moved));
    }
    else
    {
        pack_->tracks[(size_t)dest_track].blocks[(size_t)src_block] = std::move(moved);
    }

    int dest_block = -1;
    if(dest_track >= 0 && dest_track < (int)pack_->tracks.size())
    {
        dest_block = (int)pack_->tracks[(size_t)dest_track].blocks.size() - 1;
        // If we retargeted in place without moving, keep the same block index.
        if(dest_track == src_track && pack_->tracks[(size_t)dest_track].blocks.size() > (size_t)src_block)
        {
            // Prefer the block we just wrote when still present at src_block.
            if(src_block >= 0 && src_block < (int)pack_->tracks[(size_t)dest_track].blocks.size())
            {
                dest_block = src_block;
            }
        }
    }
    if(dest_block < 0)
    {
        return false;
    }

    const EffectPack::Block& keep = pack_->tracks[(size_t)dest_track].blocks[(size_t)dest_block];

    // Remove per-LED clone leftovers now covered by this span.
    for(int ti = (int)pack_->tracks.size() - 1; ti >= 0; --ti)
    {
        if(ti == dest_track)
        {
            continue;
        }
        auto& tr = pack_->tracks[(size_t)ti];
        if(tr.target.kind != EffectPack::TargetKind::Leds || tr.target.led_indices.empty())
        {
            continue;
        }
        bool covered = false;
        if(new_target.kind == EffectPack::TargetKind::Leds)
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
        else if(new_target.kind == EffectPack::TargetKind::Zone
                || new_target.kind == EffectPack::TargetKind::SceneZone)
        {
            covered = (tr.target.device_name == new_target.device_name
                       && tr.target.zone_name == new_target.zone_name);
        }
        else if(new_target.kind == EffectPack::TargetKind::Device)
        {
            covered = (tr.target.device_name == new_target.device_name);
        }
        else if(new_target.kind == EffectPack::TargetKind::All)
        {
            covered = true;
        }
        if(!covered)
        {
            continue;
        }
        for(int bi = (int)tr.blocks.size() - 1; bi >= 0; --bi)
        {
            const EffectPack::Block& b = tr.blocks[(size_t)bi];
            if(b.start_ms == keep.start_ms && b.end_ms == keep.end_ms && b.effect_id == keep.effect_id)
            {
                tr.blocks.erase(tr.blocks.begin() + bi);
            }
        }
        if(tr.blocks.empty())
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
    if(out_block)
    {
        *out_block = (dest_track >= 0 && dest_track < (int)pack_->tracks.size())
            ? std::min(dest_block, (int)pack_->tracks[(size_t)dest_track].blocks.size() - 1)
            : -1;
    }
    return dest_track >= 0;
}

void EffectPackTimelineWidget::clearBlockSelection()
{
    selection_.clear();
    selected_track_ = -1;
    selected_block_ = -1;
}

void EffectPackTimelineWidget::setPrimarySelection(int track, int block)
{
    selection_.clear();
    selected_track_ = track;
    selected_block_ = block;
    if(track >= 0 && block >= 0)
    {
        selection_.push_back({track, block});
    }
    update();
}

void EffectPackTimelineWidget::toggleBlockSelection(int track, int block)
{
    if(track < 0 || block < 0)
    {
        return;
    }
    for(int i = 0; i < selection_.size(); ++i)
    {
        if(selection_[i].track == track && selection_[i].block == block)
        {
            selection_.removeAt(i);
            if(selected_track_ == track && selected_block_ == block)
            {
                if(selection_.isEmpty())
                {
                    selected_track_ = -1;
                    selected_block_ = -1;
                }
                else
                {
                    selected_track_ = selection_.last().track;
                    selected_block_ = selection_.last().block;
                }
            }
            update();
            return;
        }
    }
    selection_.push_back({track, block});
    selected_track_ = track;
    selected_block_ = block;
    update();
}

bool EffectPackTimelineWidget::isBlockSelected(int track, int block) const
{
    for(const BlockId& id : selection_)
    {
        if(id.track == track && id.block == block)
        {
            return true;
        }
    }
    return false;
}

QRect EffectPackTimelineWidget::blockAbsRect(int track, int block_index) const
{
    if(!pack_ || track < 0 || block_index < 0
       || track >= (int)pack_->tracks.size()
       || block_index >= (int)pack_->tracks[(size_t)track].blocks.size())
    {
        return {};
    }
    const EffectPack::Block& b = pack_->tracks[(size_t)track].blocks[(size_t)block_index];
    for(int row = 0; row < visible_rows_.size(); ++row)
    {
        if(trackCoversRow(pack_->tracks[(size_t)track], visible_rows_[row]))
        {
            return blockRect(row, b);
        }
    }
    return {};
}

void EffectPackTimelineWidget::selectBlocksInRect(const QRect& rect, bool additive)
{
    if(!additive)
    {
        selection_.clear();
        selected_track_ = -1;
        selected_block_ = -1;
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
        for(int bi = 0; bi < (int)pack_->tracks[(size_t)ti].blocks.size(); ++bi)
        {
            const QRect br = blockAbsRect(ti, bi);
            if(br.isEmpty() || !br.intersects(area))
            {
                continue;
            }
            if(!isBlockSelected(ti, bi))
            {
                selection_.push_back({ti, bi});
            }
            selected_track_ = ti;
            selected_block_ = bi;
        }
    }
}

void EffectPackTimelineWidget::beginDragGroupFromSelection()
{
    drag_group_.clear();
    if(!pack_)
    {
        return;
    }
    auto add_member = [&](int track, int block) {
        if(track < 0 || block < 0
           || track >= (int)pack_->tracks.size()
           || block >= (int)pack_->tracks[(size_t)track].blocks.size())
        {
            return;
        }
        for(const DragMember& m : drag_group_)
        {
            if(m.track == track && m.block == block)
            {
                return;
            }
        }
        const EffectPack::Block& b = pack_->tracks[(size_t)track].blocks[(size_t)block];
        drag_group_.push_back({track, block, b.start_ms, b.end_ms});
    };
    if(selection_.isEmpty())
    {
        add_member(drag_track_, drag_block_);
    }
    else
    {
        for(const BlockId& id : selection_)
        {
            add_member(id.track, id.block);
        }
        add_member(drag_track_, drag_block_);
    }
}

void EffectPackTimelineWidget::forkDragGroupCopies()
{
    if(!pack_ || drag_group_.isEmpty())
    {
        return;
    }
    QVector<DragMember> copies;
    selection_.clear();
    for(const DragMember& m : drag_group_)
    {
        const int new_bi = copyBlockToTrack(m.track, m.block, m.track, m.origin_start);
        if(new_bi < 0)
        {
            continue;
        }
        copies.push_back({m.track, new_bi, m.origin_start, m.origin_end});
        selection_.push_back({m.track, new_bi});
        if(m.track == drag_track_ && m.block == drag_block_)
        {
            drag_block_ = new_bi;
            selected_track_ = m.track;
            selected_block_ = new_bi;
        }
    }
    drag_group_ = copies;
    if(selected_track_ < 0 && !selection_.isEmpty())
    {
        selected_track_ = selection_.front().track;
        selected_block_ = selection_.front().block;
        drag_track_ = selected_track_;
        drag_block_ = selected_block_;
    }
}

void EffectPackTimelineWidget::applyDragGroupTimeDelta(int primary_new_start)
{
    if(!pack_ || drag_group_.isEmpty())
    {
        return;
    }
    const int delta = primary_new_start - drag_origin_start_;
    for(const DragMember& m : drag_group_)
    {
        EffectPack::Block* b = mutableBlock(m.track, m.block);
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

void EffectPackTimelineWidget::applyDragGroupResizeStart(int primary_new_start)
{
    if(!pack_ || drag_group_.isEmpty())
    {
        return;
    }
    const int delta = primary_new_start - drag_origin_start_;
    for(const DragMember& m : drag_group_)
    {
        EffectPack::Block* b = mutableBlock(m.track, m.block);
        if(!b)
        {
            continue;
        }
        const int max_start = m.origin_end - min_block_ms_;
        b->start_ms = std::clamp(m.origin_start + delta, 0, std::max(0, max_start));
        b->end_ms = m.origin_end;
        if(b->end_ms - b->start_ms < min_block_ms_)
        {
            b->start_ms = b->end_ms - min_block_ms_;
        }
    }
}

void EffectPackTimelineWidget::applyDragGroupResizeEnd(int primary_new_end)
{
    if(!pack_ || drag_group_.isEmpty())
    {
        return;
    }
    const int delta = primary_new_end - drag_origin_end_;
    for(const DragMember& m : drag_group_)
    {
        EffectPack::Block* b = mutableBlock(m.track, m.block);
        if(!b)
        {
            continue;
        }
        const int min_end = m.origin_start + min_block_ms_;
        b->end_ms = std::clamp(m.origin_end + delta, min_end, duration_ms_);
        b->start_ms = m.origin_start;
    }
}

bool EffectPackTimelineWidget::deleteSelectedBlocks()
{
    if(!pack_ || selection_.isEmpty())
    {
        return false;
    }
    QVector<BlockId> ordered = selection_;
    std::sort(ordered.begin(), ordered.end(), [](const BlockId& a, const BlockId& b) {
        if(a.track != b.track)
        {
            return a.track > b.track;
        }
        return a.block > b.block;
    });
    for(const BlockId& id : ordered)
    {
        if(id.track < 0 || id.track >= (int)pack_->tracks.size())
        {
            continue;
        }
        auto& blocks = pack_->tracks[(size_t)id.track].blocks;
        if(id.block < 0 || id.block >= (int)blocks.size())
        {
            continue;
        }
        blocks.erase(blocks.begin() + id.block);
        if(blocks.empty())
        {
            pack_->tracks.erase(pack_->tracks.begin() + id.track);
        }
    }
    clearBlockSelection();
    update();
    emit blockEdited(-1, -1);
    return true;
}

bool EffectPackTimelineWidget::mutateSelectedBlocks(const std::function<void(EffectPack::Block&)>& mutator)
{
    if(!pack_ || selection_.isEmpty() || !mutator)
    {
        return false;
    }
    bool any = false;
    int primary_track = selected_track_;
    int primary_block = selected_block_;
    for(const BlockId& id : selection_)
    {
        EffectPack::Block* b = mutableBlock(id.track, id.block);
        if(!b)
        {
            continue;
        }
        mutator(*b);
        any = true;
        primary_track = id.track;
        primary_block = id.block;
    }
    if(!any)
    {
        return false;
    }
    update();
    emit blockEdited(primary_track, primary_block);
    return true;
}

void EffectPackTimelineWidget::copySelectedBlockToClipboard()
{
    clipboard_has_block_ = false;
    clipboard_has_multi_ = false;
    clipboard_multi_blocks_.clear();
    if(!pack_)
    {
        return;
    }
    if(selection_.size() > 1)
    {
        int min_start = std::numeric_limits<int>::max();
        for(const BlockId& id : selection_)
        {
            if(id.track < 0 || id.track >= (int)pack_->tracks.size()
               || id.block < 0 || id.block >= (int)pack_->tracks[(size_t)id.track].blocks.size())
            {
                continue;
            }
            const EffectPack::Block& b = pack_->tracks[(size_t)id.track].blocks[(size_t)id.block];
            min_start = std::min(min_start, b.start_ms);
            clipboard_multi_blocks_.push_back(b);
        }
        if(clipboard_multi_blocks_.isEmpty())
        {
            return;
        }
        for(EffectPack::Block& b : clipboard_multi_blocks_)
        {
            b.start_ms -= min_start;
            b.end_ms -= min_start;
        }
        clipboard_has_multi_ = true;
        clipboard_has_block_ = true;
        clipboard_block_ = clipboard_multi_blocks_.front();
        clipboard_has_row_ = false;
        return;
    }
    if(selected_track_ < 0 || selected_block_ < 0
       || selected_track_ >= (int)pack_->tracks.size()
       || selected_block_ >= (int)pack_->tracks[(size_t)selected_track_].blocks.size())
    {
        return;
    }
    clipboard_block_ = pack_->tracks[(size_t)selected_track_].blocks[(size_t)selected_block_];
    clipboard_has_block_ = true;
    clipboard_has_row_ = false;
}

bool EffectPackTimelineWidget::pasteClipboardBlockAt(int row, int ms)
{
    if(!clipboard_has_block_ || !pack_)
    {
        return false;
    }
    const int dest_track = findOrCreateTrackForRow(row >= 0 ? row : selected_row_);
    if(dest_track < 0)
    {
        return false;
    }
    if(clipboard_has_multi_ && !clipboard_multi_blocks_.isEmpty())
    {
        selection_.clear();
        int last = -1;
        for(const EffectPack::Block& src : clipboard_multi_blocks_)
        {
            EffectPack::Block copy = src;
            const int len = std::max(min_block_ms_, copy.end_ms - copy.start_ms);
            const int start = snapMs(std::clamp(ms + copy.start_ms, 0, std::max(0, duration_ms_ - len)));
            copy.start_ms = start;
            copy.end_ms = start + len;
            pack_->tracks[(size_t)dest_track].blocks.push_back(std::move(copy));
            last = (int)pack_->tracks[(size_t)dest_track].blocks.size() - 1;
            selection_.push_back({dest_track, last});
        }
        selected_track_ = dest_track;
        selected_block_ = last;
        selected_row_ = row;
        emit blockCreated(selected_track_, selected_block_);
        emit blockSelected(selected_track_, selected_block_);
        update();
        return true;
    }
    EffectPack::Block copy = clipboard_block_;
    const int len = std::max(min_block_ms_, copy.end_ms - copy.start_ms);
    const int start = snapMs(std::clamp(ms, 0, std::max(0, duration_ms_ - len)));
    copy.start_ms = start;
    copy.end_ms = start + len;
    pack_->tracks[(size_t)dest_track].blocks.push_back(std::move(copy));
    selected_track_ = dest_track;
    selected_block_ = (int)pack_->tracks[(size_t)dest_track].blocks.size() - 1;
    selected_row_ = row;
    setPrimarySelection(selected_track_, selected_block_);
    emit blockCreated(selected_track_, selected_block_);
    emit blockSelected(selected_track_, selected_block_);
    update();
    return true;
}

bool EffectPackTimelineWidget::duplicateSelectedBlock()
{
    if(!pack_)
    {
        return false;
    }
    if(selection_.size() > 1)
    {
        beginDragGroupFromSelection();
        // Snapshot origins then place copies after each original.
        QVector<BlockId> created;
        for(const BlockId& id : selection_)
        {
            if(id.track < 0 || id.track >= (int)pack_->tracks.size()
               || id.block < 0 || id.block >= (int)pack_->tracks[(size_t)id.track].blocks.size())
            {
                continue;
            }
            EffectPack::Block copy = pack_->tracks[(size_t)id.track].blocks[(size_t)id.block];
            const int len = std::max(min_block_ms_, copy.end_ms - copy.start_ms);
            copy.start_ms = std::clamp(copy.end_ms, 0, std::max(0, duration_ms_ - len));
            copy.end_ms = copy.start_ms + len;
            if(copy.end_ms > duration_ms_)
            {
                copy.end_ms = duration_ms_;
                copy.start_ms = std::max(0, copy.end_ms - len);
            }
            pack_->tracks[(size_t)id.track].blocks.push_back(std::move(copy));
            const int ni = (int)pack_->tracks[(size_t)id.track].blocks.size() - 1;
            created.push_back({id.track, ni});
        }
        selection_ = created;
        if(!selection_.isEmpty())
        {
            selected_track_ = selection_.last().track;
            selected_block_ = selection_.last().block;
            emit blockCreated(selected_track_, selected_block_);
            emit blockSelected(selected_track_, selected_block_);
        }
        drag_group_.clear();
        update();
        return !created.isEmpty();
    }
    if(selected_track_ < 0 || selected_block_ < 0
       || selected_track_ >= (int)pack_->tracks.size()
       || selected_block_ >= (int)pack_->tracks[(size_t)selected_track_].blocks.size())
    {
        return false;
    }
    EffectPack::Block copy = pack_->tracks[(size_t)selected_track_].blocks[(size_t)selected_block_];
    const int len = std::max(min_block_ms_, copy.end_ms - copy.start_ms);
    copy.start_ms = std::clamp(copy.end_ms, 0, std::max(0, duration_ms_ - len));
    copy.end_ms = copy.start_ms + len;
    if(copy.end_ms > duration_ms_)
    {
        copy.end_ms = duration_ms_;
        copy.start_ms = std::max(0, copy.end_ms - len);
    }
    pack_->tracks[(size_t)selected_track_].blocks.push_back(std::move(copy));
    selected_block_ = (int)pack_->tracks[(size_t)selected_track_].blocks.size() - 1;
    setPrimarySelection(selected_track_, selected_block_);
    emit blockCreated(selected_track_, selected_block_);
    emit blockSelected(selected_track_, selected_block_);
    update();
    return true;
}

void EffectPackTimelineWidget::copyRowEffectsToClipboard(int row)
{
    clipboard_has_row_ = false;
    clipboard_row_blocks_.clear();
    const int track = trackIndexForRow(row);
    if(!pack_ || track < 0 || track >= (int)pack_->tracks.size())
    {
        return;
    }
    for(const EffectPack::Block& b : pack_->tracks[(size_t)track].blocks)
    {
        clipboard_row_blocks_.push_back(b);
    }
    clipboard_has_row_ = !clipboard_row_blocks_.isEmpty();
    if(clipboard_has_row_)
    {
        clipboard_has_block_ = false;
    }
}

bool EffectPackTimelineWidget::pasteClipboardRowEffectsAt(int row)
{
    if(!clipboard_has_row_ || clipboard_row_blocks_.isEmpty() || !pack_)
    {
        return false;
    }
    const int dest_track = findOrCreateTrackForRow(row);
    if(dest_track < 0)
    {
        return false;
    }
    int last = -1;
    for(const EffectPack::Block& b : clipboard_row_blocks_)
    {
        pack_->tracks[(size_t)dest_track].blocks.push_back(b);
        last = (int)pack_->tracks[(size_t)dest_track].blocks.size() - 1;
    }
    if(last >= 0)
    {
        selected_track_ = dest_track;
        selected_block_ = last;
        selected_row_ = row;
        emit blockCreated(selected_track_, selected_block_);
        emit blockSelected(selected_track_, selected_block_);
    }
    update();
    return true;
}

int EffectPackTimelineWidget::findOrCreateTrackForRow(int row)
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
    EffectPack::Track track;
    track.name = r.label.toStdString();
    track.target = r.target;
    pack_->tracks.push_back(std::move(track));
    return (int)pack_->tracks.size() - 1;
}

bool EffectPackTimelineWidget::moveDragBlockToTrack(int dest_track)
{
    if(!pack_ || dest_track < 0 || dest_track >= (int)pack_->tracks.size()
       || drag_track_ < 0 || drag_track_ >= (int)pack_->tracks.size()
       || drag_block_ < 0)
    {
        return false;
    }
    if(dest_track == drag_track_)
    {
        return false;
    }

    auto& src_blocks = pack_->tracks[(size_t)drag_track_].blocks;
    if(drag_block_ >= (int)src_blocks.size())
    {
        return false;
    }

    EffectPack::Block moved = std::move(src_blocks[(size_t)drag_block_]);
    src_blocks.erase(src_blocks.begin() + drag_block_);
    pack_->tracks[(size_t)dest_track].blocks.push_back(std::move(moved));

    drag_track_ = dest_track;
    drag_block_ = (int)pack_->tracks[(size_t)dest_track].blocks.size() - 1;
    selected_track_ = drag_track_;
    selected_block_ = drag_block_;
    return true;
}

void EffectPackTimelineWidget::finishDrag()
{
    if(drag_op_ == DragOp::None)
    {
        return;
    }
    const DragOp op = drag_op_;
    int track = drag_track_;
    int block = drag_block_;
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
    drag_block_ = -1;
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
            selectBlocksInRect(box, marquee_add);
            emit blockSelected(selected_track_, selected_block_);
        }
        update();
        return;
    }

    // Cross-track move only for a single block; multi-select stays time-shifted on home tracks.
    if(moved && op == DragOp::Move && dest_row >= 0 && track >= 0 && block >= 0 && group_n <= 1)
    {
        const int dest_track = findOrCreateTrackForRow(dest_row);
        if(dest_track >= 0 && dest_track != track && dest_track != copy_src)
        {
            drag_track_ = track;
            drag_block_ = block;
            if(moveDragBlockToTrack(dest_track))
            {
                track = drag_track_;
                block = drag_block_;
                setPrimarySelection(track, block);
            }
            drag_track_ = -1;
            drag_block_ = -1;
        }
        selected_row_ = dest_row;
    }

    if(moved && op == DragOp::ResizeVertical && track >= 0 && block >= 0
       && span_a >= 0 && span_b >= 0 && span_a != span_b)
    {
        int out_track = track;
        int out_block = block;
        if(expandBlockAcrossRows(track, block, span_a, span_b, &out_track, &out_block))
        {
            setPrimarySelection(out_track, out_block);
            emit blockSelected(out_track, out_block);
            emit blockEdited(out_track, out_block);
        }
        update();
        return;
    }

    if(moved && (op == DragOp::Move || op == DragOp::ResizeStart || op == DragOp::ResizeEnd)
       && track >= 0 && block >= 0)
    {
        selected_track_ = track;
        selected_block_ = block;
        emit blockEdited(track, block);
        update();
    }
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
        if(hit == BlockHit::TopEdge || hit == BlockHit::BottomEdge)
        {
            setCursor(Qt::SizeVerCursor);
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
    const QList<EffectPackCatalog::Entry> entries = EffectPackCatalog::LoadEntries(effect_files_dir_);
    QHash<QString, QMenu*> menus;
    for(const QString& section : EffectPackCatalog::SectionOrder(entries))
    {
        menus.insert(section.toLower(), menu->addMenu(EffectPackCatalog::SectionLabel(section)));
    }
    for(const EffectPackCatalog::Entry& e : entries)
    {
        QMenu* dest = menus.value(e.section.toLower());
        if(!dest)
        {
            continue;
        }
        QAction* act = dest->addAction(EffectPackCatalog::MakeEffectIcon(e), e.name);
        connect(act, &QAction::triggered, this, [this, row, ms, id = e.id]() {
            emit effectAddRequested(row, ms, id);
        });
    }
}

void EffectPackTimelineWidget::showRowContextMenu(int row, int ms, const QPoint& global_pos)
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
    QAction* paste_block = menu.addAction(QStringLiteral("Paste effect\tCtrl+V"));
    paste_block->setEnabled(clipboard_has_block_);
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
    else if(chosen == paste_block)
    {
        pasteClipboardBlockAt(row, ms);
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

void EffectPackTimelineWidget::showBlockContextMenu(int row, int track, int block, const QPoint& global_pos)
{
    QMenu menu(this);
    QAction* preview_track = menu.addAction(QStringLiteral("Preview this track\tShift+Space"));
    QAction* play_block = menu.addAction(QStringLiteral("Play this block"));
    QAction* preview_pack = menu.addAction(QStringLiteral("Preview pack\tSpace"));
    menu.addSeparator();
    QAction* cut_act = menu.addAction(QStringLiteral("Cut\tCtrl+X"));
    QAction* copy_act = menu.addAction(QStringLiteral("Copy\tCtrl+C"));
    QAction* paste_act = menu.addAction(QStringLiteral("Paste\tCtrl+V"));
    paste_act->setEnabled(clipboard_has_block_);
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
    else if(chosen == play_block)
    {
        int start_ms = 0;
        if(pack_ && track >= 0 && track < (int)pack_->tracks.size()
           && block >= 0 && block < (int)pack_->tracks[(size_t)track].blocks.size())
        {
            start_ms = pack_->tracks[(size_t)track].blocks[(size_t)block].start_ms;
        }
        emit previewTrackRequested(track, start_ms);
    }
    else if(chosen == preview_pack)
    {
        emit previewPackRequested(0);
    }
    else if(chosen == cut_act)
    {
        if(!isBlockSelected(track, block))
        {
            setPrimarySelection(track, block);
        }
        copySelectedBlockToClipboard();
        deleteSelectedBlocks();
    }
    else if(chosen == copy_act)
    {
        if(!isBlockSelected(track, block))
        {
            setPrimarySelection(track, block);
        }
        copySelectedBlockToClipboard();
    }
    else if(chosen == paste_act)
    {
        const int ms = (pack_ && track >= 0 && track < (int)pack_->tracks.size()
                        && block >= 0 && block < (int)pack_->tracks[(size_t)track].blocks.size())
            ? pack_->tracks[(size_t)track].blocks[(size_t)block].end_ms
            : playhead_ms_;
        pasteClipboardBlockAt(row, ms);
    }
    else if(chosen == dup_act)
    {
        if(!isBlockSelected(track, block))
        {
            setPrimarySelection(track, block);
        }
        duplicateSelectedBlock();
    }
    else if(chosen == reverse_act || chosen == flip_h_act || chosen == flip_v_act
            || chosen == rotate_act || chosen == flip_dir_act)
    {
        if(!isBlockSelected(track, block))
        {
            setPrimarySelection(track, block);
        }
        if(chosen == reverse_act)
        {
            mutateSelectedBlocks([](EffectPack::Block& b) { b.reverse = !b.reverse; });
        }
        else if(chosen == flip_h_act)
        {
            mutateSelectedBlocks([](EffectPack::Block& b) { b.flip_h = !b.flip_h; });
        }
        else if(chosen == flip_v_act)
        {
            mutateSelectedBlocks([](EffectPack::Block& b) { b.flip_v = !b.flip_v; });
        }
        else if(chosen == rotate_act)
        {
            mutateSelectedBlocks([](EffectPack::Block& b) {
                b.rotate_quarters = (b.rotate_quarters + 1) % 4;
            });
        }
        else if(chosen == flip_dir_act)
        {
            mutateSelectedBlocks([](EffectPack::Block& b) {
                b.direction = EffectPack::OppositeDirection(b.direction);
            });
        }
    }
    else if(chosen == color_act)
    {
        editBlockColorAt(track, block, global_pos);
    }
    else if(chosen == del_act)
    {
        if(!isBlockSelected(track, block))
        {
            setPrimarySelection(track, block);
        }
        deleteSelectedBlocks();
    }
}

void EffectPackTimelineWidget::showHeaderContextMenu(const QPoint& global_pos)
{
    QMenu menu(this);
    QAction* preview_pack = menu.addAction(QStringLiteral("Preview pack\tSpace"));
    QAction* preview_from = menu.addAction(QStringLiteral("Preview from playhead"));
    menu.addSeparator();
    QAction* paste_act = menu.addAction(QStringLiteral("Paste at playhead\tCtrl+V"));
    paste_act->setEnabled(clipboard_has_block_ && selected_row_ >= 0);
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
        pasteClipboardBlockAt(selected_row_, playhead_ms_);
    }
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
    b->color_to = color;
    b->gradient = {{0.0f, color}};
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

    const QString effect_id = EffectPackCatalog::EffectIdFromMime(mime);
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

    const RGBColor current = EffectPack::EffectColorEnds(EffectPack::BlockFileId(*b)) ? b->color_from : b->color;
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
    const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
    if(ctrl && event->key() == Qt::Key_C)
    {
        copySelectedBlockToClipboard();
        event->accept();
        return;
    }
    if(ctrl && event->key() == Qt::Key_X)
    {
        if(!selection_.isEmpty())
        {
            copySelectedBlockToClipboard();
            deleteSelectedBlocks();
        }
        event->accept();
        return;
    }
    if(ctrl && event->key() == Qt::Key_V)
    {
        const int row = selected_row_ >= 0 ? selected_row_ : 0;
        pasteClipboardBlockAt(row, playhead_ms_);
        event->accept();
        return;
    }
    if(ctrl && event->key() == Qt::Key_D)
    {
        duplicateSelectedBlock();
        event->accept();
        return;
    }
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        if(!selection_.isEmpty())
        {
            deleteSelectedBlocks();
            event->accept();
            return;
        }
        if(selected_track_ >= 0 && selected_block_ >= 0)
        {
            emit blockDeleteRequested(selected_track_, selected_block_);
            event->accept();
            return;
        }
    }
    if(ctrl && event->key() == Qt::Key_A && pack_)
    {
        selection_.clear();
        for(int ti = 0; ti < (int)pack_->tracks.size(); ++ti)
        {
            for(int bi = 0; bi < (int)pack_->tracks[(size_t)ti].blocks.size(); ++bi)
            {
                selection_.push_back({ti, bi});
                selected_track_ = ti;
                selected_block_ = bi;
            }
        }
        emit blockSelected(selected_track_, selected_block_);
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
            clearBlockSelection();
            emit blockSelected(-1, -1);
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
            if(!isBlockSelected(track, block))
            {
                setPrimarySelection(track, block);
            }
            else
            {
                selected_track_ = track;
                selected_block_ = block;
            }
            emit rowSelected(hit_row);
            emit blockSelected(selected_track_, selected_block_);
            showBlockContextMenu(hit_row, track, block, event->globalPosition().toPoint());
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
    int block = -1;
    BlockHit hit = BlockHit::None;
    if(hitTestBlock(x, y, &hit_row, &track, &block, &hit))
    {
        selected_row_ = hit_row;
        emit rowSelected(hit_row);

        const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
        if(ctrl && hit == BlockHit::Body)
        {
            // Ctrl+click toggles multi-select like a file manager (no drag).
            toggleBlockSelection(track, block);
            emit blockSelected(selected_track_, selected_block_);
            update();
            return;
        }

        if(!isBlockSelected(track, block) || selection_.size() <= 1)
        {
            setPrimarySelection(track, block);
        }
        else
        {
            // Click on an already-selected block keeps the whole group for group drag.
            selected_track_ = track;
            selected_block_ = block;
        }
        emit blockSelected(selected_track_, selected_block_);

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
        drag_copy_ = false;
        drag_copy_source_track_ = -1;
        drag_dest_row_ = -1;
        drag_home_row_ = -1;
        drag_float_y_ = y;
        drag_grab_offset_y_ = 0;
        drag_span_anchor_row_ = -1;
        drag_span_hover_row_ = -1;
        beginDragGroupFromSelection();

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
        else if((hit == BlockHit::TopEdge || hit == BlockHit::BottomEdge) && selection_.size() <= 1)
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
            const QRect home = blockRect(hit_row, *b);
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
        clearBlockSelection();
        emit blockSelected(-1, -1);
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
        applyDrag(pt.x(), pt.y());
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
