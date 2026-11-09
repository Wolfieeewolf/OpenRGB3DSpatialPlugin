// SPDX-License-Identifier: GPL-2.0-only

#include "GameWrapperCapture.h"

#include <cmath>

namespace GameWrapperCapture
{
namespace
{

static RGBColor MakeRgb(unsigned char r, unsigned char g, unsigned char b)
{
    return (RGBColor)((b << 16) | (g << 8) | r);
}

} // namespace

RGBColor RenderColor(const GameLightingBridge::LightingSnapshot& snap,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid,
                     const Settings& settings)
{
    (void)grid_x;
    (void)grid_y;
    (void)grid_z;
    (void)origin_x;
    (void)origin_y;
    (void)origin_z;
    (void)grid;
    (void)settings;

    if(!snap.has_frame)
    {
        return (RGBColor)0x00000000;
    }

    /* GW01 v0: uniform RGB for the whole capture grid (chroma shim path). */
    if(snap.uniform_rgb || snap.zones.empty())
    {
        return MakeRgb((unsigned char)std::lround(snap.r * 255.0f),
                       (unsigned char)std::lround(snap.g * 255.0f),
                       (unsigned char)std::lround(snap.b * 255.0f));
    }

    /* Zone frames are not parsed yet — average zone colors as a stopgap. */
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    for(const GameLightingBridge::ZoneSample& zone : snap.zones)
    {
        r += zone.r;
        g += zone.g;
        b += zone.b;
    }
    const float inv = 1.0f / (float)snap.zones.size();
    return MakeRgb((unsigned char)std::lround(r * inv * 255.0f),
                   (unsigned char)std::lround(g * inv * 255.0f),
                   (unsigned char)std::lround(b * inv * 255.0f));
}

} // namespace GameWrapperCapture
