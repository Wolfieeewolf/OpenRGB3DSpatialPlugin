// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* Movie Sync for streaming (Netflix, Prime Video, Crunchyroll, Disney+, ...).

   What is playing and where comes from the browser extension (integrations/browser-extension,
   POST /v1/stream) or, on Windows, the system media controls. A cue file in
   movie-sync/streams/ is matched to it (by service + title ID, else title + length) and its
   cues are played on the stream's clock: pulses fire as playback passes them, levels hold while
   playback is inside them, pause / seek / speed / ads are followed. Only timing and IDs are
   used, never the picture (no DRM is touched). */

#include "filesystem.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MovieSync
{

/** One playback report. */
struct StreamState
{
    std::string source;      /* "extension" or "windows" */
    std::string service;     /* netflix, prime, crunchyroll, disney, ... */
    std::string id;          /* the service's title / episode ID ("" if unknown) */
    std::string title;       /* show or film */
    std::string episode;     /* episode label, if any */
    double position_s = 0.0;
    double duration_s = 0.0;
    double rate = 1.0;
    bool paused = true;
    bool ad = false;         /* an ad is playing: cues hold */
    std::int64_t received_ms = 0; /* steady clock when this arrived */
};

struct StreamCue
{
    std::int64_t t_ms = 0;
    std::int64_t t_end_ms = -1; /* level cues: end; -1 = pulse */
    std::string effect_id;
    std::string edge = "pulse";
};

struct StreamMatch
{
    std::string service;
    std::string id;
};

/** A cue file for streams: the usual Movie Sync sidecar plus "streams": [{service, id}]. */
struct StreamCueFile
{
    filesystem::path path;
    std::string title;
    std::vector<StreamMatch> streams;
    std::int64_t duration_ms = 0;
    std::int64_t duration_tolerance_ms = 5000;
    std::int64_t offset_ms = 0;
    std::vector<StreamCue> cues; /* sorted by t_ms */
};

/** Parse one cue file. */
bool LoadStreamCueFile(const filesystem::path& path, StreamCueFile* out, std::string* error);

/** All cue files under a folder, and which one fits a playback report. */
class StreamCueLibrary
{
public:
    void Load(const filesystem::path& dir);
    /** Best file for this report: exact service + ID first, then title + length. */
    const StreamCueFile* Match(const StreamState& state) const;
    std::size_t Count() const { return files_.size(); }
    /** Files that did not load (and duplicate IDs), for the status line. */
    const std::vector<std::string>& Errors() const { return errors_; }

private:
    std::vector<StreamCueFile> files_;
    std::vector<std::string> errors_;
};

/** Plays a cue file on a stream's clock. Tick() often (~20 ms). */
class StreamCuePlayer
{
public:
    /** effect_id, edge ("pulse" / "level"), active. */
    using EdgeFn = std::function<void(const std::string&, const std::string&, bool)>;

    void SetEdgeHandler(EdgeFn fn) { on_edge_ = std::move(fn); }
    /** Lights delay: positive = cues fire later (a TV / Bluetooth picture that lags behind),
        negative = earlier (slow LED devices). */
    void SetLightsDelayMs(int ms) { lights_delay_ms_ = ms; }
    /** The held looks were stopped elsewhere (Stop / Preview): re-hold them on the next tick. */
    void ForgetLevels();

    /** A new playback report (and the cue file that matches it, or null). */
    void OnState(const StreamState& state, const StreamCueFile* file, std::int64_t now_ms);
    void Tick(std::int64_t now_ms);
    /** Stop: release any held levels. */
    void Stop();

    /** Where the stream is now (seconds), counted on from the last report. */
    double EstimatedPosition(std::int64_t now_ms) const;
    const StreamState& State() const { return state_; }
    const StreamCueFile* File() const { return file_; }
    bool Active() const { return file_ != nullptr && has_state_; }

private:
    void ReleaseLevels();

    EdgeFn on_edge_;
    int lights_delay_ms_ = 0;
    StreamState state_;
    bool has_state_ = false;
    const StreamCueFile* file_ = nullptr;
    std::string file_key_;
    std::int64_t last_pos_ms_ = 0;
    bool has_last_ = false; /* positions can be negative (offsets), so no -1 marker */
    std::vector<std::string> levels_on_;
};

} // namespace MovieSync
