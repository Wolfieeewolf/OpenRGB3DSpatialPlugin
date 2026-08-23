// SPDX-License-Identifier: GPL-2.0-only

#include "RoomSampleMapping.h"
#include "Game/RoomSampleFrameProtocol.h"

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

static RGBColor PackRgb(float r, float g, float b, float a, bool& out_valid)
{
    out_valid = false;
    if(a <= 0.02f)
    {
        return (RGBColor)0;
    }
    const float inv = 1.0f / a;
    const int r8 = (int)std::lround(std::clamp(r * inv, 0.0f, 1.0f) * 255.0f);
    const int g8 = (int)std::lround(std::clamp(g * inv, 0.0f, 1.0f) * 255.0f);
    const int b8 = (int)std::lround(std::clamp(b * inv, 0.0f, 1.0f) * 255.0f);
    out_valid = true;
    return (RGBColor)((b8 << 16) | (g8 << 8) | r8);
}

/** Nearest cubemap texel — matches LED-mapped fill (no bilinear flash at high face sizes). */
static RGBColor NearestFaceSample(const GameTelemetryBridge::RoomSampleFrameChannel& rs,
                                  int face,
                                  float u,
                                  float v,
                                  bool& out_valid)
{
    out_valid = false;
    if(face < 0 || face >= rs.size_z || rs.size_x <= 0 || rs.size_y <= 0)
    {
        return (RGBColor)0;
    }

    const float gu = std::clamp(u, 0.0f, 1.0f) * (float)rs.size_x - 0.5f;
    const float gv = std::clamp(v, 0.0f, 1.0f) * (float)rs.size_y - 0.5f;
    const int iu = std::clamp((int)std::lround(gu), 0, rs.size_x - 1);
    const int iv = std::clamp((int)std::lround(gv), 0, rs.size_y - 1);
    const float a = SampleChannel(rs, iu, iv, face, 3);
    if(a <= 0.02f)
    {
        return (RGBColor)0;
    }
    return PackRgb(SampleChannel(rs, iu, iv, face, 0) * a,
                   SampleChannel(rs, iu, iv, face, 1) * a,
                   SampleChannel(rs, iu, iv, face, 2) * a,
                   a,
                   out_valid);
}

static RGBColor CubemapSample(const GameTelemetryBridge::RoomSampleFrameChannel& rs,
                              float grid_x,
                              float grid_y,
                              float grid_z,
                              bool& out_valid)
{
    out_valid = false;
    if(!RoomSampleFrameProtocol::IsCubemapLayout(rs.size_x, rs.size_y, rs.size_z, rs.flags))
    {
        return (RGBColor)0;
    }

    const float scale = std::clamp(rs.room_to_world_scale, 0.005f, 0.80f);
    const float eff_ox = rs.effect_origin_x;
    const float eff_oy = rs.effect_origin_y;
    const float eff_oz = rs.effect_origin_z;

    float dx = (grid_x - eff_ox) * scale;
    float dy = (grid_y - eff_oy) * scale;
    float dz = (eff_oz - grid_z) * scale;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if(len <= 1e-5f)
    {
        return (RGBColor)0;
    }
    dx /= len;
    dy /= len;
    dz /= len;

    int face = 0;
    float u = 0.0f;
    float v = 0.0f;
    RoomSampleFrameProtocol::DirectionToCubemapUv(dx, dy, dz, face, u, v);
    return NearestFaceSample(rs, face, u, v, out_valid);
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
    const RGBColor out = CubemapSample(rs, grid_x, grid_y, grid_z, valid);
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
