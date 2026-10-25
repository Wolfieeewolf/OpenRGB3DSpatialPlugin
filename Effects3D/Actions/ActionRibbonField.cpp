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
std::uint64_t g_last_seq = 0;
float g_last_time = -1.0f;
float g_ox = 0.5f;
float g_oy = 0.5f;
float g_oz = 0.5f;
float g_size = 1.0f;
std::string g_gpu_mode;

/* Arc/helix curves evaluated in the fragment shader (true GPGPU). */
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

bool PrepareGpu(const Clip& clip, float progress, const std::string& mode, std::uint64_t render_sequence)
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
    const float radius = helix
        ? std::max(0.02f, (0.03f + clip.pulse_length * 0.045f) * size)
        : std::max(0.02f, (0.035f + clip.pulse_length * 0.05f) * size);
    const float head = std::clamp(progress * std::max(0.15f, clip.speed), 0.0f, 1.0f);
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
    return g_assist->prepareProgram(render_sequence ? render_sequence : 1, progress, params, 8);
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
    g_last_seq = 0;
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
    const float progress = ClipProgress(clip, local_ms);
    const float time_key = progress * 1000.0f + (float)local_ms * 0.001f;
    const bool same_frame = (render_sequence != 0 && render_sequence == g_last_seq && g_ready);
    const bool same_tick = (render_sequence == 0 && g_ready
                            && std::fabs(time_key - g_last_time) < 1e-3f);
    if(same_frame || same_tick)
    {
        return g_ready;
    }

    g_ready = PrepareGpu(clip, progress, mode, render_sequence);
    g_last_seq = render_sequence;
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
