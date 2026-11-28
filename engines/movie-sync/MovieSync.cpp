// SPDX-License-Identifier: GPL-2.0-or-later

#include "MovieSync.h"
#include "MovieSyncHttp.h"

#include "EffectListManager3D.h"
#include "OpenRGB3DSpatialPlugin.h"
#include "OpenRGB3DSpatialTab.h"
#include "PlayerEngines.h"
#include "PluginSettingsPaths.h"
#include "PluginUiUtils.h"
#include "SpatialEffectTypes.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFontDatabase>
#include <QFrame>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QUrl>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <thread>
#include <cmath>
#include <set>
#include <nlohmann/json.hpp>

namespace
{
// Stable local type id; library identity is class_name "MovieSync" (do not add to SpatialEffectTypes.h).
constexpr SpatialEffectType kMovieSyncEffectType = static_cast<SpatialEffectType>(36);
} // namespace

QString MovieSyncEffect::effectDisplayName(const QString& effect_id) const
{
    const MovieSync::CatalogEffect* meta = MovieSync::FindEffect(catalog_, effect_id.toStdString());
    if(meta && !meta->name.empty())
    {
        return QString::fromStdString(meta->name);
    }
    return effect_id;
}

QString MovieSyncEffect::humanizeRuntimeMessage(const std::string& msg) const
{
    if(msg.empty())
    {
        return {};
    }
    const QString q = QString::fromStdString(msg);
    if(msg == "effect_id required")
    {
        return tr("Look id required");
    }
    const QString unknown_prefix = QStringLiteral("unknown effect_id: ");
    if(q.startsWith(unknown_prefix))
    {
        return tr("Unknown look: %1").arg(effectDisplayName(q.mid(unknown_prefix.size())));
    }
    // "Blocked photosensitive effect 'id' (...)"
    if(q.startsWith(QStringLiteral("Blocked photosensitive effect '"))
       && q.contains(QLatin1Char('\'')))
    {
        const int start = QStringLiteral("Blocked photosensitive effect '").size();
        const int end = q.indexOf(QLatin1Char('\''), start);
        if(end > start)
        {
            const QString id = q.mid(start, end - start);
            return tr("Blocked photosensitive look \u201c%1\u201d (enable Allow photosensitive looks).")
                .arg(effectDisplayName(id));
        }
    }
    return q;
}

void RegisterMovieSyncEngine()
{
    EffectListManager3D::get()->RegisterEffect(
        "MovieSync",
        "Movie Sync",
        "Movie Sync",
        "",
        "",
        []() { return new MovieSyncEffect; },
        SpatialEffectSettingsLayout::CustomOnly);
}

MovieSyncEffect::MovieSyncEffect(QWidget* parent)
    : SpatialEffect3D(parent)
{
    listener_ = new MovieSync::ScriptListener(this);
    tick_timer_ = new QTimer(this);
    tick_timer_->setTimerType(Qt::PreciseTimer);
    tick_timer_->setInterval(Action::kPlaybackTickMs);
    connect(tick_timer_, &QTimer::timeout, this, &MovieSyncEffect::onTick);

    listener_->SetEffectHandler([this](const std::string& effect_id, const std::string& edge, bool active) {
        handleCueEdge(effect_id, edge, active, true);
    });
    listener_->SetCatalogJson([this]() { return catalogJson(); });

    /* Streaming: the browser extension reports here; the Windows media controls are read in the
       background; a stream's cue file is played on its clock. */
    listener_->SetStreamHandler([this](const std::string& body) { onStreamReport(body); });
    stream_player_.SetEdgeHandler([this](const std::string& effect_id, const std::string& edge, bool active) {
        handleCueEdge(effect_id, edge, active, false);
    });
    stream_timer_ = new QTimer(this);
    stream_timer_->setTimerType(Qt::PreciseTimer);
    stream_timer_->setInterval(20);
    connect(stream_timer_, &QTimer::timeout, this, &MovieSyncEffect::onStreamTick);

    QTimer::singleShot(0, this, [this]() {
        if(api_enabled_ && !listener_->isListening())
        {
            startListener();
        }
    });
}

/* Artwork downloads running on worker threads: cancelled and waited for when the effect goes
   away, so no thread is still inside the plugin when OpenRGB unloads it. */
struct MovieSyncArtWorkers
{
    std::mutex mutex;
    std::condition_variable done;
    std::set<MovieSync::HttpJob*> jobs;
    int running = 0;
};

MovieSyncEffect::~MovieSyncEffect()
{
    if(art_workers_)
    {
        std::unique_lock<std::mutex> lock(art_workers_->mutex);
        for(MovieSync::HttpJob* job : art_workers_->jobs)
        {
            job->Cancel();
        }
        art_workers_->done.wait_for(lock, std::chrono::seconds(3), [this]() { return art_workers_->running == 0; });
    }
    stream_player_.SetEdgeHandler(nullptr);
    stopListener();
    runtime_.StopAll();
    delete art_original_;
    art_original_ = nullptr;
}

void MovieSyncEffect::handleCueEdge(const std::string& effect_id, const std::string& edge, bool active, bool from_vlc)
{
    bindTab();
    const bool was_playing = runtime_.IsPlaying();
    runtime_.OnEffect(effect_id, edge, active);
    const bool playing = runtime_.IsPlaying();
    if(!playing)
    {
        tick_timer_->stop();
        setPlayingUi(false);
        if(was_playing)
        {
            clearPreviewIfIdle();
        }
    }

    /* Activity log: what fired, from where. */
    const QString name = effectDisplayName(QString::fromStdString(effect_id));
    const QString from = from_vlc ? tr(" (VLC)") : QString();
    if(from_vlc)
    {
        vlc_last_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
        vlc_last_cue_ = effect_id;
    }
    if(!runtime_.lastWarning().empty())
    {
        logEvent(MovieSyncLog::Kind::Warn, humanizeRuntimeMessage(runtime_.lastWarning()) + from, !from_vlc);
    }
    else if(!runtime_.lastError().empty())
    {
        logEvent(MovieSyncLog::Kind::Warn, humanizeRuntimeMessage(runtime_.lastError()) + from, !from_vlc);
    }
    else if(edge == "level")
    {
        logEvent(active ? MovieSyncLog::Kind::LevelOn : MovieSyncLog::Kind::LevelOff,
                 (active ? tr("%1 on") : tr("%1 off")).arg(name) + from, !from_vlc);
    }
    else if(active)
    {
        logEvent(MovieSyncLog::Kind::Cue, name + from, !from_vlc);
    }

    if(!runtime_.lastWarning().empty())
    {
        setStatus(humanizeRuntimeMessage(runtime_.lastWarning()));
    }
    else if(!runtime_.lastError().empty())
    {
        setStatus(humanizeRuntimeMessage(runtime_.lastError()));
    }
    else if(playing && active)
    {
        setStatus(tr("Playing cue: %1").arg(name));
        setPlayingUi(true);
        if(!tick_timer_->isActive())
        {
            last_tick_ms_ = 0;
            tick_timer_->start();
        }
    }
    else if(listener_->isListening())
    {
        setStatus(tr("Listening on 127.0.0.1:%1").arg(listener_->port()));
    }
}

namespace
{
/* What the system's "now playing" is called here. */
QString SystemMediaName()
{
#if defined(_WIN32)
    return QCoreApplication::translate("MovieSyncEffect", "Windows media controls");
#elif defined(__linux__)
    return QCoreApplication::translate("MovieSyncEffect", "system media (MPRIS)");
#else
    return QCoreApplication::translate("MovieSyncEffect", "system media");
#endif
}

std::int64_t SteadyNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

QString FormatClock(double s)
{
    const int total = (int)std::max(0.0, s);
    const int h = total / 3600;
    const int m = (total % 3600) / 60;
    const int sec = total % 60;
    return h > 0 ? QString::asprintf("%d:%02d:%02d", h, m, sec) : QString::asprintf("%d:%02d", m, sec);
}
} // namespace

void MovieSyncEffect::onStreamReport(const std::string& json_body)
{
    try
    {
        const nlohmann::json j = nlohmann::json::parse(json_body.empty() ? "{}" : json_body);
        MovieSync::StreamState st;
        st.source = j.value("source", std::string("extension"));
        st.service = j.value("service", std::string());
        st.id = j.value("id", std::string());
        st.title = j.value("title", std::string());
        st.episode = j.value("episode", std::string());
        /* Numbers from outside: finite and clamped before any conversion (48 h, 0.05-16x). */
        const auto real = [&j](const char* k, double lo, double hi, double fallback) {
            if(!j.contains(k) || !j[k].is_number())
            {
                return fallback;
            }
            const double d = j[k].get<double>();
            return std::isfinite(d) ? std::clamp(d, lo, hi) : fallback;
        };
        constexpr double kMaxMs = 48.0 * 3600.0 * 1000.0;
        st.position_s = real("position_ms", 0.0, kMaxMs, 0.0) / 1000.0;
        st.duration_s = real("duration_ms", 0.0, kMaxMs, 0.0) / 1000.0;
        st.rate = real("rate", 0.05, 16.0, 1.0);
        st.paused = j.value("paused", true);
        st.ad = j.value("ad", false);
        st.artwork_url = j.value("artwork_url", std::string());
        if(st.artwork_url.rfind("https://", 0) != 0 || st.artwork_url.size() > 2048)
        {
            st.artwork_url.clear();
        }
        if(j.contains("meta") && j["meta"].is_object())
        {
            const nlohmann::json& m = j["meta"];
            const auto text = [&m](const char* k, std::size_t cap) {
                return m.contains(k) && m[k].is_string() ? m[k].get<std::string>().substr(0, cap) : std::string();
            };
            const auto number = [&m](const char* k) {
                if(!m.contains(k) || !m[k].is_number())
                {
                    return 0;
                }
                const double d = m[k].get<double>();
                return std::isfinite(d) ? (int)std::clamp(d, 0.0, 100000.0) : 0;
            };
            st.meta.show = text("show", 200);
            st.meta.season = number("season");
            st.meta.episode = number("episode");
            st.meta.episode_title = text("episode_title", 200);
            st.meta.synopsis = text("synopsis", 700);
            st.meta.rating = text("rating", 40);
            st.meta.advisories = text("advisories", 300);
            st.meta.photosensitive = m.contains("photosensitive") && m["photosensitive"].is_boolean()
                                     && m["photosensitive"].get<bool>();
            if(m.contains("genres") && m["genres"].is_array())
            {
                for(const auto& g : m["genres"])
                {
                    if(g.is_string() && st.meta.genres.size() < 8)
                    {
                        st.meta.genres.push_back(g.get<std::string>().substr(0, 40));
                    }
                }
            }
            if(m.contains("artwork") && m["artwork"].is_array())
            {
                for(const auto& a : m["artwork"])
                {
                    if(a.is_string() && a.get<std::string>().rfind("https://", 0) == 0 && a.get<std::string>().size() <= 2048
                       && st.meta.artwork.size() < 6)
                    {
                        st.meta.artwork.push_back(a.get<std::string>());
                    }
                }
            }
            if(m.contains("markers") && m["markers"].is_array())
            {
                for(const auto& mk : m["markers"])
                {
                    if(!mk.is_object() || !mk.contains("kind") || !mk["kind"].is_string() || !mk.contains("start_ms")
                       || !mk["start_ms"].is_number() || st.meta.markers.size() >= 12)
                    {
                        continue;
                    }
                    MovieSync::StreamMarker sm;
                    sm.kind = mk["kind"].get<std::string>();
                    if(sm.kind != "intro" && sm.kind != "recap" && sm.kind != "credits" && sm.kind != "preview")
                    {
                        continue;
                    }
                    const auto ms = [](const nlohmann::json& v) -> std::int64_t {
                        const double d = v.get<double>();
                        return std::isfinite(d) && d >= 0.0 ? (std::int64_t)std::min(d, 48.0 * 3600.0 * 1000.0) : -1;
                    };
                    sm.start_ms = ms(mk["start_ms"]);
                    sm.end_ms = mk.contains("end_ms") && mk["end_ms"].is_number() ? ms(mk["end_ms"]) : -1;
                    if(sm.start_ms < 0)
                    {
                        continue;
                    }
                    st.meta.markers.push_back(sm);
                }
            }
        }
        st.received_ms = SteadyNowMs();
        if(j.value("stopped", false))
        {
            extension_last_ms_ = 0;
            return;
        }
        extension_state_ = st;
        extension_last_ms_ = st.received_ms;
    }
    catch(...)
    {
    }
}

void MovieSyncEffect::onStreamTick()
{
    const std::int64_t now = SteadyNowMs();
    /* The extension knows most (exact IDs, live position): it wins while it is reporting.
       Otherwise the Windows media controls, if something is playing there. */
    MovieSync::StreamState st;
    bool have = false;
    if(extension_last_ms_ > 0 && now - extension_last_ms_ < 5000)
    {
        st = extension_state_;
        have = true;
    }
    else if(system_media_.Latest(&st) && now - st.received_ms < 3000)
    {
        have = true;
    }
    if(!have)
    {
        if(!stream_match_key_.empty())
        {
            logEvent(MovieSyncLog::Kind::Info, tr("Playback stopped"), false);
            resetStream();
        }
        /* Nothing playing: check four times a second instead of fifty. */
        if(stream_timer_->interval() != 250)
        {
            stream_timer_->setInterval(250);
        }
        refreshNowPlaying(now);
        return;
    }
    if(stream_timer_->interval() != 20)
    {
        stream_timer_->setInterval(20);
    }

    /* Match a cue file once per title (service, ID, name, length). */
    const std::string key = st.service + "|" + st.id + "|" + st.title + "|" + st.episode + "|"
                            + std::to_string((long long)std::llround(st.duration_s));
    if(st.source != seen_source_)
    {
        logEvent(MovieSyncLog::Kind::Info,
                 st.source == "extension" ? tr("Following the browser extension")
                                          : tr("Following %1").arg(SystemMediaName()),
                 false);
        seen_source_ = st.source;
    }
    if(key != stream_match_key_)
    {
        stream_match_key_ = key;
        match_how_.clear();
        stream_file_ = stream_cues_.Match(st, &match_how_);
        QString what = QString::fromStdString(st.title);
        if(!st.episode.empty())
        {
            what += QStringLiteral(" \u2014 ") + QString::fromStdString(st.episode);
        }
        if(what.isEmpty())
        {
            what = st.id.empty() ? tr("an unknown title") : QStringLiteral("#") + QString::fromStdString(st.id);
        }
        if(stream_file_)
        {
            logEvent(MovieSyncLog::Kind::Info,
                     tr("Now playing %1 (%2): cue file %3, %4")
                         .arg(what, QString::fromStdString(st.service),
                              QString::fromStdString(stream_file_->path.filename().string()),
                              QString::fromStdString(match_how_)),
                     false);
        }
        else
        {
            logEvent(MovieSyncLog::Kind::Info,
                     tr("Now playing %1 (%2): no cue file for it").arg(what, QString::fromStdString(st.service)), false);
        }
        seen_paused_ = st.paused;
        seen_ad_ = st.ad;
        seen_pos_s_ = -1.0;
        seen_marker_.clear();
    }
    noteTitleDetails(st);

    /* Pause / play / ad / seek, as they happen. */
    const bool fresh = st.received_ms != stream_player_.State().received_ms;
    if(fresh)
    {
        /* Seek: the report is far from where the clock had it (before this report's pause). */
        if(seen_pos_s_ >= 0.0)
        {
            const double expected = seen_pos_s_ + (seen_paused_ ? 0.0 : (double)(now - seen_pos_ms_) / 1000.0 * st.rate);
            if(std::abs(st.position_s - expected) > 2.5)
            {
                int skipped = 0;
                if(stream_file_ && st.position_s > expected)
                {
                    /* Cue time -> stream time: minus the file's offset, plus the lights delay. */
                    const double shift_s = (double)(stream_file_->offset_ms - lights_delay_ms_) / 1000.0;
                    for(const MovieSync::StreamCue& c : stream_file_->cues)
                    {
                        const double t = (double)c.t_ms / 1000.0 - shift_s;
                        if(c.edge != "level" && t > expected && t <= st.position_s)
                        {
                            ++skipped;
                        }
                    }
                }
                logEvent(MovieSyncLog::Kind::Info,
                         skipped > 0 ? tr("Seek %1 \u2192 %2 (%3 cue(s) skipped)")
                                           .arg(FormatClock(expected), FormatClock(st.position_s))
                                           .arg(skipped)
                                     : tr("Seek %1 \u2192 %2").arg(FormatClock(expected), FormatClock(st.position_s)));
            }
        }
        if(st.ad != seen_ad_)
        {
            logEvent(MovieSyncLog::Kind::Info, st.ad ? tr("Ad started: cues hold") : tr("Ad over"));
            seen_ad_ = st.ad;
        }
        else if(st.paused != seen_paused_)
        {
            logEvent(MovieSyncLog::Kind::Info, st.paused ? tr("Paused") : tr("Playing"));
            seen_paused_ = st.paused;
        }
        seen_pos_s_ = st.position_s;
        seen_pos_ms_ = now;
    }

    stream_player_.SetLightsDelayMs(lights_delay_ms_);
    bool play_changed = false;
    const MovieSync::StreamCueFile* play = rebuildPlayFile(st, &play_changed);
    /* A changed list (new marks, another look picked) keeps its name, so the player keeps what
       it holds and its place: only looks no longer wanted are let go, nothing fires twice. */
    if(fresh || play_changed || !stream_player_.Active())
    {
        stream_player_.OnState(st, play, now);
    }
    else
    {
        stream_player_.Tick(now);
    }

    /* Into / out of the service's intro, recap, credits. */
    {
        const std::int64_t pos_ms = (std::int64_t)std::llround(stream_player_.EstimatedPosition(now) * 1000.0);
        std::string in;
        for(const MovieSync::StreamMarker& m : st.meta.markers)
        {
            if(pos_ms >= m.start_ms && (m.end_ms <= m.start_ms || pos_ms < m.end_ms))
            {
                in = m.kind;
            }
        }
        if(in != seen_marker_)
        {
            const auto name = [this](const std::string& k) {
                return k == "intro" ? tr("Intro") : k == "recap" ? tr("Recap") : k == "credits" ? tr("End credits") : tr("Preview");
            };
            const auto look = [this](const std::string& k) {
                const int i = k == "intro" ? 0 : k == "recap" ? 1 : k == "credits" ? 2 : -1;
                return i >= 0 && !marker_look_[i].empty() ? effectDisplayName(QString::fromStdString(marker_look_[i])) : QString();
            };
            if(!in.empty())
            {
                const QString l = look(in);
                logEvent(MovieSyncLog::Kind::Info, l.isEmpty() ? name(in) : tr("%1: %2").arg(name(in), l));
            }
            else if(!seen_marker_.empty())
            {
                logEvent(MovieSyncLog::Kind::Info, tr("%1 over").arg(name(seen_marker_)));
            }
            seen_marker_ = in;
        }
    }
    refreshNowPlaying(now);
}

void MovieSyncEffect::resetStream()
{
    restoreTitleSafety();
    safety_key_.clear();
    play_sig_.clear();
    play_has_ = false;
    timeline_key_.clear();
    details_sig_.clear();
    markers_sig_.clear();
    seen_marker_.clear();
    seen_source_.clear();
    stream_player_.Stop();
    stream_file_ = nullptr;
    stream_match_key_.clear();
    stream_label_ms_ = 0;
}

const MovieSync::StreamCueFile* MovieSyncEffect::rebuildPlayFile(const MovieSync::StreamState& st, bool* changed)
{
    std::string sig = stream_match_key_ + "|" + (stream_file_ ? stream_file_->path.string() : std::string());
    for(const std::string& l : marker_look_)
    {
        sig += "|" + l;
    }
    for(const MovieSync::StreamMarker& m : st.meta.markers)
    {
        sig += "|" + m.kind + ":" + std::to_string(m.start_ms) + "-" + std::to_string(m.end_ms);
    }
    *changed = sig != play_sig_;
    if(*changed)
    {
        play_sig_ = sig;
        play_file_ = stream_file_ ? *stream_file_ : MovieSync::StreamCueFile();
        if(!stream_file_)
        {
            play_file_.path = filesystem::path("service-marks");
        }
        int added = 0;
        for(const MovieSync::StreamMarker& m : st.meta.markers)
        {
            const int i = m.kind == "intro" ? 0 : m.kind == "recap" ? 1 : m.kind == "credits" ? 2 : -1;
            if(i < 0 || marker_look_[i].empty())
            {
                continue;
            }
            /* Marks are in stream time; cue time = stream time + the file's offset. */
            MovieSync::StreamCue c;
            c.t_ms = m.start_ms + play_file_.offset_ms;
            c.t_end_ms = m.end_ms > m.start_ms ? m.end_ms + play_file_.offset_ms : -1;
            c.effect_id = marker_look_[i];
            c.edge = "level";
            play_file_.cues.push_back(c);
            ++added;
        }
        std::sort(play_file_.cues.begin(), play_file_.cues.end(),
                  [](const MovieSync::StreamCue& a, const MovieSync::StreamCue& b) { return a.t_ms < b.t_ms; });
        play_has_ = stream_file_ != nullptr || added > 0;
    }
    return play_has_ ? &play_file_ : nullptr;
}

void MovieSyncEffect::restoreTitleSafety()
{
    if(safety_forced_)
    {
        safety_forced_ = false;
        runtime_.SetAllowPhotosensitive(allow_photosensitive_);
        logEvent(MovieSyncLog::Kind::Info, tr("Photosensitive looks allowed again (title over)"), false);
    }
}

void MovieSyncEffect::noteTitleDetails(const MovieSync::StreamState& st)
{
    const MovieSync::StreamMeta& m = st.meta;
    if(safety_key_ != stream_match_key_)
    {
        restoreTitleSafety();
        safety_key_ = stream_match_key_;
    }

    /* The service's marks, once per title (they can arrive a little after it starts). */
    std::string msig = stream_match_key_;
    QStringList marks;
    for(const MovieSync::StreamMarker& mk : m.markers)
    {
        msig += "|" + mk.kind + std::to_string(mk.start_ms);
        const QString kind = mk.kind == "intro" ? tr("intro") : mk.kind == "recap" ? tr("recap")
                           : mk.kind == "credits" ? tr("credits") : tr("preview");
        marks << (mk.end_ms > mk.start_ms
                      ? QStringLiteral("%1 %2–%3").arg(kind, FormatClock(mk.start_ms / 1000.0), FormatClock(mk.end_ms / 1000.0))
                      : tr("%1 from %2").arg(kind, FormatClock(mk.start_ms / 1000.0)));
    }
    if(!marks.isEmpty() && msig != markers_sig_)
    {
        markers_sig_ = msig;
        logEvent(MovieSyncLog::Kind::Info, tr("Service marks: %1").arg(marks.join(QStringLiteral(", "))), false);
    }

    static const QRegularExpression kPhoto(QStringLiteral("flash|strob|photosensitiv|seizure|epilep"),
                                           QRegularExpression::CaseInsensitiveOption);
    const bool marked = m.photosensitive || kPhoto.match(QString::fromStdString(m.advisories)).hasMatch();
    const std::string sig = stream_match_key_ + "|" + m.rating + "|" + m.advisories + "|" + (marked ? "P" : "") + "|"
                            + (auto_safety_ ? "A" : "") + (allow_photosensitive_ ? "S" : "");
    if(sig == details_sig_)
    {
        return;
    }
    details_sig_ = sig;
    QStringList bits;
    if(!m.rating.empty())
    {
        bits << tr("Rated %1").arg(QString::fromStdString(m.rating));
    }
    if(!m.advisories.empty())
    {
        bits << QString::fromStdString(m.advisories);
    }
    if(!bits.isEmpty())
    {
        logEvent(MovieSyncLog::Kind::Info, bits.join(QStringLiteral(" · ")), false);
    }
    if(!marked)
    {
        return;
    }
    if(!allow_photosensitive_)
    {
        logEvent(MovieSyncLog::Kind::Info, tr("Title marked photosensitive (photosensitive looks are off anyway)"), false);
    }
    else if(auto_safety_ && safety_override_key_ != stream_match_key_)
    {
        if(!safety_forced_)
        {
            safety_forced_ = true;
            runtime_.SetAllowPhotosensitive(false);
        }
        logEvent(MovieSyncLog::Kind::Warn, tr("Title marked photosensitive: photosensitive looks blocked for it (auto safety)"),
                 false);
    }
    else
    {
        logEvent(MovieSyncLog::Kind::Warn,
                 tr("Title marked photosensitive: photosensitive looks are allowed (Settings: auto safety blocks them for "
                    "marked titles)"),
                 false);
    }
}

void MovieSyncEffect::refreshMarkerCombos()
{
    for(int i = 0; i < 3; ++i)
    {
        QComboBox* combo = marker_combo_[i];
        if(!combo)
        {
            continue;
        }
        QSignalBlocker b(combo);
        combo->clear();
        combo->addItem(tr("Nothing"), QString());
        std::vector<const MovieSync::CatalogEffect*> list;
        for(const MovieSync::CatalogEffect& e : catalog_.effects)
        {
            list.push_back(&e);
        }
        std::sort(list.begin(), list.end(), [](const MovieSync::CatalogEffect* a, const MovieSync::CatalogEffect* b) {
            return a->name < b->name;
        });
        int sel = 0;
        for(const MovieSync::CatalogEffect* e : list)
        {
            combo->addItem(QString::fromStdString(e->name) + (e->photosensitive ? tr(" (photosensitive)") : QString()),
                           QString::fromStdString(e->id));
            if(e->id == marker_look_[i])
            {
                sel = combo->count() - 1;
            }
        }
        if(sel == 0 && !marker_look_[i].empty())
        {
            /* Saved look not in the library (yet): keep it rather than drop it. */
            combo->addItem(QString::fromStdString(marker_look_[i]), QString::fromStdString(marker_look_[i]));
            sel = combo->count() - 1;
        }
        combo->setCurrentIndex(sel);
    }
}

void MovieSyncEffect::logEvent(MovieSyncLog::Kind kind, const QString& text, bool with_film_time)
{
    QString film;
    if(with_film_time && !stream_match_key_.empty())
    {
        film = FormatClock(stream_player_.EstimatedPosition(SteadyNowMs()));
    }
    MovieSyncLog::Add(log_list_, kind, film, text);
}

void MovieSyncEffect::showArtwork(const QPixmap& pixmap)
{
    if(!now_art_)
    {
        return;
    }
    if(!art_original_)
    {
        art_original_ = new QPixmap();
    }
    *art_original_ = pixmap;
    art_scaled_w_ = 0;
    if(pixmap.isNull())
    {
        now_art_->clear();
        now_art_->hide();
        return;
    }
    now_art_->show();
    rescaleArtwork();
}

void MovieSyncEffect::rescaleArtwork()
{
    if(!now_art_ || !art_original_ || art_original_->isNull())
    {
        return;
    }
    /* The card's full width, at most 240 px tall (a tall poster is shown centred). */
    int w = now_art_->width();
    if(w < 60 && now_art_->parentWidget())
    {
        w = now_art_->parentWidget()->contentsRect().width() - 20;
    }
    w = std::max(w, 120);
    const qreal dpr = now_art_->devicePixelRatioF();
    QPixmap scaled = art_original_->scaled(QSize((int)(w * dpr), (int)(240 * dpr)), Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    now_art_->setPixmap(scaled);
    now_art_->setFixedHeight(std::max(1, (int)std::lround(scaled.height() / dpr)));
    art_scaled_w_ = now_art_->width();
}

void MovieSyncEffect::updateArtwork(const MovieSync::StreamState& st)
{
    /* Where the picture can come from, best first: your own image beside the cue file
       (<name>.jpg / .png next to <name>.movie_sync.json), the service's artwork (each link
       may have a JPEG twin first: OpenRGB cannot read WebP / AVIF), the Windows thumbnail or
       the page's image. The next one is tried when one fails. */
    std::vector<std::string> cands;
    if(!stream_match_key_.empty())
    {
        if(stream_file_)
        {
            const std::string name = stream_file_->path.filename().string();
            const std::string base = name.size() > 16 ? name.substr(0, name.size() - 16) : name;
            for(const char* ext : {".jpg", ".jpeg", ".png"})
            {
                std::error_code ec;
                const filesystem::path p = stream_file_->path.parent_path() / (base + ext);
                if(filesystem::exists(p, ec))
                {
                    cands.push_back("file:" + p.string());
                    break;
                }
            }
        }
        for(const std::string& a : st.meta.artwork)
        {
            cands.push_back(a);
        }
        if(!st.artwork_url.empty())
        {
            cands.push_back(st.artwork_url);
        }
    }
    std::string key;
    for(const std::string& c : cands)
    {
        key += c + "\n";
    }
    if(key == art_key_)
    {
        return;
    }
    art_key_ = key;
    art_candidates_ = cands;
    if(art_reply_)
    {
        art_reply_->abort();
    }
    tryArtwork(0);
}

void MovieSyncEffect::tryArtwork(std::size_t index)
{
    if(index >= art_candidates_.size())
    {
        showArtwork(QPixmap());
        return;
    }
    const std::string want = art_candidates_[index];
    const std::string key_now = art_key_;
    const auto cached = art_cache_.find(want);
    if(cached != art_cache_.end())
    {
        showArtwork(cached->second);
        return;
    }
    if(art_inflight_.count(want))
    {
        return; /* already downloading: it shows when it arrives */
    }
    const auto failed = art_failed_.find(want);
    if(failed != art_failed_.end() && SteadyNowMs() - failed->second < 60000)
    {
        artworkFailed(index, want, "failed a moment ago");
        return;
    }
    /* Keep a decoded picture (scaled down, at most 24 in memory); false if it did not decode. */
    const auto keep = [this](const std::string& url, const QImage& img) {
        if(img.isNull())
        {
            return false;
        }
        QPixmap px = QPixmap::fromImage(img.width() > 1200 || img.height() > 1200
                                            ? img.scaled(1200, 1200, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                            : img);
        if(art_cache_.size() >= 24)
        {
            art_cache_.erase(art_cache_.begin());
        }
        art_cache_[url] = px;
        showArtwork(px);
        return true;
    };
    if(want.rfind("file:", 0) == 0)
    {
        if(!keep(want, QImage(QString::fromStdString(want.substr(5)))))
        {
            artworkFailed(index, want, "cannot read the image file");
        }
        return;
    }
    if(want.rfind("smtc:", 0) == 0)
    {
        const std::vector<std::uint8_t> bytes = system_media_.Thumbnail(std::strtoull(want.c_str() + 5, nullptr, 10));
        QImage img;
        if(!bytes.empty())
        {
            img.loadFromData(bytes.data(), (int)bytes.size());
        }
        if(!keep(want, img))
        {
            artworkFailed(index, want, "no Windows thumbnail");
        }
        return;
    }
    const QUrl url(QString::fromStdString(want));
    if(url.scheme() != QStringLiteral("https") || url.host().isEmpty())
    {
        artworkFailed(index, want, "not an https link");
        return;
    }
    if(index == 0)
    {
        showArtwork(QPixmap());
    }
    static constexpr qint64 kMaxBytes = 8 * 1024 * 1024;
    if(MovieSync::NativeHttpsAvailable())
    {
        /* Windows: the system's HTTP stack, on a worker thread (OpenRGB's Qt has no TLS). */
        QPointer<MovieSyncEffect> self(this);
        if(!art_workers_)
        {
            art_workers_ = std::make_shared<MovieSyncArtWorkers>();
        }
        std::shared_ptr<MovieSyncArtWorkers> workers = art_workers_;
        {
            std::lock_guard<std::mutex> lock(workers->mutex);
            ++workers->running;
        }
        art_inflight_.insert(want);
        std::thread([self, workers, want, key_now, index]() {
            std::vector<std::uint8_t> bytes;
            std::string why;
            MovieSync::HttpJob job;
            {
                std::lock_guard<std::mutex> lock(workers->mutex);
                workers->jobs.insert(&job);
            }
            const bool ok = MovieSync::HttpsGet(want, &bytes, &why, (std::size_t)kMaxBytes, 15000, &job);
            {
                std::lock_guard<std::mutex> lock(workers->mutex);
                workers->jobs.erase(&job);
            }
            /* Decoded here by Windows (it reads the WebP Netflix serves; OpenRGB's Qt cannot). */
            std::vector<std::uint8_t> pixels;
            int w = 0, h = 0;
            const bool decoded = ok && !job.Cancelled() && MovieSync::DecodeImageNative(bytes, 1200, &w, &h, &pixels);
            if(!job.Cancelled() && QCoreApplication::instance())
            {
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [self, want, key_now, index, ok, why, decoded, w, h, pixels = std::move(pixels),
                     bytes = std::move(bytes)]() {
                        if(!self)
                        {
                            return;
                        }
                        self->art_inflight_.erase(want);
                        QImage img;
                        if(decoded)
                        {
                            img = QImage(pixels.data(), w, h, w * 4, QImage::Format_ARGB32_Premultiplied).copy();
                        }
                        else if(ok)
                        {
                            img.loadFromData(bytes.data(), (int)bytes.size());
                        }
                        if(!img.isNull())
                        {
                            /* Kept even if the title moved on (it may come back). */
                            const QPixmap& px = self->storeArtwork(want, img);
                            if(key_now == self->art_key_)
                            {
                                self->showArtwork(px);
                            }
                            return;
                        }
                        self->art_failed_[want] = SteadyNowMs();
                        if(self->art_failed_.size() > 64)
                        {
                            self->art_failed_.erase(self->art_failed_.begin());
                        }
                        if(key_now == self->art_key_)
                        {
                            self->artworkFailed(index, want, ok ? std::string("not a picture OpenRGB can read") : why);
                        }
                    },
                    Qt::QueuedConnection);
            }
            /* Last: the effect may now finish going away. */
            std::lock_guard<std::mutex> lock(workers->mutex);
            --workers->running;
            workers->done.notify_all();
        }).detach();
        return;
    }
    if(!net_)
    {
        net_ = new QNetworkAccessManager(this);
    }
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(10000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("OpenRGB-MovieSync"));
    /* Ask for formats OpenRGB can read. */
    req.setRawHeader("Accept", "image/jpeg,image/png,image/gif;q=0.8,*/*;q=0.1");
    QNetworkReply* reply = net_->get(req);
    art_reply_ = reply;
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 got, qint64 total) {
        if(got > kMaxBytes || total > kMaxBytes)
        {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, want, keep, key_now, index]() {
        reply->deleteLater();
        if(key_now != art_key_)
        {
            return; /* another title by now */
        }
        QImage img;
        if(reply->error() == QNetworkReply::NoError)
        {
            const QByteArray data = reply->readAll();
            if(data.size() <= kMaxBytes)
            {
                img.loadFromData(data);
            }
        }
        if(!keep(want, img))
        {
            artworkFailed(index, want,
                          reply->error() != QNetworkReply::NoError ? reply->errorString().toStdString()
                                                                   : std::string("not a picture OpenRGB can read"));
        }
    });
}

const QPixmap& MovieSyncEffect::storeArtwork(const std::string& url, const QImage& img)
{
    QPixmap px = QPixmap::fromImage(img.width() > 1200 || img.height() > 1200
                                        ? img.scaled(1200, 1200, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                        : img);
    if(art_cache_.size() >= 24 && !art_cache_.count(url))
    {
        art_cache_.erase(art_cache_.begin());
    }
    art_cache_[url] = px;
    return art_cache_[url];
}

void MovieSyncEffect::artworkFailed(std::size_t index, const std::string& url, const std::string& why)
{
    if(index + 1 < art_candidates_.size())
    {
        tryArtwork(index + 1);
        return;
    }
    showArtwork(QPixmap());
    /* Every source failed: say why once per title (the last one's reason). */
    if(art_warned_key_ != art_key_)
    {
        art_warned_key_ = art_key_;
        const QString host = QUrl(QString::fromStdString(url)).host();
        logEvent(MovieSyncLog::Kind::Warn,
                 tr("No artwork: %1 (%2)").arg(QString::fromStdString(why), host.isEmpty() ? tr("local") : host), false);
    }
}

void MovieSyncEffect::refreshNowPlaying(std::int64_t now_ms)
{
    if(!now_title_ || now_ms - stream_label_ms_ < 250)
    {
        return;
    }
    stream_label_ms_ = now_ms;
    /* The panel was resized: fit the banner to the new width. */
    if(now_art_ && now_art_->isVisible() && now_art_->width() != art_scaled_w_)
    {
        rescaleArtwork();
    }

    /* Connection lights: green = live, grey = idle / not seen. */
    if(connections_label_)
    {
        const auto dot = [](bool on, const QString& text, const QString& tip) {
            return QStringLiteral("<span title=\"%3\" style=\"color:%1\">\u25CF</span> %2")
                .arg(on ? QStringLiteral("#3fbf5f") : QStringLiteral("#7a7a7a"), text, tip);
        };
        const bool api = listener_ && listener_->isListening();
        const bool ext = extension_last_ms_ > 0 && now_ms - extension_last_ms_ < 5000;
        MovieSync::StreamState win;
        const bool winp = system_media_.Latest(&win) && now_ms - win.received_ms < 3000;
        const bool vlc = vlc_last_ms_ > 0 && now_ms - vlc_last_ms_ < 60000;
        connections_label_->setText(
            dot(api, api ? tr("Script API :%1").arg(listener_->port()) : tr("Script API off"),
                tr("Local API for VLC and the browser extension")) +
            QStringLiteral("&nbsp;&nbsp;&nbsp;") +
            dot(ext, tr("Browser extension"),
                ext ? tr("Reporting now") : tr("Not reporting (not installed, or nothing playing)")) +
            QStringLiteral("&nbsp;&nbsp;&nbsp;") +
            dot(winp, SystemMediaName(),
                winp ? tr("%1 is playing").arg(QString::fromStdString(win.service)) : tr("Nothing reported")) +
            QStringLiteral("&nbsp;&nbsp;&nbsp;") +
            dot(vlc, tr("VLC"),
                vlc ? tr("Last cue: %1").arg(QString::fromStdString(vlc_last_cue_)) : tr("No VLC cue in the last minute")));
    }

    if(stream_match_key_.empty())
    {
        now_source_->setText(tr("Nothing playing"));
        now_title_->setText(tr("Play something on a streaming service, or a movie in VLC"));
        now_time_->clear();
        QString cues = tr("%1 stream cue file(s) in movie-sync/streams").arg(stream_cues_.Count());
        if(!stream_cues_.Errors().empty())
        {
            cues += QStringLiteral("\n") + tr("Problem: %1").arg(QString::fromStdString(stream_cues_.Errors().front()));
        }
        now_cuefile_->setText(cues);
        now_next_->clear();
        now_meta_->clear();
        now_synopsis_->clear();
        now_synopsis_->setToolTip(QString());
        updateArtwork(MovieSync::StreamState());
        if(timeline_ && !timeline_key_.empty())
        {
            timeline_->Clear();
            timeline_key_.clear();
        }
        return;
    }

    const MovieSync::StreamState& st = stream_player_.State();
    static const char* kServices[][2] = {{"netflix", "Netflix"}, {"prime", "Prime Video"}, {"crunchyroll", "Crunchyroll"},
                                         {"disney", "Disney+"}, {"stan", "Stan"}, {"binge", "Binge"},
                                         {"youtube", "YouTube"}, {"paramount", "Paramount+"}, {"max", "Max"},
                                         {"appletv", "Apple TV"}, {"vlc", "VLC"}, {"app:firefox", "Firefox"},
                                         {"app:chrome", "Chrome"}, {"app:edge", "Edge"}, {"app:brave", "Brave"},
                                         {"app:opera", "Opera"}, {"app:vivaldi", "Vivaldi"}};
    QString service = QString::fromStdString(st.service);
    for(const auto& sv : kServices)
    {
        if(st.service == sv[0])
        {
            service = QString::fromUtf8(sv[1]);
        }
    }
    now_source_->setText(tr("%1  \u00B7  via %2")
                             .arg(service, st.source == "extension" ? tr("browser extension") : SystemMediaName()));
    const MovieSync::StreamMeta& meta = st.meta;
    QString title;
    if(!meta.show.empty())
    {
        /* The service's own details: show, then "S1 E10 · Episode title". */
        title = QString::fromStdString(meta.show);
        QString ep;
        if(meta.season > 0)
        {
            ep += tr("S%1 ").arg(meta.season);
        }
        if(meta.episode > 0)
        {
            ep += tr("E%1").arg(meta.episode);
        }
        ep = ep.trimmed();
        if(!meta.episode_title.empty() && meta.episode_title != meta.show)
        {
            ep += (ep.isEmpty() ? QString() : QStringLiteral("  \u00B7  ")) + QString::fromStdString(meta.episode_title);
        }
        if(!ep.isEmpty())
        {
            title += QStringLiteral("  \u2014  ") + ep;
        }
    }
    else
    {
        title = QString::fromStdString(st.title);
        if(!st.episode.empty())
        {
            title += QStringLiteral("  \u2014  ") + QString::fromStdString(st.episode);
        }
    }
    if(title.isEmpty())
    {
        title = st.id.empty() ? tr("Unknown title") : tr("Title #%1").arg(QString::fromStdString(st.id));
    }
    now_title_->setText(title);

    QStringList info;
    if(!meta.rating.empty())
    {
        info << QString::fromStdString(meta.rating);
    }
    for(const std::string& g : meta.genres)
    {
        info << QString::fromStdString(g);
    }
    if(!meta.advisories.empty())
    {
        info << QString::fromStdString(meta.advisories);
    }
    if(meta.photosensitive)
    {
        info << (safety_forced_ ? tr("\u26A0 Photosensitive (blocked)") : tr("\u26A0 Photosensitive"));
    }
    now_meta_->setText(info.join(QStringLiteral("  \u00B7  ")));
    const QString syn = QString::fromStdString(meta.synopsis);
    now_synopsis_->setText(syn.size() > 240 ? syn.left(237) + QStringLiteral("\u2026") : syn);
    now_synopsis_->setToolTip(syn.size() > 240 ? syn : QString());
    updateArtwork(st);

    const double pos = stream_player_.EstimatedPosition(now_ms);
    const QString state = st.ad ? tr("Ad") : (st.paused ? tr("Paused") : tr("Playing"));
    const QString rate = std::abs(st.rate - 1.0) > 0.01 ? QStringLiteral("  \u00B7  %1\u00D7").arg(st.rate, 0, 'g', 3) : QString();
    now_time_->setText(QStringLiteral("%1 / %2  \u00B7  %3%4").arg(FormatClock(pos), FormatClock(st.duration_s), state, rate));

    /* Timeline: rebuilt when the title / cue file / offsets change. */
    if(timeline_)
    {
        const std::int64_t shift = play_has_ ? play_file_.offset_ms - lights_delay_ms_ : 0;
        const std::string tkey = play_sig_ + "|" + std::to_string(shift);
        if(tkey != timeline_key_)
        {
            timeline_key_ = tkey;
            static const std::vector<MovieSync::StreamCue> kNone;
            timeline_->SetTimeline(st.duration_s, play_has_ ? play_file_.cues : kNone, shift);
            timeline_->SetMarkers(meta.markers);
        }
        timeline_->SetPosition(pos);
    }

    if(stream_file_)
    {
        now_cuefile_->setText(tr("Cue file: %1  (%2 cues, matched %3)")
                                  .arg(QString::fromStdString(stream_file_->path.filename().string()))
                                  .arg(stream_file_->cues.size())
                                  .arg(QString::fromStdString(match_how_)));
    }
    else if(play_has_)
    {
        now_cuefile_->setText(tr("No cue file: lights follow the service's intro / recap / credits marks only."));
    }
    if(play_has_)
    {
        /* Next cue after the playhead (in stream time). */
        const double shift_s = (double)(play_file_.offset_ms - lights_delay_ms_) / 1000.0;
        const MovieSync::StreamCue* next = nullptr;
        for(const MovieSync::StreamCue& c : play_file_.cues)
        {
            if((double)c.t_ms / 1000.0 - shift_s > pos)
            {
                next = &c;
                break;
            }
        }
        if(next)
        {
            const double in_s = (double)next->t_ms / 1000.0 - shift_s - pos;
            now_next_->setText(tr("Next: %1 in %2%3")
                                   .arg(effectDisplayName(QString::fromStdString(next->effect_id)), FormatClock(in_s),
                                        next->edge == "level" ? tr(" (held)") : QString()));
        }
        else
        {
            now_next_->setText(tr("No more cues in this title"));
        }
    }
    if(!stream_file_ && !play_has_ && st.service.rfind("app:", 0) == 0)
    {
        /* Not a known service (a browser tab without the extension, another app): no ID. */
        now_cuefile_->setText(tr("No cue file for this title. Without the browser extension it is matched by title and "
                                 "length only: give the cue file this \"title\" and \"duration_ms\" (or use the "
                                 "extension for exact IDs)."));
        now_next_->clear();
    }
    else if(!stream_file_ && !play_has_)
    {
        now_cuefile_->setText(tr("No cue file for this title. Add one to movie-sync/streams (service \"%1\", id \"%2\") "
                                 "and press Reload.")
                                  .arg(QString::fromStdString(st.service), QString::fromStdString(st.id)));
        now_next_->clear();
    }
}

EffectInfo3D MovieSyncEffect::GetEffectInfo() const
{
    EffectInfo3D info = {};
    info.effect_name = "Movie Sync";
    info.effect_description =
        "Script API listener for VLC movie sync. Sidecar .movie_sync.json next to the video; shared looks in movie-sync/.";
    info.category = "Movie Sync";
    info.effect_type = kMovieSyncEffectType;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 0;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_frequency = false;
    info.use_size_parameter = false;
    info.show_color_controls = false;
    info.show_speed_control = false;
    info.show_brightness_control = false;
    info.show_frequency_control = false;
    info.show_detail_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_fps_control = false;
    info.show_surface_control = false;
    info.show_position_offset_control = false;
    info.show_room_output_control = false;
    return info;
}

void MovieSyncEffect::SetupCustomUI(QWidget* parent)
{
    if(!parent)
    {
        return;
    }
    bindTab();

    QWidget* panel = new QWidget();
    panel->setObjectName(QStringLiteral("MovieSyncEffectSettings"));
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    /* ---- Now Playing card ---- */
    QFrame* card = new QFrame(panel);
    QVBoxLayout* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(10, 8, 10, 8);
    card_layout->setSpacing(3);
    /* Banner across the top of the card, the details under it. */
    now_art_ = new QLabel(card);
    now_art_->setAlignment(Qt::AlignCenter);
    now_art_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    now_art_->setMinimumWidth(0);
    now_art_->hide();
    card_layout->addWidget(now_art_);
    QVBoxLayout* card_info = new QVBoxLayout();
    card_info->setSpacing(3);
    card_layout->addLayout(card_info);
    now_source_ = new QLabel(card);
    PluginUiApplyMutedSecondaryLabel(now_source_);
    card_info->addWidget(now_source_);
    now_title_ = new QLabel(card);
    QFont title_font = now_title_->font();
    title_font.setBold(true);
    title_font.setPointSizeF(title_font.pointSizeF() * 1.2);
    now_title_->setFont(title_font);
    now_title_->setWordWrap(true);
    now_title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    card_info->addWidget(now_title_);
    now_meta_ = new QLabel(card);
    now_meta_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(now_meta_);
    card_info->addWidget(now_meta_);
    now_synopsis_ = new QLabel(card);
    now_synopsis_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(now_synopsis_);
    card_info->addWidget(now_synopsis_);
    now_time_ = new QLabel(card);
    card_info->addWidget(now_time_);
    now_next_ = new QLabel(card);
    now_next_->setStyleSheet(QStringLiteral("color:#ffaa46;"));
    card_info->addWidget(now_next_);
    card_info->addStretch(1);
    timeline_ = new MovieSyncTimeline(card);
    card_layout->addWidget(timeline_);
    now_cuefile_ = new QLabel(card);
    now_cuefile_->setWordWrap(true);
    now_cuefile_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    PluginUiApplyMutedSecondaryLabel(now_cuefile_);
    card_layout->addWidget(now_cuefile_);
    layout->addWidget(card);
    PluginUiApplyControllerCardChrome(card);
    /* The card's frame style must not draw a box around every label in it (QLabel is a QFrame). */
    card->setStyleSheet(card->styleSheet() + QStringLiteral(" QLabel { border: none; background: transparent; }"));

    connections_label_ = new QLabel(panel);
    connections_label_->setTextFormat(Qt::RichText);
    connections_label_->setWordWrap(true);
    layout->addWidget(connections_label_);

    status_label_ = new QLabel(panel);
    status_label_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(status_label_);
    layout->addWidget(status_label_);

    /* ---- Activity log ---- */
    QWidget* log_body = new QWidget();
    QVBoxLayout* log_layout = new QVBoxLayout(log_body);
    log_layout->setContentsMargins(0, 0, 0, 0);
    log_list_ = new QListWidget(log_body);
    log_list_->setMinimumHeight(160);
    log_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    log_list_->setUniformItemSizes(true);
    log_list_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    log_layout->addWidget(log_list_);
    QHBoxLayout* log_row = new QHBoxLayout();
    QPushButton* copy_log = new QPushButton(tr("Copy"), log_body);
    copy_log->setToolTip(tr("Copy the selected lines (or all of them) to the clipboard"));
    connect(copy_log, &QPushButton::clicked, this, [this]() {
        QStringList lines;
        const QList<QListWidgetItem*> sel = log_list_->selectedItems();
        for(int i = log_list_->count() - 1; i >= 0; --i)
        {
            QListWidgetItem* it = log_list_->item(i);
            if(sel.isEmpty() || sel.contains(it))
            {
                lines << it->text();
            }
        }
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    });
    log_row->addWidget(copy_log);
    QPushButton* clear_log = new QPushButton(tr("Clear"), log_body);
    connect(clear_log, &QPushButton::clicked, log_list_, &QListWidget::clear);
    log_row->addWidget(clear_log);
    log_row->addStretch(1);
    log_layout->addLayout(log_row);
    PluginUiAddSectionBlock(layout, tr("Activity"),
                            tr("What Movie Sync did: cues fired, held looks on and off, title matches, "
                               "pauses, seeks and ads. [film time] is where playback was."),
                            log_body);

    /* ---- Looks library ---- */
    QWidget* looks_body = new QWidget();
    QVBoxLayout* looks_layout = new QVBoxLayout(looks_body);
    looks_layout->setContentsMargins(0, 0, 0, 0);
    QFormLayout* filter_form = new QFormLayout();
    filter_form->setContentsMargins(0, 0, 0, 0);
    category_combo_ = new QComboBox(looks_body);
    category_combo_->setToolTip(tr("Filter by category folder"));
    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MovieSyncEffect::onCategoryChanged);
    filter_form->addRow(tr("Category"), category_combo_);
    looks_layout->addLayout(filter_form);

    effect_list_ = new QListWidget(looks_body);
    effect_list_->setMinimumHeight(180);
    effect_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(effect_list_, &QListWidget::currentRowChanged, this, &MovieSyncEffect::onEffectSelectionChanged);
    connect(effect_list_, &QListWidget::itemDoubleClicked, this, &MovieSyncEffect::onEffectDoubleClicked);
    looks_layout->addWidget(effect_list_, 1);

    notes_label_ = new QLabel(looks_body);
    notes_label_->setWordWrap(true);
    PluginUiApplyMutedSecondaryLabel(notes_label_);
    notes_label_->setText(tr("Notes: —"));
    looks_layout->addWidget(notes_label_);

    QHBoxLayout* row = new QHBoxLayout();
    QPushButton* reload = new QPushButton(tr("Reload"), looks_body);
    reload->setToolTip(tr("Reload the looks and the stream cue files"));
    connect(reload, &QPushButton::clicked, this, &MovieSyncEffect::onReloadCatalog);
    row->addWidget(reload);
    preview_button_ = new QPushButton(tr("Preview"), looks_body);
    preview_button_->setToolTip(tr("Play the selected look once"));
    connect(preview_button_, &QPushButton::clicked, this, &MovieSyncEffect::onPreview);
    row->addWidget(preview_button_);
    stop_button_ = new QPushButton(tr("Stop"), looks_body);
    stop_button_->setEnabled(false);
    stop_button_->setToolTip(tr("Stop the current preview or cue"));
    connect(stop_button_, &QPushButton::clicked, this, &MovieSyncEffect::onStopPreview);
    row->addWidget(stop_button_);
    row->addStretch(1);
    looks_layout->addLayout(row);
    PluginUiAddSectionBlock(layout, tr("Looks library"),
                            tr("Every look a cue can name. Double-click or Preview to see one."), looks_body,
                            nullptr, false);

    /* ---- Settings ---- */
    QWidget* set_body = new QWidget();
    QVBoxLayout* set_layout = new QVBoxLayout(set_body);
    set_layout->setContentsMargins(0, 0, 0, 0);
    api_check_ = new QCheckBox(tr("Enable Script API (127.0.0.1)"), set_body);
    api_check_->setToolTip(tr("VLC and the browser extension talk to Movie Sync through this"));
    api_check_->setChecked(api_enabled_);
    connect(api_check_, &QCheckBox::toggled, this, &MovieSyncEffect::onApiToggled);
    set_layout->addWidget(api_check_);
    allow_strobe_check_ = new QCheckBox(tr("Allow photosensitive looks"), set_body);
    allow_strobe_check_->setChecked(allow_photosensitive_);
    connect(allow_strobe_check_, &QCheckBox::toggled, this, &MovieSyncEffect::onAllowStrobeToggled);
    set_layout->addWidget(allow_strobe_check_);
    auto_safety_check_ = new QCheckBox(tr("Block them for titles the service marks photosensitive"), set_body);
    auto_safety_check_->setToolTip(tr("Prime Video marks some titles Photosensitive, and advisories can mention flashing "
                                      "lights. With this on, photosensitive looks are blocked while such a title plays, "
                                      "even if they are allowed above. Off: the title is only noted in the log."));
    auto_safety_check_->setChecked(auto_safety_);
    connect(auto_safety_check_, &QCheckBox::toggled, this, [this](bool on) {
        auto_safety_ = on;
        if(!on)
        {
            restoreTitleSafety();
        }
        details_sig_.clear(); /* look at the playing title again */
    });
    set_layout->addWidget(auto_safety_check_);
    QFormLayout* stream_form = new QFormLayout();
    stream_form->setContentsMargins(0, 0, 0, 0);
    lights_delay_spin_ = new QSpinBox(set_body);
    lights_delay_spin_->setRange(-2000, 2000);
    lights_delay_spin_->setSingleStep(25);
    lights_delay_spin_->setSuffix(tr(" ms"));
    lights_delay_spin_->setValue(lights_delay_ms_);
    lights_delay_spin_->setToolTip(tr("Positive: cues fire later (your TV or Bluetooth makes the picture / "
                                      "sound lag behind). Negative: cues fire earlier (your LED devices are "
                                      "slow to change)."));
    connect(lights_delay_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        lights_delay_ms_ = v;
        timeline_key_.clear();
    });
    stream_form->addRow(tr("Lights delay"), lights_delay_spin_);
    /* What lights do in the parts of a title the service marks (Netflix, Prime, Crunchyroll):
       held for as long as the part lasts, cue file or not. */
    const QString mark_names[3] = {tr("During the intro"), tr("During the recap"), tr("During end credits")};
    for(int i = 0; i < 3; ++i)
    {
        marker_combo_[i] = new QComboBox(set_body);
        marker_combo_[i]->setToolTip(tr("A look held while the service's marked part plays (works without a cue file). "
                                        "Nothing = leave the lights to the cue file."));
        connect(marker_combo_[i], QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i](int) {
            marker_look_[i] = marker_combo_[i]->currentData().toString().toStdString();
        });
        stream_form->addRow(mark_names[i], marker_combo_[i]);
    }
    set_layout->addLayout(stream_form);
    help_label_ = new QLabel(
        tr("<b>Movies (VLC):</b> put Movie.movie_sync.json next to the video, enable the Script API, play in VLC.<br>"
           "<b>Streaming</b> (Netflix, Prime Video, Crunchyroll, Disney+ and others): install the browser extension "
           "(integrations/browser-extension); it also works in the Netflix / Prime Video apps on Windows. Without it, "
           "apps that report to Windows media controls or Linux MPRIS still work (by title and length). Cue files go in movie-sync/streams/ "
           "(VLC sidecar format plus \"streams\": [{\"service\", \"id\"}]); press Reload after adding one.<br>"
           "<b>Artwork:</b> the service's own image is shown when it gives one; put <i>name</i>.jpg or .png next to "
           "<i>name</i>.movie_sync.json to use your own."),
        set_body);
    help_label_->setWordWrap(true);
    help_label_->setTextFormat(Qt::RichText);
    PluginUiApplyMutedSecondaryLabel(help_label_);
    set_layout->addWidget(help_label_);
    PluginUiAddSectionBlock(layout, tr("Settings and help"), QString(), set_body, nullptr, false);

    AddWidgetToParent(panel, parent);

    onReloadCatalog();
    if(api_enabled_)
    {
        startListener();
    }
    else
    {
        setStatus(tr("Script API off"));
    }
    stream_label_ms_ = 0;
    refreshNowPlaying(SteadyNowMs());
    logEvent(MovieSyncLog::Kind::Info, tr("Movie Sync ready: %1 look(s), %2 stream cue file(s)")
                                           .arg(effect_list_->count())
                                           .arg(stream_cues_.Count()),
             false);
}

RGBColor MovieSyncEffect::CalculateColorGrid(float, float, float, float, const GridContext3D&)
{
    return ToRGBColor(0, 0, 0);
}

nlohmann::json MovieSyncEffect::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["movie_sync_api"] = api_enabled_;
    j["movie_sync_allow_strobe"] = allow_photosensitive_;
    j["movie_sync_port"] = port_;
    j["movie_sync_lights_delay_ms"] = lights_delay_ms_;
    j["movie_sync_auto_photosensitive_safety"] = auto_safety_;
    j["movie_sync_intro_look"] = marker_look_[0];
    j["movie_sync_recap_look"] = marker_look_[1];
    j["movie_sync_credits_look"] = marker_look_[2];
    return j;
}

void MovieSyncEffect::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    api_enabled_ = settings.value("movie_sync_api", true);
    allow_photosensitive_ = settings.value("movie_sync_allow_strobe", false);
    port_ = static_cast<quint16>(settings.value("movie_sync_port", 27180));
    lights_delay_ms_ = std::clamp(settings.value("movie_sync_lights_delay_ms", 0), -2000, 2000);
    auto_safety_ = settings.value("movie_sync_auto_photosensitive_safety", false);
    marker_look_[0] = settings.value("movie_sync_intro_look", std::string());
    marker_look_[1] = settings.value("movie_sync_recap_look", std::string());
    marker_look_[2] = settings.value("movie_sync_credits_look", std::string());
    if(auto_safety_check_)
    {
        QSignalBlocker b(auto_safety_check_);
        auto_safety_check_->setChecked(auto_safety_);
    }
    refreshMarkerCombos();
    if(lights_delay_spin_)
    {
        QSignalBlocker b(lights_delay_spin_);
        lights_delay_spin_->setValue(lights_delay_ms_);
    }
    if(api_check_)
    {
        QSignalBlocker b(api_check_);
        api_check_->setChecked(api_enabled_);
    }
    if(allow_strobe_check_)
    {
        QSignalBlocker b(allow_strobe_check_);
        allow_strobe_check_->setChecked(allow_photosensitive_);
    }
    /* Saved choice applies; a marked title playing now is looked at again next tick. */
    safety_forced_ = false;
    details_sig_.clear();
    runtime_.SetAllowPhotosensitive(allow_photosensitive_);
    if(api_enabled_)
    {
        onReloadCatalog();
        startListener();
    }
    else
    {
        stopListener();
    }
}

void MovieSyncEffect::onApiToggled(bool on)
{
    api_enabled_ = on;
    if(on)
    {
        startListener();
    }
    else
    {
        stopListener();
        runtime_.StopAll();
        tick_timer_->stop();
        setPlayingUi(false);
        clearPreviewIfIdle();
        setStatus(tr("Script API off"));
    }
}

void MovieSyncEffect::onAllowStrobeToggled(bool on)
{
    if(on && safety_forced_)
    {
        /* Allowed by hand while auto safety held a marked title: the user decides. */
        safety_forced_ = false;
        safety_override_key_ = stream_match_key_;
    }
    allow_photosensitive_ = on;
    details_sig_.clear();
    runtime_.SetAllowPhotosensitive(on);
    if(!runtime_.IsPlaying())
    {
        tick_timer_->stop();
        setPlayingUi(false);
        clearPreviewIfIdle();
    }
}

void MovieSyncEffect::onReloadCatalog()
{
    const filesystem::path movie_dir = movieEffectsDir();
    catalog_ = {};
    std::string err;
    MovieSync::LoadCatalog(MovieSync::CatalogFilePath(movie_dir), &catalog_, &err);
    MovieSync::ScanCategoryFolders(movie_dir, &catalog_);

    runtime_.SetMovieEffectsDir(movie_dir);
    runtime_.SetCatalog(&catalog_);

    /* Stream cue files: re-matched (and the play list rebuilt) against whatever plays next tick. */
    resetStream();
    /* movie_dir is movie-sync/effects; stream cue files live beside it in movie-sync/streams. */
    filesystem::path base = movie_dir.lexically_normal();
    if(base.filename().empty())
    {
        base = base.parent_path(); /* trailing slash */
    }
    stream_cues_.Load((base.filename() == "effects" ? base.parent_path() : base) / "streams");

    refreshCategoryCombo();
    refreshEffectList();
    if(listener_ && listener_->isListening())
    {
        setStatus(tr("Listening on 127.0.0.1:%1 — %2 Movie Sync look(s)")
                      .arg(listener_->port())
                      .arg(catalog_.effects.size()));
    }
    else
    {
        if(catalog_.effects.empty())
        {
            setStatus(tr("No looks in movie-sync/"));
        }
        else
        {
            setStatus(tr("Catalog: %1 Movie Sync look(s)").arg(catalog_.effects.size()));
        }
    }
}

void MovieSyncEffect::onCategoryChanged(int)
{
    refreshEffectList();
}

void MovieSyncEffect::onPreview()
{
    fireSelectedCatalogEffect();
}

void MovieSyncEffect::onStopPreview()
{
    runtime_.StopAll();
    stream_player_.ForgetLevels();
    tick_timer_->stop();
    setPlayingUi(false);
    clearPreviewIfIdle();
    restoreIdleStatus();
}

void MovieSyncEffect::onEffectSelectionChanged()
{
    const QString id = selectedEffectId();
    if(!notes_label_)
    {
        return;
    }
    if(id.isEmpty())
    {
        notes_label_->setText(tr("Notes: —"));
        return;
    }
    const MovieSync::CatalogEffect* meta = MovieSync::FindEffect(catalog_, id.toStdString());
    if(!meta)
    {
        notes_label_->setText(tr("Notes: unknown look (%1)").arg(id));
        return;
    }
    const QString notes = meta->photosensitive ? tr("photosensitive") : tr("none");
    // Sidecar authors need the stable Movie Sync id next to the display name.
    notes_label_->setText(tr("Notes: %1  ·  id: %2").arg(notes, id));
}

void MovieSyncEffect::onEffectDoubleClicked()
{
    fireSelectedCatalogEffect();
}

void MovieSyncEffect::onTick()
{
    int dt = Action::MeasureTickMs(&last_tick_ms_);
    /* The film is paused: running looks freeze with it (and carry on when it plays). */
    if(stream_player_.Active() && stream_player_.State().paused)
    {
        dt = 0;
    }
    if(!runtime_.Tick(dt))
    {
        tick_timer_->stop();
        setPlayingUi(false);
        // Once packs must go black when they finish (do not leave the last lit frame).
        clearPreviewIfIdle();
        restoreIdleStatus();
    }
}

void MovieSyncEffect::bindTab()
{
    if(!tab_)
    {
        for(QWidget* w = parentWidget(); w; w = w->parentWidget())
        {
            if(OpenRGB3DSpatialTab* t = qobject_cast<OpenRGB3DSpatialTab*>(w))
            {
                tab_ = t;
                break;
            }
        }
    }
    if(!tab_ && qApp)
    {
        const QWidgetList tops = qApp->topLevelWidgets();
        for(QWidget* top : tops)
        {
            if(OpenRGB3DSpatialTab* t = qobject_cast<OpenRGB3DSpatialTab*>(top))
            {
                tab_ = t;
                break;
            }
            if(OpenRGB3DSpatialTab* t = top->findChild<OpenRGB3DSpatialTab*>())
            {
                tab_ = t;
                break;
            }
        }
    }
    if(!tab_)
    {
        return;
    }
    runtime_.SetApplyCallbacks(
        [this]() {
            if(tab_)
            {
                tab_->PrepareActionPreview();
            }
        },
        [this](const Action::Pack& pack, int local_ms, bool force_hw) {
            if(tab_)
            {
                tab_->ApplyActionPreviewFrame(pack, local_ms, force_hw, this);
            }
        });
}

void MovieSyncEffect::refreshCategoryCombo()
{
    if(!category_combo_)
    {
        return;
    }
    const QString previous = category_combo_->currentData().toString();
    QSignalBlocker blocker(category_combo_);
    category_combo_->clear();
    category_combo_->setEnabled(true);

    std::set<std::string> cats;
    for(const MovieSync::CatalogEffect& e : catalog_.effects)
    {
        cats.insert(e.category.empty() ? "uncategorized" : e.category);
    }
    for(const std::string& cat : cats)
    {
        category_combo_->addItem(QString::fromStdString(cat), QString::fromStdString(cat));
    }
    if(category_combo_->count() == 0)
    {
        category_combo_->addItem(tr("(empty)"), QString());
        return;
    }
    int idx = category_combo_->findData(previous);
    if(idx < 0)
    {
        idx = 0;
    }
    category_combo_->setCurrentIndex(idx);
}

void MovieSyncEffect::refreshEffectList()
{
    if(!effect_list_)
    {
        return;
    }
    const QString previous_id = selectedEffectId();
    QSignalBlocker blocker(effect_list_);
    effect_list_->clear();

    const QString cat = selectedCategory();
    for(const MovieSync::CatalogEffect& e : catalog_.effects)
    {
        const QString ecat = QString::fromStdString(e.category.empty() ? "uncategorized" : e.category);
        if(!cat.isEmpty() && ecat != cat)
        {
            continue;
        }
        auto* item = new QListWidgetItem(QString::fromStdString(e.name));
        item->setData(Qt::UserRole, QString::fromStdString(e.id));
        QString tip = tr("Movie Sync id: %1").arg(QString::fromStdString(e.id));
        if(e.photosensitive)
        {
            tip += tr(" · photosensitive");
        }
        item->setToolTip(tip);
        effect_list_->addItem(item);
    }

    int restore = -1;
    for(int i = 0; i < effect_list_->count(); ++i)
    {
        if(effect_list_->item(i)->data(Qt::UserRole).toString() == previous_id)
        {
            restore = i;
            break;
        }
    }
    if(restore < 0 && effect_list_->count() > 0)
    {
        restore = 0;
    }
    effect_list_->setCurrentRow(restore);
    onEffectSelectionChanged();
    refreshMarkerCombos();
}

void MovieSyncEffect::setStatus(const QString& text)
{
    if(status_label_)
    {
        status_label_->setText(text);
    }
}

void MovieSyncEffect::setPlayingUi(bool playing)
{
    if(preview_button_)
    {
        preview_button_->setEnabled(!playing);
    }
    if(stop_button_)
    {
        stop_button_->setEnabled(playing);
    }
}

void MovieSyncEffect::clearPreviewIfIdle()
{
    if(tab_ && !runtime_.IsPlaying())
    {
        tab_->ClearActionPreview(true, this);
    }
}

void MovieSyncEffect::restoreIdleStatus()
{
    if(listener_ && listener_->isListening())
    {
        setStatus(tr("Listening on 127.0.0.1:%1 — %2 Movie Sync look(s)")
                      .arg(listener_->port())
                      .arg(catalog_.effects.size()));
    }
    else if(!api_enabled_)
    {
        setStatus(tr("Script API off"));
    }
    else if(catalog_.effects.empty())
    {
        setStatus(tr("No looks in movie-sync/"));
    }
    else
    {
        setStatus(tr("Catalog: %1 Movie Sync look(s)").arg(catalog_.effects.size()));
    }
}

void MovieSyncEffect::startListener()
{
    bindTab();
    if(catalog_.effects.empty())
    {
        onReloadCatalog();
    }
    QString err;
    if(!listener_->Start(port_, &err))
    {
        setStatus(tr("API failed: %1").arg(err));
        return;
    }
    port_ = listener_->port();
    setStatus(tr("Listening on 127.0.0.1:%1 — %2 Movie Sync look(s)")
                  .arg(port_)
                  .arg(catalog_.effects.size()));
    system_media_.Start();
    if(stream_timer_ && !stream_timer_->isActive())
    {
        stream_timer_->start();
    }
}

void MovieSyncEffect::stopListener()
{
    if(listener_)
    {
        listener_->Stop();
    }
    if(stream_timer_)
    {
        stream_timer_->stop();
    }
    system_media_.Stop();
    resetStream();
}

void MovieSyncEffect::fireSelectedCatalogEffect()
{
    const QString id = selectedEffectId();
    if(id.isEmpty())
    {
        setStatus(tr("Select a look first"));
        return;
    }
    if(!api_enabled_ || !listener_->isListening())
    {
        startListener();
    }
    bindTab();
    runtime_.SetAllowPhotosensitive(allow_photosensitive_);
    runtime_.StopAll();
    stream_player_.ForgetLevels();
    clearPreviewIfIdle();
    runtime_.OnEffect(id.toStdString(), "pulse", true);
    if(!runtime_.lastWarning().empty())
    {
        setPlayingUi(false);
        setStatus(humanizeRuntimeMessage(runtime_.lastWarning()));
        return;
    }
    if(!runtime_.lastError().empty())
    {
        setPlayingUi(false);
        setStatus(humanizeRuntimeMessage(runtime_.lastError()));
        return;
    }
    setStatus(tr("Preview: %1").arg(effectDisplayName(id)));
    setPlayingUi(runtime_.IsPlaying());
    if(runtime_.IsPlaying() && !tick_timer_->isActive())
    {
        last_tick_ms_ = 0;
        tick_timer_->start();
    }
}

QString MovieSyncEffect::selectedEffectId() const
{
    if(!effect_list_)
    {
        return {};
    }
    QListWidgetItem* item = effect_list_->currentItem();
    if(!item)
    {
        return {};
    }
    return item->data(Qt::UserRole).toString();
}

QString MovieSyncEffect::selectedCategory() const
{
    if(!category_combo_)
    {
        return {};
    }
    return category_combo_->currentData().toString();
}

filesystem::path MovieSyncEffect::movieEffectsDir() const
{
    return PluginSettingsPaths::MovieSyncEffectsDir(OpenRGB3DSpatialPlugin::APIPointer);
}

std::string MovieSyncEffect::catalogJson() const
{
    nlohmann::json j;
    j["effects"] = nlohmann::json::array();
    for(const MovieSync::CatalogEffect& e : catalog_.effects)
    {
        j["effects"].push_back({
            {"id", e.id},
            {"name", e.name},
            {"category", e.category},
            {"source", "movie_sync"},
            {"photosensitive", e.photosensitive},
        });
    }
    return j.dump();
}
