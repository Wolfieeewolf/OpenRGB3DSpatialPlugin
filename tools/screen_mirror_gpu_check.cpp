// SPDX-License-Identifier: GPL-2.0-only
// Standalone math check for Screen Mirror GPU packing + plane mapping.
// g++ -std=c++17 -O2 -o /tmp/screen_mirror_gpu_check tools/screen_mirror_gpu_check.cpp && /tmp/screen_mirror_gpu_check

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../Effects3D/ScreenMirror/ScreenMirrorWaveMath.h"

namespace
{
    float Pack01(float a, float b)
    {
        a = std::clamp(a, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        const float ai = std::floor(a * 4095.0f + 0.5f);
        const float bi = std::floor(b * 4095.0f + 0.5f);
        return ai * 4096.0f + bi;
    }

    void Unpack01(float p, float& a, float& b)
    {
        float bi = std::floor(std::fmod(p, 4096.0f));
        float ai = std::floor(p / 4096.0f);
        a = ai / 4095.0f;
        b = bi / 4095.0f;
    }

    void MapToScreen(float led_x, float led_y, float led_z,
                     float ref_x, float ref_y, float ref_z,
                     float rx, float ry, float rz,
                     float ux, float uy, float uz,
                     float grid_scale_mm,
                     float& u, float& v, float& dist_mm)
    {
        float dx = led_x - ref_x;
        float dy = led_y - ref_y;
        float dz = led_z - ref_z;
        float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        dist_mm = len * grid_scale_mm;
        if(len < 1e-6f)
        {
            u = 0.5f;
            v = 0.5f;
            return;
        }
        float inv = 1.0f / len;
        float dir_x = dx * inv;
        float dir_y = dy * inv;
        float dir_z = dz * inv;
        float dir_right = dir_x * rx + dir_y * ry + dir_z * rz;
        float dir_up = dir_x * ux + dir_y * uy + dir_z * uz;
        const float calib_rad = -25.5f * 3.14159265359f / 180.0f;
        const float cc = std::cos(calib_rad);
        const float ss = std::sin(calib_rad);
        const float dir_r_rot = dir_right * cc - dir_up * ss;
        const float dir_u_rot = dir_right * ss + dir_up * cc;
        const float inv_half_sqrt2 = 1.414213562373095f;
        u = std::clamp(0.5f + 0.5f * dir_r_rot * inv_half_sqrt2, 0.0f, 1.0f);
        v = std::clamp(0.5f + 0.5f * dir_u_rot * inv_half_sqrt2, 0.0f, 1.0f);
    }

    void GpuMap(float p01x, float p01y, float p01z,
                float ref_uvx, float ref_uvy, float ref_uvz,
                float span_mmx, float span_mmy, float span_mmz,
                float rx, float ry, float rz,
                float ux, float uy, float uz,
                float& u, float& v, float& dist_mm)
    {
        float dmx = (p01x - ref_uvx) * span_mmx;
        float dmy = (p01y - ref_uvy) * span_mmy;
        float dmz = (p01z - ref_uvz) * span_mmz;
        dist_mm = std::sqrt(dmx * dmx + dmy * dmy + dmz * dmz);
        if(dist_mm < 1e-4f)
        {
            u = 0.5f;
            v = 0.5f;
            return;
        }
        float dir_x = dmx / dist_mm;
        float dir_y = dmy / dist_mm;
        float dir_z = dmz / dist_mm;
        float dir_right = dir_x * rx + dir_y * ry + dir_z * rz;
        float dir_up = dir_x * ux + dir_y * uy + dir_z * uz;
        float cc = 0.9025853f;
        float ss = -0.4305111f;
        float dir_r_rot = dir_right * cc - dir_up * ss;
        float dir_u_rot = dir_right * ss + dir_up * cc;
        u = std::clamp(0.5f + 0.5f * dir_r_rot * 1.41421356f, 0.0f, 1.0f);
        v = std::clamp(0.5f + 0.5f * dir_u_rot * 1.41421356f, 0.0f, 1.0f);
    }
}

int main()
{
    int fails = 0;
    const float pack_cases[][2] = {
        {0.0f, 0.0f}, {1.0f, 1.0f}, {0.5f, 0.25f}, {0.333f, 0.667f}, {0.01f, 0.99f}
    };
    for(size_t i = 0; i < sizeof(pack_cases) / sizeof(pack_cases[0]); ++i)
    {
        float a = 0, b = 0;
        Unpack01(Pack01(pack_cases[i][0], pack_cases[i][1]), a, b);
        if(std::fabs(a - pack_cases[i][0]) > 0.0004f || std::fabs(b - pack_cases[i][1]) > 0.0004f)
        {
            std::fprintf(stderr, "pack fail (%f,%f) -> (%f,%f)\n",
                         pack_cases[i][0], pack_cases[i][1], a, b);
            fails++;
        }
    }

    const float gmin_x = -50.f, gmin_y = 0.f, gmin_z = -20.f;
    const float span_u_x = 200.f, span_u_y = 80.f, span_u_z = 160.f;
    const float scale = 10.f;
    const float span_mmx = span_u_x * scale;
    const float span_mmy = span_u_y * scale;
    const float span_mmz = span_u_z * scale;
    const float ref_x = 10.f, ref_y = 40.f, ref_z = 0.f;
    const float ref_uvx = (ref_x - gmin_x) / span_u_x;
    const float ref_uvy = (ref_y - gmin_y) / span_u_y;
    const float ref_uvz = (ref_z - gmin_z) / span_u_z;
    const float rx = 1.f, ry = 0.f, rz = 0.f;
    const float ux = 0.f, uy = 1.f, uz = 0.f;

    const float leds[][3] = {
        {10.f, 40.f, 0.f},
        {0.f, 10.f, 30.f},
        {150.f, 70.f, 100.f},
        {-50.f, 0.f, -20.f},
        {150.f, 80.f, 140.f}
    };
    for(size_t i = 0; i < sizeof(leds) / sizeof(leds[0]); ++i)
    {
        float u0, v0, d0, u1, v1, d1;
        MapToScreen(leds[i][0], leds[i][1], leds[i][2],
                    ref_x, ref_y, ref_z, rx, ry, rz, ux, uy, uz, scale, u0, v0, d0);
        float p01x = (leds[i][0] - gmin_x) / span_u_x;
        float p01y = (leds[i][1] - gmin_y) / span_u_y;
        float p01z = (leds[i][2] - gmin_z) / span_u_z;
        GpuMap(p01x, p01y, p01z, ref_uvx, ref_uvy, ref_uvz,
               span_mmx, span_mmy, span_mmz, rx, ry, rz, ux, uy, uz, u1, v1, d1);
        if(std::fabs(u0 - u1) > 1e-4f || std::fabs(v0 - v1) > 1e-4f || std::fabs(d0 - d1) > 0.05f)
        {
            std::fprintf(stderr, "map fail led %zu cpu=(%f,%f,%f) gpu=(%f,%f,%f)\n",
                         i, u0, v0, d0, u1, v1, d1);
            fails++;
        }
    }

    float layout = 2.f + 10.f * 4.f + 100.f + 1000.f + 2000.f;
    float L = std::floor(layout + 0.5f);
    float flip1 = (L >= 1999.5f) ? 1.f : 0.f;
    L -= flip1 * 2000.f;
    float flip0 = (L >= 999.5f) ? 1.f : 0.f;
    L -= flip0 * 1000.f;
    float use_q = (L >= 99.5f) ? 1.f : 0.f;
    L -= use_q * 100.f;
    float nhist = std::floor(L / 10.f);
    float nmon = L - nhist * 10.f;
    if(nmon != 2.f || nhist != 4.f || use_q != 1.f || flip0 != 1.f || flip1 != 1.f)
    {
        std::fprintf(stderr, "layout decode fail nmon=%f nhist=%f q=%f f0=%f f1=%f\n",
                     nmon, nhist, use_q, flip0, flip1);
        fails++;
    }

    float layout8 = 2.f + 10.f * 8.f + 100.f + 1000.f + 2000.f;
    L = std::floor(layout8 + 0.5f);
    flip1 = (L >= 1999.5f) ? 1.f : 0.f;
    L -= flip1 * 2000.f;
    flip0 = (L >= 999.5f) ? 1.f : 0.f;
    L -= flip0 * 1000.f;
    use_q = (L >= 99.5f) ? 1.f : 0.f;
    L -= use_q * 100.f;
    nhist = std::floor(L / 10.f);
    nmon = L - nhist * 10.f;
    if(nmon != 2.f || nhist != 8.f || use_q != 1.f || flip0 != 1.f || flip1 != 1.f)
    {
        std::fprintf(stderr, "layout8 decode fail nmon=%f nhist=%f q=%f f0=%f f1=%f\n",
                     nmon, nhist, use_q, flip0, flip1);
        fails++;
    }

    float speed_short = ResolveWaveSpeedMmPerMs(0.2f, 0.0f, 3000.0f);
    if(speed_short < 0.1f)
    {
        std::fprintf(stderr, "time-to-edge 0.2s should enable wave, got %f\n", speed_short);
        fails++;
    }
    float expected_speed = 3000.0f / 200.0f;
    if(std::fabs(speed_short - expected_speed) > 0.02f)
    {
        std::fprintf(stderr, "time-to-edge speed %f want %f\n", speed_short, expected_speed);
        fails++;
    }
    if(ResolveWaveSpeedMmPerMs(0.0f, 4.0f, 3000.0f) != 0.0f)
    {
        std::fprintf(stderr, "wave intensity 4%% should stay off\n");
        fails++;
    }
    if(ResolveWaveSpeedMmPerMs(0.0f, 5.0f, 3000.0f) < 0.1f)
    {
        std::fprintf(stderr, "wave intensity 5%% should enable wave\n");
        fails++;
    }

    float span = WaveHistorySpanMs(1.0f, 2000.0f, 500.0f);
    if(std::fabs(span - 2500.0f) > 0.01f)
    {
        std::fprintf(stderr, "span %f want 2500\n", span);
        fails++;
    }
    float row = WaveHistoryRow(1000.0f, 2000.0f, 8);
    if(std::fabs(row - 3.5f) > 0.01f)
    {
        std::fprintf(stderr, "history row %f want 3.5\n", row);
        fails++;
    }
    float w_front = WaveTrailWeight(500.0f, 500.0f, 500.0f, 2000.0f, 8);
    float w_tail = WaveTrailWeight(1000.0f, 500.0f, 500.0f, 2000.0f, 8);
    float w_future = WaveTrailWeight(0.0f, 500.0f, 500.0f, 2000.0f, 8);
    if(std::fabs(w_front - 1.0f) > 0.01f)
    {
        std::fprintf(stderr, "trail front weight %f want 1\n", w_front);
        fails++;
    }
    float want_tail = std::exp(-1.0f);
    if(std::fabs(w_tail - want_tail) > 0.02f)
    {
        std::fprintf(stderr, "trail weight %f want %f\n", w_tail, want_tail);
        fails++;
    }
    if(w_future > 0.01f)
    {
        std::fprintf(stderr, "future tile should not trail, got %f\n", w_future);
        fails++;
    }
    if(std::fabs(RadialMapUiToInternal(kRadialMapUiNeutral)) > 0.01f)
    {
        std::fprintf(stderr, "radial center should be 0, got %f\n", RadialMapUiToInternal(kRadialMapUiNeutral));
        fails++;
    }
    if(std::fabs(RadialMapUiToInternal(0) + 50.0f) > 0.01f ||
       std::fabs(RadialMapUiToInternal(100) - 50.0f) > 0.01f)
    {
        std::fprintf(stderr, "radial ends want -50/+50\n");
        fails++;
    }

    /* Display plane at the front wall, Spatial Anchor at the LED centroid.
     * Screen UVs must follow SpatialMapToScreen(plane.position), not the anchor. */
    const float plane_x = 150.f, plane_y = 40.f, plane_z = -20.f;
    const float anchor_x = ref_x, anchor_y = ref_y, anchor_z = ref_z;
    const float plane_uvx = (plane_x - gmin_x) / span_u_x;
    const float plane_uvy = (plane_y - gmin_y) / span_u_y;
    const float plane_uvz = (plane_z - gmin_z) / span_u_z;
    int plane_map_hits = 0;
    int anchor_map_mismatch = 0;
    for(size_t i = 0; i < sizeof(leds) / sizeof(leds[0]); ++i)
    {
        float u_plane, v_plane, d_plane, u_gpu, v_gpu, d_gpu, u_anchor, v_anchor, d_anchor;
        MapToScreen(leds[i][0], leds[i][1], leds[i][2],
                    plane_x, plane_y, plane_z, rx, ry, rz, ux, uy, uz, scale,
                    u_plane, v_plane, d_plane);
        float p01x = (leds[i][0] - gmin_x) / span_u_x;
        float p01y = (leds[i][1] - gmin_y) / span_u_y;
        float p01z = (leds[i][2] - gmin_z) / span_u_z;
        GpuMap(p01x, p01y, p01z, plane_uvx, plane_uvy, plane_uvz,
               span_mmx, span_mmy, span_mmz, rx, ry, rz, ux, uy, uz, u_gpu, v_gpu, d_gpu);
        MapToScreen(leds[i][0], leds[i][1], leds[i][2],
                    anchor_x, anchor_y, anchor_z, rx, ry, rz, ux, uy, uz, scale,
                    u_anchor, v_anchor, d_anchor);
        if(std::fabs(u_plane - u_gpu) > 1e-4f || std::fabs(v_plane - v_gpu) > 1e-4f)
        {
            std::fprintf(stderr, "plane-map fail led %zu cpu=(%f,%f) gpu=(%f,%f)\n",
                         i, u_plane, v_plane, u_gpu, v_gpu);
            fails++;
        }
        else
        {
            plane_map_hits++;
        }
        if(std::fabs(u_plane - u_anchor) > 0.02f || std::fabs(v_plane - v_anchor) > 0.02f)
        {
            anchor_map_mismatch++;
        }
        float fall_dx = (p01x - ((anchor_x - gmin_x) / span_u_x)) * span_mmx;
        float fall_dy = (p01y - ((anchor_y - gmin_y) / span_u_y)) * span_mmy;
        float fall_dz = (p01z - ((anchor_z - gmin_z) / span_u_z)) * span_mmz;
        float fall_gpu = std::sqrt(fall_dx * fall_dx + fall_dy * fall_dy + fall_dz * fall_dz);
        if(std::fabs(fall_gpu - d_anchor) > 0.05f)
        {
            std::fprintf(stderr, "falloff-from-anchor fail led %zu gpu=%f cpu=%f\n",
                         i, fall_gpu, d_anchor);
            fails++;
        }
    }
    if(plane_map_hits < 3 || anchor_map_mismatch < 1)
    {
        std::fprintf(stderr, "plane-vs-anchor fixture too weak hits=%d mismatch=%d\n",
                     plane_map_hits, anchor_map_mismatch);
        fails++;
    }

    /* Atlas Y flip (the old ConvertRgbaAtlasToRgb bug) must not match plane mapping. */
    {
        float u0, v0, d0, uflip, vflip, dflip;
        const float ledx = 0.f, ledy = 10.f, ledz = 30.f;
        MapToScreen(ledx, ledy, ledz, plane_x, plane_y, plane_z, rx, ry, rz, ux, uy, uz, scale,
                    u0, v0, d0);
        float p01x = (ledx - gmin_x) / span_u_x;
        float p01y = 1.f - (ledy - gmin_y) / span_u_y;
        float p01z = (ledz - gmin_z) / span_u_z;
        GpuMap(p01x, p01y, p01z, plane_uvx, plane_uvy, plane_uvz,
               span_mmx, span_mmy, span_mmz, rx, ry, rz, ux, uy, uz, uflip, vflip, dflip);
        if(std::fabs(u0 - uflip) < 1e-3f && std::fabs(v0 - vflip) < 1e-3f)
        {
            std::fprintf(stderr, "flipped-Y atlas UV should not match SpatialMapToScreen\n");
            fails++;
        }
    }

    /* u_media: first uploaded QImage row is texture t=0. Live capture applies 1-v
     * so mapper v=1 (plane up) samples capture row 0 (screen top), matching CPU. */
    {
        const float mapper_v_up = 1.0f;
        const float live_tex_v = 1.0f - mapper_v_up;
        const float cal_tex_v = mapper_v_up;
        if(std::fabs(live_tex_v) > 1e-6f)
        {
            std::fprintf(stderr, "live tex v for mapper-up want 0, got %f\n", live_tex_v);
            fails++;
        }
        if(std::fabs(cal_tex_v - 1.0f) > 1e-6f)
        {
            std::fprintf(stderr, "calibration tex v for mapper-up want 1, got %f\n", cal_tex_v);
            fails++;
        }
    }

    const int kShared = 6;
    const int kMon = 24;
    if(kShared + 2 * kMon > 64)
    {
        std::fprintf(stderr, "monitor pack %d exceeds kMaxParams 64\n", kShared + 2 * kMon);
        fails++;
    }
    const int mon1_base = kShared + kMon;
    const int blend0 = kShared + 23;
    const int blend1 = mon1_base + 23;
    if(mon1_base != 30 || blend0 != 29 || blend1 != 53)
    {
        std::fprintf(stderr, "param slots mon1=%d blend0=%d blend1=%d\n",
                     mon1_base, blend0, blend1);
        fails++;
    }

    if(fails != 0)
    {
        std::fprintf(stderr, "FAILED %d checks\n", fails);
        return 1;
    }
        std::printf("screen_mirror_gpu_check: pack, map, plane-uv, layout, wave timing, trail, radial OK\n");
    return 0;
}
