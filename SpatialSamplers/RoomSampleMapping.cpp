// SPDX-License-Identifier: GPL-2.0-only

#include "RoomSampleMapping.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace RoomSampleMapping
{
namespace
{
static float SampleChannel(const GameTelemetryBridge::RoomSampleFrameChannel& rs,
                           int ix,
                           int iy,
                           int iz,
                           int channel)
{
    if(ix < 0 || iy < 0 || iz < 0 || ix >= rs.size_x || iy >= rs.size_y || iz >= rs.size_z)
    {
        return 0.0f;
    }
    const std::size_t sy = (std::size_t)rs.size_y;
    const std::size_t sz = (std::size_t)rs.size_z;
    const std::size_t idx = ((std::size_t)ix * sy + (std::size_t)iy) * sz + (std::size_t)iz;
    const std::size_t bi = idx * 4u + (std::size_t)channel;
    if(!rs.rgba || bi >= rs.rgba->size())
    {
        return 0.0f;
    }
    return (float)(*rs.rgba)[bi] / 255.0f;
}

static float LerpF(float a, float b, float t)
{
    return a + (b - a) * t;
}

static RGBColor TrilinearSample(const GameTelemetryBridge::RoomSampleFrameChannel& rs,
                                float grid_x,
                                float grid_y,
                                float grid_z,
                                bool& out_valid)
{
    out_valid = false;

    const float span_x = std::max(1e-6f, rs.room_max_x - rs.room_min_x);
    const float span_y = std::max(1e-6f, rs.room_max_y - rs.room_min_y);
    const float span_z = std::max(1e-6f, rs.room_max_z - rs.room_min_z);

    const float gx = std::clamp((grid_x - rs.room_min_x) / span_x, 0.0f, 1.0f) * (float)rs.size_x - 0.5f;
    const float gy = std::clamp((grid_y - rs.room_min_y) / span_y, 0.0f, 1.0f) * (float)rs.size_y - 0.5f;
    const float gz = std::clamp((grid_z - rs.room_min_z) / span_z, 0.0f, 1.0f) * (float)rs.size_z - 0.5f;

    const int ix0 = std::clamp((int)std::floor(gx), 0, rs.size_x - 1);
    const int iy0 = std::clamp((int)std::floor(gy), 0, rs.size_y - 1);
    const int iz0 = std::clamp((int)std::floor(gz), 0, rs.size_z - 1);
    const int ix1 = std::min(ix0 + 1, rs.size_x - 1);
    const int iy1 = std::min(iy0 + 1, rs.size_y - 1);
    const int iz1 = std::min(iz0 + 1, rs.size_z - 1);
    const float tx = std::clamp(gx - (float)ix0, 0.0f, 1.0f);
    const float ty = std::clamp(gy - (float)iy0, 0.0f, 1.0f);
    const float tz = std::clamp(gz - (float)iz0, 0.0f, 1.0f);

    float acc_r = 0.0f;
    float acc_g = 0.0f;
    float acc_b = 0.0f;
    float acc_a = 0.0f;

    for(int sx = 0; sx < 2; sx++)
    {
        const int ix = (sx == 0) ? ix0 : ix1;
        const float wx = (sx == 0) ? (1.0f - tx) : tx;
        for(int sy = 0; sy < 2; sy++)
        {
            const int iy = (sy == 0) ? iy0 : iy1;
            const float wy = (sy == 0) ? (1.0f - ty) : ty;
            for(int sz = 0; sz < 2; sz++)
            {
                const int iz = (sz == 0) ? iz0 : iz1;
                const float wz = (sz == 0) ? (1.0f - tz) : tz;
                const float w = wx * wy * wz;
                const float a = SampleChannel(rs, ix, iy, iz, 3);
                if(a <= 0.02f)
                {
                    continue;
                }
                acc_r += SampleChannel(rs, ix, iy, iz, 0) * a * w;
                acc_g += SampleChannel(rs, ix, iy, iz, 1) * a * w;
                acc_b += SampleChannel(rs, ix, iy, iz, 2) * a * w;
                acc_a += a * w;
            }
        }
    }

    if(acc_a <= 0.02f)
    {
        return (RGBColor)0;
    }

    const float inv = 1.0f / acc_a;
    const int r8 = (int)std::lround(std::clamp(acc_r * inv, 0.0f, 1.0f) * 255.0f);
    const int g8 = (int)std::lround(std::clamp(acc_g * inv, 0.0f, 1.0f) * 255.0f);
    const int b8 = (int)std::lround(std::clamp(acc_b * inv, 0.0f, 1.0f) * 255.0f);
    out_valid = true;
    return (RGBColor)((b8 << 16) | (g8 << 8) | r8);
}
}

RGBColor SampleAtRoomGrid(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                          float grid_x,
                          float grid_y,
                          float grid_z,
                          bool* out_got_sample)
{
    if(out_got_sample)
    {
        *out_got_sample = false;
    }

    const GameTelemetryBridge::RoomSampleFrameChannel& rs = telemetry.room_sample;
    if(!rs.has_frame || rs.size_x <= 0 || rs.size_y <= 0 || rs.size_z <= 0 || !rs.rgba || rs.rgba->empty())
    {
        return (RGBColor)0;
    }

    bool valid = false;
    const RGBColor out = TrilinearSample(rs, grid_x, grid_y, grid_z, valid);
    if(!valid)
    {
        return (RGBColor)0;
    }

    if(out_got_sample)
    {
        *out_got_sample = true;
    }
    return out;
}

}
