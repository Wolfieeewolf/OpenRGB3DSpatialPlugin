// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackGradientBar.h"

#include <QColorDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace
{

QColor ToQc(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor FromQc(const QColor& c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

} // namespace

EffectPackGradientBar::EffectPackGradientBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(bar_h_ + marker_h_ + 8);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    stops_ = {
        {0.0f, ToRGBColor(255, 0, 0)},
        {1.0f, ToRGBColor(255, 255, 255)},
    };
}

void EffectPackGradientBar::setStops(std::vector<EffectPack::GradientStop> stops)
{
    stops_ = std::move(stops);
    if(stops_.empty())
    {
        stops_ = {{0.0f, ToRGBColor(255, 0, 0)}, {1.0f, ToRGBColor(255, 255, 255)}};
    }
    sortStops();
    update();
}

QSize EffectPackGradientBar::sizeHint() const
{
    return QSize(220, bar_h_ + marker_h_ + 8);
}

QSize EffectPackGradientBar::minimumSizeHint() const
{
    return QSize(120, bar_h_ + marker_h_ + 8);
}

QRect EffectPackGradientBar::barRect() const
{
    return QRect(6, 4, width() - 12, bar_h_);
}

float EffectPackGradientBar::xToPos(int x) const
{
    const QRect br = barRect();
    if(br.width() <= 1)
    {
        return 0.0f;
    }
    return std::clamp((float)(x - br.left()) / (float)br.width(), 0.0f, 1.0f);
}

int EffectPackGradientBar::posToX(float pos) const
{
    const QRect br = barRect();
    return br.left() + (int)std::lround(std::clamp(pos, 0.0f, 1.0f) * br.width());
}

void EffectPackGradientBar::sortStops()
{
    std::sort(stops_.begin(), stops_.end(),
              [](const EffectPack::GradientStop& a, const EffectPack::GradientStop& b) {
                  return a.pos < b.pos;
              });
}

int EffectPackGradientBar::hitTestStop(const QPoint& pos) const
{
    const QRect br = barRect();
    for(int i = 0; i < (int)stops_.size(); ++i)
    {
        const int x = posToX(stops_[(size_t)i].pos);
        const QRect hit(x - 7, br.bottom() - 2, 14, marker_h_ + 6);
        if(hit.contains(pos))
        {
            return i;
        }
    }
    return -1;
}

void EffectPackGradientBar::editStopColor(int index)
{
    if(index < 0 || index >= (int)stops_.size())
    {
        return;
    }
    const QColor picked = QColorDialog::getColor(ToQc(stops_[(size_t)index].color), this,
                                                 QStringLiteral("Gradient stop color"));
    if(!picked.isValid())
    {
        return;
    }
    stops_[(size_t)index].color = FromQc(picked);
    emit stopsChanged();
    update();
}

void EffectPackGradientBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRect br = barRect();

    QLinearGradient grad(br.topLeft(), br.topRight());
    if(stops_.size() == 1)
    {
        grad.setColorAt(0.0, ToQc(stops_.front().color));
        grad.setColorAt(1.0, ToQc(stops_.front().color));
    }
    else
    {
        for(const EffectPack::GradientStop& s : stops_)
        {
            grad.setColorAt(std::clamp(s.pos, 0.0f, 1.0f), ToQc(s.color));
        }
    }
    p.setPen(QColor(60, 60, 66));
    p.setBrush(grad);
    p.drawRoundedRect(br, 3, 3);

    for(int i = 0; i < (int)stops_.size(); ++i)
    {
        const int x = posToX(stops_[(size_t)i].pos);
        const int top = br.bottom() + 1;
        QPainterPath path;
        path.moveTo(x, top);
        path.lineTo(x - 6, top + 7);
        path.lineTo(x - 6, top + marker_h_);
        path.lineTo(x + 6, top + marker_h_);
        path.lineTo(x + 6, top + 7);
        path.closeSubpath();
        p.setPen(QColor(30, 30, 34));
        p.setBrush(QColor(160, 160, 168));
        if(i == drag_index_)
        {
            p.setBrush(QColor(220, 200, 90));
        }
        p.drawPath(path);
        // Color tip
        p.setBrush(ToQc(stops_[(size_t)i].color));
        p.drawRect(x - 4, top + marker_h_ - 5, 8, 4);
    }
}

void EffectPackGradientBar::mousePressEvent(QMouseEvent* event)
{
    const QPoint pt = event->position().toPoint();
    if(event->button() == Qt::RightButton)
    {
        const int hit = hitTestStop(pt);
        if(hit >= 0 && stops_.size() > 2)
        {
            stops_.erase(stops_.begin() + hit);
            emit stopsChanged();
            update();
        }
        return;
    }
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    drag_index_ = hitTestStop(pt);
    if(drag_index_ >= 0)
    {
        update();
        return;
    }
    const QRect br = barRect();
    if(br.adjusted(0, 0, 0, marker_h_ + 4).contains(pt))
    {
        EffectPack::GradientStop stop;
        stop.pos = xToPos(pt.x());
        // Sample current gradient colour at click
        EffectPack::Block tmp;
        tmp.gradient = stops_;
        stop.color = EffectPack::SampleGradient(tmp, stop.pos);
        stops_.push_back(stop);
        sortStops();
        drag_index_ = hitTestStop(pt);
        emit stopsChanged();
        update();
    }
}

void EffectPackGradientBar::mouseMoveEvent(QMouseEvent* event)
{
    if(drag_index_ < 0 || !(event->buttons() & Qt::LeftButton))
    {
        return;
    }
    if(drag_index_ >= (int)stops_.size())
    {
        return;
    }
    stops_[(size_t)drag_index_].pos = xToPos(event->position().toPoint().x());
    // Keep sorted while preserving the dragged stop identity (no nearest-pos hop).
    while(drag_index_ > 0
          && stops_[(size_t)drag_index_].pos < stops_[(size_t)drag_index_ - 1].pos)
    {
        std::swap(stops_[(size_t)drag_index_], stops_[(size_t)drag_index_ - 1]);
        --drag_index_;
    }
    while(drag_index_ + 1 < (int)stops_.size()
          && stops_[(size_t)drag_index_].pos > stops_[(size_t)drag_index_ + 1].pos)
    {
        std::swap(stops_[(size_t)drag_index_], stops_[(size_t)drag_index_ + 1]);
        ++drag_index_;
    }
    emit stopsChanged();
    update();
}

void EffectPackGradientBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton)
    {
        const bool was_dragging = drag_index_ >= 0;
        drag_index_ = -1;
        if(was_dragging)
        {
            sortStops();
            emit stopsChanged(); // sync primary colours after drag finishes
        }
        update();
    }
}

void EffectPackGradientBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int idx = hitTestStop(event->position().toPoint());
    if(idx >= 0)
    {
        editStopColor(idx);
    }
}
