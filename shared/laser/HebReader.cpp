// SPDX-License-Identifier: GPL-2.0-only
//
// ShowEditor / HE-Laserscan .heb figure reader (reverse-engineered).
// Layout (little-endian):
//   u8 magic[3]   — 0x0E or 0x0F repeated (format family)
//   u32 frame_n   — 0 = single static frame; else animated frame count
//   if frame_n == 0:
//     u32 point_n
//     29 bytes metadata
//     point_n * 9-byte points
//     3218-byte effect trailer (ignored)
//   else:
//     u32 point_n[frame_n]
//     32-byte frame table header
//     sum(point_n) * 9-byte points (frames concatenated)
//     effect trailer (ignored)
// Point: int16 x, int16 y, u16 colour, u16 aux, u8 status
// Colour low 12 bits = RGB444. Blanking: status 2/3 (0x0E) or colour high-nibble / 0x0B.

#include "HebReader.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <vector>

namespace
{
constexpr size_t kMaxHebBytes = 64u * 1024u * 1024u;
constexpr size_t kStaticPointOffset = 40;
constexpr size_t kStaticTrailer = 3218;
constexpr size_t kAnimHeaderExtra = 32;
constexpr size_t kPointBytes = 9;

uint32_t ReadU32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

int16_t ReadI16(const uint8_t* p)
{
    return static_cast<int16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}

uint16_t ReadU16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

float NormCoord(int16_t v)
{
    return static_cast<float>(v) / 32768.0f;
}

void ColourFromHeb(uint16_t c, float& r, float& g, float& b)
{
    const int ri = (c >> 8) & 0x0F;
    const int gi = (c >> 4) & 0x0F;
    const int bi = c & 0x0F;
    r = static_cast<float>(ri) / 15.0f;
    g = static_cast<float>(gi) / 15.0f;
    b = static_cast<float>(bi) / 15.0f;
}

bool PointLit(uint8_t status, uint16_t colour)
{
    if(status == 2)
    {
        return false;
    }
    if(status == 3)
    {
        return true;
    }
    if((colour & 0x0FFFu) == 0)
    {
        return false;
    }
    // Animated / optimized figures: bright when high nibble set (e.g. 0xFF0F).
    if((colour & 0xF000u) != 0)
    {
        return true;
    }
    // Static 0x0RGB figures often use status 0x0B with real colour.
    return status == 0x0B;
}

void AppendPoints(LaserFrame& frame, const uint8_t* data, uint32_t count)
{
    frame.points.reserve(frame.points.size() + count);
    for(uint32_t i = 0; i < count; ++i)
    {
        const uint8_t* p = data + static_cast<size_t>(i) * kPointBytes;
        const int16_t x = ReadI16(p + 0);
        const int16_t y = ReadI16(p + 2);
        const uint16_t colour = ReadU16(p + 4);
        const uint8_t status = p[8];

        LaserPoint pt;
        pt.x = NormCoord(x);
        pt.y = NormCoord(y);
        pt.z = 0.0f;
        ColourFromHeb(colour, pt.r, pt.g, pt.b);
        pt.lit = PointLit(status, colour);
        if(!pt.lit)
        {
            pt.r = pt.g = pt.b = 0.0f;
        }
        frame.points.push_back(pt);
    }
}
} // namespace

LaserShow HebReader::LoadFile(const std::string& path)
{
    LaserShow show;
    show.source_path = path;

    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        show.error = "Could not open HEB file";
        return show;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff sz = in.tellg();
    in.seekg(0, std::ios::beg);
    if(sz < 48 || static_cast<size_t>(sz) > kMaxHebBytes)
    {
        show.error = "HEB file empty or too large";
        return show;
    }

    std::vector<uint8_t> data(static_cast<size_t>(sz));
    if(!in.read(reinterpret_cast<char*>(data.data()), sz))
    {
        show.error = "Failed reading HEB file";
        return show;
    }

    if(data[0] != data[1] || data[1] != data[2] || (data[0] != 0x0E && data[0] != 0x0F))
    {
        show.error = "Not a ShowEditor HEB figure (bad magic)";
        return show;
    }

    const uint32_t frame_n = ReadU32(data.data() + 3);
    if(frame_n == 0)
    {
        if(data.size() < kStaticPointOffset + kPointBytes)
        {
            show.error = "Truncated static HEB header";
            return show;
        }
        const uint32_t point_n = ReadU32(data.data() + 7);
        const size_t need = kStaticPointOffset + static_cast<size_t>(point_n) * kPointBytes;
        if(point_n == 0 || need > data.size())
        {
            show.error = "Truncated static HEB points";
            return show;
        }
        // Trailer is typically 3218 bytes; allow shorter/longer for variants.
        if(data.size() + 16 < need + kStaticTrailer && data.size() != need)
        {
            // Still accept if points fit; some tools strip trailers.
        }

        LaserFrame frame;
        frame.name = "heb";
        frame.scanner = 0;
        AppendPoints(frame, data.data() + kStaticPointOffset, point_n);
        frame.RebuildSegments();
        show.frames.push_back(std::move(frame));
        show.scanners = {0};
        return show;
    }

    if(frame_n > 100000u)
    {
        show.error = "HEB frame count too large";
        return show;
    }

    const size_t array_end = 7 + static_cast<size_t>(frame_n) * 4u;
    if(array_end + kAnimHeaderExtra > data.size())
    {
        show.error = "Truncated HEB frame table";
        return show;
    }

    std::vector<uint32_t> counts(frame_n);
    uint64_t total_pts = 0;
    for(uint32_t i = 0; i < frame_n; ++i)
    {
        counts[i] = ReadU32(data.data() + 7 + static_cast<size_t>(i) * 4u);
        if(counts[i] > 100000u)
        {
            show.error = "HEB point count too large";
            return show;
        }
        total_pts += counts[i];
    }

    const size_t points_off = array_end + kAnimHeaderExtra;
    const size_t need = points_off + static_cast<size_t>(total_pts) * kPointBytes;
    if(need > data.size())
    {
        show.error = "Truncated HEB animation points";
        return show;
    }

    size_t cursor = points_off;
    show.frames.reserve(frame_n);
    for(uint32_t i = 0; i < frame_n; ++i)
    {
        LaserFrame frame;
        frame.name = "heb";
        frame.scanner = 0;
        AppendPoints(frame, data.data() + cursor, counts[i]);
        cursor += static_cast<size_t>(counts[i]) * kPointBytes;
        frame.RebuildSegments();
        show.frames.push_back(std::move(frame));
    }

    show.scanners = {0};
    if(show.frames.empty())
    {
        show.error = "No HEB frames found";
    }
    return show;
}
