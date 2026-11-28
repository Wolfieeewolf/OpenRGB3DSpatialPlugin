// SPDX-License-Identifier: GPL-2.0-or-later

#include "MovieSyncPanel.h"
#include "PluginUiUtils.h"

#include <QDateTime>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

MovieSyncTimeline::MovieSyncTimeline(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip(tr("The whole title: orange ticks are one-off cues, blue bands are held looks, the strip "
                  "along the top is the service's intro (violet), recap (grey-blue) and credits (grey), "
                  "the line is where playback is."));
}

void MovieSyncTimeline::SetTimeline(double duration_s, const std::vector<MovieSync::StreamCue>& cues,
                                    std::int64_t shift_ms)
{
    duration_s_ = std::max(0.0, duration_s);
    cues_ = cues;
    shift_ms_ = shift_ms;
    update();
}

void MovieSyncTimeline::SetPosition(double position_s)
{
    if(std::abs(position_s - position_s_) < 0.05)
    {
        return;
    }
    position_s_ = position_s;
    update();
}

void MovieSyncTimeline::SetMarkers(const std::vector<MovieSync::StreamMarker>& markers)
{
    markers_ = markers;
    update();
}

void MovieSyncTimeline::Clear()
{
    duration_s_ = 0.0;
    position_s_ = 0.0;
    cues_.clear();
    markers_.clear();
    update();
}

QSize MovieSyncTimeline::sizeHint() const
{
    return QSize(240, 26);
}

QSize MovieSyncTimeline::minimumSizeHint() const
{
    return QSize(60, 26);
}

void MovieSyncTimeline::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    const QRectF r = QRectF(rect()).adjusted(1, 3, -1, -3);
    const QColor track = PluginUiBlendColors(pal.color(QPalette::Base), pal.color(QPalette::WindowText), 0.10f);
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor level_col(70, 170, 200);
    const QColor pulse_col(255, 160, 60);

    QPainterPath track_path;
    track_path.addRoundedRect(r, 4, 4);
    p.fillPath(track_path, track);
    if(duration_s_ <= 0.0)
    {
        return;
    }
    const auto x_of = [&](double s) { return r.left() + r.width() * std::clamp(s / duration_s_, 0.0, 1.0); };
    const double played_x = x_of(position_s_);

    /* Played part. */
    p.save();
    p.setClipPath(track_path);
    p.fillRect(QRectF(r.left(), r.top(), played_x - r.left(), r.height()),
               PluginUiBlendColors(track, accent, 0.35f));

    /* Held looks: bands in the lower half. */
    const double shift_s = (double)shift_ms_ / 1000.0;
    for(const MovieSync::StreamCue& c : cues_)
    {
        if(c.edge != "level")
        {
            continue;
        }
        const double a = (double)c.t_ms / 1000.0 - shift_s;
        const double b = (c.t_end_ms > c.t_ms ? (double)c.t_end_ms / 1000.0 : duration_s_ + shift_s) - shift_s;
        QColor col = level_col;
        col.setAlphaF(b < position_s_ ? 0.30f : 0.75f);
        p.fillRect(QRectF(x_of(a), r.center().y(), std::max(2.0, x_of(b) - x_of(a)), r.height() / 2), col);
    }

    /* Service marks: a strip along the top (intro violet, recap grey-blue, credits grey). */
    for(const MovieSync::StreamMarker& m : markers_)
    {
        QColor col = m.kind == "intro" ? QColor(160, 110, 230)
                   : m.kind == "recap" ? QColor(110, 130, 160)
                   : m.kind == "credits" ? QColor(150, 150, 150)
                                         : QColor(120, 120, 120);
        const double a = (double)m.start_ms / 1000.0;
        const double b = m.end_ms > m.start_ms ? (double)m.end_ms / 1000.0 : duration_s_;
        col.setAlphaF(b < position_s_ ? 0.45f : 0.9f);
        p.fillRect(QRectF(x_of(a), r.top(), std::max(2.0, x_of(b) - x_of(a)), r.height() * 0.3), col);
    }
    p.restore();

    /* One-off cues: ticks across the bar (dimmed once passed). */
    for(const MovieSync::StreamCue& c : cues_)
    {
        if(c.edge == "level")
        {
            continue;
        }
        const double t = (double)c.t_ms / 1000.0 - shift_s;
        QColor col = pulse_col;
        col.setAlphaF(t < position_s_ ? 0.35f : 1.0f);
        p.setPen(QPen(col, 2));
        const double x = x_of(t);
        p.drawLine(QPointF(x, r.top() + 1), QPointF(x, r.bottom() - 1));
    }

    /* Playhead. */
    p.setPen(QPen(pal.color(QPalette::WindowText), 2));
    p.drawLine(QPointF(played_x, rect().top() + 1), QPointF(played_x, rect().bottom() - 1));
}

namespace MovieSyncLog
{

void Add(QListWidget* list, Kind kind, const QString& film_time, const QString& text)
{
    if(!list)
    {
        return;
    }
    QString line = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    line += film_time.isEmpty() ? QStringLiteral("            ") : QStringLiteral("  [%1]  ").arg(film_time, 7);
    line += text;
    auto* item = new QListWidgetItem(line);
    switch(kind)
    {
    case Kind::Cue:
        item->setForeground(QColor(255, 170, 70));
        break;
    case Kind::LevelOn:
        item->setForeground(QColor(90, 190, 220));
        break;
    case Kind::LevelOff:
        item->setForeground(PluginUiBlendColors(QColor(90, 190, 220), list->palette().color(QPalette::Base), 0.4f));
        break;
    case Kind::Warn:
        item->setForeground(QColor(235, 110, 90));
        break;
    case Kind::Info:
    default:
        item->setForeground(PluginUiBlendColors(list->palette().color(QPalette::WindowText),
                                                list->palette().color(QPalette::Base), 0.25f));
        break;
    }
    list->insertItem(0, item);
    while(list->count() > 300)
    {
        delete list->takeItem(list->count() - 1);
    }
}

} // namespace MovieSyncLog
