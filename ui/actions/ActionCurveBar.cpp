// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionCurveBar.h"

#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

ActionCurveBar::ActionCurveBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setMinimumHeight(72);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip(QStringLiteral(
        "Drag points to adjust · double-click empty space to add a point · double-click a point to remove.\n"
        "Select a point then press Delete / Backspace to remove it. Right-click for the context menu."));
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        showContextMenu(pos);
    });
    points_ = {{0.0f, 1.0f}, {1.0f, 1.0f}};
}

void ActionCurveBar::setPoints(std::vector<Action::CurvePoint> points)
{
    points_ = std::move(points);
    drag_index_ = -1;
    selected_index_ = -1;
    ensureEndpoints();
    sortPoints();
    update();
}

QSize ActionCurveBar::sizeHint() const
{
    return QSize(220, 72);
}

QSize ActionCurveBar::minimumSizeHint() const
{
    return QSize(120, 56);
}

QRect ActionCurveBar::plotRect() const
{
    return QRect(8, 6, width() - 16, height() - 12);
}

float ActionCurveBar::xToPos(int x) const
{
    const QRect pr = plotRect();
    if(pr.width() <= 1)
    {
        return 0.0f;
    }
    return std::clamp((float)(x - pr.left()) / (float)pr.width(), 0.0f, 1.0f);
}

float ActionCurveBar::yToValue(int y) const
{
    const QRect pr = plotRect();
    if(pr.height() <= 1)
    {
        return 1.0f;
    }
    return std::clamp(1.0f - (float)(y - pr.top()) / (float)pr.height(), 0.0f, 1.0f);
}

int ActionCurveBar::posToX(float pos) const
{
    const QRect pr = plotRect();
    return pr.left() + (int)std::lround(std::clamp(pos, 0.0f, 1.0f) * pr.width());
}

int ActionCurveBar::valueToY(float value) const
{
    const QRect pr = plotRect();
    return pr.top() + (int)std::lround((1.0f - std::clamp(value, 0.0f, 1.0f)) * pr.height());
}

void ActionCurveBar::sortPoints()
{
    std::sort(points_.begin(), points_.end(),
              [](const Action::CurvePoint& a, const Action::CurvePoint& b) {
                  return a.pos < b.pos;
              });
}

void ActionCurveBar::ensureEndpoints()
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

int ActionCurveBar::hitTestPoint(const QPoint& pos) const
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

void ActionCurveBar::paintEvent(QPaintEvent*)
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
        for(const Action::CurvePoint& pt : points_)
        {
            poly << QPointF(posToX(pt.pos), valueToY(pt.value));
        }
        poly << QPointF(pr.right(), pr.bottom());
        p.setBrush(QColor(80, 140, 220, 50));
        p.setPen(Qt::NoPen);
        p.drawPolygon(poly);

        QPolygon line;
        for(const Action::CurvePoint& pt : points_)
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
        const bool active = (i == drag_index_ || i == selected_index_);
        p.setBrush(active ? QColor(255, 220, 80) : QColor(230, 230, 240));
        p.setPen(QPen(QColor(20, 20, 24), 1));
        p.drawEllipse(QPoint(x, y), 5, 5);
    }
}

void ActionCurveBar::mousePressEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    setFocus(Qt::MouseFocusReason);
    drag_index_ = hitTestPoint(event->position().toPoint());
    if(drag_index_ >= 0)
    {
        selected_index_ = drag_index_;
        setCursor(Qt::ClosedHandCursor);
        update();
    }
    else
    {
        selected_index_ = -1;
        update();
    }
}

void ActionCurveBar::mouseMoveEvent(QMouseEvent* event)
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
    Action::CurvePoint& cp = points_[(size_t)drag_index_];
    cp.value = yToValue(pt.y());
    // Keep endpoints pinned to 0/1 on the time axis so the curve always spans the clip.
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

void ActionCurveBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    const bool was = drag_index_ >= 0;
    if(was)
    {
        selected_index_ = drag_index_;
    }
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

void ActionCurveBar::mouseDoubleClickEvent(QMouseEvent* event)
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
        selected_index_ = -1;
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

void ActionCurveBar::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        if(selected_index_ > 0 && selected_index_ < (int)points_.size() - 1)
        {
            points_.erase(points_.begin() + selected_index_);
            selected_index_ = -1;
            sortPoints();
            ensureEndpoints();
            emit pointsChanged();
            update();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void ActionCurveBar::showContextMenu(const QPoint& pos)
{
    const int hit = hitTestPoint(pos);
    if(hit >= 0)
    {
        selected_index_ = hit;
        update();
    }
    QMenu menu(this);
    const bool can_remove = selected_index_ > 0
                          && selected_index_ < (int)points_.size() - 1;
    QAction* remove_act = menu.addAction(QStringLiteral("Remove point\tDel"));
    remove_act->setEnabled(can_remove);
    menu.addSeparator();
    QAction* reset_act = menu.addAction(QStringLiteral("Reset to flat"));
    QAction* chosen = menu.exec(mapToGlobal(pos));
    if(chosen == remove_act && can_remove)
    {
        points_.erase(points_.begin() + selected_index_);
        selected_index_ = -1;
        sortPoints();
        ensureEndpoints();
        emit pointsChanged();
        update();
    }
    else if(chosen == reset_act)
    {
        points_ = {{0.0f, 1.0f}, {1.0f, 1.0f}};
        selected_index_ = -1;
        drag_index_ = -1;
        emit pointsChanged();
        update();
    }
}
