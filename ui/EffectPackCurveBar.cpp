// SPDX-License-Identifier: GPL-2.0-only

#include "EffectPackCurveBar.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

EffectPackCurveBar::EffectPackCurveBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(72);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip(QStringLiteral("Drag points · double-click empty space to add · double-click a point to remove"));
    points_ = {{0.0f, 1.0f}, {1.0f, 1.0f}};
}

void EffectPackCurveBar::setPoints(std::vector<EffectPack::CurvePoint> points)
{
    points_ = std::move(points);
    ensureEndpoints();
    sortPoints();
    update();
}

QSize EffectPackCurveBar::sizeHint() const
{
    return QSize(220, 72);
}

QSize EffectPackCurveBar::minimumSizeHint() const
{
    return QSize(120, 56);
}

QRect EffectPackCurveBar::plotRect() const
{
    return QRect(8, 6, width() - 16, height() - 12);
}

float EffectPackCurveBar::xToPos(int x) const
{
    const QRect pr = plotRect();
    if(pr.width() <= 1)
    {
        return 0.0f;
    }
    return std::clamp((float)(x - pr.left()) / (float)pr.width(), 0.0f, 1.0f);
}

float EffectPackCurveBar::yToValue(int y) const
{
    const QRect pr = plotRect();
    if(pr.height() <= 1)
    {
        return 1.0f;
    }
    return std::clamp(1.0f - (float)(y - pr.top()) / (float)pr.height(), 0.0f, 1.0f);
}

int EffectPackCurveBar::posToX(float pos) const
{
    const QRect pr = plotRect();
    return pr.left() + (int)std::lround(std::clamp(pos, 0.0f, 1.0f) * pr.width());
}

int EffectPackCurveBar::valueToY(float value) const
{
    const QRect pr = plotRect();
    return pr.top() + (int)std::lround((1.0f - std::clamp(value, 0.0f, 1.0f)) * pr.height());
}

void EffectPackCurveBar::sortPoints()
{
    std::sort(points_.begin(), points_.end(),
              [](const EffectPack::CurvePoint& a, const EffectPack::CurvePoint& b) {
                  return a.pos < b.pos;
              });
}

void EffectPackCurveBar::ensureEndpoints()
{
    if(points_.empty())
    {
        points_ = {{0.0f, 1.0f}, {1.0f, 1.0f}};
        return;
    }
    sortPoints();
    if(points_.front().pos > 0.001f)
    {
        points_.insert(points_.begin(), {0.0f, points_.front().value});
    }
    else
    {
        points_.front().pos = 0.0f;
    }
    if(points_.back().pos < 0.999f)
    {
        points_.push_back({1.0f, points_.back().value});
    }
    else
    {
        points_.back().pos = 1.0f;
    }
}

int EffectPackCurveBar::hitTestPoint(const QPoint& pos) const
{
    for(int i = 0; i < (int)points_.size(); ++i)
    {
        const int x = posToX(points_[(size_t)i].pos);
        const int y = valueToY(points_[(size_t)i].value);
        if(QRect(x - 6, y - 6, 12, 12).contains(pos))
        {
            return i;
        }
    }
    return -1;
}

void EffectPackCurveBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRect pr = plotRect();
    p.fillRect(pr, QColor(28, 28, 32));
    p.setPen(QColor(55, 55, 62));
    p.drawRect(pr.adjusted(0, 0, -1, -1));

    if(points_.size() >= 2)
    {
        QPolygonF poly;
        poly << QPointF(pr.left(), pr.bottom());
        for(const EffectPack::CurvePoint& pt : points_)
        {
            poly << QPointF(posToX(pt.pos), valueToY(pt.value));
        }
        poly << QPointF(pr.right(), pr.bottom());
        p.setBrush(QColor(80, 140, 220, 50));
        p.setPen(Qt::NoPen);
        p.drawPolygon(poly);

        QPolygon line;
        for(const EffectPack::CurvePoint& pt : points_)
        {
            line << QPoint(posToX(pt.pos), valueToY(pt.value));
        }
        p.setPen(QPen(QColor(180, 210, 255), 2));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(line);
    }

    for(int i = 0; i < (int)points_.size(); ++i)
    {
        const int x = posToX(points_[(size_t)i].pos);
        const int y = valueToY(points_[(size_t)i].value);
        p.setBrush(i == drag_index_ ? QColor(255, 220, 80) : QColor(230, 230, 240));
        p.setPen(QPen(QColor(20, 20, 24), 1));
        p.drawEllipse(QPoint(x, y), 5, 5);
    }
}

void EffectPackCurveBar::mousePressEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    drag_index_ = hitTestPoint(event->position().toPoint());
    if(drag_index_ >= 0)
    {
        setCursor(Qt::ClosedHandCursor);
        update();
    }
}

void EffectPackCurveBar::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pt = event->position().toPoint();
    if(drag_index_ < 0 || !(event->buttons() & Qt::LeftButton))
    {
        setCursor(hitTestPoint(pt) >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    if(drag_index_ >= (int)points_.size())
    {
        return;
    }
    EffectPack::CurvePoint& cp = points_[(size_t)drag_index_];
    cp.value = yToValue(pt.y());
    // Keep endpoints pinned to 0/1 on the time axis so the curve always spans the block.
    if(drag_index_ == 0)
    {
        cp.pos = 0.0f;
    }
    else if(drag_index_ == (int)points_.size() - 1)
    {
        cp.pos = 1.0f;
    }
    else
    {
        cp.pos = xToPos(pt.x());
        const float lo = points_[(size_t)drag_index_ - 1].pos + 0.01f;
        const float hi = points_[(size_t)drag_index_ + 1].pos - 0.01f;
        cp.pos = std::clamp(cp.pos, lo, hi);
    }
    update();
}

void EffectPackCurveBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    const bool was = drag_index_ >= 0;
    drag_index_ = -1;
    unsetCursor();
    if(was)
    {
        sortPoints();
        ensureEndpoints();
        emit pointsChanged();
        update();
    }
}

void EffectPackCurveBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    const QPoint pt = event->position().toPoint();
    const int hit = hitTestPoint(pt);
    if(hit > 0 && hit < (int)points_.size() - 1)
    {
        points_.erase(points_.begin() + hit);
        emit pointsChanged();
        update();
        return;
    }
    if(hit < 0 && plotRect().contains(pt))
    {
        points_.push_back({xToPos(pt.x()), yToValue(pt.y())});
        sortPoints();
        ensureEndpoints();
        emit pointsChanged();
        update();
    }
}
