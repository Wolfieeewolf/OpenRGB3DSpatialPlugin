// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPack.h"
#include <algorithm>

namespace EffectPack
{

/** Lightweight playback clock for a loaded pack (no OpenRGB device I/O yet). */
class Player
{
public:
    void SetPack(const Pack& pack)
    {
        pack_ = pack;
        elapsed_ms_ = 0;
        playing_ = false;
    }

    const Pack& GetPack() const { return pack_; }

    void Play() { playing_ = true; }
    void Stop()
    {
        playing_ = false;
        elapsed_ms_ = 0;
    }
    void Pause() { playing_ = false; }

    bool IsPlaying() const { return playing_; }
    int ElapsedMs() const { return elapsed_ms_; }

    /** Advance clock by dt_ms. event_active matters for while_active loop mode. */
    bool Tick(int dt_ms, bool event_active)
    {
        if(!playing_)
        {
            return false;
        }
        elapsed_ms_ += std::max(0, dt_ms);
        int local = 0;
        if(!MapPlaybackTime(pack_, elapsed_ms_, event_active, &local))
        {
            playing_ = false;
            return false;
        }
        local_ms_ = local;
        return true;
    }

    int LocalMs() const { return local_ms_; }

    bool ColorForTrack(size_t track_index, RGBColor* out_color, float* out_intensity) const
    {
        if(track_index >= pack_.tracks.size())
        {
            return false;
        }
        return EvaluateTrackColor(pack_.tracks[track_index], local_ms_, out_color, out_intensity);
    }

private:
    Pack pack_;
    int elapsed_ms_ = 0;
    int local_ms_ = 0;
    bool playing_ = false;
};

} // namespace EffectPack
