// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackTimelineWidget.h"
#include "EffectPacks/EffectPackApplier.h"

#include <QColorDialog>
#include <QCursor>
#include <QEvent>
#include <QKeyEvent>
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
    // Exact target only (device / zone / LED). Like Vixen/xLights: an effect placed on a
    // parent does not ghost onto finer rows — granularity stays where you authored it.
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

void EffectPackTimelineWidget::paintBlockGradientBar(QPainter& p, const QRect& br,
                                                         const EffectPack::Block& block, int alpha) const
{
    EffectPack::Block sample = block;
    EffectPack::EnsureBlockGradient(&sample);
    // Vixen IntentRasterizer: full-height horizontal color gradient across the mark.
    QLinearGradient grad(br.topLeft(), br.topRight());
    if(sample.gradient.empty())
    {
        QColor c = RgbToQColor(sample.color);
        c.setAlpha(alpha);
        grad.setColorAt(0.0, c);
        grad.setColorAt(1.0, c);
    }
    else if(sample.type == EffectPack::BlockType::Solid && sample.gradient.size() <= 2
            && sample.gradient.front().color == sample.gradient.back().color)
    {
        QColor c = RgbToQColor(sample.gradient.front().color);
        c.setAlpha(alpha);
        grad.setColorAt(0.0, c);
        grad.setColorAt(1.0, c);
    }
    else
    {
        for(const EffectPack::GradientStop& s : sample.gradient)
        {
            QColor c = RgbToQColor(s.color);
            c.setAlpha(alpha);
            grad.setColorAt(std::clamp(s.pos, 0.0f, 1.0f), c);
        }
    }
    p.fillRect(br, grad);

    // Pulse: subtle period bands so the cycle still reads on solid-looking fills.
    if(sample.type == EffectPack::BlockType::Pulse && br.width() > 8)
    {
        const float speed = std::max(0.05f, sample.speed);
        const int period = std::max(1, (int)std::lround((float)std::max(1, sample.period_ms) / speed));
        const int cycles = std::max(1, (sample.end_ms - sample.start_ms) / period);
        for(int i = 0; i < cycles; ++i)
        {
            const float t0 = (float)i / (float)cycles;
            const float t1 = (float)(i + 1) / (float)cycles;
            const int x0 = br.left() + (int)std::lround(t0 * (float)br.width());
            const int x1 = br.left() + (int)std::lround(t1 * (float)br.width());
            const int mid = (x0 + x1) / 2;
            QLinearGradient pulse(mid, br.top(), mid, br.bottom());
            pulse.setColorAt(0.0, QColor(0, 0, 0, 0));
            pulse.setColorAt(0.5, QColor(255, 255, 255, 36));
            pulse.setColorAt(1.0, QColor(0, 0, 0, 0));
            p.fillRect(QRect(x0, br.top(), std::max(1, x1 - x0), br.height()), pulse);
        }
    }
}

void EffectPackTimelineWidget::paintBlockSpatialRaster(QPainter& p, const QRect& br, const PaintBlock& pb,
                                                       const EffectPack::Block& sample) const
{
    std::vector<float> axes;
    std::vector<int> seeds;
    if(pack_ && transforms_ && !pb.single_led_row)
    {
        EffectPack::BuildSpatialAxesForTarget(*pack_, pb.view_target, sample.direction,
                                              transforms_, &axes, &seeds);
    }

    // Sort by axis so wipe/chase form clean Vixen diagonals (Y = spatial order).
    std::vector<int> order;
    if(!axes.empty())
    {
        order.resize((int)axes.size());
        for(int i = 0; i < (int)order.size(); ++i)
        {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return axes[(size_t)a] < axes[(size_t)b];
        });
    }

    const int led_n = pb.single_led_row
        ? 1
        : (!order.empty() ? (int)order.size() : std::max(1, pb.led_count > 1 ? pb.led_count : 24));

    // Match Vixen EffectRasterizer: tmpsiz = height/2 + 1, skip elements when dense.
    const int max_rows = pb.single_led_row ? 1 : std::max(2, br.height() / 2 + 1);
    const int skip = (led_n > max_rows) ? (led_n / max_rows) : 1;
    const int rows = pb.single_led_row ? 1 : std::max(1, (led_n + skip - 1) / skip);
    const float row_h = (float)br.height() / (float)rows;

    // Sample ~every 2px like Vixen IntentRasterizer static chunks (~50ms).
    const int n_chunks = std::max(1, (br.width() + 1) / 2);
    const int dur = std::max(1, sample.end_ms - sample.start_ms);
    const float floor_i = std::clamp(sample.min_intensity, 0.0f, 1.0f);
    const bool twinkle = (sample.type == EffectPack::BlockType::Twinkle);

    auto sampleLed = [&](int led_slot, int ms, RGBColor* c, float* intens) -> bool {
        if(pb.single_led_row)
        {
            if(!axes.empty() && pb.led_index >= 0 && pb.led_index < (int)axes.size())
            {
                return EffectPack::EvaluateBlockAtAxis(sample, ms,
                                                       axes[(size_t)pb.led_index],
                                                       seeds[(size_t)pb.led_index],
                                                       c, intens);
            }
            return EffectPack::EvaluateBlockAtLed(sample, ms, pb.led_index,
                                                  std::max(1, pb.led_count), c, intens);
        }
        if(!order.empty())
        {
            const int idx = order[(size_t)led_slot];
            return EffectPack::EvaluateBlockAtAxis(sample, ms,
                                                   axes[(size_t)idx], seeds[(size_t)idx],
                                                   c, intens);
        }
        return EffectPack::EvaluateBlockAtLed(sample, ms, led_slot, led_n, c, intens);
    };

    auto toPaintColor = [&](RGBColor c, float /*intens*/) -> QColor {
        // Intensity is already baked into RGB by Evaluate*; keep mark opaque like Vixen.
        QColor qc = RgbToQColor(c);
        qc.setAlpha(255);
        return qc;
    };

    auto visible = [&](bool on, float intens) -> bool {
        if(!on)
        {
            return false;
        }
        // Twinkle: only paint flashes so the mark reads as sparse colored dashes.
        if(twinkle)
        {
            return intens > (floor_i + 0.08f);
        }
        // Chase tails: drop near-black tips so diagonals stay thin/crisp.
        if(sample.type == EffectPack::BlockType::Chase)
        {
            return intens > 0.18f;
        }
        return intens > 0.02f;
    };

    for(int row = 0, led = 0; row < rows && led < led_n; ++row, led += skip)
    {
        const float y0 = (float)br.top() + row_h * (float)row;
        const float y1 = (float)br.top() + row_h * (float)(row + 1);
        const QRectF row_rect(br.left(), y0, br.width(), std::max(1.0f, y1 - y0));

        for(int chunk = 0; chunk < n_chunks; ++chunk)
        {
            const float t0 = (float)chunk / (float)n_chunks;
            const float t1 = (float)(chunk + 1) / (float)n_chunks;
            const int ms0 = sample.start_ms + (int)std::lround(t0 * (float)dur);
            const int ms1 = sample.start_ms + (int)std::lround(t1 * (float)dur);
            const int s0 = std::min(ms0, sample.end_ms - 1);
            const int s1 = std::min(std::max(ms1 - 1, sample.start_ms), sample.end_ms - 1);

            RGBColor c0 = 0, c1 = 0;
            float i0 = 0.0f, i1 = 0.0f;
            const bool on0 = sampleLed(led, s0, &c0, &i0);
            const bool on1 = sampleLed(led, s1, &c1, &i1);
            const bool v0 = visible(on0, i0);
            const bool v1 = visible(on1, i1);
            if(!v0 && !v1)
            {
                continue;
            }

            const float x0 = (float)br.left() + t0 * (float)br.width();
            const float x1 = (float)br.left() + t1 * (float)br.width();
            QRectF cell(x0, row_rect.top(), std::max(1.0f, x1 - x0), row_rect.height());
            // Tiny inset avoids 1px bleed between stacked intent rows (Vixen gradient quirk).
            cell.adjust(0.0, 0.15, 0.0, -0.15);

            if(v0 && v1 && c0 == c1)
            {
                p.fillRect(cell, toPaintColor(c0, std::max(i0, i1)));
            }
            else if(v0 && v1)
            {
                QLinearGradient g(cell.topLeft(), cell.topRight());
                g.setColorAt(0.0, toPaintColor(c0, i0));
                g.setColorAt(1.0, toPaintColor(c1, i1));
                p.fillRect(cell, g);
            }
            else
            {
                p.fillRect(cell, toPaintColor(v0 ? c0 : c1, v0 ? i0 : i1));
            }
        }
    }
}

void EffectPackTimelineWidget::paintBlockVisual(QPainter& p, const QRect& br, const PaintBlock& pb, bool selected) const
{
    if(!pb.block || br.width() < 2 || br.height() < 2)
    {
        return;
    }
    EffectPack::Block sample = *pb.block;
    EffectPack::EnsureBlockGradient(&sample);

    const int alpha = 255;
    p.save();
    p.setClipRect(br);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Vixen marks sit on an opaque near-black plate; grid never shows through.
    p.fillRect(br, QColor(0, 0, 0));

    switch(sample.type)
    {
        case EffectPack::BlockType::Solid:
        case EffectPack::BlockType::Fade:
        case EffectPack::BlockType::Pulse:
            paintBlockGradientBar(p, br, sample, alpha);
            break;
        case EffectPack::BlockType::Wipe:
        case EffectPack::BlockType::Chase:
        case EffectPack::BlockType::Twinkle:
        case EffectPack::BlockType::ColorWash:
            paintBlockSpatialRaster(p, br, pb, sample);
            break;
        default:
            paintBlockGradientBar(p, br, sample, alpha);
            break;
    }

    {
        const int grip = std::min(3, std::max(2, br.width() / 10));
        p.fillRect(br.left(), br.top(), grip, br.height(), QColor(255, 255, 255, 28));
        p.fillRect(br.right() - grip + 1, br.top(), grip, br.height(), QColor(255, 255, 255, 28));
    }

    // Thin dark outline (gold when selected) — matches Vixen element chrome.
    p.setPen(selected ? QColor(255, 220, 80) : QColor(40, 40, 44));
    p.drawRect(br.adjusted(0, 0, -1, -1));
    p.restore();
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

void EffectPackTimelineWidget::showAddEffectMenu(int row, int ms, const QPoint& global_pos)
{
    QMenu menu(this);
    QMenu* add = menu.addMenu(QStringLiteral("Add effect"));
    const struct { const char* name; EffectPack::BlockType type; } items[] = {
        {"Set Level", EffectPack::BlockType::Solid},
        {"Fade", EffectPack::BlockType::Fade},
        {"Pulse", EffectPack::BlockType::Pulse},
        {"Wipe", EffectPack::BlockType::Wipe},
        {"Chase", EffectPack::BlockType::Chase},
        {"Twinkle", EffectPack::BlockType::Twinkle},
        {"ColorWash", EffectPack::BlockType::ColorWash},
    };
    for(const auto& it : items)
    {
        QAction* act = add->addAction(QString::fromUtf8(it.name));
        connect(act, &QAction::triggered, this, [this, row, ms, type = it.type]() {
            emit effectAddRequested(row, ms, (int)type);
        });
    }
    menu.exec(global_pos);
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

void EffectPackTimelineWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(32, 32, 36));

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
    p.drawText(8, header_height_ - 8, QStringLiteral("Pack scope"));

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
            const QColor accent = (r.target.kind == EffectPack::TargetKind::All)
                ? QColor(220, 160, 60) : QColor(90, 140, 220);
            p.fillRect(0, y, 3, row_height_, accent);
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

        const QVector<PaintBlock> blocks = paintBlocksForRow(row);
        for(const PaintBlock& pb : blocks)
        {
            if(!pb.block)
            {
                continue;
            }
            const bool selected = (pb.track == selected_track_ && pb.block_index == selected_block_);
            paintBlockVisual(p, blockRect(row, *pb.block), pb, selected);
        }
    }

    const int px = timeToX(playhead_ms_);
    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(px, 0, px, height());
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
