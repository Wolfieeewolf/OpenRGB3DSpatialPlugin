// SPDX-License-Identifier: GPL-2.0-only

#include "GpuPanoramaMapping.h"

#include "SpatialBasisUtils.h"
#include "SpatialCoordinateSpaces.h"

#include <algorithm>
#include <cmath>

namespace GpuPanoramaMapping
{
namespace
{
static float SampleChannelBilinear(const GameTelemetryBridge::GpuPanoramaFrameChannel& pano,
                                 int face,
                                 float u,
                                 float v,
                                 int channel)
{
    if(face < 0 || face >= pano.face_count || pano.face_w <= 0 || pano.face_h <= 0)
    {
        return 0.0f;
    }

    const float gx = std::clamp(u, 0.0f, 1.0f) * (float)pano.face_w - 0.5f;
    const float gy = std::clamp(v, 0.0f, 1.0f) * (float)pano.face_h - 0.5f;
    const int x0 = std::clamp((int)std::floor(gx), 0, pano.face_w - 1);
    const int y0 = std::clamp((int)std::floor(gy), 0, pano.face_h - 1);
    const int x1 = std::min(x0 + 1, pano.face_w - 1);
    const int y1 = std::min(y0 + 1, pano.face_h - 1);
    const float tx = std::clamp(gx - (float)x0, 0.0f, 1.0f);
    const float ty = std::clamp(gy - (float)y0, 0.0f, 1.0f);

    const std::size_t face_pixels = (std::size_t)pano.face_w * (std::size_t)pano.face_h;
    const std::size_t face_offset = (std::size_t)face * face_pixels * 4u;

    auto at = [&](int x, int y) -> float {
        if(!pano.rgba)
        {
            return 0.0f;
        }
        const std::vector<unsigned char>& pixels = *pano.rgba;
        const std::size_t idx = face_offset + ((std::size_t)y * (std::size_t)pano.face_w + (std::size_t)x) * 4u +
                                (std::size_t)channel;
        if(idx >= pixels.size())
        {
            return 0.0f;
        }
        return (float)pixels[idx] / 255.0f;
    };

    const float c00 = at(x0, y0);
    const float c10 = at(x1, y0);
    const float c01 = at(x0, y1);
    const float c11 = at(x1, y1);
    const float c0 = c00 + (c10 - c00) * tx;
    const float c1 = c01 + (c11 - c01) * tx;
    return c0 + (c1 - c0) * ty;
}

/** Minecraft GameWorld axes: +X east, +Y up, +Z south (right-handed). */
static bool DirectionToCubemapFace(float dx, float dy, float dz, int& out_face, float& out_u, float& out_v)
{
    const float ax = std::fabs(dx);
    const float ay = std::fabs(dy);
    const float az = std::fabs(dz);
    const float max_a = std::max(ax, std::max(ay, az));
    if(max_a <= 1e-6f)
    {
        return false;
    }

    float uc = 0.0f;
    float vc = 0.0f;
    if(ax >= ay && ax >= az)
    {
        if(dx >= 0.0f)
        {
            out_face = 0;
            uc = -dz / ax;
            vc = -dy / ax;
        }
        else
        {
            out_face = 1;
            uc = dz / ax;
            vc = -dy / ax;
        }
    }
    else if(ay >= ax && ay >= az)
    {
        if(dy >= 0.0f)
        {
            out_face = 2;
            uc = dx / ay;
            vc = dz / ay;
        }
        else
        {
            out_face = 3;
            uc = dx / ay;
            vc = -dz / ay;
        }
    }
    else
    {
        if(dz >= 0.0f)
        {
            out_face = 4;
            uc = dx / az;
            vc = -dy / az;
        }
        else
        {
            out_face = 5;
            uc = -dx / az;
            vc = -dy / az;
        }
    }

    out_u = std::clamp(uc * 0.5f + 0.5f, 0.0f, 1.0f);
    out_v = std::clamp(vc * 0.5f + 0.5f, 0.0f, 1.0f);
    return true;
}
}

RGBColor SampleCubemapDirection(const GameTelemetryBridge::GpuPanoramaFrameChannel& pano,
                                float dir_x,
                                float dir_y,
                                float dir_z,
                                bool* out_got_sample)
{
    if(out_got_sample)
    {
        *out_got_sample = false;
    }

    if(!pano.has_frame || !pano.rgba || pano.rgba->empty() || pano.face_w <= 0 || pano.face_h <= 0 || pano.face_count <= 0)
    {
        return (RGBColor)0;
    }

    const float len = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
    if(len <= 1e-5f)
    {
        return (RGBColor)0;
    }

    int face = 0;
    float u = 0.0f;
    float v = 0.0f;
    if(!DirectionToCubemapFace(dir_x / len, dir_y / len, dir_z / len, face, u, v))
    {
        return (RGBColor)0;
    }

    const float rf = SampleChannelBilinear(pano, face, u, v, 0);
    const float gf = SampleChannelBilinear(pano, face, u, v, 1);
    const float bf = SampleChannelBilinear(pano, face, u, v, 2);
    const float af = SampleChannelBilinear(pano, face, u, v, 3);
    if(out_got_sample)
    {
        *out_got_sample = true;
    }

    const float luminance = rf + gf + bf;
    if(luminance <= 1e-4f && af <= 0.02f)
    {
        return (RGBColor)0;
    }

    const int r8 = (int)std::lround(std::clamp(rf, 0.0f, 1.0f) * 255.0f);
    const int g8 = (int)std::lround(std::clamp(gf, 0.0f, 1.0f) * 255.0f);
    const int b8 = (int)std::lround(std::clamp(bf, 0.0f, 1.0f) * 255.0f);
    return (RGBColor)((b8 << 16) | (g8 << 8) | r8);
}

float EstimateForwardFaceLuminance(const GameTelemetryBridge::GpuPanoramaFrameChannel& pano)
{
    if(!pano.has_frame || !pano.rgba || pano.rgba->empty() || pano.face_w <= 0 || pano.face_h <= 0 || pano.face_count <= 0)
    {
        return 0.0f;
    }

    float max_luma = 0.0f;
    for(int face = 0; face < pano.face_count; ++face)
    {
        const float rf = SampleChannelBilinear(pano, face, 0.5f, 0.5f, 0);
        const float gf = SampleChannelBilinear(pano, face, 0.5f, 0.5f, 1);
        const float bf = SampleChannelBilinear(pano, face, 0.5f, 0.5f, 2);
        max_luma = std::max(max_luma, rf + gf + bf);
    }
    return max_luma;
}

RGBColor SampleRoomPoint(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                         float room_to_world_scale,
                         float heading_offset_deg,
                         float room_x,
                         float room_y,
                         float room_z,
                         float effect_origin_x,
                         float effect_origin_y,
                         float effect_origin_z,
                         float pos_offset_forward_blocks,
                         float pos_offset_right_blocks,
                         float pos_offset_up_blocks,
                         bool* out_got_sample)
{
    if(out_got_sample)
    {
        *out_got_sample = false;
    }

    if(!telemetry.gpu_panorama.has_frame || !telemetry.has_player_pose || !telemetry.gpu_panorama.rgba ||
       telemetry.gpu_panorama.rgba->empty())
    {
        return (RGBColor)0;
    }

    const float scale = std::clamp(room_to_world_scale, 0.005f, 0.80f);
    const float grid_units_per_block = 1.0f / scale;

    const float eff_ox = effect_origin_x - pos_offset_right_blocks * grid_units_per_block;
    const float eff_oy = effect_origin_y - pos_offset_up_blocks * grid_units_per_block;
    const float eff_oz = effect_origin_z + pos_offset_forward_blocks * grid_units_per_block;

    const SpatialCoordinateSpaces::RoomGridDelta local =
        SpatialCoordinateSpaces::RoomGridToPlayerLocalBlocks(room_x,
                                                             room_y,
                                                             room_z,
                                                             eff_ox,
                                                             eff_oy,
                                                             eff_oz,
                                                             scale);

    const SpatialBasisUtils::BasisVectors basis = SpatialBasisUtils::BuildHorizontalBasis(
        telemetry.forward_x,
        telemetry.forward_y,
        telemetry.forward_z,
        heading_offset_deg);

    float target_x = 0.0f;
    float target_y = 0.0f;
    float target_z = 0.0f;
    SpatialCoordinateSpaces::PlayerLocalBlocksToGameWorld(basis,
                                                          eff_ox,
                                                          eff_oy,
                                                          eff_oz,
                                                          local,
                                                          target_x,
                                                          target_y,
                                                          target_z);

    constexpr float kMcDefaultEyeHeightBlocks = 1.62f;
    float eye_x = telemetry.gpu_panorama.anchor_x;
    float eye_y = telemetry.gpu_panorama.anchor_y;
    float eye_z = telemetry.gpu_panorama.anchor_z;
    if(eye_y <= telemetry.player_y + 0.05f)
    {
        eye_y += kMcDefaultEyeHeightBlocks;
    }

    float dir_x = target_x - eye_x;
    float dir_y = target_y - eye_y;
    float dir_z = target_z - eye_z;
    const float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
    if(dir_len <= 0.05f)
    {
        return (RGBColor)0;
    }
    dir_x /= dir_len;
    dir_y /= dir_len;
    dir_z /= dir_len;

    return SampleCubemapDirection(telemetry.gpu_panorama, dir_x, dir_y, dir_z, out_got_sample);
}

}
