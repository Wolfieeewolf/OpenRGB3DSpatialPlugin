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
#endif

} // namespace

SystemMediaReader::~SystemMediaReader()
{
    Stop();
}

void SystemMediaReader::Start()
{
#ifdef _WIN32
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
#endif
}

} // namespace MovieSync
