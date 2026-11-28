// SPDX-License-Identifier: GPL-2.0-or-later
/* Kept free of Qt headers: C++/WinRT and Qt's keyword macros do not mix. */

#include "MovieSyncSystemMedia.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <string>

#ifdef _WIN32
/* The project builds as C++17, where C++/WinRT falls back to the older coroutine headers;
   this file only uses blocking .get(), so the deprecation does not matter here. */
#ifndef _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#endif
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#elif defined(__linux__)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#endif

namespace MovieSync
{

namespace
{

#ifdef _WIN32
std::int64_t SteadyMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/* Wait at most `ms` for a WinRT call into another app: a hung app must never hang OpenRGB
   (Stop() joins this thread on the UI thread). */
template<typename Op>
bool WaitFor(const Op& op, int ms)
{
    if(op.wait_for(std::chrono::milliseconds(ms)) == winrt::Windows::Foundation::AsyncStatus::Completed)
    {
        return true;
    }
    try
    {
        op.Cancel();
    }
    catch(...)
    {
    }
    return false;
}
std::string Narrow(const winrt::hstring& s)
{
    return winrt::to_string(s);
}

std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

/* Which service an app is, from its app ID (e.g. 4DF9E0F8.Netflix_...!Netflix.App). */
std::string ServiceFromApp(const std::string& app)
{
    const std::string a = Lower(app);
    if(a.find("netflix") != std::string::npos) return "netflix";
    if(a.find("primevideo") != std::string::npos || a.find("amazonvideo") != std::string::npos) return "prime";
    if(a.find("crunchyroll") != std::string::npos) return "crunchyroll";
    if(a.find("disney") != std::string::npos) return "disney";
    if(a.rfind("stan", 0) == 0 || a.find(".stan") != std::string::npos) return "stan";
    if(a.find("binge") != std::string::npos) return "binge";
    if(a.find("youtube") != std::string::npos) return "youtube";
    if(a.find("appletv") != std::string::npos) return "appletv";
    if(a.find("vlc") != std::string::npos) return "vlc";
    return "app:" + app;
}
#elif defined(__linux__)
std::int64_t SteadyMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

/* Which service: an app that names it (a desktop web app), else the player app itself. */
std::string ServiceFromPlayer(const std::string& bus_name, const std::string& identity, const std::string& title)
{
    const std::string a = Lower(bus_name + " " + identity);
    if(a.find("netflix") != std::string::npos) return "netflix";
    if(a.find("primevideo") != std::string::npos || a.find("prime video") != std::string::npos) return "prime";
    if(a.find("crunchyroll") != std::string::npos) return "crunchyroll";
    if(a.find("vlc") != std::string::npos) return "vlc";
    (void)title;
    /* org.mpris.MediaPlayer2.firefox.instance_1_42 -> app:firefox */
    std::string app = bus_name.substr(std::string("org.mpris.MediaPlayer2.").size());
    const std::size_t dot = app.find('.');
    if(dot != std::string::npos)
    {
        app = app.substr(0, dot);
    }
    return "app:" + Lower(app);
}

/* A D-Bus a{sv} (often wrapped in QDBusArgument) as a QVariantMap. */
QVariantMap ToMap(const QVariant& v)
{
    if(v.canConvert<QDBusArgument>())
    {
        QVariantMap m;
        v.value<QDBusArgument>() >> m;
        return m;
    }
    return v.toMap();
}

QVariant Unwrap(const QVariant& v)
{
    return v.canConvert<QDBusVariant>() ? v.value<QDBusVariant>().variant() : v;
}
#endif

} // namespace

SystemMediaReader::~SystemMediaReader()
{
    Stop();
}

void SystemMediaReader::Start()
{
#if defined(_WIN32) || defined(__linux__)
    if(thread_.joinable())
    {
        return;
    }
    stop_ = false;
    thread_ = std::thread([this]() { Run(); });
#endif
}

void SystemMediaReader::Stop()
{
    stop_ = true;
    if(thread_.joinable())
    {
        thread_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    has_ = false;
}

std::vector<std::uint8_t> SystemMediaReader::Thumbnail(std::uint64_t serial) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return serial == thumb_serial_ ? thumb_ : std::vector<std::uint8_t>();
}

bool SystemMediaReader::Latest(StreamState* out) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!has_ || !out)
    {
        return false;
    }
    *out = latest_;
    return true;
}

void SystemMediaReader::Run()
{
#ifdef _WIN32
    using namespace winrt::Windows::Media::Control;
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    catch(...)
    {
    }
    GlobalSystemMediaTransportControlsSessionManager manager{nullptr};
    while(!stop_)
    {
        try
        {
            if(!manager)
            {
                const auto op = GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
                if(!WaitFor(op, 2000))
                {
                    throw std::runtime_error("media controls unavailable");
                }
                manager = op.GetResults();
            }
            /* A playing video service wins, then anything playing (music apps lose to films),
               else the session Windows calls current. */
            GlobalSystemMediaTransportControlsSession pick{nullptr};
            int best = -1;
            for(const auto& s : manager.GetSessions())
            {
                const bool playing = s.GetPlaybackInfo().PlaybackStatus()
                                     == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
                const bool video = ServiceFromApp(Narrow(s.SourceAppUserModelId())).rfind("app:", 0) != 0;
                const int score = (playing ? 2 : 0) + (video ? 1 : 0);
                if(playing && score > best)
                {
                    best = score;
                    pick = s;
                }
            }
            if(!pick)
            {
                pick = manager.GetCurrentSession();
            }
            StreamState st;
            bool ok = false;
            if(pick)
            {
                const auto props_op = pick.TryGetMediaPropertiesAsync();
                if(!WaitFor(props_op, 500))
                {
                    throw std::runtime_error("media app not answering");
                }
                const auto props = props_op.GetResults();
                const auto timeline = pick.GetTimelineProperties();
                const auto playback = pick.GetPlaybackInfo();
                st.source = "windows";
                st.service = ServiceFromApp(Narrow(pick.SourceAppUserModelId()));
                st.title = Narrow(props.Title());
                const std::string prime_prefix = "Prime Video: ";
                if(st.title.rfind(prime_prefix, 0) == 0)
                {
                    st.title = st.title.substr(prime_prefix.size());
                }
                st.episode = Narrow(props.Artist()); /* Crunchyroll: show in artist, episode in title */
                if(!st.episode.empty())
                {
                    std::swap(st.title, st.episode);
                }
                if(st.service == "netflix" && st.title == "Netflix")
                {
                    st.title.clear(); /* the Netflix app does not say what is playing */
                }
                st.paused = playback.PlaybackStatus()
                            != GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
                const auto rate = playback.PlaybackRate();
                st.rate = rate ? rate.Value() : 1.0;
                const double ticks_per_s = 10000000.0;
                double pos = (double)timeline.Position().count() / ticks_per_s;
                /* The app reports a position at LastUpdatedTime; count on to now while playing. */
                const auto updated = timeline.LastUpdatedTime();
                if(!st.paused && updated.time_since_epoch().count() > 0)
                {
                    const double age = (double)(winrt::clock::now() - updated).count() / ticks_per_s;
                    if(age > 0.0 && age < 6 * 3600.0)
                    {
                        pos += age * st.rate;
                    }
                }
                st.position_s = pos;
                st.duration_s = (double)(timeline.EndTime() - timeline.StartTime()).count() / ticks_per_s;
                st.received_ms = SteadyMs();
                ok = st.duration_s > 0.0;

                /* Thumbnail: read once per title (apps often set it a moment after the title,
                   so try again every 2 s, a few times, while there is none). */
                const std::string tkey = st.service + "|" + st.title + "|" + st.episode;
                if(tkey != thumb_key_)
                {
                    thumb_key_ = tkey;
                    thumb_tries_ = 0;
                    thumb_at_ms_ = 0;
                    std::lock_guard<std::mutex> lock(mutex_);
                    thumb_.clear();
                    ++thumb_serial_;
                }
                bool have_thumb;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    have_thumb = !thumb_.empty();
                }
                if(!have_thumb && thumb_tries_ < 5 && st.received_ms - thumb_at_ms_ > 2000)
                {
                    ++thumb_tries_;
                    thumb_at_ms_ = st.received_ms;
                    std::vector<std::uint8_t> bytes;
                    try
                    {
                        const auto ref = props.Thumbnail();
                        if(ref)
                        {
                            const auto open_op = ref.OpenReadAsync();
                            if(WaitFor(open_op, 1000))
                            {
                                const auto stream = open_op.GetResults();
                                const std::uint64_t size = stream.Size();
                                if(size > 0 && size <= 8u * 1024u * 1024u)
                                {
                                    winrt::Windows::Storage::Streams::DataReader reader(stream);
                                    const auto load_op = reader.LoadAsync((std::uint32_t)size);
                                    if(WaitFor(load_op, 1000))
                                    {
                                        bytes.resize(load_op.GetResults());
                                        reader.ReadBytes(bytes);
                                    }
                                }
                            }
                        }
                    }
                    catch(...)
                    {
                        bytes.clear();
                    }
                    if(!bytes.empty())
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        thumb_ = std::move(bytes);
                        ++thumb_serial_;
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if(!thumb_.empty())
                    {
                        st.artwork_url = "smtc:" + std::to_string(thumb_serial_);
                    }
                }
            }
            std::lock_guard<std::mutex> lock(mutex_);
            latest_ = st;
            has_ = ok;
        }
        catch(...)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            has_ = false;
            manager = nullptr;
        }
        for(int i = 0; i < 5 && !stop_; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
#elif defined(__linux__)
    /* MPRIS: every org.mpris.MediaPlayer2.* on the session bus; a playing video player or
       browser wins, then anything playing. Calls time out quickly (a hung player must not hold
       OpenRGB up: Stop() joins this thread). */
    QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("orgb_movie_sync_mpris"));
    const auto call = [&bus](const QString& service, const QString& path, const QString& iface, const QString& method,
                             const QVariantList& args) {
        QDBusMessage m = QDBusMessage::createMethodCall(service, path, iface, method);
        m.setArguments(args);
        return bus.call(m, QDBus::Block, 400);
    };
    while(!stop_)
    {
        StreamState best;
        int best_score = -1;
        if(bus.isConnected() && bus.interface())
        {
            const QDBusReply<QStringList> names = bus.interface()->registeredServiceNames();
            for(const QString& name : names.value())
            {
                if(stop_ || !name.startsWith(QStringLiteral("org.mpris.MediaPlayer2.")))
                {
                    continue;
                }
                const QDBusMessage props = call(name, QStringLiteral("/org/mpris/MediaPlayer2"),
                                                QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"),
                                                {QStringLiteral("org.mpris.MediaPlayer2.Player")});
                if(props.type() != QDBusMessage::ReplyMessage || props.arguments().isEmpty())
                {
                    continue;
                }
                const QVariantMap p = ToMap(props.arguments().at(0));
                const QVariantMap md = ToMap(Unwrap(p.value(QStringLiteral("Metadata"))));
                const QString status = Unwrap(p.value(QStringLiteral("PlaybackStatus"))).toString();
                const qlonglong length_us = Unwrap(md.value(QStringLiteral("mpris:length"))).toLongLong();
                if(length_us <= 0)
                {
                    continue; /* live, or nothing loaded */
                }
                QString identity;
                const QDBusMessage idm = call(name, QStringLiteral("/org/mpris/MediaPlayer2"),
                                              QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"),
                                              {QStringLiteral("org.mpris.MediaPlayer2"), QStringLiteral("Identity")});
                if(idm.type() == QDBusMessage::ReplyMessage && !idm.arguments().isEmpty())
                {
                    identity = Unwrap(idm.arguments().at(0)).toString();
                }
                StreamState st;
                st.source = "windows"; /* "the system's media controls" */
                st.title = Unwrap(md.value(QStringLiteral("xesam:title"))).toString().toStdString();
                const QStringList artists = Unwrap(md.value(QStringLiteral("xesam:artist"))).toStringList();
                if(!artists.isEmpty() && !artists.front().isEmpty())
                {
                    /* Show in artist, episode in title (as Crunchyroll and others report it). */
                    st.episode = st.title;
                    st.title = artists.front().toStdString();
                }
                st.service = ServiceFromPlayer(name.toStdString(), identity.toStdString(), st.title);
                st.paused = status != QStringLiteral("Playing");
                bool rate_ok = false;
                const double rate = Unwrap(p.value(QStringLiteral("Rate"))).toDouble(&rate_ok);
                st.rate = rate_ok && rate > 0.0 ? rate : 1.0;
                st.position_s = (double)Unwrap(p.value(QStringLiteral("Position"))).toLongLong() / 1e6;
                st.duration_s = (double)length_us / 1e6;
                st.received_ms = SteadyMs();
                const QString art = Unwrap(md.value(QStringLiteral("mpris:artUrl"))).toString();
                if(art.startsWith(QStringLiteral("https://")) && art.size() <= 2048)
                {
                    st.artwork_url = art.toStdString();
                }
                else if(art.startsWith(QStringLiteral("file://")))
                {
                    st.artwork_url = "file:" + QUrl(art).toLocalFile().toStdString();
                }
                const bool playing = !st.paused;
                const std::string app = Lower(name.toStdString());
                const bool video = app.find("firefox") != std::string::npos || app.find("chrom") != std::string::npos
                                   || app.find("brave") != std::string::npos || app.find("edge") != std::string::npos
                                   || app.find("vivaldi") != std::string::npos || app.find("opera") != std::string::npos
                                   || app.find("vlc") != std::string::npos || app.find("mpv") != std::string::npos
                                   || app.find("celluloid") != std::string::npos || app.find("totem") != std::string::npos
                                   || st.service.rfind("app:", 0) != 0;
                const int score = (playing ? 2 : 0) + (video ? 1 : 0);
                if(score > best_score)
                {
                    best_score = score;
                    best = st;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            latest_ = best;
            has_ = best_score >= 0;
        }
        for(int i = 0; i < 5 && !stop_; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    QDBusConnection::disconnectFromBus(QStringLiteral("orgb_movie_sync_mpris"));
#endif
}

} // namespace MovieSync
