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


void EffectPackTimelineWidget::paintBlockSpatialRaster(QPainter& p, const QRect& br, const PaintBlock& pb,
                                                       const EffectPack::Block& sample) const
{
    {
        EffectPack::Block grad_sample = sample;
        EffectPack::EnsureBlockGradient(&grad_sample);
        QLinearGradient under(br.topLeft(), br.topRight());
        if(grad_sample.gradient.empty())
        {
            QColor c = RgbToQColor(grad_sample.color);
            c.setAlpha(220);
            under.setColorAt(0.0, c);
            under.setColorAt(1.0, c);
        }
        else
        {
            for(const EffectPack::GradientStop& s : grad_sample.gradient)
            {
                QColor c = RgbToQColor(s.color);
                c.setAlpha(200);
                under.setColorAt(std::clamp(s.pos, 0.0f, 1.0f), c);
            }
        }
        p.fillRect(br, under);
        p.fillRect(br, QColor(0, 0, 0, 140));
    }

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

    const int max_rows = pb.single_led_row ? 1 : std::max(2, br.height() / 2 + 1);
    const int skip = (led_n > max_rows) ? std::max(1, led_n / max_rows) : 1;
    const int rows = pb.single_led_row ? 1 : std::max(1, (led_n + skip - 1) / skip);

    QImage img(std::max(1, br.width()), rows, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const int dur = std::max(1, sample.end_ms - sample.start_ms);
    const float floor_i = std::clamp(sample.min_intensity, 0.0f, 1.0f);
    const bool twinkle = (sample.type == EffectPack::BlockType::Twinkle);
    const bool chase = (sample.type == EffectPack::BlockType::Chase
                        || sample.type == EffectPack::BlockType::Spin
                        || sample.type == EffectPack::BlockType::Orbit);
    const bool world_eval = EffectPack::BlockNeedsWorldEval(sample.type);

    auto sampleLed = [&](int led_slot, int ms, RGBColor* c, float* intens) -> bool {
        constexpr float k0 = 0.0f;
        constexpr float k1 = 1.0f;
        if(pb.single_led_row)
        {
            if(!order.empty() && pb.led_index >= 0 && pb.led_index < (int)axes.size())
            {
                const int idx = pb.led_index;
                if(world_eval && idx < (int)nxs.size())
                {
                    return EffectPack::EvaluateBlockAtWorld(sample, ms,
                                                            nxs[(size_t)idx], nys[(size_t)idx], nzs[(size_t)idx],
                                                            k0, k1, k0, k1, k0, k1,
                                                            seeds[(size_t)idx], c, intens);
                }
                return EffectPack::EvaluateBlockAtAxis(sample, ms,
                                                       axes[(size_t)idx],
                                                       seeds[(size_t)idx],
                                                       c, intens);
            }
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
            RGBColor c = 0;
            float intens = 0.0f;
            if(!sampleLed(led, sample_ms, &c, &intens))
            {
                continue;
            }
            if(twinkle && intens <= floor_i + 0.05f)
            {
                continue;
            }
            if(chase && intens < 0.12f)
            {
                continue;
            }
            if(intens < 0.02f)
            {
                continue;
            }
            QColor qc = RgbToQColor(c);
            qc.setAlpha(255);
            img.setPixelColor(x, row, qc);
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
        // Guard: QLinearGradient needs at least one stop.
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
    p.setClipRect(br, Qt::IntersectClip);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 8, 10));
    p.drawRect(br);

    switch(sample.type)
    {
        case EffectPack::BlockType::Solid:
        case EffectPack::BlockType::Fade:
        case EffectPack::BlockType::Pulse:
        case EffectPack::BlockType::Strobe:
        case EffectPack::BlockType::Candle:
            paintBlockGradientBar(p, br, sample, alpha);
            break;
        case EffectPack::BlockType::Wipe:
        case EffectPack::BlockType::Chase:
        case EffectPack::BlockType::Twinkle:
        case EffectPack::BlockType::ColorWash:
        case EffectPack::BlockType::Alternating:
        case EffectPack::BlockType::Spin:
        case EffectPack::BlockType::Dissolve:
        case EffectPack::BlockType::Plasma:
        case EffectPack::BlockType::Snow:
        case EffectPack::BlockType::Fire:
        case EffectPack::BlockType::Balls:
        case EffectPack::BlockType::Bars:
        case EffectPack::BlockType::SphereWipe:
        case EffectPack::BlockType::Orbit:
        case EffectPack::BlockType::Ripple:
        case EffectPack::BlockType::Meteor:
        case EffectPack::BlockType::Noise3D:
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

