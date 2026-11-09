// SPDX-License-Identifier: GPL-2.0-only
//
// ShowEditor show folder: .shw (text meta) + Key.ord (ASCII→.heb) + .seq (timeline) + .heb figures.
// SEQ figure events (reverse-engineered): 8-byte LE double time_ms, then … 0x8F, key_char, 0x00, 0x01 …

#include "ShowFolderReader.h"

#include "HebReader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#  include <filesystem>
namespace fs = std::filesystem;
#else
#  include <filesystem>
namespace fs = std::filesystem;
#endif

namespace
{
constexpr size_t kMaxFileBytes = 64u * 1024u * 1024u;
/** Key.ord / .shw / small text sidecars — fail closed on absurd sizes. */
constexpr size_t kMaxTextBytes = 1u * 1024u * 1024u;
constexpr size_t kMaxHebNameBytes = 256u;
constexpr double kDefaultHoldMs = 400.0;

std::string Trim(std::string s)
{
    while(!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
    {
        s.pop_back();
    }
    size_t i = 0;
    while(i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    {
        ++i;
    }
    return s.substr(i);
}

bool ReadAllBytes(const fs::path& path, std::vector<uint8_t>& out, std::string& err)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        err = "Could not open " + path.filename().string();
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff sz = in.tellg();
    in.seekg(0, std::ios::beg);
    if(sz <= 0 || static_cast<size_t>(sz) > kMaxFileBytes)
    {
        err = "File empty or too large: " + path.filename().string();
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    if(!in.read(reinterpret_cast<char*>(out.data()), sz))
    {
        err = "Failed reading " + path.filename().string();
        return false;
    }
    return true;
}

std::vector<std::string> ReadLines(const fs::path& path, std::string& err)
{
    std::vector<std::string> lines;
    std::error_code ec;
    const auto sz = fs::file_size(path, ec);
    if(ec || sz == 0 || sz > kMaxTextBytes)
    {
        err = "Text file missing or too large: " + path.filename().string();
        return lines;
    }
    std::ifstream in(path);
    if(!in)
    {
        err = "Could not open " + path.filename().string();
        return lines;
    }
    std::string line;
    while(std::getline(in, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

/** Key.ord → HEB leaf only — no absolute paths, separators, or parent hops. */
bool IsSafeHebLeafName(const std::string& name)
{
    if(name.empty() || name == "####" || name.size() > kMaxHebNameBytes)
    {
        return false;
    }
    if(name.find("..") != std::string::npos)
    {
        return false;
    }
    for(unsigned char c : name)
    {
        if(c == '/' || c == '\\' || c == ':' || c < 32)
        {
            return false;
        }
    }
    const fs::path p(name);
    return !p.is_absolute() && !p.has_parent_path() && p.filename() == p;
}

bool PathIsInsideDir(const fs::path& file, const fs::path& dir)
{
    std::error_code ec;
    const fs::path cfile = fs::weakly_canonical(file, ec);
    if(ec)
    {
        return false;
    }
    const fs::path cdir = fs::weakly_canonical(dir, ec);
    if(ec)
    {
        return false;
    }
    auto file_it = cfile.begin();
    for(auto dir_it = cdir.begin(); dir_it != cdir.end(); ++dir_it, ++file_it)
    {
        if(file_it == cfile.end() || *file_it != *dir_it)
        {
            return false;
        }
    }
    return true;
}

fs::path FindShw(const fs::path& folder)
{
    for(const auto& ent : fs::directory_iterator(folder))
    {
        if(!ent.is_regular_file())
        {
            continue;
        }
        std::string ext = ent.path().extension().string();
        for(char& c : ext)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if(ext == ".shw")
        {
            return ent.path();
        }
    }
    return {};
}

/**
 * ShowEditor Spur/track pre-byte: 0x01 + track * 19
 * (0x01, 0x14, 0x27, 0x3A, 0x4D, 0x60, 0x73, 0x86, 0x99, 0xAC, 0xBF, 0xD2, …).
 */
int TrackIndexFromCode(int code)
{
    if(code <= 0)
    {
        return 0;
    }
    if(((code - 1) % 19) == 0)
    {
        return (code - 1) / 19;
    }
    return code; /* unknown encoding — keep raw so it stays filterable */
}

bool TimeMsOk(double t)
{
    return (t >= 0.0) && (t <= 24.0 * 60.0 * 60.0 * 1000.0) && !std::isnan(t) && !std::isinf(t);
}

void ParseSeqEvents(const std::vector<uint8_t>& data, std::vector<ShowSeqEvent>& events)
{
    events.clear();
    if(data.size() < 16)
    {
        return;
    }
    /*
     * Figure-key cue records (two layouts seen in Free Laser Show Pack):
     *   [f64 time_ms][u8  track_code][0x8F][u8 key][0x00][0x01]
     *   [f64 time_ms][u16 track_code LE, hi=0][0x8F][u8 key][0x00][0x01]
     * Scan at 0x8F; accept either packing.
     */
    for(size_t p = 3; p + 3 < data.size(); ++p)
    {
        if(data[p] != 0x8F || data[p + 2] != 0x00 || data[p + 3] != 0x01)
        {
            continue;
        }
        const unsigned char key_u = data[p + 1];
        if(key_u < 32)
        {
            continue;
        }

        int track_code = 0;
        size_t time_off = 0;
        if(p >= 2 && data[p - 1] == 0x00)
        {
            track_code = static_cast<int>(data[p - 2]);
            if(p < 10)
            {
                continue;
            }
            time_off = p - 10;
        }
        else
        {
            track_code = static_cast<int>(data[p - 1]);
            if(p < 9)
            {
                continue;
            }
            time_off = p - 9;
        }

        double t = 0.0;
        std::memcpy(&t, data.data() + time_off, sizeof(double));
        if(!TimeMsOk(t))
        {
            continue;
        }

        ShowSeqEvent ev;
        ev.time_ms = t;
        ev.key = static_cast<char>(key_u);
        ev.key_index = static_cast<int>(key_u);
        ev.track_code = track_code;
        ev.track = TrackIndexFromCode(track_code);
        events.push_back(ev);
    }
    std::sort(events.begin(), events.end(),
              [](const ShowSeqEvent& a, const ShowSeqEvent& b) {
                  if(a.time_ms != b.time_ms)
                  {
                      return a.time_ms < b.time_ms;
                  }
                  if(a.track != b.track)
                  {
                      return a.track < b.track;
                  }
                  return a.key_index < b.key_index;
              });
    events.erase(std::unique(events.begin(), events.end(),
                             [](const ShowSeqEvent& a, const ShowSeqEvent& b) {
                                 return a.time_ms == b.time_ms
                                     && a.key_index == b.key_index
                                     && a.track == b.track;
                             }),
                 events.end());
}

std::string ExtractInfoNotes(const fs::path& folder)
{
    std::string notes;
    std::error_code ec;
    for(const auto& ent : fs::directory_iterator(folder, ec))
    {
        if(!ent.is_regular_file())
        {
            continue;
        }
        const std::string name = ent.path().filename().string();
        std::string lower = name;
        for(char& c : lower)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        const bool is_info = (lower.find("info") != std::string::npos)
            || (lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".info") == 0)
            || (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".txt") == 0
                && lower.find("info") != std::string::npos);
        if(!is_info && lower != "[info].txt")
        {
            /* Also accept exact [INFO].txt */
            if(name.find("INFO") == std::string::npos && name.find("info") == std::string::npos)
            {
                continue;
            }
        }

        std::vector<uint8_t> bytes;
        std::string err;
        if(!ReadAllBytes(ent.path(), bytes, err) || bytes.empty())
        {
            continue;
        }
        /* Prefer printable runs (covers binary Showinfo Version 2 + ASCII [INFO].txt). */
        std::string run;
        auto flush = [&]() {
            if(run.size() >= 8)
            {
                if(!notes.empty())
                {
                    notes.push_back('\n');
                }
                notes += run;
            }
            run.clear();
        };
        for(uint8_t b : bytes)
        {
            if(b >= 32 && b < 127)
            {
                run.push_back(static_cast<char>(b));
            }
            else if(b == '\n' || b == '\r' || b == '\t')
            {
                if(!run.empty())
                {
                    run.push_back(' ');
                }
            }
            else
            {
                flush();
            }
        }
        flush();
        if(!notes.empty())
        {
            break;
        }
    }
    return notes;
}
} // namespace

LaserFrame ShowFolder::FrameAt(double time_ms, float figure_fps, int track_filter) const
{
    LaserFrame out;
    out.name = show_name;
    out.scanner = (track_filter >= 0) ? track_filter : 0;
    if(events.empty() || figures.empty())
    {
        return out;
    }

    /* Per (key, track): last press at or before time_ms. */
    std::unordered_map<std::uint64_t, size_t> last_idx;
    auto pack_kt = [](int key, int track) -> std::uint64_t {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(key)))
             | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(track)) << 32);
    };

    for(size_t i = 0; i < events.size(); ++i)
    {
        if(events[i].time_ms > time_ms)
        {
            break;
        }
        if(track_filter >= 0 && events[i].track != track_filter)
        {
            continue;
        }
        last_idx[pack_kt(events[i].key_index, events[i].track)] = i;
    }

    const float fps = std::max(1.0f, figure_fps);
    for(const auto& kv : last_idx)
    {
        const size_t ei = kv.second;
        const ShowSeqEvent& ev = events[ei];
        double end_ms = std::max(duration_ms, ev.time_ms + kDefaultHoldMs);
        for(size_t j = ei + 1; j < events.size(); ++j)
        {
            if(events[j].key_index == ev.key_index && events[j].track == ev.track)
            {
                end_ms = events[j].time_ms;
                break;
            }
        }
        if(time_ms >= end_ms)
        {
            continue;
        }

        if(ev.key_index < 0 || static_cast<size_t>(ev.key_index) >= key_slots.size())
        {
            continue;
        }
        const std::string& fname = key_slots[static_cast<size_t>(ev.key_index)];
        if(fname.empty() || fname == "####")
        {
            continue;
        }
        const auto fit = figures.find(fname);
        if(fit == figures.end() || fit->second.frames.empty())
        {
            continue;
        }

        const LaserShow& fig = fit->second;
        const double local_ms = time_ms - ev.time_ms;
        const size_t n = fig.frames.size();
        size_t fi = static_cast<size_t>(local_ms * static_cast<double>(fps) / 1000.0) % n;
        const LaserFrame& src = fig.frames[fi];
        for(LaserSegment seg : src.segments)
        {
            seg.track = ev.track;
            out.segments.push_back(seg);
        }
        for(LaserDot dot : src.dots)
        {
            dot.track = ev.track;
            out.dots.push_back(dot);
        }
    }
    return out;
}

ShowFolder ShowFolderReader::LoadPath(const std::string& shw_or_folder_path)
{
    ShowFolder show;
    show.source_path = shw_or_folder_path;

    fs::path path(shw_or_folder_path);
    std::error_code ec;
    if(!fs::exists(path, ec))
    {
        show.error = "Show path not found";
        return show;
    }

    fs::path folder = path;
    fs::path shw_path;
    if(fs::is_regular_file(path, ec))
    {
        std::string ext = path.extension().string();
        for(char& c : ext)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if(ext == ".shw")
        {
            shw_path = path;
            folder = path.parent_path();
        }
        else
        {
            folder = path.parent_path();
            shw_path = FindShw(folder);
        }
    }
    else
    {
        shw_path = FindShw(folder);
    }

    if(shw_path.empty())
    {
        show.error = "No .shw file in show folder";
        return show;
    }

    std::string err;
    const std::vector<std::string> shw_lines = ReadLines(shw_path, err);
    if(!err.empty())
    {
        show.error = err;
        return show;
    }
    // Line0 magic, line1 show name, line2 audio
    if(shw_lines.size() >= 2)
    {
        show.show_name = Trim(shw_lines[1]);
    }
    if(shw_lines.size() >= 3)
    {
        show.audio_name = Trim(shw_lines[2]);
    }
    if(show.show_name.empty())
    {
        show.show_name = shw_path.stem().string();
    }

    const fs::path ord_path = folder / "Key.ord";
    show.key_slots = ReadLines(ord_path, err);
    if(!err.empty())
    {
        show.error = err;
        return show;
    }

    const fs::path seq_path = folder / (show.show_name + ".seq");
    const fs::path seq_alt = folder / (shw_path.stem().string() + ".seq");
    fs::path seq_use = fs::exists(seq_path, ec) ? seq_path : seq_alt;
    if(!fs::exists(seq_use, ec))
    {
        // Any .seq in folder
        for(const auto& ent : fs::directory_iterator(folder, ec))
        {
            if(ent.is_regular_file() && ent.path().extension() == ".seq")
            {
                seq_use = ent.path();
                break;
            }
        }
    }
    if(!fs::exists(seq_use, ec))
    {
        show.error = "No .seq timeline in show folder";
        return show;
    }

    std::vector<uint8_t> seq_bytes;
    if(!ReadAllBytes(seq_use, seq_bytes, err))
    {
        show.error = err;
        return show;
    }
    ParseSeqEvents(seq_bytes, show.events);
    if(show.events.empty())
    {
        show.error = "No timeline key events found in .seq";
        return show;
    }
    show.duration_ms = show.events.back().time_ms + kDefaultHoldMs;

    {
        std::vector<int> tr;
        tr.reserve(show.events.size());
        for(const ShowSeqEvent& ev : show.events)
        {
            tr.push_back(ev.track);
        }
        std::sort(tr.begin(), tr.end());
        tr.erase(std::unique(tr.begin(), tr.end()), tr.end());
        show.tracks = std::move(tr);
    }

    show.info_notes = ExtractInfoNotes(folder);

    // Load every .heb named by Key.ord (and referenced by events).
    std::vector<std::string> needed;
    for(const ShowSeqEvent& ev : show.events)
    {
        if(ev.key_index < 0 || static_cast<size_t>(ev.key_index) >= show.key_slots.size())
        {
            continue;
        }
        const std::string& name = show.key_slots[static_cast<size_t>(ev.key_index)];
        if(!name.empty() && name != "####")
        {
            needed.push_back(name);
        }
    }
    std::sort(needed.begin(), needed.end());
    needed.erase(std::unique(needed.begin(), needed.end()), needed.end());

    for(const std::string& name : needed)
    {
        if(!IsSafeHebLeafName(name))
        {
            continue;
        }
        const fs::path heb = folder / name;
        if(!fs::exists(heb, ec) || !PathIsInsideDir(heb, folder))
        {
            continue;
        }
        LaserShow fig = HebReader::LoadFile(heb.string());
        if(!fig.error.empty() || fig.frames.empty())
        {
            continue;
        }
        for(LaserFrame& fr : fig.frames)
        {
            /* Blanked travel stays invisible — matches ilda-viewer / ShowEditor Beam. */
            fr.RebuildSegments(false);
        }
        show.figures.emplace(name, std::move(fig));
    }

    if(show.figures.empty())
    {
        show.error = "No HEB figures could be loaded for timeline keys";
    }
    return show;
}
