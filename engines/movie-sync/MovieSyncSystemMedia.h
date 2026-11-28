// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* The system's "now playing": Windows media controls (the volume flyout) and, on Linux,
   MPRIS (D-Bus; Chrome, Firefox, VLC, mpv, Celluloid ... all report there). Apps report title,
   play state and position, so Movie Sync can follow them with nothing installed (by title and
   length; the browser extension adds exact IDs). Polled on a worker thread. Does nothing on
   macOS (the browser extension covers it). */

#include "MovieSyncStream.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace MovieSync
{

class SystemMediaReader
{
public:
    SystemMediaReader() = default;
    ~SystemMediaReader();
    SystemMediaReader(const SystemMediaReader&) = delete;
    SystemMediaReader& operator=(const SystemMediaReader&) = delete;

    void Start();
    void Stop();
    /** Latest report (position counted to the moment it was read); false if nothing plays. */
    bool Latest(StreamState* out) const;
    /** The playing app's thumbnail (encoded image bytes) whose artwork_url is "smtc:<serial>". */
    std::vector<std::uint8_t> Thumbnail(std::uint64_t serial) const;

private:
    void Run();

    std::thread thread_;
    std::atomic<bool> stop_{false};
    mutable std::mutex mutex_;
    StreamState latest_;
    bool has_ = false;
    std::vector<std::uint8_t> thumb_;
    std::uint64_t thumb_serial_ = 0;
    std::string thumb_key_;
    int thumb_tries_ = 0;
    std::int64_t thumb_at_ms_ = 0;
};

} // namespace MovieSync
