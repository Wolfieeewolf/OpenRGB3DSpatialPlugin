// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "LaserFrame.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

/** One ShowEditor timeline key event (from .seq). */
struct ShowSeqEvent
{
    double time_ms = 0.0;
    char key = 0;
    /** Key.ord slot / ASCII index — resolves to a .heb name. */
    int key_index = 0;
    /**
     * ShowEditor output track / “Spur” (0–11 typical).
     * Encoded in .seq as pre-byte = 1 + track * 19 (0x01, 0x14, 0x27, 0x3A, …).
     * Multi-projector packs route Spur A/B/C/D (and beyond) on separate tracks.
     */
    int track = 0;
    /** Raw pre-byte from the record (0 = legacy / unknown). */
    int track_code = 0;
};

/**
 * Loaded ShowEditor show folder (.shw + Key.ord + .seq + .heb figures).
 * Playback: pick active figures at time_ms and composite into a LaserFrame.
 */
struct ShowFolder
{
    std::string source_path;
    std::string show_name;
    std::string audio_name;
    /** Free-text notes from [INFO].txt / embedded .info strings. */
    std::string info_notes;
    std::string error;

    /** Key.ord lines; index = ASCII code of the keyboard key. */
    std::vector<std::string> key_slots;
    std::vector<ShowSeqEvent> events;
    /** filename → figure frames (multi-frame HEBs animate via fps). */
    std::unordered_map<std::string, LaserShow> figures;

    /** Distinct Spur/track indices seen in the timeline (sorted). */
    std::vector<int> tracks;
    double duration_ms = 0.0;

    bool ok() const { return error.empty() && !events.empty(); }

    /**
     * Build the composite lit geometry for show clock time_ms.
     * @param track_filter  -1 = all tracks; else only that Spur/track.
     * Each (key, track) stays on its last figure until the next press of the
     * same key on the same track (or show end).
     */
    LaserFrame FrameAt(double time_ms, float figure_fps, int track_filter = -1) const;
};

namespace ShowFolderReader
{
/** Load by .shw path or any file inside the show folder. */
ShowFolder LoadPath(const std::string& shw_or_folder_path);
}
