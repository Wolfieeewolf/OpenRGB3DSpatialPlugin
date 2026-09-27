// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackTimelineWidget.h"
#include "EffectPackCatalog.h"
#include "EffectPacks/EffectPackApplier.h"
#include "EffectPacks/EffectScript.h"
#include "ZoneManager3D.h"

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

namespace
{

QColor RgbToQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

} // namespace


void EffectPackTimelineWidget::paintBlockSpatialRaster(QPainter& p, const QRect& br, const PaintBlock& pb,
                                                       const EffectPack::Block& sample) const
{
    // True black base so off / zero-intensity LEDs read as black (not tinted effect colour).
    p.fillRect(br, QColor(0, 0, 0));

    std::vector<float> axes;
    std::vector<int> seeds;
    std::vector<float> nxs, nys, nzs;
    if(pack_ && transforms_ && !pb.single_led_row)
    {
        EffectPack::BuildSpatialAxesForTarget(*pack_, pb.view_target, sample,
                                              transforms_, &axes, &seeds, &nxs, &nys, &nzs,
                                              zone_manager_);
    }

    std::vector<int> order;
    if(!axes.empty() && axes.size() == seeds.size())
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

    // Use full block height so multi-LED patterns stay readable; odd skip so
    // every-other-LED effects (alternating) are not aliased to a solid colour.
    const int max_rows = pb.single_led_row ? 1 : std::max(2, br.height());
    int skip = (led_n > max_rows) ? std::max(1, (led_n + max_rows - 1) / max_rows) : 1;
    if(skip > 1 && (skip % 2) == 0)
    {
        ++skip;
    }
    const int rows = pb.single_led_row ? 1 : std::max(1, (led_n + skip - 1) / skip);

    QImage img(std::max(1, br.width()), rows, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const int dur = std::max(1, sample.end_ms - sample.start_ms);
    const bool world_eval = EffectPack::EffectScriptUsesWorld(EffectPack::BlockFileId(sample));

    auto sampleLed = [&](int led_slot, int ms, RGBColor* c, float* intens) -> bool {
        constexpr float k0 = 0.0f;
        constexpr float k1 = 1.0f;
        if(pb.single_led_row)
        {
            // Expanded LED child under a parent track: evaluate this LED's slot in its
            // sibling strip so wipe/alternating match the hardware row.
            return EffectPack::EvaluateBlockAtLed(sample, ms, pb.led_index,
                                                  std::max(1, pb.led_count), c, intens);
        }
        if(!order.empty())
        {
            const int idx = order[(size_t)std::clamp(led_slot, 0, (int)order.size() - 1)];
            if(world_eval && idx < (int)nxs.size())
            {
                return EffectPack::EvaluateBlockAtWorld(sample, ms,
                                                        nxs[(size_t)idx], nys[(size_t)idx], nzs[(size_t)idx],
                                                        k0, k1, k0, k1, k0, k1,
                                                        seeds[(size_t)idx], c, intens);
            }
            return EffectPack::EvaluateBlockAtAxis(sample, ms,
                                                   axes[(size_t)idx], seeds[(size_t)idx],
                                                   c, intens);
        }
        if(world_eval)
        {
            const float t = (led_n <= 1) ? 0.5f : (float)led_slot / (float)(led_n - 1);
            return EffectPack::EvaluateBlockAtWorld(sample, ms, t, 0.5f, 0.5f,
                                                    k0, k1, k0, k1, k0, k1,
                                                    led_slot, c, intens);
        }
        return EffectPack::EvaluateBlockAtLed(sample, ms, led_slot, led_n, c, intens);
    };

    auto pixelFromSample = [&](RGBColor c, float intens) -> QColor {
        intens = std::clamp(intens, 0.0f, 1.0f);
        if(intens < 0.02f || c == 0)
        {
            return QColor(0, 0, 0);
        }
        QColor qc = RgbToQColor(c);
        qc.setAlpha(255);
        return qc;
    };

    for(int x = 0; x < img.width(); ++x)
    {
        const float t = (img.width() <= 1) ? 0.0f : (float)x / (float)(img.width() - 1);
        const int ms = sample.start_ms + (int)std::lround(t * (float)dur);
        const int sample_ms = std::min(std::max(ms, sample.start_ms), sample.end_ms - 1);

        int row = 0;
        for(int led = 0; led < led_n; led += skip, ++row)
        {
            if(row >= rows)
            {
                break;
            }
            RGBColor c0 = 0;
            float i0 = 0.0f;
            if(!sampleLed(led, sample_ms, &c0, &i0))
            {
                c0 = 0;
                i0 = 0.0f;
            }
            QColor a = pixelFromSample(c0, i0);

            // When several LEDs collapse into one preview row, also sample the neighbour
            // so alternating on/off does not bake into a single solid colour.
            if(skip > 1 && led + 1 < led_n)
            {
                RGBColor c1 = 0;
                float i1 = 0.0f;
                if(!sampleLed(led + 1, sample_ms, &c1, &i1))
                {
                    c1 = 0;
                    i1 = 0.0f;
                }
                const QColor b = pixelFromSample(c1, i1);
                if(a.rgb() != b.rgb())
                {
                    img.setPixelColor(x, row, ((x + row) & 1) ? a : b);
                    continue;
                }
            }
            img.setPixelColor(x, row, a);
        }
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(br, img);
}

void EffectPackTimelineWidget::paintBlockGradientBar(QPainter& p, const QRect& br,
                                                         const EffectPack::Block& block, int alpha) const
{
    EffectPack::Block sample = block;
    EffectPack::EnsureBlockGradient(&sample);
    QLinearGradient grad(QPointF(br.left(), br.top()), QPointF(br.right() + 1, br.top()));
    if(sample.gradient.empty())
    {
        QColor c = RgbToQColor(sample.color);
        c.setAlpha(alpha);
        grad.setColorAt(0.0, c);
        grad.setColorAt(1.0, c);
    }
    else if(sample.gradient.size() <= 2
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
        if(sample.gradient.empty())
        {
            QColor c = RgbToQColor(sample.color);
            c.setAlpha(alpha);
            grad.setColorAt(0.0, c);
            grad.setColorAt(1.0, c);
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRect(br);

    if(EffectPack::EffectPreviewPulse(EffectPack::BlockFileId(sample)) && br.width() > 8)
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

void EffectPackTimelineWidget::paintBlockIntensityCurve(QPainter& p, const QRect& br,
                                                        const EffectPack::Block& block) const
{
    if(block.intensity_curve.empty() || br.width() < 4)
    {
        return;
    }
    // Guide line only — colour/intensity are already baked by the temporal raster.
    const int steps = std::max(8, br.width());
    QPolygon line;
    line.reserve(steps);
    for(int i = 0; i < steps; ++i)
    {
        const float t = ((float)i + 0.5f) / (float)steps;
        const float level = std::clamp(EffectPack::SampleCurve(block.intensity_curve, t), 0.0f, 1.0f);
        const int x0 = br.left() + (i * br.width()) / steps;
        const int y = br.bottom() - (int)std::lround(level * (float)(br.height() - 1));
        line << QPoint(x0, y);
    }
    p.setPen(QPen(QColor(255, 255, 255, 180), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);
}

void EffectPackTimelineWidget::paintBlockVisual(QPainter& p, const QRect& br, const PaintBlock& pb, bool selected) const
{
    if(!pb.block || br.width() < 2 || br.height() < 2)
    {
        return;
    }
    EffectPack::Block sample = *pb.block;
    EffectPack::EnsureBlockGradient(&sample);

    p.save();
    p.setClipRect(br, Qt::IntersectClip);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 8, 10));
    p.drawRect(br);

    paintBlockSpatialRaster(p, br, pb, sample);

    {
        const int grip = std::min(3, std::max(2, br.width() / 10));
        p.fillRect(br.left(), br.top(), grip, br.height(), QColor(255, 255, 255, 28));
        p.fillRect(br.right() - grip + 1, br.top(), grip, br.height(), QColor(255, 255, 255, 28));
    }

    paintBlockIntensityCurve(p, br, sample);

    p.setBrush(Qt::NoBrush);
    p.setPen(selected ? QColor(255, 220, 80) : QColor(70, 70, 78));
    p.drawRect(br.adjusted(0, 0, -1, -1));
    p.restore();
}

void EffectPackTimelineWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(32, 32, 36));

    p.fillRect(0, 0, gutter_width_, height(), QColor(40, 40, 44));
    p.fillRect(gutter_width_, 0, width() - gutter_width_, header_height_, QColor(45, 45, 50));

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
        const int second_px = timeToX(1000) - timeToX(0);
        p.setPen(QColor(255, 255, 255, 28));
        for(int sec = 1; sec * 1000 <= duration_ms_; ++sec)
        {
            const int x = timeToX(sec * 1000);
            p.drawLine(x, y, x, y + row_height_);
        }
        if(second_px >= 48)
        {
            p.setPen(QColor(255, 255, 255, 14));
            for(int half = 1; half * 500 <= duration_ms_; half += 2)
            {
                const int x = timeToX(half * 500);
                p.drawLine(x, y, x, y + row_height_);
            }
        }
        p.setPen(QColor(55, 55, 60));
        p.drawLine(0, y + row_height_ - 1, width(), y + row_height_ - 1);

        if(r.depth == 0 || r.target.kind == EffectPack::TargetKind::SceneZone)
        {
            QColor accent = QColor(90, 140, 220);
            if(r.target.kind == EffectPack::TargetKind::All)
            {
                accent = QColor(220, 160, 60);
            }
            else if(r.target.kind == EffectPack::TargetKind::SceneZone)
            {
                accent = QColor(120, 200, 140);
            }
            p.fillRect(0, y, 3, row_height_, accent);
        }
        if(row_reorder_from_ >= 0 && row == row_reorder_hover_ && row != row_reorder_from_)
        {
            p.fillRect(0, y, gutter_width_, 2, QColor(255, 220, 80));
        }
        if(drag_op_ == DragOp::Move && drag_moved_ && drag_dest_row_ == row)
        {
            p.fillRect(gutter_width_, y, width() - gutter_width_, row_height_, QColor(120, 180, 255, 28));
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
            // Hide the live block while floating — it is drawn under the pointer instead.
            if(drag_op_ == DragOp::Move && drag_moved_
               && pb.track == drag_track_ && pb.block_index == drag_block_)
            {
                continue;
            }
            // Hide while vertical-resize paints one continuous span preview.
            if(drag_op_ == DragOp::ResizeVertical
               && pb.track == drag_track_ && pb.block_index == drag_block_)
            {
                continue;
            }
            // Paint on every covered row (parent + LED children). A single tall
            // draw on the parent is covered by later opaque row backgrounds.
            const bool selected = isBlockSelected(pb.track, pb.block_index);
            paintBlockVisual(p, blockRect(row, *pb.block), pb, selected);
        }
    }

    // Origin ghost + floating block during Move.
    if(drag_op_ == DragOp::Move && drag_moved_
       && drag_track_ >= 0 && drag_block_ >= 0 && pack_
       && drag_track_ < (int)pack_->tracks.size()
       && drag_block_ < (int)pack_->tracks[(size_t)drag_track_].blocks.size())
    {
        const EffectPack::Block& b = pack_->tracks[(size_t)drag_track_].blocks[(size_t)drag_block_];
        if(drag_home_row_ >= 0 && drag_home_row_ < visible_rows_.size())
        {
            EffectPack::Block origin = b;
            origin.start_ms = drag_origin_start_;
            origin.end_ms = drag_origin_end_;
            const QRect home = blockRect(drag_home_row_, origin);
            p.fillRect(home, QColor(255, 255, 255, 18));
            p.setPen(QPen(QColor(200, 200, 210, 140), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(home.adjusted(0, 0, -1, -1));
        }

        PaintBlock pb;
        pb.block = &b;
        pb.track = drag_track_;
        pb.block_index = drag_block_;
        const QRect live = blockRect(0, b);
        int float_top = drag_float_y_ - drag_grab_offset_y_;
        const int min_top = header_height_ + 3;
        const int row_count = (int)visible_rows_.size();
        const int max_top = header_height_ + std::max(0, row_count) * row_height_ - (row_height_ - 3);
        float_top = std::clamp(float_top, min_top, std::max(min_top, max_top));
        const QRect floating(live.x(), float_top, live.width(), row_height_ - 6);
        p.setOpacity(0.92);
        paintBlockVisual(p, floating, pb, true);
        p.setOpacity(1.0);
        p.setPen(QPen(QColor(140, 200, 255, 220), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(floating.adjusted(0, 0, -1, -1));
    }

    // Vertical span preview: one continuous block covering the track rows.
    if(drag_op_ == DragOp::ResizeVertical
       && drag_span_anchor_row_ >= 0 && drag_span_hover_row_ >= 0
       && drag_track_ >= 0 && drag_block_ >= 0 && pack_
       && drag_track_ < (int)pack_->tracks.size()
       && drag_block_ < (int)pack_->tracks[(size_t)drag_track_].blocks.size())
    {
        const EffectPack::Block& b = pack_->tracks[(size_t)drag_track_].blocks[(size_t)drag_block_];
        const int lo = std::min(drag_span_anchor_row_, drag_span_hover_row_);
        const int hi = std::max(drag_span_anchor_row_, drag_span_hover_row_);
        const int x0 = timeToX(b.start_ms);
        const int x1 = std::max(x0 + 4, timeToX(b.end_ms));
        const int y0 = header_height_ + lo * row_height_ + 3;
        const int y1 = header_height_ + (hi + 1) * row_height_ - 3;
        PaintBlock pb;
        pb.block = &b;
        pb.track = drag_track_;
        pb.block_index = drag_block_;
        pb.single_led_row = false;
        pb.led_index = 0;
        pb.led_count = std::max(1, hi - lo + 1);
        pb.view_target = pack_->tracks[(size_t)drag_track_].target;
        const QRect span(x0, y0, x1 - x0, y1 - y0);
        p.setOpacity(0.88);
        paintBlockVisual(p, span, pb, true);
        p.setOpacity(1.0);
        p.setPen(QPen(QColor(140, 200, 255, 220), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(span.adjusted(0, 0, -1, -1));
    }

    // Marquee rubber-band while dragging a selection box.
    if(drag_op_ == DragOp::Marquee)
    {
        const QRect box = QRect(marquee_origin_, marquee_current_).normalized();
        p.fillRect(box, QColor(120, 180, 255, 40));
        p.setPen(QPen(QColor(140, 200, 255, 220), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(box.adjusted(0, 0, -1, -1));
    }

    p.setPen(QPen(QColor(168, 168, 176), 2));
    p.drawLine(gutter_width_, 0, gutter_width_, height());

    const int px = timeToX(playhead_ms_);
    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(px, 0, px, height());
}

