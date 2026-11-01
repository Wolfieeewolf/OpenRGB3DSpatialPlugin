// SPDX-License-Identifier: GPL-2.0-only

#include "ActionGradientBar.h"
#include "ActionCatalog.h"

#include <QColorDialog>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMenu>
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

ActionGradientBar::ActionGradientBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        showStopContextMenu(pos);
    });
    setToolTip(QStringLiteral(
        "Drop a colour to recolour the stop under the cursor. Ctrl+drop adds a stop.\n"
        "Remove stop: right-click → Delete stop, or Delete / Backspace when a stop is selected."));
    setMinimumHeight(bar_h_ + marker_h_ + 8);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    stops_ = {
        {0.0f, ToRGBColor(255, 0, 0)},
    };
}

void ActionGradientBar::setStops(std::vector<Action::GradientStop> stops)
{
    stops_ = std::move(stops);
    if(stops_.empty())
    {
        stops_ = {{0.0f, ToRGBColor(255, 0, 0)}};
    }
    sortStops();
    update();
}

QSize ActionGradientBar::sizeHint() const
{
    return QSize(220, bar_h_ + marker_h_ + 8);
}

QSize ActionGradientBar::minimumSizeHint() const
{
    return QSize(120, bar_h_ + marker_h_ + 8);
}

QRect ActionGradientBar::barRect() const
{
    return QRect(6, 4, width() - 12, bar_h_);
}

float ActionGradientBar::xToPos(int x) const
{
    const QRect br = barRect();
    if(br.width() <= 1)
    {
        return 0.0f;
    }
    return std::clamp((float)(x - br.left()) / (float)br.width(), 0.0f, 1.0f);
}

int ActionGradientBar::posToX(float pos) const
{
    const QRect br = barRect();
    return br.left() + (int)std::lround(std::clamp(pos, 0.0f, 1.0f) * br.width());
}

void ActionGradientBar::sortStops()
{
    std::sort(stops_.begin(), stops_.end(),
              [](const Action::GradientStop& a, const Action::GradientStop& b) {
                  return a.pos < b.pos;
              });
}

int ActionGradientBar::hitTestStop(const QPoint& pos) const
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

int ActionGradientBar::nearestStop(const QPoint& pos) const
{
    if(stops_.empty())
    {
        return -1;
    }
    int best = 0;
    int best_dist = std::abs(posToX(stops_.front().pos) - pos.x());
    for(int i = 1; i < (int)stops_.size(); ++i)
    {
        const int dist = std::abs(posToX(stops_[(size_t)i].pos) - pos.x());
        if(dist < best_dist)
        {
            best = i;
            best_dist = dist;
        }
    }
    return best;
}

void ActionGradientBar::dragEnterEvent(QDragEnterEvent* event)
{
    RGBColor color = 0;
    if(ActionCatalog::ColorFromMime(event->mimeData(), &color))
    {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ActionGradientBar::dragMoveEvent(QDragMoveEvent* event)
{
    RGBColor color = 0;
    if(ActionCatalog::ColorFromMime(event->mimeData(), &color))
    {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ActionGradientBar::dropEvent(QDropEvent* event)
{
    RGBColor color = 0;
    if(!ActionCatalog::ColorFromMime(event->mimeData(), &color))
    {
        event->ignore();
        return;
    }
    const QPoint pt = event->position().toPoint();
    if(event->modifiers() & Qt::ControlModifier)
    {
        Action::GradientStop stop;
        stop.pos = xToPos(pt.x());
        stop.color = color;
        stops_.push_back(stop);
        sortStops();
    }
    else
    {
        int index = hitTestStop(pt);
        if(index < 0)
        {
            index = nearestStop(pt);
        }
        if(index < 0)
        {
            event->ignore();
            return;
        }
        stops_[(size_t)index].color = color;
    }
    emit stopsChanged();
    update();
    event->acceptProposedAction();
}

int ActionGradientBar::selectedStopIndex() const
{
    if(selected_stop_index_ >= 0 && selected_stop_index_ < (int)stops_.size())
    {
        return selected_stop_index_;
    }
    return -1;
}

void ActionGradientBar::removeStopAt(int index)
{
    if(index < 0 || index >= (int)stops_.size() || stops_.size() <= 1)
    {
        return;
    }
    stops_.erase(stops_.begin() + index);
    if(selected_stop_index_ == index)
    {
        selected_stop_index_ = -1;
    }
    else if(selected_stop_index_ > index)
    {
        --selected_stop_index_;
    }
    if(drag_index_ == index)
    {
        drag_index_ = -1;
    }
    else if(drag_index_ > index)
    {
        --drag_index_;
    }
    emit stopsChanged();
    update();
}

void ActionGradientBar::showStopContextMenu(const QPoint& local_pos)
{
    const int hit = hitTestStop(local_pos);
    if(hit >= 0)
    {
        selected_stop_index_ = hit;
        update();
    }

    QMenu menu(this);
    QAction* delete_act = menu.addAction(QStringLiteral("Delete stop"));
    delete_act->setEnabled(stops_.size() > 1 && selectedStopIndex() >= 0);
    QAction* color_act = menu.addAction(QStringLiteral("Edit stop color…"));
    color_act->setEnabled(selectedStopIndex() >= 0);

    QAction* chosen = menu.exec(mapToGlobal(local_pos));
    if(chosen == delete_act)
    {
        removeStopAt(selectedStopIndex());
    }
    else if(chosen == color_act)
    {
        editStopColor(selectedStopIndex());
    }
}

void ActionGradientBar::keyPressEvent(QKeyEvent* event)
{
    if(!event)
    {
        QWidget::keyPressEvent(event);
        return;
    }
    if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        const int idx = selectedStopIndex();
        if(idx >= 0 && stops_.size() > 1)
        {
            removeStopAt(idx);
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void ActionGradientBar::editStopColor(int index)
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

void ActionGradientBar::paintEvent(QPaintEvent*)
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
        for(const Action::GradientStop& s : stops_)
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
        if(i == drag_index_ || i == selected_stop_index_)
        {
            p.setBrush(QColor(220, 200, 90));
        }
        p.drawPath(path);
        p.setBrush(ToQc(stops_[(size_t)i].color));
        p.drawRect(x - 4, top + marker_h_ - 5, 8, 4);
    }
}

void ActionGradientBar::mousePressEvent(QMouseEvent* event)
{
    const QPoint pt = event->position().toPoint();
    if(event->button() == Qt::RightButton)
    {
        return;
    }
    if(event->button() != Qt::LeftButton)
    {
        return;
    }
    setFocus(Qt::MouseFocusReason);
    drag_index_ = hitTestStop(pt);
    if(drag_index_ >= 0)
    {
        selected_stop_index_ = drag_index_;
        update();
        return;
    }
    const QRect br = barRect();
    if(br.adjusted(0, 0, 0, marker_h_ + 4).contains(pt))
    {
        Action::GradientStop stop;
        stop.pos = xToPos(pt.x());
        Action::Clip tmp;
        tmp.gradient = stops_;
        stop.color = Action::SampleGradient(tmp, stop.pos);
        stops_.push_back(stop);
        sortStops();
        drag_index_ = hitTestStop(pt);
        selected_stop_index_ = drag_index_;
        emit stopsChanged();
        update();
    }
}

void ActionGradientBar::mouseMoveEvent(QMouseEvent* event)
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

void ActionGradientBar::mouseReleaseEvent(QMouseEvent* event)
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

void ActionGradientBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int idx = hitTestStop(event->position().toPoint());
    if(idx >= 0)
    {
        editStopColor(idx);
    }
}
