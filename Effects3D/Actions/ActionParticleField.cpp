// SPDX-License-Identifier: GPL-2.0-only

#include "ActionParticleField.h"
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
#include <vector>

namespace Action
{
namespace particle_field
{
namespace
{

struct Particle
{
    float x = 0.5f;
    float y = 0.5f;
    float z = 0.5f;
    float life = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

std::mutex g_mu;
std::vector<Particle> g_particles;
std::unique_ptr<SpatialVolumeFieldAssist> g_assist;
bool g_ready = false;
bool g_gpu = false;
bool g_shader_loaded = false;
std::uint64_t g_last_seq = 0;
float g_last_time = -1.0f;
float g_soft = 0.08f;
float g_ox = 0.5f;
float g_oy = 0.5f;
float g_oz = 0.5f;
float g_size = 1.0f;

/* GPGPU-style: particle positions are generated in the fragment shader (no CPU splat list upload). */
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

std::string g_gpu_mode;

float Hash01(unsigned int h)
{
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFFu) / (float)0xFFFFFFu;
}

void UnitSphere(unsigned int seed, float* ox, float* oy, float* oz)
{
    const float u = Hash01(seed);
    const float v = Hash01(seed * 747796405u + 2891336453u);
    const float z = u * 2.0f - 1.0f;
    const float a = v * 6.2831853f;
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    *ox = r * std::cos(a);
    *oy = r * std::sin(a);
    *oz = z;
}

void RebuildBurst(const Clip& clip, float progress)
{
    g_particles.clear();
    const int count = std::clamp((int)std::lround(12.0f + clip.pulse_length * 52.0f), 12, 64);
    const float speed = std::max(0.15f, clip.speed);
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    const float reach = (0.15f + clip.pulse_length * 0.55f) * size;
    const float t = std::clamp(progress, 0.0f, 1.0f) * speed;
    g_soft = (0.045f + clip.pulse_length * 0.06f) * size;

    const unsigned int base = 0xA17C3u ^ (unsigned int)clip.start_ms * 2654435761u;
    g_particles.reserve((size_t)count);
    for(int i = 0; i < count; ++i)
    {
        float dx = 0.0f, dy = 0.0f, dz = 0.0f;
        UnitSphere(base + (unsigned int)i * 97u, &dx, &dy, &dz);
        const float phase = Hash01(base + (unsigned int)i * 131u);
        const float age = std::fmod(t + phase, 1.0f);
        const float rad = age * reach;
        Particle p;
        p.x = std::clamp(g_ox + dx * rad, 0.0f, 1.0f);
        p.y = std::clamp(g_oy + dy * rad, 0.0f, 1.0f);
        p.z = std::clamp(g_oz + dz * rad, 0.0f, 1.0f);
        p.life = (1.0f - age) * (1.0f - age);
        p.r = 0.65f + 0.35f * Hash01(base + (unsigned int)i * 17u);
        p.g = 0.45f + 0.40f * Hash01(base + (unsigned int)i * 29u);
        p.b = 0.75f + 0.25f * Hash01(base + (unsigned int)i * 43u);
        g_particles.push_back(p);
    }
    g_ready = !g_particles.empty();
}

void RebuildRain(const Clip& clip, float progress)
{
    g_particles.clear();
    const int count = std::clamp((int)std::lround(20.0f + clip.pulse_length * 60.0f), 16, 80);
    const float speed = std::max(0.2f, clip.speed);
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    const float spread = 0.35f * size;
    const float fall = std::fmod(progress * speed, 1.0f);
    g_soft = (0.03f + clip.pulse_length * 0.04f) * size;

    const unsigned int base = 0x51C3u ^ (unsigned int)clip.start_ms * 2654435761u;
    g_particles.reserve((size_t)count);
    for(int i = 0; i < count; ++i)
    {
        const float ux = Hash01(base + (unsigned int)i * 17u) * 2.0f - 1.0f;
        const float uz = Hash01(base + (unsigned int)i * 29u) * 2.0f - 1.0f;
        const float phase = Hash01(base + (unsigned int)i * 41u);
        const float y = std::fmod(1.0f - fall + phase, 1.0f);
        Particle p;
        p.x = std::clamp(g_ox + ux * spread, 0.0f, 1.0f);
        p.y = std::clamp(y, 0.0f, 1.0f);
        p.z = std::clamp(g_oz + uz * spread, 0.0f, 1.0f);
        p.life = 0.55f + 0.45f * (1.0f - std::fabs(y - 0.5f) * 1.2f);
        p.r = 0.55f + 0.25f * Hash01(base + (unsigned int)i * 7u);
        p.g = 0.70f + 0.20f * Hash01(base + (unsigned int)i * 11u);
        p.b = 0.95f;
        g_particles.push_back(p);
    }
    g_ready = !g_particles.empty();
}

void RebuildFireflies(const Clip& clip, float progress)
{
    g_particles.clear();
    const int count = std::clamp((int)std::lround(10.0f + clip.pulse_length * 36.0f), 8, 48);
    const float speed = std::max(0.1f, clip.speed);
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    const float orbit = (0.12f + clip.pulse_length * 0.28f) * size;
    const float t = progress * 6.2831853f * speed;
    g_soft = (0.05f + clip.pulse_length * 0.05f) * size;

    const unsigned int base = 0xF1E5u ^ (unsigned int)clip.start_ms * 2654435761u;
    g_particles.reserve((size_t)count);
    for(int i = 0; i < count; ++i)
    {
        const float a0 = Hash01(base + (unsigned int)i * 19u) * 6.2831853f;
        const float b0 = Hash01(base + (unsigned int)i * 37u) * 6.2831853f;
        const float wobble = 0.65f + 0.35f * Hash01(base + (unsigned int)i * 53u);
        Particle p;
        p.x = std::clamp(g_ox + std::cos(a0 + t * wobble) * orbit, 0.0f, 1.0f);
        p.y = std::clamp(g_oy + 0.35f * std::sin(b0 + t * 0.7f) * orbit, 0.0f, 1.0f);
        p.z = std::clamp(g_oz + std::sin(a0 + t * wobble) * orbit, 0.0f, 1.0f);
        const float blink = 0.5f + 0.5f * std::sin(t * 3.0f + a0 * 4.0f);
        p.life = 0.35f + 0.65f * blink;
        p.r = 0.85f + 0.15f * Hash01(base + (unsigned int)i * 3u);
        p.g = 0.75f + 0.20f * Hash01(base + (unsigned int)i * 5u);
        p.b = 0.25f + 0.25f * Hash01(base + (unsigned int)i * 9u);
        g_particles.push_back(p);
    }
    g_ready = !g_particles.empty();
}

bool BakeGpuAtlas(const Clip& clip, float progress, const std::string& mode, std::uint64_t render_sequence)
{
    if(!g_assist)
    {
        g_assist = std::make_unique<SpatialVolumeFieldAssist>();
    }
    const char* body = kBurstBody;
    int count = (int)g_particles.size();
    float reach_or_spread = 0.2f;
    const float size = std::clamp(g_size > 0.0f ? g_size : clip.effect_size, 0.05f, 2.0f);
    if(mode == "rain")
    {
        body = kRainBody;
        reach_or_spread = 0.35f * size;
    }
    else if(mode == "fireflies")
    {
        body = kFirefliesBody;
        reach_or_spread = (0.12f + clip.pulse_length * 0.28f) * size;
    }
    else
    {
        reach_or_spread = (0.15f + clip.pulse_length * 0.55f) * size;
    }
    if(!g_shader_loaded || g_gpu_mode != mode)
    {
        g_assist->setFragmentBody(QString::fromUtf8(body));
        g_assist->setResolution(28);
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
        std::max(0.001f, g_soft),
        (float)std::max(1, count),
        std::max(0.05f, clip.speed)
    };
    const float time_sec = progress * std::max(0.05f, clip.speed);
    return g_assist->prepare(render_sequence ? render_sequence : 1, time_sec, params, 8);
}

SampleRgb SampleCpu(float nx, float ny, float nz)
{
    SampleRgb out;
    if(!g_ready || g_particles.empty())
    {
        return out;
    }
    const float x = std::clamp(nx, 0.0f, 1.0f);
    const float y = std::clamp(ny, 0.0f, 1.0f);
    const float z = std::clamp(nz, 0.0f, 1.0f);
    const float s2 = std::max(1e-6f, g_soft * g_soft);
    float wr = 0.0f, wg = 0.0f, wb = 0.0f, w = 0.0f;
    for(const Particle& p : g_particles)
    {
        const float dx = x - p.x;
        const float dy = y - p.y;
        const float dz = z - p.z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        const float contrib = p.life * std::exp(-d2 / s2);
        wr += contrib * p.r;
        wg += contrib * p.g;
        wb += contrib * p.b;
        w += contrib;
    }
    if(w <= 1e-6f)
    {
        return out;
    }
    const float dens = std::clamp(w, 0.0f, 1.0f);
    out.r = std::clamp(wr / w, 0.0f, 1.0f) * dens;
    out.g = std::clamp(wg / w, 0.0f, 1.0f) * dens;
    out.b = std::clamp(wb / w, 0.0f, 1.0f) * dens;
    return out;
}

} // namespace

void Reset()
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_particles.clear();
    g_assist.reset();
    g_ready = false;
    g_gpu = false;
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

    if(mode == "rain")
    {
        RebuildRain(clip, progress);
    }
    else if(mode == "fireflies")
    {
        RebuildFireflies(clip, progress);
    }
    else
    {
        RebuildBurst(clip, progress);
    }
    g_gpu = g_ready && BakeGpuAtlas(clip, progress, mode, render_sequence);
    g_last_seq = render_sequence;
    g_last_time = time_key;
    return g_ready;
}

bool IsAvailable()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return g_ready && !g_particles.empty();
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

} // namespace particle_field
} // namespace Action
