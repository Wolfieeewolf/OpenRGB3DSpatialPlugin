// SPDX-License-Identifier: GPL-2.0-only

#include "ActionRibbonField.h"
#include "ActionScript.h"
#include "SpatialVolumeFieldAssist.h"

#include <QString>

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

std::mutex g_mu;
std::unique_ptr<SpatialVolumeFieldAssist> g_assist;
bool g_ready = false;
bool g_shader_loaded = false;
float g_last_time = -1.0f;
float g_ox = 0.5f;
float g_oy = 0.5f;
float g_oz = 0.5f;
float g_size = 1.0f;
std::string g_gpu_mode;

/* Sweep: primary beam + faint dual ghost planes (laser / wipe feel). */
const char* kSweepBody = R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float reach = u_params[4];
    float head = clamp(u_params[5], 0.0, 1.0);
    float radius = max(0.001, u_params[7]);
    float prog = clamp(u_params[0], 0.0, 1.0);
    float plane = mix(origin.x - reach, origin.x + reach, head);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    for(int g = 0; g < 3; ++g)
    {
        float fg = float(g);
        float offset = (fg - 1.0) * radius * 2.8;
        float dx = p01.x - (plane + offset);
        float dz = p01.z - (origin.z + (p01.x - origin.x) * 0.08);
        float d2 = dx * dx + dz * dz * 0.35;
        float w = exp(-d2 / (radius * radius));
        w *= 0.85 + 0.15 * (1.0 - abs(p01.y - origin.y));
        w *= 0.45 + 0.55 * exp(-abs(dx) * 10.0);
        float amp = (g == 1) ? 1.0 : 0.35;
        w *= amp;
        dens += w;
        vec3 cool = vec3(0.0, 0.35, 0.55);
        vec3 hot = vec3(0.55, 1.0, 1.0);
        col += w * mix(cool, hot, clamp(w * 1.4, 0.0, 1.0));
    }
    dens *= 1.0 - smoothstep(0.88, 1.0, prog);
    dens = clamp(dens * 1.35, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

/* Sample nearest point on a polyline segment list driven by caller-supplied endpoints. */
/* Arc: three parallel neon tubes + traveling accent beads (sparse-LED friendly). */
const char* kArcBody = R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float reach = u_params[4];
    float radius = max(0.001, u_params[7]);
    float head = fract(u_time * max(0.15, u_params[6]) * 0.35);
    float speed = max(0.15, u_params[6]);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    const int tubes = 3;
    const int segs = 20;
    for(int tube = 0; tube < tubes; ++tube)
    {
        float ft = float(tube);
        float lat = (ft - 1.0) * reach * 0.16;
        float y_bias = (ft - 1.0) * reach * 0.10;
        float head_t = fract(head + ft * 0.22);
        float r_tube = radius * (1.0 - 0.12 * abs(ft - 1.0));
        float r2 = r_tube * r_tube;
        float best_d2 = 1e9;
        float along = 0.0;
        for(int i = 0; i < segs; ++i)
        {
            float u0 = float(i) / float(segs);
            float u1 = float(i + 1) / float(segs);
            float ang0 = (u0 - 0.5) * 3.14159265;
            float ang1 = (u1 - 0.5) * 3.14159265;
            vec3 a = clamp(vec3(
                origin.x + sin(ang0) * reach + lat,
                origin.y + sin(u0 * 6.2831853 + u_time * 6.2831853 * speed) * reach * 0.22 + y_bias,
                origin.z + (u0 - 0.5) * 2.0 * reach), 0.0, 1.0);
            vec3 b = clamp(vec3(
                origin.x + sin(ang1) * reach + lat,
                origin.y + sin(u1 * 6.2831853 + u_time * 6.2831853 * speed) * reach * 0.22 + y_bias,
                origin.z + (u1 - 0.5) * 2.0 * reach), 0.0, 1.0);
            vec3 ab = b - a;
            float ab2 = dot(ab, ab);
            float t = ab2 > 1e-12 ? clamp(dot(p01 - a, ab) / ab2, 0.0, 1.0) : 0.0;
            float d2 = dot(p01 - (a + ab * t), p01 - (a + ab * t));
            if(d2 < best_d2) { best_d2 = d2; along = mix(u0, u1, t); }
        }
        float tube_d = exp(-best_d2 / r2);
        float head_dist = abs(along - head_t);
        float head_win = min(head_dist, 1.0 - head_dist);
        tube_d *= 0.38 + 0.62 * exp(-head_win * head_win * 24.0);
        tube_d += 0.20 * exp(-best_d2 / (r2 * 4.0));
        /* Accent beads racing the tube. */
        for(int k = 0; k < 3; ++k)
        {
            float fk = float(k);
            float bu = fract(head_t + 0.18 * fk + ft * 0.07);
            float ang = (bu - 0.5) * 3.14159265;
            vec3 bead = clamp(vec3(
                origin.x + sin(ang) * reach + lat,
                origin.y + sin(bu * 6.2831853 + u_time * 6.2831853 * speed) * reach * 0.22 + y_bias,
                origin.z + (bu - 0.5) * 2.0 * reach), 0.0, 1.0);
            float bd = dot(p01 - bead, p01 - bead);
            tube_d += 0.85 * exp(-bd / (r2 * 0.55));
        }
        dens += tube_d;
        float h = along * 6.2831853 + ft * 1.7;
        vec3 tc = vec3(0.55 + 0.45 * sin(h), 0.30 + 0.50 * sin(h + 2.094), 0.70 + 0.30 * sin(h + 4.188));
        tc = mix(tc, vec3(1.0), exp(-head_win * head_win * 36.0) * 0.55);
        col += tube_d * tc;
    }
    dens = clamp(dens * 1.15, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

/* Dual helix tubes + accent beads. */
const char* kHelixBody = R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float reach = u_params[4];
    float height = u_params[6];
    float radius = max(0.001, u_params[7]);
    float spin = u_time * 2.15;
    float head = fract(u_time * 0.32);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    const int strands = 2;
    const int segs = 32;
    for(int strand = 0; strand < strands; ++strand)
    {
        float fs = float(strand);
        float phase = fs * 3.14159265;
        float head_t = fract(head + fs * 0.5);
        float r2 = radius * radius;
        float best_d2 = 1e9;
        float along = 0.0;
        for(int i = 0; i < segs; ++i)
        {
            float u0 = float(i) / float(segs);
            float u1 = float(i + 1) / float(segs);
            float ang0 = u0 * 6.2831853 * 2.35 + spin + phase;
            float ang1 = u1 * 6.2831853 * 2.35 + spin + phase;
            float r0 = reach * (0.92 + 0.08 * sin(u0 * 12.0 + spin));
            float r1 = reach * (0.92 + 0.08 * sin(u1 * 12.0 + spin));
            vec3 a = clamp(vec3(origin.x + cos(ang0) * r0,
                                origin.y + (u0 - 0.5) * 2.0 * height,
                                origin.z + sin(ang0) * r0), 0.0, 1.0);
            vec3 b = clamp(vec3(origin.x + cos(ang1) * r1,
                                origin.y + (u1 - 0.5) * 2.0 * height,
                                origin.z + sin(ang1) * r1), 0.0, 1.0);
            vec3 ab = b - a;
            float ab2 = dot(ab, ab);
            float t = ab2 > 1e-12 ? clamp(dot(p01 - a, ab) / ab2, 0.0, 1.0) : 0.0;
            float d2 = dot(p01 - (a + ab * t), p01 - (a + ab * t));
            if(d2 < best_d2) { best_d2 = d2; along = mix(u0, u1, t); }
        }
        float tube_d = exp(-best_d2 / r2);
        float head_dist = abs(along - head_t);
        float head_win = min(head_dist, 1.0 - head_dist);
        tube_d *= 0.35 + 0.65 * exp(-head_win * head_win * 22.0);
        tube_d += 0.22 * exp(-best_d2 / (r2 * 4.5));
        for(int k = 0; k < 4; ++k)
        {
            float fk = float(k);
            float bu = fract(head_t + 0.14 * fk);
            float ang = bu * 6.2831853 * 2.35 + spin + phase;
            float rr = reach * (0.92 + 0.08 * sin(bu * 12.0 + spin));
            vec3 bead = clamp(vec3(origin.x + cos(ang) * rr,
                                   origin.y + (bu - 0.5) * 2.0 * height,
                                   origin.z + sin(ang) * rr), 0.0, 1.0);
            float bd = dot(p01 - bead, p01 - bead);
            tube_d += 0.75 * exp(-bd / (r2 * 0.50));
        }
        dens += tube_d;
        float h = along * 6.2831853 + spin * 0.5 + phase;
        vec3 tc = vec3(0.45 + 0.55 * sin(h), 0.30 + 0.50 * sin(h + 2.094), 0.70 + 0.30 * sin(h + 4.188));
        tc = mix(tc, vec3(1.0), exp(-head_win * head_win * 40.0) * 0.55);
        col += tube_d * tc;
    }
    dens = clamp(dens * 1.15, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

/* Loose multi-cable bundle — more tubes, cooler cyan/magenta accents. */
const char* kCablesBody = R"GLSL(
void volumeMain(out vec4 out_color, vec3 p01)
{
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float reach = u_params[4];
    float radius = max(0.001, u_params[7]);
    float speed = max(0.15, u_params[6]);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    const int cables = 5;
    const int segs = 18;
    for(int c = 0; c < cables; ++c)
    {
        float fc = float(c);
        float yaw = fc * 1.256637;
        float lift = (fc - 2.0) * reach * 0.09;
        float head = fract(u_time * speed * 0.28 + fc * 0.17);
        float r_c = radius * (0.75 + 0.08 * sin(fc * 2.1));
        float r2 = r_c * r_c;
        float best_d2 = 1e9;
        float along = 0.0;
        for(int i = 0; i < segs; ++i)
        {
            float u0 = float(i) / float(segs);
            float u1 = float(i + 1) / float(segs);
            float w0 = sin(u0 * 6.2831853 * 1.5 + u_time * speed + yaw) * reach * 0.18;
            float w1 = sin(u1 * 6.2831853 * 1.5 + u_time * speed + yaw) * reach * 0.18;
            vec3 a = clamp(vec3(
                origin.x + (u0 - 0.5) * 2.0 * reach * cos(yaw) + w0 * sin(yaw),
                origin.y + lift + sin(u0 * 3.14159265) * reach * 0.35,
                origin.z + (u0 - 0.5) * 2.0 * reach * sin(yaw) - w0 * cos(yaw)), 0.0, 1.0);
            vec3 b = clamp(vec3(
                origin.x + (u1 - 0.5) * 2.0 * reach * cos(yaw) + w1 * sin(yaw),
                origin.y + lift + sin(u1 * 3.14159265) * reach * 0.35,
                origin.z + (u1 - 0.5) * 2.0 * reach * sin(yaw) - w1 * cos(yaw)), 0.0, 1.0);
            vec3 ab = b - a;
            float ab2 = dot(ab, ab);
            float t = ab2 > 1e-12 ? clamp(dot(p01 - a, ab) / ab2, 0.0, 1.0) : 0.0;
            float d2 = dot(p01 - (a + ab * t), p01 - (a + ab * t));
            if(d2 < best_d2) { best_d2 = d2; along = mix(u0, u1, t); }
        }
        float tube_d = exp(-best_d2 / r2);
        float head_dist = abs(along - head);
        float head_win = min(head_dist, 1.0 - head_dist);
        tube_d *= 0.40 + 0.60 * exp(-head_win * head_win * 20.0);
        tube_d += 0.18 * exp(-best_d2 / (r2 * 3.8));
        float bu = fract(head);
        float bw = sin(bu * 6.2831853 * 1.5 + u_time * speed + yaw) * reach * 0.18;
        vec3 bead = clamp(vec3(
            origin.x + (bu - 0.5) * 2.0 * reach * cos(yaw) + bw * sin(yaw),
            origin.y + lift + sin(bu * 3.14159265) * reach * 0.35,
            origin.z + (bu - 0.5) * 2.0 * reach * sin(yaw) - bw * cos(yaw)), 0.0, 1.0);
        tube_d += 0.95 * exp(-dot(p01 - bead, p01 - bead) / (r2 * 0.45));
        dens += tube_d;
        vec3 cool = vec3(0.15, 0.75, 1.0);
        vec3 hot = vec3(1.0, 0.25, 0.85);
        vec3 tc = mix(cool, hot, fract(fc * 0.19 + along));
        tc = mix(tc, vec3(1.0), exp(-head_win * head_win * 32.0) * 0.5);
        col += tube_d * tc;
    }
    dens = clamp(dens * 1.10, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

bool PrepareGpu(const Clip& clip,
                float progress,
                int anim_ms,
                const std::string& mode,
                std::uint64_t render_sequence)
{
    if(!g_assist)
    {
        g_assist = std::make_unique<SpatialVolumeFieldAssist>();
    }
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    const bool helix = (mode == "helix");
    const bool sweep = (mode == "sweep");
    const bool cables = (mode == "cables");
    const char* body = helix ? kHelixBody
        : (sweep ? kSweepBody : (cables ? kCablesBody : kArcBody));
    if(!g_shader_loaded || g_gpu_mode != mode)
    {
        g_assist->setFragmentBody(QString::fromUtf8(body));
        g_assist->clearMediaTexture();
        g_shader_loaded = true;
        g_gpu_mode = mode;
    }
    const float reach = helix
        ? (0.28f + clip.pulse_length * 0.35f) * size
        : sweep
            ? (0.55f + clip.pulse_length * 0.4f) * size
            : cables
                ? (0.30f + clip.pulse_length * 0.38f) * size
                : (0.32f + clip.pulse_length * 0.4f) * size;
    const float height_or_speed = helix
        ? (0.45f + clip.pulse_length * 0.4f) * size
        : std::max(0.15f, clip.speed);
    const float radius = helix
        ? std::max(0.05f, (0.06f + clip.pulse_length * 0.08f) * size)
        : sweep
            ? std::max(0.04f, (0.05f + clip.pulse_length * 0.07f) * size)
            : cables
                ? std::max(0.045f, (0.05f + clip.pulse_length * 0.07f) * size)
                : std::max(0.05f, (0.06f + clip.pulse_length * 0.08f) * size);
    // Sweep is a one-shot wipe driven by span progress; helix/arc/cables head from u_time.
    const float head = sweep ? std::clamp(progress, 0.0f, 1.0f) : 0.0f;
    float params[8] = {
        progress,
        g_ox,
        g_oy,
        g_oz,
        reach,
        head,
        height_or_speed,
        std::max(0.001f, radius)
    };
    const float time_sec = (float)std::max(0, anim_ms) * 0.001f * std::max(0.05f, clip.speed);
    return g_assist->prepareProgram(render_sequence ? render_sequence : 1, time_sec, params, 8);
}

bool EvalPointsLocked(const float* xyz01, int count, float* out_rgb)
{
    if(!g_ready || !g_assist || !g_assist->isAvailable() || !xyz01 || !out_rgb || count <= 0)
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

} // namespace

void Reset()
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_assist.reset();
    g_ready = false;
    g_shader_loaded = false;
    g_gpu_mode.clear();
    g_last_time = -1.0f;
    g_ox = g_oy = g_oz = 0.5f;
    g_size = 1.0f;
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
    const float progress = ClipSpanProgress(clip, local_ms);
    const int anim_ms = GpuAnimMsOr(local_ms);
    const float time_key = progress * 1000.0f + (float)anim_ms * 0.001f;
    // seq==0 preview only: skip when mode+clock unchanged. Never skip on a shared
    // pack-frame sequence — multi-track packs need a fresh setParams per clip.
    if(render_sequence == 0 && g_ready && g_gpu_mode == mode
       && std::fabs(time_key - g_last_time) < 1e-3f)
    {
        return g_ready;
    }

    g_ready = PrepareGpu(clip, progress, anim_ms, mode, render_sequence);
    g_last_time = time_key;
    return g_ready;
}

bool IsAvailable()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return g_ready && g_assist && g_assist->isAvailable();
}

bool EvaluateAtPoints(const float* xyz01, int count, float* out_rgb)
{
    std::lock_guard<std::mutex> lock(g_mu);
    return EvalPointsLocked(xyz01, count, out_rgb);
}

float SampleDensity01(float nx, float ny, float nz)
{
    float xyz[3] = {nx, ny, nz};
    float rgb[3] = {};
    std::lock_guard<std::mutex> lock(g_mu);
    if(!EvalPointsLocked(xyz, 1, rgb))
    {
        return 0.0f;
    }
    return std::max(rgb[0], std::max(rgb[1], rgb[2]));
}

SampleRgb Sample01(float nx, float ny, float nz)
{
    SampleRgb out;
    float xyz[3] = {nx, ny, nz};
    float rgb[3] = {};
    std::lock_guard<std::mutex> lock(g_mu);
    if(!EvalPointsLocked(xyz, 1, rgb))
    {
        return out;
    }
    out.r = rgb[0];
    out.g = rgb[1];
    out.b = rgb[2];
    return out;
}

} // namespace ribbon_field
} // namespace Action
