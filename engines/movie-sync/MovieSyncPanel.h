// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* Movie Sync panel pieces: the cue timeline strip (the whole title with every cue on it and
   a moving playhead) and the activity log. */

#include "MovieSyncStream.h"

#include <QString>
#include <QWidget>

#include <vector>

class QListWidget;

/** One bar across the title: pulse cues as ticks, held looks as bands, the playhead, and what
    is already behind it dimmed. Also the progress bar of the Now Playing card. */
class MovieSyncTimeline : public QWidget
{
    Q_OBJECT

public:
    explicit MovieSyncTimeline(QWidget* parent = nullptr);

    /** duration_s: the title's length; cues in cue time; shift_ms turns cue time into stream
        time (stream = cue - offset + lights delay). No cues = a plain progress bar. */
    void SetTimeline(double duration_s, const std::vector<MovieSync::StreamCue>& cues, std::int64_t shift_ms);
    void SetPosition(double position_s);
    /** Intro / recap / credits stretches the service marks (stream time), shown along the top. */
    void SetMarkers(const std::vector<MovieSync::StreamMarker>& markers);
    void Clear();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double duration_s_ = 0.0;
    double position_s_ = 0.0;
    std::vector<MovieSync::StreamCue> cues_;
    std::vector<MovieSync::StreamMarker> markers_;
    std::int64_t shift_ms_ = 0;
};

namespace MovieSyncLog
{
enum class Kind
{
    Cue,     /* a pulse fired */
    LevelOn, /* a held look started */
    LevelOff,
    Info,    /* matched, paused, seek, source ... */
    Warn     /* blocked, not found, errors */
};

/** Newest first, capped; each entry "hh:mm:ss  [film time]  text", coloured by kind. */
void Add(QListWidget* list, Kind kind, const QString& film_time, const QString& text);
} // namespace MovieSyncLog
