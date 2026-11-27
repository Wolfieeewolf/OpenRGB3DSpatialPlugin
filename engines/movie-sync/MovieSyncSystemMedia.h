// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* Windows system media controls (the "now playing" Windows shows on the volume flyout):
   apps such as Crunchyroll and Prime Video report title, play state and position there, so
   Movie Sync can follow them with nothing installed. Polled on a worker thread. Does nothing
   on Linux / macOS (they use the browser extension). */

#include "MovieSyncStream.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

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

private:
    void Run();

    std::thread thread_;
    std::atomic<bool> stop_{false};
    mutable std::mutex mutex_;
    StreamState latest_;
    bool has_ = false;
};

} // namespace MovieSync
