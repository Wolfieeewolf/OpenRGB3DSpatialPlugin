// SPDX-License-Identifier: GPL-2.0-only
//
// ILDA Image Data Transfer Format reader.
// Spec notes (ILDA IDTF + LaserBoy summary):
//   https://laserboy.org/ilda_file_format.html
//   https://laserboy.org/formatt/IDTF05-finaldraft.pdf
// Formats 0/1: indexed colour, 16-bit status (BE): lo=colour index, bit14=blank, bit15=last.
// Format 2: RGB palette (up to 256).
// Format 3: true-colour table (LaserBoy / draft) — one RGB per following frame point.
// Formats 4/5: true colour per point (BGR order in the point record).
// Header byte 31 = scanner head / projector number (multi-scanner channel).

#include "IldaReader.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <set>
#include <vector>

namespace
{
constexpr size_t kMaxIldaBytes = 64u * 1024u * 1024u;

#include "IldaStdPalette.inc"

struct Rgb8
{
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
};

uint16_t ReadBE16(const uint8_t* p)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t ReadBE32(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
         | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

int16_t ReadBE16s(const uint8_t* p)
{
    return static_cast<int16_t>(ReadBE16(p));
}

float NormCoord(int16_t v)
{
    // ILDA range is [-32768, +32767]; map onto ~[-1, 1].
    return static_cast<float>(v) / 32768.0f;
}

void FillDefaultPalette(std::vector<Rgb8>& pal)
{
    // LaserBoy: if no palette section, assume default ILDA-style palette.
    pal.resize(256);
    const size_t n = sizeof(kIldaStdPalette) / sizeof(kIldaStdPalette[0]);
    for(size_t i = 0; i < 256; ++i)
    {
        if(i < n)
        {
            pal[i].r = kIldaStdPalette[i][0];
            pal[i].g = kIldaStdPalette[i][1];
            pal[i].b = kIldaStdPalette[i][2];
        }
        else
        {
            pal[i] = {255, 255, 255};
        }
    }
}

Rgb8 Lookup(const std::vector<Rgb8>& pal, uint8_t index)
{
    if(index < pal.size())
    {
        return pal[index];
    }
    return {255, 255, 255};
}

std::string ReadName8(const uint8_t* p)
{
    char buf[9] = {};
    for(int i = 0; i < 8; ++i)
    {
        buf[i] = static_cast<char>(p[i] ? p[i] : ' ');
    }
    std::string s(buf, 8);
    while(!s.empty() && (s.back() == ' ' || s.back() == '\0'))
    {
        s.pop_back();
    }
    return s;
}

/** Status word bit 14 clear ⇒ laser on (draw). High byte of BE status is at p[0]. */
bool StatusLit(uint8_t status_hi)
{
    return (status_hi & 0x40u) == 0;
}

bool IsIldaSig(const std::vector<uint8_t>& data, size_t offset)
{
    return offset + 4 <= data.size()
        && data[offset] == 'I' && data[offset + 1] == 'L'
        && data[offset + 2] == 'D' && data[offset + 3] == 'A';
}

size_t FindNextIlda(const std::vector<uint8_t>& data, size_t from)
{
    for(size_t i = from; i + 4 <= data.size(); ++i)
    {
        if(IsIldaSig(data, i))
        {
            return i;
        }
    }
    return data.size();
}
} // namespace

LaserShow IldaReader::LoadFile(const std::string& path)
{
    LaserShow show;
    show.source_path = path;

    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        show.error = "Could not open ILDA file";
        return show;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff sz = in.tellg();
    in.seekg(0, std::ios::beg);
    if(sz <= 0 || static_cast<size_t>(sz) > kMaxIldaBytes)
    {
        show.error = "ILDA file empty or too large";
        return show;
    }

    std::vector<uint8_t> data(static_cast<size_t>(sz));
    if(!in.read(reinterpret_cast<char*>(data.data()), sz))
    {
        show.error = "Failed reading ILDA file";
        return show;
    }

    std::vector<Rgb8> palette;
    FillDefaultPalette(palette);
    std::vector<Rgb8> pending_true_color; // format 3 table for the next coordinate frame
    std::set<int> scanner_set;

    size_t offset = 0;
    while(offset + 8 <= data.size())
    {
        if(!IsIldaSig(data, offset))
        {
            show.error = "Invalid ILDA signature";
            show.frames.clear();
            return show;
        }

        // Format code is last of the 4-byte big-endian field at bytes 5–8 (index 4..7).
        const uint8_t fmt = data[offset + 7];

        // Draft format 3 can use a short header (ILDA + fmt + data_length). Detect when
        // bytes 8-11 look like a length and there is no classic name field of "ILDA".
        // LaserBoy / common files keep a uniform 32-byte header for format 3 as well.
        if(fmt == 3 && offset + 16 <= data.size())
        {
            // Prefer classic 32-byte header when the following bytes look like a name (printable).
            const bool classic = offset + 32 <= data.size()
                && data[offset + 8] >= 32 && data[offset + 8] < 127;
            if(!classic)
            {
                const uint32_t data_len = ReadBE32(&data[offset + 8]);
                offset += 12;
                if(offset + data_len > data.size())
                {
                    show.error = "Truncated ILDA format-3 section";
                    show.frames.clear();
                    return show;
                }
                // data_len includes number-of-points (4) + RGB*n; skip for now if odd.
                if(data_len >= 4)
                {
                    const uint32_t npoints = ReadBE32(&data[offset]);
                    pending_true_color.clear();
                    if(npoints > 0 && offset + 4 + npoints * 3 <= offset + data_len)
                    {
                        pending_true_color.resize(npoints);
                        for(uint32_t i = 0; i < npoints; ++i)
                        {
                            const uint8_t* c = &data[offset + 4 + i * 3];
                            pending_true_color[i] = {c[0], c[1], c[2]}; // R,G,B per draft
                        }
                    }
                }
                offset += data_len;
                continue;
            }
        }

        if(offset + 32 > data.size())
        {
            break;
        }

        const std::string frame_name = ReadName8(&data[offset + 8]);
        const uint16_t record_count = ReadBE16(&data[offset + 24]);
        const int scanner = static_cast<int>(data[offset + 30]);
        offset += 32;

        if(record_count == 0)
        {
            break; // EOF header
        }

        size_t rec_size = 0;
        switch(fmt)
        {
            case 0: rec_size = 8; break;
            case 1: rec_size = 6; break;
            case 2: rec_size = 3; break;
            case 3: rec_size = 3; break; // LaserBoy true-colour table (uniform header)
            case 4: rec_size = 10; break;
            case 5: rec_size = 8; break;
            default:
                // Spec: skip unfamiliar formats. Search for the next header.
                offset = FindNextIlda(data, offset);
                continue;
        }

        const size_t need = static_cast<size_t>(record_count) * rec_size;
        if(offset + need > data.size())
        {
            show.error = "Truncated ILDA frame";
            show.frames.clear();
            return show;
        }

        if(fmt == 2)
        {
            palette.assign(256, Rgb8{0, 0, 0});
            const size_t n = std::min<size_t>(record_count, 256);
            for(size_t i = 0; i < n; ++i)
            {
                const uint8_t* p = &data[offset + i * 3];
                palette[i] = {p[0], p[1], p[2]}; // R,G,B
            }
            offset += need;
            continue;
        }

        if(fmt == 3)
        {
            pending_true_color.resize(record_count);
            for(uint16_t i = 0; i < record_count; ++i)
            {
                const uint8_t* p = &data[offset + static_cast<size_t>(i) * 3];
                pending_true_color[i] = {p[0], p[1], p[2]};
            }
            offset += need;
            continue;
        }

        LaserFrame frame;
        frame.name = frame_name;
        frame.scanner = scanner;
        scanner_set.insert(scanner);
        frame.points.reserve(record_count);

        const bool use_true = !pending_true_color.empty()
            && pending_true_color.size() == static_cast<size_t>(record_count);

        for(uint16_t i = 0; i < record_count; ++i)
        {
            const uint8_t* p = &data[offset + static_cast<size_t>(i) * rec_size];
            LaserPoint pt;
            uint8_t status_hi = 0;

            if(fmt == 0)
            {
                pt.x = NormCoord(ReadBE16s(p + 0));
                pt.y = NormCoord(ReadBE16s(p + 2));
                pt.z = NormCoord(ReadBE16s(p + 4));
                status_hi = p[6]; // high byte of status word
                if(use_true)
                {
                    const Rgb8& c = pending_true_color[i];
                    pt.r = c.r / 255.0f;
                    pt.g = c.g / 255.0f;
                    pt.b = c.b / 255.0f;
                }
                else
                {
                    const Rgb8 c = Lookup(palette, p[7]); // colour index in low status byte
                    pt.r = c.r / 255.0f;
                    pt.g = c.g / 255.0f;
                    pt.b = c.b / 255.0f;
                }
            }
            else if(fmt == 1)
            {
                pt.x = NormCoord(ReadBE16s(p + 0));
                pt.y = NormCoord(ReadBE16s(p + 2));
                pt.z = 0.0f;
                status_hi = p[4];
                if(use_true)
                {
                    const Rgb8& c = pending_true_color[i];
                    pt.r = c.r / 255.0f;
                    pt.g = c.g / 255.0f;
                    pt.b = c.b / 255.0f;
                }
                else
                {
                    const Rgb8 c = Lookup(palette, p[5]);
                    pt.r = c.r / 255.0f;
                    pt.g = c.g / 255.0f;
                    pt.b = c.b / 255.0f;
                }
            }
            else if(fmt == 4)
            {
                // X Y Z Status B G R (10 bytes) — modern true colour
                pt.x = NormCoord(ReadBE16s(p + 0));
                pt.y = NormCoord(ReadBE16s(p + 2));
                pt.z = NormCoord(ReadBE16s(p + 4));
                status_hi = p[6];
                pt.b = p[7] / 255.0f;
                pt.g = p[8] / 255.0f;
                pt.r = p[9] / 255.0f;
            }
            else // fmt == 5
            {
                pt.x = NormCoord(ReadBE16s(p + 0));
                pt.y = NormCoord(ReadBE16s(p + 2));
                pt.z = 0.0f;
                status_hi = p[4];
                pt.b = p[5] / 255.0f;
                pt.g = p[6] / 255.0f;
                pt.r = p[7] / 255.0f;
            }

            pt.lit = StatusLit(status_hi);
            if(!pt.lit)
            {
                pt.r = pt.g = pt.b = 0.0f;
            }
            frame.points.push_back(pt);
        }

        pending_true_color.clear();
        offset += need;
        frame.RebuildSegments();
        show.frames.push_back(std::move(frame));
    }

    show.scanners.assign(scanner_set.begin(), scanner_set.end());
    if(show.frames.empty() && show.error.empty())
    {
        show.error = "No ILDA frames found";
    }
    return show;
}
