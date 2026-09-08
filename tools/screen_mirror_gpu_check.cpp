// SPDX-License-Identifier: GPL-2.0-only
// Standalone math check for Screen Mirror plane mapping + wave/occupancy.
// g++ -std=c++17 -O2 -o /tmp/screen_mirror_gpu_check tools/screen_mirror_gpu_check.cpp && /tmp/screen_mirror_gpu_check

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../Effects3D/ScreenMirror/ScreenMirrorWaveMath.h"

namespace
{
    struct Vec3
    {
        float x;
        float y;
        float z;
    };

    struct Xform
    {
        Vec3 position;
        Vec3 rotation;
        Vec3 scale;
    };

    Vec3 LocalToWorld(const Vec3& local_pos, const Xform& transform)
    {
        Vec3 rotated = {
            local_pos.x * transform.scale.x,
            local_pos.y * transform.scale.y,
            local_pos.z * transform.scale.z,
        };

        const float rx = transform.rotation.x * 3.14159265359f / 180.0f;
        const float ry = transform.rotation.y * 3.14159265359f / 180.0f;
        const float rz = transform.rotation.z * 3.14159265359f / 180.0f;

        float temp_y = rotated.y * std::cos(rx) - rotated.z * std::sin(rx);
        float temp_z = rotated.y * std::sin(rx) + rotated.z * std::cos(rx);
        rotated.y = temp_y;
        rotated.z = temp_z;

        float temp_x = rotated.x * std::cos(ry) + rotated.z * std::sin(ry);
        temp_z = -rotated.x * std::sin(ry) + rotated.z * std::cos(ry);
        rotated.x = temp_x;
        rotated.z = temp_z;

        temp_x = rotated.x * std::cos(rz) - rotated.y * std::sin(rz);
        temp_y = rotated.x * std::sin(rz) + rotated.y * std::cos(rz);
        rotated.x = temp_x;
        rotated.y = temp_y;

        return {
            rotated.x + transform.position.x,
            rotated.y + transform.position.y,
            rotated.z + transform.position.z,
        };
    }

    Vec3 WorldToLocal(const Vec3& world_pos, const Xform& transform)
    {
        Vec3 p{
            world_pos.x - transform.position.x,
            world_pos.y - transform.position.y,
            world_pos.z - transform.position.z,
        };

        const float deg = 3.14159265359f / 180.0f;
        const float rx = -transform.rotation.x * deg;
        const float ry = -transform.rotation.y * deg;
        const float rz = -transform.rotation.z * deg;

        float temp_x = p.x * std::cos(rz) - p.y * std::sin(rz);
        float temp_y = p.x * std::sin(rz) + p.y * std::cos(rz);
        p.x = temp_x;
        p.y = temp_y;

        temp_x = p.x * std::cos(ry) + p.z * std::sin(ry);
        float temp_z = -p.x * std::sin(ry) + p.z * std::cos(ry);
        p.x = temp_x;
        p.z = temp_z;

        temp_y = p.y * std::cos(rx) - p.z * std::sin(rx);
        temp_z = p.y * std::sin(rx) + p.z * std::cos(rx);
        p.y = temp_y;
        p.z = temp_z;

        const float inv_sx = (std::fabs(transform.scale.x) > 1e-8f) ? 1.0f / transform.scale.x : 1.0f;
        const float inv_sy = (std::fabs(transform.scale.y) > 1e-8f) ? 1.0f / transform.scale.y : 1.0f;
        const float inv_sz = (std::fabs(transform.scale.z) > 1e-8f) ? 1.0f / transform.scale.z : 1.0f;
        return {p.x * inv_sx, p.y * inv_sy, p.z * inv_sz};
    }

    void MapRectangle(const Vec3& led, const Xform& plane, float width_mm, float height_mm,
                      float grid_scale_mm, float& u, float& v, float& dist_mm)
    {
        const Vec3 local = WorldToLocal(led, plane);
        const float width_units = std::max(width_mm / grid_scale_mm, 1e-4f);
        const float height_units = std::max(height_mm / grid_scale_mm, 1e-4f);
        u = std::clamp(0.5f + local.x / width_units, 0.0f, 1.0f);
        v = std::clamp(0.5f + local.y / height_units, 0.0f, 1.0f);
        dist_mm = std::fabs(local.z) * grid_scale_mm;
    }

    float Dist3(const Vec3& a, const Vec3& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
}

int main()
{
    int fails = 0;

    {
        const Vec3 locals[] = {
            {0.f, 0.f, 0.f},
            {50.f, 0.f, 0.f},
            {0.f, 30.f, 0.f},
            {-40.f, -20.f, 8.f},
            {12.f, -7.f, -15.f},
        };
        const Xform xforms[] = {
            {{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}},
            {{10.f, 40.f, -20.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}},
            {{0.f, 0.f, 0.f}, {0.f, 0.f, 90.f}, {1.f, 1.f, 1.f}},
            {{5.f, -8.f, 12.f}, {25.f, -40.f, 70.f}, {1.f, 1.f, 1.f}},
            {{2.f, 3.f, 4.f}, {-15.f, 33.f, -8.f}, {2.f, 0.5f, 1.5f}},
        };
        for(size_t xi = 0; xi < sizeof(xforms) / sizeof(xforms[0]); ++xi)
        {
            for(size_t li = 0; li < sizeof(locals) / sizeof(locals[0]); ++li)
            {
                const Vec3 world = LocalToWorld(locals[li], xforms[xi]);
                const Vec3 back = WorldToLocal(world, xforms[xi]);
                if(Dist3(back, locals[li]) > 1e-4f)
                {
                    std::fprintf(stderr, "roundtrip fail xform %zu local %zu got (%f,%f,%f) want (%f,%f,%f)\n",
                                 xi, li, back.x, back.y, back.z,
                                 locals[li].x, locals[li].y, locals[li].z);
                    fails++;
                }
            }
        }
    }

    const float scale = 10.f;
    const float width_mm = 1000.f;
    const float height_mm = 600.f;
    const float half_w = (width_mm / scale) * 0.5f;
    const float half_h = (height_mm / scale) * 0.5f;
    const Xform identity{{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};

    {
        float u, v, d;
        MapRectangle({0.f, 0.f, 0.f}, identity, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u - 0.5f) > 1e-5f || std::fabs(v - 0.5f) > 1e-5f || d > 1e-4f)
        {
            std::fprintf(stderr, "center map (%f,%f,%f) want (0.5,0.5,0)\n", u, v, d);
            fails++;
        }
        MapRectangle({half_w, 0.f, 0.f}, identity, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u - 1.0f) > 1e-5f || std::fabs(v - 0.5f) > 1e-5f)
        {
            std::fprintf(stderr, "right edge (%f,%f) want (1,0.5)\n", u, v);
            fails++;
        }
        MapRectangle({-half_w, 0.f, 0.f}, identity, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u) > 1e-5f || std::fabs(v - 0.5f) > 1e-5f)
        {
            std::fprintf(stderr, "left edge (%f,%f) want (0,0.5)\n", u, v);
            fails++;
        }
        MapRectangle({0.f, half_h, 0.f}, identity, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u - 0.5f) > 1e-5f || std::fabs(v - 1.0f) > 1e-5f)
        {
            std::fprintf(stderr, "top edge (%f,%f) want (0.5,1)\n", u, v);
            fails++;
        }
        MapRectangle({0.f, -half_h, 0.f}, identity, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u - 0.5f) > 1e-5f || std::fabs(v) > 1e-5f)
        {
            std::fprintf(stderr, "bottom edge (%f,%f) want (0.5,0)\n", u, v);
            fails++;
        }
        MapRectangle({half_w * 3.f, 0.f, 0.f}, identity, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u - 1.0f) > 1e-5f)
        {
            std::fprintf(stderr, "outside right should clamp u=1, got %f\n", u);
            fails++;
        }
        MapRectangle({0.f, 0.f, 12.f}, identity, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u - 0.5f) > 1e-5f || std::fabs(v - 0.5f) > 1e-5f || std::fabs(d - 120.f) > 0.05f)
        {
            std::fprintf(stderr, "ortho behind plane (%f,%f,%f) want (0.5,0.5,120)\n", u, v, d);
            fails++;
        }
    }

    {
        const Xform rz90{{0.f, 0.f, 0.f}, {0.f, 0.f, 90.f}, {1.f, 1.f, 1.f}};
        const Vec3 local_right{half_w, 0.f, 0.f};
        const Vec3 world_right = LocalToWorld(local_right, rz90);
        float u, v, d;
        MapRectangle(world_right, rz90, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u - 1.0f) > 1e-4f || std::fabs(v - 0.5f) > 1e-4f)
        {
            std::fprintf(stderr, "Rz90 right edge (%f,%f) want (1,0.5) world=(%f,%f,%f)\n",
                         u, v, world_right.x, world_right.y, world_right.z);
            fails++;
        }
        const Vec3 local_top{0.f, half_h, 0.f};
        const Vec3 world_top = LocalToWorld(local_top, rz90);
        MapRectangle(world_top, rz90, width_mm, height_mm, scale, u, v, d);
        if(std::fabs(u - 0.5f) > 1e-4f || std::fabs(v - 1.0f) > 1e-4f)
        {
            std::fprintf(stderr, "Rz90 top edge (%f,%f) want (0.5,1)\n", u, v);
            fails++;
        }
    }

    {
        const Xform moved{{10.f, 40.f, -20.f}, {25.f, -40.f, 70.f}, {1.f, 1.f, 1.f}};
        const Vec3 corners[4] = {
            {-half_w, -half_h, 0.f},
            {half_w, -half_h, 0.f},
            {half_w, half_h, 0.f},
            {-half_w, half_h, 0.f},
        };
        const float want[4][2] = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
        for(int i = 0; i < 4; ++i)
        {
            const Vec3 world = LocalToWorld(corners[i], moved);
            float u, v, d;
            MapRectangle(world, moved, width_mm, height_mm, scale, u, v, d);
            if(std::fabs(u - want[i][0]) > 1e-4f || std::fabs(v - want[i][1]) > 1e-4f || d > 0.05f)
            {
                std::fprintf(stderr, "gizmo corner %d uv=(%f,%f) dist=%f want (%f,%f)\n",
                             i, u, v, d, want[i][0], want[i][1]);
                fails++;
            }
        }
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
    {
        const float tte_only = ResolveWaveSpeedMmPerMs(1.0f, 0.0f, 3000.0f);
        const float tte_wins = ResolveWaveSpeedMmPerMs(1.0f, 100.0f, 3000.0f);
        if(std::fabs(tte_only - tte_wins) > 1e-4f)
        {
            std::fprintf(stderr, "time-to-edge should replace intensity, got %f vs %f\n", tte_wins, tte_only);
            fails++;
        }
    }

    float span = WaveHistorySpanMs(1.0f, 2000.0f, 500.0f);
    if(std::fabs(span - 2500.0f) > 0.01f)
    {
        std::fprintf(stderr, "span %f want 2500\n", span);
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

    auto try_room_uv = [](float x, float y, float z,
                          float minx, float miny, float minz,
                          float spx, float spy, float spz,
                          float& u, float& v, float& w) -> bool {
        u = (x - minx) / spx;
        v = (y - miny) / spy;
        w = (z - minz) / spz;
        constexpr float kEps = 1e-4f;
        if(u < -kEps || u > 1.0f + kEps || v < -kEps || v > 1.0f + kEps || w < -kEps || w > 1.0f + kEps)
        {
            return false;
        }
        u = std::max(0.0f, std::min(1.0f, u));
        v = std::max(0.0f, std::min(1.0f, v));
        w = std::max(0.0f, std::min(1.0f, w));
        return true;
    };
    auto clamp_room_uv = [](float x, float y, float z,
                            float minx, float miny, float minz,
                            float spx, float spy, float spz,
                            float& u, float& v, float& w) {
        u = std::max(0.0f, std::min(1.0f, (x - minx) / spx));
        v = std::max(0.0f, std::min(1.0f, (y - miny) / spy));
        w = std::max(0.0f, std::min(1.0f, (z - minz) / spz));
    };

    const float gmin_x = -50.f, gmin_y = 0.f, gmin_z = -20.f;
    const float span_u_x = 200.f, span_u_y = 80.f, span_u_z = 160.f;
    float ru = 0.f, rv = 0.f, rw = 0.f;
    const float mid_y = gmin_y + 0.5f * span_u_y;
    const float mid_x = gmin_x + 0.5f * span_u_x;
    const float mid_z = gmin_z + 0.5f * span_u_z;
    const bool in_ok = try_room_uv(mid_x, mid_y, mid_z, gmin_x, gmin_y, gmin_z,
                                   span_u_x, span_u_y, span_u_z, ru, rv, rw);
    if(!in_ok || std::fabs(ru - 0.5f) > 1e-4f)
    {
        std::fprintf(stderr, "in-AABB room UV should sample, got ok=%d u=%f\n",
                     in_ok ? 1 : 0, ru);
        fails++;
    }

    const float outside[4][3] = {
        {gmin_x + span_u_x + 40.f, mid_y, mid_z},
        {gmin_x - 40.f, mid_y, mid_z},
        {mid_x, mid_y, gmin_z + span_u_z + 40.f},
        {mid_x, mid_y, gmin_z - 40.f}
    };
    float clamped_faces[4][3];
    int rejected = 0;
    for(int i = 0; i < 4; ++i)
    {
        float cu, cv, cw;
        clamp_room_uv(outside[i][0], outside[i][1], outside[i][2],
                      gmin_x, gmin_y, gmin_z, span_u_x, span_u_y, span_u_z, cu, cv, cw);
        clamped_faces[i][0] = cu;
        clamped_faces[i][1] = cv;
        clamped_faces[i][2] = cw;
        if(!try_room_uv(outside[i][0], outside[i][1], outside[i][2],
                        gmin_x, gmin_y, gmin_z, span_u_x, span_u_y, span_u_z, ru, rv, rw))
        {
            rejected++;
        }
        else
        {
            std::fprintf(stderr, "outside-AABB sample %d should be unlit, got uv=(%f,%f,%f)\n",
                         i, ru, rv, rw);
            fails++;
        }
    }
    if(rejected != 4)
    {
        std::fprintf(stderr, "four-quadrant reject count %d want 4\n", rejected);
        fails++;
    }
    int distinct_clamped = 0;
    for(int i = 0; i < 4; ++i)
    {
        bool unique = true;
        for(int j = 0; j < i; ++j)
        {
            if(std::fabs(clamped_faces[i][0] - clamped_faces[j][0]) < 1e-4f &&
               std::fabs(clamped_faces[i][1] - clamped_faces[j][1]) < 1e-4f &&
               std::fabs(clamped_faces[i][2] - clamped_faces[j][2]) < 1e-4f)
            {
                unique = false;
                break;
            }
        }
        if(unique)
        {
            distinct_clamped++;
        }
    }
    if(distinct_clamped < 4)
    {
        std::fprintf(stderr, "face-clamp fixture too weak distinct=%d\n", distinct_clamped);
        fails++;
    }

    {
        const float box = 0.f, span = 100.f, origin = 20.f, far = 90.f;
        const float room_u = (far - box) / span;
        const float hw = span * 0.5f;
        const float origin_local_u = 0.5f + 0.5f * (far - origin) / hw;
        const float origin_local_clamped = std::max(0.0f, std::min(1.0f, origin_local_u));
        if(std::fabs(room_u - 0.9f) > 1e-4f)
        {
            std::fprintf(stderr, "far-wall room UV %f want 0.9\n", room_u);
            fails++;
        }
        if(origin_local_clamped < 0.999f)
        {
            std::fprintf(stderr, "origin-local half-box fixture should clamp, got %f\n",
                         origin_local_clamped);
            fails++;
        }
        if(std::fabs(room_u - origin_local_clamped) < 0.05f)
        {
            std::fprintf(stderr, "room UV must not match origin-local face clamp\n");
            fails++;
        }
    }

    {
        struct Room
        {
            float min_x, min_y, min_z;
            float span_u_x, span_u_y, span_u_z;
            float scale;
        };
        auto span_mm_x = [](const Room& r) { return r.span_u_x * r.scale; };
        auto span_mm_y = [](const Room& r) { return r.span_u_y * r.scale; };
        auto span_mm_z = [](const Room& r) { return r.span_u_z * r.scale; };
        auto room_uv = [](const Room& r, float x, float y, float z,
                          float& u, float& v, float& w) {
            u = (x - r.min_x) / r.span_u_x;
            v = (y - r.min_y) / r.span_u_y;
            w = (z - r.min_z) / r.span_u_z;
        };

        const Room small{0.f, 0.f, 0.f, 100.f, 80.f, 60.f, 10.f};
        const Room large{0.f, 0.f, 0.f, 1000.f, 800.f, 600.f, 10.f};
        const Room rooms[2] = {small, large};
        float max_mm_rooms[2] = {0.f, 0.f};

        for(int ri = 0; ri < 2; ++ri)
        {
            const Room& r = rooms[ri];
            const float led_x = r.min_x + 0.5f * r.span_u_x;
            const float led_y = r.min_y + 0.5f * r.span_u_y;
            const float led_z = r.min_z + 0.5f * r.span_u_z;
            const float plane_x = r.min_x + 0.5f * r.span_u_x;
            const float plane_y = r.min_y + 0.5f * r.span_u_y;
            const float plane_z = r.min_z;
            float p01x, p01y, p01z, map_u, map_v, map_w;
            room_uv(r, led_x, led_y, led_z, p01x, p01y, p01z);
            room_uv(r, plane_x, plane_y, plane_z, map_u, map_v, map_w);
            if(std::fabs(p01x - 0.5f) > 1e-4f || std::fabs(p01y - 0.5f) > 1e-4f ||
               std::fabs(p01z - 0.5f) > 1e-4f)
            {
                std::fprintf(stderr, "room %d mid LED UV (%f,%f,%f) want 0.5\n",
                             ri, p01x, p01y, p01z);
                fails++;
            }
            const float dmx = (p01x - map_u) * span_mm_x(r);
            const float dmy = (p01y - map_v) * span_mm_y(r);
            const float dmz = (p01z - map_w) * span_mm_z(r);
            const float want_x = (led_x - plane_x) * r.scale;
            const float want_y = (led_y - plane_y) * r.scale;
            const float want_z = (led_z - plane_z) * r.scale;
            if(std::fabs(dmx - want_x) > 0.05f || std::fabs(dmy - want_y) > 0.05f ||
               std::fabs(dmz - want_z) > 0.05f)
            {
                std::fprintf(stderr, "room %d delta_mm (%f,%f,%f) want (%f,%f,%f)\n",
                             ri, dmx, dmy, dmz, want_x, want_y, want_z);
                fails++;
            }

            const float max_mm = RoomCornerMaxDistanceMm(span_mm_x(r), span_mm_y(r), span_mm_z(r),
                                                         0.5f, 0.5f, 0.5f);
            max_mm_rooms[ri] = max_mm;
            if(std::fabs(max_mm - 3000.0f) < 1.0f)
            {
                std::fprintf(stderr, "room %d max_mm %f must not be the old 3000 fallback\n",
                             ri, max_mm);
                fails++;
            }
            const float speed_1s = ResolveWaveSpeedMmPerMs(1.0f, 0.0f, max_mm);
            const float want_speed = max_mm / 1000.0f;
            if(std::fabs(speed_1s - want_speed) > 0.02f)
            {
                std::fprintf(stderr, "room %d time-to-edge 1s speed %f want %f\n",
                             ri, speed_1s, want_speed);
                fails++;
            }
        }

        if(max_mm_rooms[0] > 1.0f)
        {
            const float ratio = max_mm_rooms[1] / max_mm_rooms[0];
            if(std::fabs(ratio - 10.0f) > 0.02f)
            {
                std::fprintf(stderr, "10x room max_mm ratio %f want 10 (small=%f large=%f)\n",
                             ratio, max_mm_rooms[0], max_mm_rooms[1]);
                fails++;
            }
        }

        float degenerate = 0.0f;
        if(degenerate <= 0.0f)
        {
            degenerate = RoomSpanLengthMm(0.0f, 0.0f, 0.0f);
        }
        if(degenerate <= 0.0f)
        {
            degenerate = 1.0f;
        }
        if(std::fabs(degenerate - 3000.0f) < 1.0f)
        {
            std::fprintf(stderr, "GLSL-style empty-span fallback %f must not be 3000\n", degenerate);
            fails++;
        }
        if(std::fabs(degenerate - 1.0f) > 1e-4f)
        {
            std::fprintf(stderr, "empty-span fallback %f want 1\n", degenerate);
            fails++;
        }
    }

    if(fails != 0)
    {
        std::fprintf(stderr, "FAILED %d checks\n", fails);
        return 1;
    }
        std::printf("screen_mirror_gpu_check: plane-rect, roundtrip, wave, radial, no-face-clamp, live-room OK\n");
    return 0;
}
