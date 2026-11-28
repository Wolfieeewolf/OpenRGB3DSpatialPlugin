// SPDX-License-Identifier: GPL-2.0-or-later

#include "MovieSyncStream.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

namespace MovieSync
{

namespace
{

std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

/* Letters and digits only, lower case: "Chainsmoker Cat" == "chainsmoker-cat". */
std::string Fold(const std::string& s)
{
    std::string out;
    for(unsigned char c : s)
    {
        if(std::isalnum(c))
        {
            out.push_back((char)std::tolower(c));
        }
    }
    return out;
}

} // namespace

bool LoadStreamCueFile(const filesystem::path& path, StreamCueFile* out, std::string* error)
{
    try
    {
        std::ifstream in(path);
        if(!in)
        {
            if(error) *error = "cannot open";
            return false;
        }
        const nlohmann::json j = nlohmann::json::parse(in);
        StreamCueFile f;
        f.path = path;
        f.title = j.value("title", path.stem().string());
        f.duration_ms = j.value("duration_ms", (std::int64_t)0);
        f.duration_tolerance_ms = j.value("duration_tolerance_ms", (std::int64_t)5000);
        f.offset_ms = j.value("offset_ms", (std::int64_t)0);
        if(j.contains("streams") && j["streams"].is_array())
        {
            for(const auto& s : j["streams"])
            {
                StreamMatch m;
                m.service = Lower(s.value("service", ""));
                /* IDs may be written as numbers (Netflix): accept both. */
                if(s.contains("id") && s["id"].is_number_integer())
                {
                    m.id = std::to_string(s["id"].get<long long>());
                }
                else if(s.contains("id") && s["id"].is_string())
                {
                    m.id = s["id"].get<std::string>();
                }
                if(!m.service.empty())
                {
                    f.streams.push_back(m);
                }
            }
        }
        if(j.contains("cues") && j["cues"].is_array())
        {
            for(const auto& c : j["cues"])
            {
                StreamCue cue;
                cue.t_ms = c.value("t_ms", (std::int64_t)0);
                cue.t_end_ms = c.value("t_end_ms", (std::int64_t)-1);
                cue.effect_id = c.value("effect_id", "");
                cue.edge = c.value("edge", cue.t_end_ms >= 0 ? "level" : "pulse");
                if(!cue.effect_id.empty())
                {
                    f.cues.push_back(cue);
                }
            }
        }
        std::sort(f.cues.begin(), f.cues.end(), [](const StreamCue& a, const StreamCue& b) { return a.t_ms < b.t_ms; });
        *out = std::move(f);
        return true;
    }
    catch(const std::exception& e)
    {
        if(error) *error = e.what();
        return false;
    }
}

void StreamCueLibrary::Load(const filesystem::path& dir)
{
    files_.clear();
    errors_.clear();
    std::error_code ec;
    if(!filesystem::exists(dir, ec))
    {
        return;
    }
    for(filesystem::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    {
        if(!it->is_regular_file(ec))
        {
            continue;
        }
        const std::string name = it->path().filename().string();
        if(name.size() < 16 || Lower(name).rfind(".movie_sync.json") != name.size() - 16)
        {
            continue;
        }
        StreamCueFile f;
        std::string err;
        if(LoadStreamCueFile(it->path(), &f, &err))
        {
            files_.push_back(std::move(f));
        }
        else
        {
            errors_.push_back(name + ": " + err);
        }
    }
    /* Two files claiming the same title: the first wins, say so. */
    for(std::size_t a = 0; a < files_.size(); ++a)
    {
        for(std::size_t b = a + 1; b < files_.size(); ++b)
        {
            for(const StreamMatch& ma : files_[a].streams)
            {
                for(const StreamMatch& mb : files_[b].streams)
                {
                    if(!ma.id.empty() && ma.service == mb.service && ma.id == mb.id)
                    {
                        errors_.push_back(files_[b].path.filename().string() + ": same " + ma.service + " id " + ma.id
                                          + " as " + files_[a].path.filename().string() + " (ignored)");
                    }
                }
            }
        }
    }
}

const StreamCueFile* StreamCueLibrary::Match(const StreamState& state, std::string* how) const
{
    const std::string service = Lower(state.service);
    const std::int64_t dur_ms = (std::int64_t)std::llround(state.duration_s * 1000.0);
    /* 1. Exact: the service's own title / episode ID (and a length that fits: an ad playing
          under the title's ID, e.g. a YouTube pre-roll, must not fire the film's cues). */
    if(!state.id.empty())
    {
        for(const StreamCueFile& f : files_)
        {
            for(const StreamMatch& m : f.streams)
            {
                if(m.service == service && m.id == state.id
                   && (f.duration_ms <= 0 || dur_ms <= 0 || std::llabs(f.duration_ms - dur_ms) <= f.duration_tolerance_ms))
                {
                    if(how) *how = "by " + service + " ID";
                    return &f;
                }
            }
        }
        /* Crunchyroll gives each language version its own ID (GE00384332ENUS, ...JAJP): the
           same episode in another language matches too, if the length fits. */
        /* Only episode IDs of the form GE<digits><language> (e.g. GE00384332ENUS) with a known
           language suffix, IDs of the same length, and both lengths known. */
        static const char* kLocales[] = {"ENUS", "JAJP", "ESLA", "ESES", "ES419", "PTBR", "PTPT", "FRFR", "DEDE",
                                         "ITIT", "RURU", "ARSA", "ARME", "HIIN", "IDID", "MSMY", "THTH", "VIVN",
                                         "ZHCN", "ZHTW", "ZHHK", "KOKR", "TATA", "TEIN", "PLPL", "CAES", "TRTR"};
        const auto base = [](const std::string& id) -> std::string {
            if(id.size() < 8 || id.compare(0, 2, "GE") != 0)
            {
                return std::string();
            }
            for(const char* loc : kLocales)
            {
                const std::size_t n = std::char_traits<char>::length(loc);
                if(id.size() > n + 2 && id.compare(id.size() - n, n, loc) == 0)
                {
                    const std::string head = id.substr(2, id.size() - n - 2);
                    if(!head.empty() && std::all_of(head.begin(), head.end(), [](unsigned char c) { return std::isdigit(c); }))
                    {
                        return id.substr(0, id.size() - n);
                    }
                }
            }
            return std::string();
        };
        const std::string want_base = service == "crunchyroll" ? base(state.id) : std::string();
        if(!want_base.empty() && dur_ms > 0)
        {
            for(const StreamCueFile& f : files_)
            {
                for(const StreamMatch& m : f.streams)
                {
                    if(m.service == service && m.id.size() >= want_base.size() && base(m.id) == want_base
                       && f.duration_ms > 0 && std::llabs(f.duration_ms - dur_ms) <= f.duration_tolerance_ms)
                    {
                        if(how) *how = "by crunchyroll ID (other language)";
                        return &f;
                    }
                }
            }
        }
    }
    /* 2. Title (+ episode) and length within the file's tolerance. */
    const std::string want = Fold(state.title + state.episode);
    const std::string want_title = Fold(state.title);
    const StreamCueFile* best = nullptr;
    for(const StreamCueFile& f : files_)
    {
        const std::string have = Fold(f.title);
        if(have.empty() || (have != want && have != want_title))
        {
            continue;
        }
        if(f.duration_ms > 0 && dur_ms > 0 && std::llabs(f.duration_ms - dur_ms) > f.duration_tolerance_ms)
        {
            continue;
        }
        if(!best || have == want)
        {
            best = &f;
        }
    }
    if(best && how)
    {
        *how = "by title and length";
    }
    return best;
}

double StreamCuePlayer::EstimatedPosition(std::int64_t now_ms) const
{
    if(!has_state_)
    {
        return 0.0;
    }
    double pos = state_.position_s;
    if(!state_.paused && !state_.ad)
    {
        pos += (double)std::max<std::int64_t>(0, now_ms - state_.received_ms) / 1000.0 * state_.rate;
    }
    if(state_.duration_s > 0.0)
    {
        pos = std::min(pos, state_.duration_s);
    }
    return pos;
}

void StreamCuePlayer::ReleaseLevels()
{
    for(const std::string& id : levels_on_)
    {
        if(on_edge_)
        {
            on_edge_(id, "level", false);
        }
    }
    levels_on_.clear();
}

void StreamCuePlayer::ForgetLevels()
{
    levels_on_.clear();
}

void StreamCuePlayer::Stop()
{
    ReleaseLevels();
    has_state_ = false;
    file_ = nullptr;
    file_key_.clear();
    has_last_ = false;
}

void StreamCuePlayer::OnState(const StreamState& state, const StreamCueFile* file, std::int64_t now_ms)
{
    const std::string key = file ? file->path.string() : std::string();
    if(key != file_key_)
    {
        /* Another title (or none): drop what the old cue file was holding. */
        ReleaseLevels();
        file_key_ = key;
        has_last_ = false;
    }
    file_ = file;
    state_ = state;
    has_state_ = true;
    Tick(now_ms);
}

void StreamCuePlayer::Tick(std::int64_t now_ms)
{
    if(!file_ || !has_state_)
    {
        return;
    }
    /* Cue time = stream time + the file's offset (like VLC: positive = earlier) - the lights
       delay (positive = lights later, for a TV / Bluetooth picture that lags). */
    std::int64_t pos_ms = (std::int64_t)std::llround(EstimatedPosition(now_ms) * 1000.0) + file_->offset_ms
                          - lights_delay_ms_;

    /* Reports arrive a little late and the clock is counted on in between, so each new report
       can step the position back by some tens of ms. Hold the furthest position reached unless
       it really went back (a seek of a second or more): otherwise a cue in that sliver fires
       twice and held looks blink off and on. */
    if(has_last_ && pos_ms < last_pos_ms_ && last_pos_ms_ - pos_ms < 1000)
    {
        pos_ms = last_pos_ms_;
    }

    /* Pulses: fire the ones playback passed since last tick, only while playing normally (a
       seek or a jump of more than 2 s fires nothing, so skipping ahead never sets off a
       backlog of explosions). Just picked up: look back one report's worth so a cue right at
       the start of it is not lost. */
    const std::int64_t from_ms = has_last_ ? last_pos_ms_ : pos_ms - 300;
    if(!state_.paused && !state_.ad)
    {
        const std::int64_t step = pos_ms - from_ms;
        if(step > 0 && step <= 2000)
        {
            for(const StreamCue& c : file_->cues)
            {
                if(c.edge != "level" && c.t_ms > from_ms && c.t_ms <= pos_ms && on_edge_)
                {
                    on_edge_(c.effect_id, c.edge, true);
                }
            }
        }
    }
    last_pos_ms_ = pos_ms;
    has_last_ = true;

    /* Levels: on while playback is inside them (also after a seek), off outside / during ads. */
    std::vector<std::string> want;
    if(!state_.ad)
    {
        for(const StreamCue& c : file_->cues)
        {
            /* A level without an end holds to the end of the title (as VLC does). */
            const std::int64_t end = c.t_end_ms > c.t_ms ? c.t_end_ms : INT64_MAX;
            if(c.edge == "level" && pos_ms >= c.t_ms && pos_ms < end
               && std::find(want.begin(), want.end(), c.effect_id) == want.end())
            {
                want.push_back(c.effect_id);
            }
        }
    }
    for(const std::string& id : levels_on_)
    {
        if(std::find(want.begin(), want.end(), id) == want.end() && on_edge_)
        {
            on_edge_(id, "level", false);
        }
    }
    for(const std::string& id : want)
    {
        if(std::find(levels_on_.begin(), levels_on_.end(), id) == levels_on_.end() && on_edge_)
        {
            on_edge_(id, "level", true);
        }
    }
    levels_on_ = want;
}

} // namespace MovieSync
