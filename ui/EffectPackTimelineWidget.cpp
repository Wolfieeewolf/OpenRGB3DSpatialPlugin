// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackTimelineWidget.h"
#include "EffectPacks/EffectPackApplier.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

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
        default:
            return false;
    }
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

void EffectPackTimelineWidget::setRows(const QVector<Row>& rows)
{
    rows_ = rows;
    updateGeometrySize();
    emit contentHeightChanged(contentHeight());
    update();
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
    if(!pack_ || row < 0 || row >= rows_.size())
    {
        return -1;
    }
    const EffectPack::Target& target = rows_[row].target;
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
    return (int)std::lround((ms / 1000.0) * pixels_per_second_);
}

int EffectPackTimelineWidget::xToTime(int x) const
{
    const int ms = (int)std::lround((x / pixels_per_second_) * 1000.0);
    return std::clamp(ms, 0, duration_ms_);
}

int EffectPackTimelineWidget::contentWidth() const
{
    return std::max(200, timeToX(duration_ms_) + 40);
}

int EffectPackTimelineWidget::contentHeight() const
{
    return header_height_ + rows_.size() * row_height_ + 8;
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
    return QSize(320, std::max(120, contentHeight()));
}

bool EffectPackTimelineWidget::hitTestBlock(int x, int y, int* out_row, int* out_track, int* out_block) const
{
    if(y < header_height_ || !pack_)
    {
        return false;
    }
    const int row = (y - header_height_) / row_height_;
    if(row < 0 || row >= rows_.size())
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
        const int x0 = timeToX(block.start_ms);
        const int x1 = timeToX(block.end_ms);
        if(x >= x0 && x <= x1)
        {
            if(out_row) *out_row = row;
            if(out_track) *out_track = track;
            if(out_block) *out_block = b;
            return true;
        }
    }
    return false;
}

void EffectPackTimelineWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(32, 32, 36));

    // Header / ruler
    p.fillRect(0, 0, width(), header_height_, QColor(45, 45, 50));
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

    // Rows
    for(int row = 0; row < rows_.size(); ++row)
    {
        const int y = header_height_ + row * row_height_;
        const QColor bg = (row % 2 == 0) ? QColor(38, 38, 42) : QColor(34, 34, 38);
        p.fillRect(0, y, width(), row_height_, bg);
        p.setPen(QColor(55, 55, 60));
        p.drawLine(0, y + row_height_ - 1, width(), y + row_height_ - 1);

        const int track = trackIndexForRow(row);
        if(pack_ && track >= 0 && track < (int)pack_->tracks.size())
        {
            const auto& blocks = pack_->tracks[(size_t)track].blocks;
            for(int b = 0; b < (int)blocks.size(); ++b)
            {
                const EffectPack::Block& block = blocks[(size_t)b];
                const int x0 = timeToX(block.start_ms);
                const int x1 = std::max(x0 + 4, timeToX(block.end_ms));
                QColor fill = BlockFillColor(block);
                fill.setAlpha(200);
                const QRect br(x0, y + 3, x1 - x0, row_height_ - 6);
                p.fillRect(br, fill);
                const bool selected = (track == selected_track_ && b == selected_block_);
                p.setPen(selected ? QColor(255, 220, 80) : QColor(20, 20, 20));
                p.drawRect(br.adjusted(0, 0, -1, -1));
            }
        }
    }

    // Playhead
    const int px = timeToX(playhead_ms_);
    p.setPen(QPen(QColor(255, 80, 80), 2));
    p.drawLine(px, 0, px, height());
}

void EffectPackTimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    const int x = event->position().toPoint().x();
    const int y = event->position().toPoint().y();

    if(y < header_height_)
    {
        playhead_ms_ = xToTime(x);
        emit playheadChanged(playhead_ms_);
        update();
        return;
    }

    int row = 0;
    int track = -1;
    int block = -1;
    if(hitTestBlock(x, y, &row, &track, &block))
    {
        selected_track_ = track;
        selected_block_ = block;
        emit blockSelected(track, block);
        update();
        return;
    }

    row = (y - header_height_) / row_height_;
    if(row >= 0 && row < rows_.size())
    {
        const int ms = xToTime(x);
        playhead_ms_ = ms;
        emit playheadChanged(ms);
        emit emptyCellClicked(row, ms);
        update();
    }
}

void EffectPackTimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    if(event->buttons() & Qt::LeftButton)
    {
        if(event->position().toPoint().y() < header_height_)
        {
            playhead_ms_ = xToTime(event->position().toPoint().x());
            emit playheadChanged(playhead_ms_);
            update();
        }
    }
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
