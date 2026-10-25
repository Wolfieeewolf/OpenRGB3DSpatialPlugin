// SPDX-License-Identifier: GPL-2.0-only

#include "ActionParticleField.h"
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
namespace particle_field
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

/* Particle positions are generated in the fragment shader (true GPGPU). */
const char* kBurstBody = R"GLSL(
float hash11(float n) { return fract(sin(n) * 43758.5453123); }
void volumeMain(out vec4 out_color, vec3 p01)
{
    int n = int(u_params[6] + 0.5);
    float soft = max(0.001, u_params[5]);
    float soft2 = soft * soft;
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float reach = u_params[4];
    float t = clamp(u_params[0], 0.0, 1.0) * max(0.15, u_params[7]);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    for(int i = 0; i < 64; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        float u = hash11(fi * 17.1 + 3.1);
        float v = hash11(fi * 29.7 + 7.3);
        float z = u * 2.0 - 1.0;
        float a = v * 6.2831853;
        float r = sqrt(max(0.0, 1.0 - z * z));
        vec3 dir = vec3(r * cos(a), r * sin(a), z);
        float phase = hash11(fi * 13.0 + 1.7);
        float age = fract(t + phase);
        vec3 pos = clamp(origin + dir * (age * reach), 0.0, 1.0);
        float life = (1.0 - age) * (1.0 - age);
        vec3 d = p01 - pos;
        float w = life * exp(-dot(d, d) / soft2);
        dens += w;
        col += w * vec3(0.65 + 0.35 * hash11(fi + 1.0),
                        0.45 + 0.40 * hash11(fi + 2.0),
                        0.75 + 0.25 * hash11(fi + 3.0));
    }
    dens = clamp(dens, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

const char* kRainBody = R"GLSL(
float hash11(float n) { return fract(sin(n) * 43758.5453123); }
void volumeMain(out vec4 out_color, vec3 p01)
{
    int n = int(u_params[6] + 0.5);
    float soft = max(0.001, u_params[5]);
    float soft2 = soft * soft;
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float spread = u_params[4];
    float fall = fract(u_params[0] * max(0.2, u_params[7]));
    float dens = 0.0;
    vec3 col = vec3(0.0);
    for(int i = 0; i < 80; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        float ux = hash11(fi * 17.0) * 2.0 - 1.0;
        float uz = hash11(fi * 29.0) * 2.0 - 1.0;
        float phase = hash11(fi * 41.0);
        float y = fract(1.0 - fall + phase);
        vec3 pos = clamp(vec3(origin.x + ux * spread, y, origin.z + uz * spread), 0.0, 1.0);
        float life = 0.55 + 0.45 * (1.0 - abs(y - 0.5) * 1.2);
        vec3 d = p01 - pos;
        float w = life * exp(-dot(d, d) / soft2);
        dens += w;
        col += w * vec3(0.55, 0.75, 0.95);
    }
    dens = clamp(dens, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

const char* kFirefliesBody = R"GLSL(
float hash11(float n) { return fract(sin(n) * 43758.5453123); }
void volumeMain(out vec4 out_color, vec3 p01)
{
    int n = int(u_params[6] + 0.5);
    float soft = max(0.001, u_params[5]);
    float soft2 = soft * soft;
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float orbit = u_params[4];
    float t = u_params[0] * 6.2831853 * max(0.1, u_params[7]);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    for(int i = 0; i < 48; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        float a0 = hash11(fi * 19.0) * 6.2831853;
        float b0 = hash11(fi * 37.0) * 6.2831853;
        float wobble = 0.65 + 0.35 * hash11(fi * 53.0);
        vec3 pos = clamp(vec3(
            origin.x + cos(a0 + t * wobble) * orbit,
            origin.y + 0.35 * sin(b0 + t * 0.7) * orbit,
            origin.z + sin(a0 + t * wobble) * orbit), 0.0, 1.0);
        float blink = 0.5 + 0.5 * sin(t * 3.0 + a0 * 4.0);
        float life = 0.35 + 0.65 * blink;
        vec3 d = p01 - pos;
        float w = life * exp(-dot(d, d) / soft2);
        dens += w;
        col += w * vec3(0.9, 0.8, 0.35);
    }
    dens = clamp(dens, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

bool PrepareGpu(const Clip& clip, float progress, const std::string& mode, std::uint64_t render_sequence)
{
    if(!g_assist)
    {
        g_assist = std::make_unique<SpatialVolumeFieldAssist>();
    }
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    const char* body = kBurstBody;
    int count = std::clamp((int)std::lround(12.0f + clip.pulse_length * 52.0f), 12, 64);
    float reach_or_spread = (0.15f + clip.pulse_length * 0.55f) * size;
    float soft = (0.045f + clip.pulse_length * 0.06f) * size;
    if(mode == "rain")
    {
        body = kRainBody;
        count = std::clamp((int)std::lround(20.0f + clip.pulse_length * 60.0f), 16, 80);
        reach_or_spread = 0.35f * size;
        soft = (0.03f + clip.pulse_length * 0.04f) * size;
    }
    else if(mode == "fireflies")
    {
        body = kFirefliesBody;
        count = std::clamp((int)std::lround(10.0f + clip.pulse_length * 36.0f), 8, 48);
        reach_or_spread = (0.12f + clip.pulse_length * 0.28f) * size;
        soft = (0.05f + clip.pulse_length * 0.05f) * size;
    }
    if(!g_shader_loaded || g_gpu_mode != mode)
    {
        g_assist->setFragmentBody(QString::fromUtf8(body));
        g_assist->clearMediaTexture();
        g_shader_loaded = true;
        g_gpu_mode = mode;
    }
    float params[8] = {
        progress,
        g_ox,
        g_oy,
        g_oz,
        reach_or_spread,
        std::max(0.001f, soft),
        (float)std::max(1, count),
        std::max(0.05f, clip.speed)
    };
    const float time_sec = progress * std::max(0.05f, clip.speed);
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
    const std::string mode = script::ParticlesMode(effect_id);
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

} // namespace particle_field
} // namespace Action
