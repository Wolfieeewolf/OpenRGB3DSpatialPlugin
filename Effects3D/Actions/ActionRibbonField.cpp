// SPDX-License-Identifier: GPL-2.0-only

#include "ActionRibbonField.h"
#include "ActionScript.h"
#include "SpatialVolumeFieldAssist.h"

#include <QImage>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Action
{
namespace ribbon_field
{
namespace
{

struct Point
{
    float x = 0.5f;
    float y = 0.5f;
    float z = 0.5f;
};

std::mutex g_mu;
std::vector<Point> g_pts;
std::unique_ptr<SpatialVolumeFieldAssist> g_assist;
bool g_ready = false;
bool g_gpu = false;
bool g_shader_loaded = false;
std::uint64_t g_last_seq = 0;
float g_last_time = -1.0f;
float g_ox = 0.5f;
float g_oy = 0.5f;
float g_oz = 0.5f;
float g_size = 1.0f;
float g_radius = 0.06f;
float g_head = 0.0f;

/* GPGPU-style: arc/helix curves evaluated in the fragment shader. */
const char* kArcBody = R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float reach = u_params[4];
    float radius = max(0.001, u_params[7]);
    float head = clamp(u_params[5], 0.0, 1.0);
    float progress = u_params[0];
    float speed = max(0.15, u_params[6]);
    float r2 = radius * radius;
    float best_d2 = 1e9;
    float along = 0.0;
    const int segs = 24;
    for(int i = 0; i < segs; ++i)
    {
        float u0 = float(i) / float(segs);
        float u1 = float(i + 1) / float(segs);
        float ang0 = (u0 - 0.5) * 3.14159265;
        float ang1 = (u1 - 0.5) * 3.14159265;
        vec3 a = clamp(vec3(
            origin.x + sin(ang0) * reach,
            origin.y + sin(u0 * 6.2831853 + progress * 6.2831853 * speed) * reach * 0.22,
            origin.z + (u0 - 0.5) * 2.0 * reach), 0.0, 1.0);
        vec3 b = clamp(vec3(
            origin.x + sin(ang1) * reach,
            origin.y + sin(u1 * 6.2831853 + progress * 6.2831853 * speed) * reach * 0.22,
            origin.z + (u1 - 0.5) * 2.0 * reach), 0.0, 1.0);
        vec3 ab = b - a;
        float ab2 = dot(ab, ab);
        float t = ab2 > 1e-12 ? clamp(dot(p01 - a, ab) / ab2, 0.0, 1.0) : 0.0;
        vec3 c = a + ab * t;
        float d2 = dot(p01 - c, p01 - c);
        if(d2 < best_d2) { best_d2 = d2; along = mix(u0, u1, t); }
    }
    float dens = exp(-best_d2 / r2);
    float head_dist = abs(along - head);
    float head_win = min(head_dist, 1.0 - head_dist);
    dens *= 0.35 + 0.65 * exp(-head_win * head_win * 40.0);
    dens = clamp(dens, 0.0, 1.0);
    float h = along * 6.2831853;
    vec3 col = vec3(0.55 + 0.45 * sin(h), 0.35 + 0.45 * sin(h + 2.094), 0.65 + 0.35 * sin(h + 4.188));
    out_color = vec4(col * dens, dens);
}
)GLSL";

const char* kHelixBody = R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float reach = u_params[4];
    float height = u_params[6];
    float radius = max(0.001, u_params[7]);
    float head = clamp(u_params[5], 0.0, 1.0);
    float spin = u_params[0] * 6.2831853;
    float r2 = radius * radius;
    float best_d2 = 1e9;
    float along = 0.0;
    const int segs = 32;
    for(int i = 0; i < segs; ++i)
    {
        float u0 = float(i) / float(segs);
        float u1 = float(i + 1) / float(segs);
        float ang0 = u0 * 6.2831853 * 2.0 + spin;
        float ang1 = u1 * 6.2831853 * 2.0 + spin;
        vec3 a = clamp(vec3(origin.x + cos(ang0) * reach,
                            origin.y + (u0 - 0.5) * 2.0 * height,
                            origin.z + sin(ang0) * reach), 0.0, 1.0);
        vec3 b = clamp(vec3(origin.x + cos(ang1) * reach,
                            origin.y + (u1 - 0.5) * 2.0 * height,
                            origin.z + sin(ang1) * reach), 0.0, 1.0);
        vec3 ab = b - a;
        float ab2 = dot(ab, ab);
        float t = ab2 > 1e-12 ? clamp(dot(p01 - a, ab) / ab2, 0.0, 1.0) : 0.0;
        float d2 = dot(p01 - (a + ab * t), p01 - (a + ab * t));
        if(d2 < best_d2) { best_d2 = d2; along = mix(u0, u1, t); }
    }
    float dens = exp(-best_d2 / r2);
    float head_dist = abs(along - head);
    float head_win = min(head_dist, 1.0 - head_dist);
    dens *= 0.35 + 0.65 * exp(-head_win * head_win * 40.0);
    dens = clamp(dens, 0.0, 1.0);
    float h = along * 6.2831853;
    vec3 col = vec3(0.55 + 0.45 * sin(h), 0.35 + 0.45 * sin(h + 2.094), 0.65 + 0.35 * sin(h + 4.188));
    out_color = vec4(col * dens, dens);
}
)GLSL";

std::string g_gpu_mode;

float DistPointSeg2(float px, float py, float pz,
                    float ax, float ay, float az,
                    float bx, float by, float bz,
                    float* out_t)
{
    const float abx = bx - ax;
    const float aby = by - ay;
    const float abz = bz - az;
    const float apx = px - ax;
    const float apy = py - ay;
    const float apz = pz - az;
    const float ab2 = abx * abx + aby * aby + abz * abz;
    float t = 0.0f;
    if(ab2 > 1e-12f)
    {
        t = std::clamp((apx * abx + apy * aby + apz * abz) / ab2, 0.0f, 1.0f);
    }
    if(out_t)
    {
        *out_t = t;
    }
    const float cx = ax + abx * t;
    const float cy = ay + aby * t;
    const float cz = az + abz * t;
    const float dx = px - cx;
    const float dy = py - cy;
    const float dz = pz - cz;
    return dx * dx + dy * dy + dz * dz;
}

void NearestOnRibbon(float px, float py, float pz, float* out_d2, float* out_along01)
{
    *out_d2 = 1e9f;
    *out_along01 = 0.0f;
    if(g_pts.size() < 2)
    {
        return;
    }
    const float seg_w = 1.0f / (float)(g_pts.size() - 1);
    for(size_t i = 0; i + 1 < g_pts.size(); ++i)
    {
        const Point& a = g_pts[i];
        const Point& b = g_pts[i + 1];
        float t = 0.0f;
        const float d2 = DistPointSeg2(px, py, pz, a.x, a.y, a.z, b.x, b.y, b.z, &t);
        if(d2 < *out_d2)
        {
            *out_d2 = d2;
            *out_along01 = ((float)i + t) * seg_w;
        }
    }
}

void RebuildArc(const Clip& clip, float progress)
{
    g_pts.clear();
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    const float reach = (0.22f + clip.pulse_length * 0.35f) * size;
    g_radius = std::max(0.02f, (0.035f + clip.pulse_length * 0.05f) * size);
    g_head = std::clamp(progress * std::max(0.15f, clip.speed), 0.0f, 1.0f);

    constexpr int kSegs = 24;
    g_pts.reserve((size_t)kSegs + 1);
    for(int i = 0; i <= kSegs; ++i)
    {
        const float u = (float)i / (float)kSegs;
        const float ang = (u - 0.5f) * 3.14159265f;
        Point p;
        p.x = g_ox + std::sin(ang) * reach;
        p.y = g_oy + std::sin(u * 6.2831853f + progress * 6.2831853f * clip.speed) * reach * 0.22f;
        p.z = g_oz + (u - 0.5f) * 2.0f * reach;
        p.x = std::clamp(p.x, 0.0f, 1.0f);
        p.y = std::clamp(p.y, 0.0f, 1.0f);
        p.z = std::clamp(p.z, 0.0f, 1.0f);
        g_pts.push_back(p);
    }
    g_ready = g_pts.size() >= 2;
}

void RebuildHelix(const Clip& clip, float progress)
{
    g_pts.clear();
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    const float reach = (0.18f + clip.pulse_length * 0.30f) * size;
    const float height = (0.35f + clip.pulse_length * 0.35f) * size;
    g_radius = std::max(0.02f, (0.03f + clip.pulse_length * 0.045f) * size);
    g_head = std::clamp(progress * std::max(0.15f, clip.speed), 0.0f, 1.0f);
    const float spin = progress * 6.2831853f * std::max(0.15f, clip.speed);

    constexpr int kSegs = 32;
    g_pts.reserve((size_t)kSegs + 1);
    for(int i = 0; i <= kSegs; ++i)
    {
        const float u = (float)i / (float)kSegs;
        const float ang = u * 6.2831853f * 2.0f + spin;
        Point p;
        p.x = g_ox + std::cos(ang) * reach;
        p.y = g_oy + (u - 0.5f) * 2.0f * height;
        p.z = g_oz + std::sin(ang) * reach;
        p.x = std::clamp(p.x, 0.0f, 1.0f);
        p.y = std::clamp(p.y, 0.0f, 1.0f);
        p.z = std::clamp(p.z, 0.0f, 1.0f);
        g_pts.push_back(p);
    }
    g_ready = g_pts.size() >= 2;
}

bool BakeGpuAtlas(const Clip& clip, float progress, const std::string& mode, std::uint64_t render_sequence)
{
    if(!g_assist)
    {
        g_assist = std::make_unique<SpatialVolumeFieldAssist>();
    }
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    const bool helix = (mode == "helix");
    const char* body = helix ? kHelixBody : kArcBody;
    if(!g_shader_loaded || g_gpu_mode != mode)
    {
        g_assist->setFragmentBody(QString::fromUtf8(body));
        g_assist->setResolution(28);
        g_assist->clearMediaTexture();
        g_shader_loaded = true;
        g_gpu_mode = mode;
    }
    const float reach = helix
        ? (0.18f + clip.pulse_length * 0.30f) * size
        : (0.22f + clip.pulse_length * 0.35f) * size;
    const float height_or_speed = helix
        ? (0.35f + clip.pulse_length * 0.35f) * size
        : std::max(0.15f, clip.speed);
    float params[8] = {
        progress,
        g_ox,
        g_oy,
        g_oz,
        reach,
        g_head,
        height_or_speed,
        std::max(0.001f, g_radius)
    };
    return g_assist->prepare(render_sequence ? render_sequence : 1, progress, params, 8);
}

SampleRgb SampleCpu(float nx, float ny, float nz)
{
    SampleRgb out;
    if(!g_ready || g_pts.size() < 2)
    {
        return out;
    }
    const float x = std::clamp(nx, 0.0f, 1.0f);
    const float y = std::clamp(ny, 0.0f, 1.0f);
    const float z = std::clamp(nz, 0.0f, 1.0f);
    float d2 = 0.0f;
    float along = 0.0f;
    NearestOnRibbon(x, y, z, &d2, &along);
    const float r2 = std::max(1e-6f, g_radius * g_radius);
    float dens = std::exp(-d2 / r2);
    const float head_dist = std::fabs(along - g_head);
    const float head_win = std::min(head_dist, 1.0f - head_dist);
    dens *= 0.35f + 0.65f * std::exp(-head_win * head_win * 40.0f);
    dens = std::clamp(dens, 0.0f, 1.0f);
    if(dens <= 1e-4f)
    {
        return out;
    }
    const float h = along * 6.2831853f;
    const float cr = 0.55f + 0.45f * std::sin(h);
    const float cg = 0.35f + 0.45f * std::sin(h + 2.094f);
    const float cb = 0.65f + 0.35f * std::sin(h + 4.188f);
    out.r = std::clamp(cr * dens, 0.0f, 1.0f);
    out.g = std::clamp(cg * dens, 0.0f, 1.0f);
    out.b = std::clamp(cb * dens, 0.0f, 1.0f);
    return out;
}

} // namespace

void Reset()
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_pts.clear();
    g_assist.reset();
    g_ready = false;
    g_gpu = false;
    g_shader_loaded = false;
    g_gpu_mode.clear();
    g_last_seq = 0;
    g_last_time = -1.0f;
    g_ox = g_oy = g_oz = 0.5f;
    g_size = 1.0f;
    g_radius = 0.06f;
    g_head = 0.0f;
}

void SetSampleOrigin01(float ox, float oy, float oz, float size)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_ox = std::clamp(ox, 0.0f, 1.0f);
    g_oy = std::clamp(oy, 0.0f, 1.0f);
    g_oz = std::clamp(oz, 0.0f, 1.0f);
    g_size = std::clamp(size, 0.05f, 2.0f);
}

bool PrepareForClip(const Clip& clip, int local_ms, std::uint64_t render_sequence)
{
    const std::string effect_id = script::FileId(clip);
    const std::string mode = script::RibbonsMode(effect_id);
    if(mode.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mu);
    const float progress = ClipProgress(clip, local_ms);
    const float time_key = progress * 1000.0f + (float)local_ms * 0.001f;
    const bool same_frame = (render_sequence != 0 && render_sequence == g_last_seq && g_ready);
    const bool same_tick = (render_sequence == 0 && g_ready
                            && std::fabs(time_key - g_last_time) < 1e-3f);
    if(same_frame || same_tick)
    {
        return g_ready;
    }

    if(mode == "helix")
    {
        RebuildHelix(clip, progress);
    }
    else
    {
        RebuildArc(clip, progress);
    }
    g_gpu = g_ready && BakeGpuAtlas(clip, progress, mode, render_sequence);
    g_last_seq = render_sequence;
    g_last_time = time_key;
    return g_ready;
}

bool IsAvailable()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return g_ready && g_pts.size() >= 2;
}

bool EvaluateAtPoints(const float* xyz01, int count, float* out_rgb)
{
    if(!xyz01 || !out_rgb || count <= 0)
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    if(!g_gpu || !g_assist || !g_assist->isAvailable())
    {
        return false;
    }
    std::vector<float> clamped((size_t)count * 3u);
    for(int i = 0; i < count; ++i)
    {
        clamped[(size_t)i * 3u + 0u] = std::clamp(xyz01[(size_t)i * 3u + 0u], 0.0f, 1.0f);
        clamped[(size_t)i * 3u + 1u] = std::clamp(xyz01[(size_t)i * 3u + 1u], 0.0f, 1.0f);
        clamped[(size_t)i * 3u + 2u] = std::clamp(xyz01[(size_t)i * 3u + 2u], 0.0f, 1.0f);
    }
    return g_assist->evaluateAtPoints(clamped.data(), count, out_rgb);
}

float SampleDensity01(float nx, float ny, float nz)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if(g_gpu && g_assist && g_assist->isAvailable())
    {
        const QVector3D s = g_assist->sample01(std::clamp(nx, 0.0f, 1.0f),
                                               std::clamp(ny, 0.0f, 1.0f),
                                               std::clamp(nz, 0.0f, 1.0f));
        return std::max(s.x(), std::max(s.y(), s.z()));
    }
    const SampleRgb s = SampleCpu(nx, ny, nz);
    return std::max(s.r, std::max(s.g, s.b));
}

SampleRgb Sample01(float nx, float ny, float nz)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if(g_gpu && g_assist && g_assist->isAvailable())
    {
        SampleRgb out;
        const QVector3D s = g_assist->sample01(std::clamp(nx, 0.0f, 1.0f),
                                               std::clamp(ny, 0.0f, 1.0f),
                                               std::clamp(nz, 0.0f, 1.0f));
        out.r = s.x();
        out.g = s.y();
        out.b = s.z();
        return out;
    }
    return SampleCpu(nx, ny, nz);
}

} // namespace ribbon_field
} // namespace Action
