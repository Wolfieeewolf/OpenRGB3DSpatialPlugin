// SPDX-License-Identifier: GPL-2.0-only

#include "ActionParticleField.h"
#include "ActionScript.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
bool g_ready = false;
std::uint64_t g_last_seq = 0;
float g_last_time = -1.0f;
float g_soft = 0.08f;
float g_ox = 0.5f;
float g_oy = 0.5f;
float g_oz = 0.5f;
float g_size = 1.0f;

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
    // Uniform-ish direction from two hashes (clean-room; not a port of any demo).
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

} // namespace

void Reset()
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_particles.clear();
    g_ready = false;
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
    g_last_seq = render_sequence;
    g_last_time = time_key;
    return g_ready;
}

bool IsAvailable()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return g_ready && !g_particles.empty();
}

float SampleDensity01(float nx, float ny, float nz)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if(!g_ready || g_particles.empty())
    {
        return 0.0f;
    }
    const float x = std::clamp(nx, 0.0f, 1.0f);
    const float y = std::clamp(ny, 0.0f, 1.0f);
    const float z = std::clamp(nz, 0.0f, 1.0f);
    const float soft2 = std::max(1e-6f, g_soft * g_soft);
    float dens = 0.0f;
    for(const Particle& p : g_particles)
    {
        const float dx = x - p.x;
        const float dy = y - p.y;
        const float dz = z - p.z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        dens += p.life * std::exp(-d2 / soft2);
    }
    return std::clamp(dens, 0.0f, 1.0f);
}

SampleRgb Sample01(float nx, float ny, float nz)
{
    std::lock_guard<std::mutex> lock(g_mu);
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
    out.r = std::clamp(wr / w, 0.0f, 1.0f);
    out.g = std::clamp(wg / w, 0.0f, 1.0f);
    out.b = std::clamp(wb / w, 0.0f, 1.0f);
    // Scale by total density so sparse regions go dark.
    const float dens = std::clamp(w, 0.0f, 1.0f);
    out.r *= dens;
    out.g *= dens;
    out.b *= dens;
    return out;
}

} // namespace particle_field
} // namespace Action
