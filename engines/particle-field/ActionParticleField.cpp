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
float g_last_time = -1.0f;
float g_ox = 0.5f;
float g_oy = 0.5f;
float g_oz = 0.5f;
float g_size = 1.0f;
std::string g_gpu_mode;

/*
 * Multi-pass particle bodies (LED-room / cube language):
 * + fat kernels that hit sparse devices, hot heads / cool tails, clear motion
 * - thin misses, orange mush, sync blink, progress-wrap flashes, linger after once
 */
const char* kBurstBody = R"GLSL(
float hash11(float n) { return fract(sin(n) * 43758.5453123); }
void volumeMain(out vec4 out_color, vec3 p01)
{
    int n = int(u_params[6] + 0.5);
    float soft = max(0.001, u_params[5]);
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float reach = u_params[4];
    float prog = clamp(u_params[0], 0.0, 1.0);
    float speed = max(0.15, u_params[7]);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    // Pack gate + early explosiveness (most shards spawn in the first beat)
    float gate = pow(max(0.0, 1.0 - prog * 1.2), 1.7);
    gate *= 1.0 - smoothstep(0.88, 1.0, prog);
    if(gate < 1e-4) { out_color = vec4(0.0); return; }
    for(int i = 0; i < 64; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        float u = hash11(fi * 17.1 + 3.1);
        float v = hash11(fi * 29.7 + 7.3);
        float z = u * 2.0 - 1.0;
        float a = v * 6.2831853;
        float rr = sqrt(max(0.0, 1.0 - z * z));
        vec3 dir = normalize(vec3(rr * cos(a), rr * sin(a) * 0.85 + 0.25, z));
        float phase = hash11(fi * 13.0 + 1.7) * 0.18;
        float life_scale = 0.75 + 0.45 * hash11(fi * 7.3);
        float age = clamp((prog * speed * 1.15 + phase) * life_scale, 0.0, 1.0);
        if(age >= 0.995) continue;
        // Gravity arc so shards read as sparks, not expanding foam
        float dist = age * reach * (0.85 + 0.3 * hash11(fi * 5.1));
        vec3 pos = origin + dir * dist;
        pos.y -= age * age * 0.22 * reach;
        pos = clamp(pos, 0.0, 1.0);
        float life = age < 0.18 ? (age / 0.18) : pow(1.0 - (age - 0.18) / 0.82, 1.7);
        life *= gate;
        // Velocity-aligned ellipsoid (streak, not ball) + bright core
        vec3 d = p01 - pos;
        float along = dot(d, dir);
        vec3 perp = d - dir * along;
        float rx = soft * 0.85;
        float ry = soft * (2.4 + 1.6 * (1.0 - age));
        float e = dot(perp, perp) / (rx * rx) + (along * along) / (ry * ry);
        float w = life * exp(-e);
        float core = life * 1.4 * exp(-dot(d, d) / (soft * soft * 0.35));
        w += core;
        dens += w;
        vec3 hot = vec3(1.0, 0.95, 0.85);
        vec3 mid = vec3(1.0, 0.45 + 0.2 * hash11(fi + 2.0), 0.08);
        vec3 cool = vec3(0.55, 0.12, 0.02);
        col += w * mix(cool, mix(mid, hot, core / max(w, 1e-4)), life);
    }
    dens = clamp(dens * 1.25, 0.0, 1.0);
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
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float spread = u_params[4];
    float rate = max(0.25, u_params[7]);
    // Continuous fall — each drop has its own speed so the field never "resets"
    float dens = 0.0;
    vec3 col = vec3(0.0);
    float wind = 0.12 * sin(u_time * 0.35);
    for(int i = 0; i < 80; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        float ux = hash11(fi * 17.0) * 2.0 - 1.0;
        float uz = hash11(fi * 29.0) * 2.0 - 1.0;
        float phase = hash11(fi * 41.0);
        float spd = 0.7 + 0.6 * hash11(fi * 13.0);
        float len = 0.7 + 0.9 * hash11(fi * 59.0);
        float y = fract(1.0 - u_time * rate * spd + phase);
        float shear = wind * (1.0 - y) + ux * 0.08;
        vec3 tip = clamp(vec3(
            origin.x + ux * spread + shear,
            y,
            origin.z + uz * spread + wind * 0.35 * (1.0 - y)), 0.0, 1.0);
        // Fall direction for anisotropic streak
        vec3 dir = normalize(vec3(shear * 0.4, -1.0, wind * 0.15));
        float rx = soft * (1.05 + 0.35 * hash11(fi * 3.0));
        float ry = soft * (3.2 + 2.4 * len);
        vec3 d = p01 - tip;
        float along = dot(d, dir);
        // Keep the bright tip; soft wake above it
        if(along > soft * 0.4 || along < -ry * 2.2) continue;
        vec3 perp = d - dir * along;
        float e = dot(perp, perp) / (rx * rx) + (along * along) / (ry * ry);
        float tip_glow = exp(-dot(d, d) / (soft * soft * 0.55));
        float life = 0.55 + 0.45 * (1.0 - y);
        float w = life * (exp(-e) + 0.85 * tip_glow);
        // Floor splash (reads on desk/floor devices)
        float splash = (1.0 - smoothstep(0.0, 0.14, y)) * life * 0.55
            * exp(-((p01.x - tip.x) * (p01.x - tip.x) + (p01.z - tip.z) * (p01.z - tip.z))
                  / (soft * soft * 2.8));
        w += splash;
        dens += w;
        vec3 bodyc = vec3(0.35, 0.55, 0.95);
        vec3 tipc = vec3(0.85, 0.95, 1.0);
        col += w * mix(bodyc, tipc, tip_glow);
    }
    dens = clamp(dens * 1.85, 0.0, 1.0);
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
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float orbit = u_params[4];
    float t = u_time * max(0.1, u_params[7]);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    for(int i = 0; i < 48; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        float a0 = hash11(fi * 19.0) * 6.2831853;
        float b0 = hash11(fi * 37.0) * 6.2831853;
        float c0 = hash11(fi * 53.0) * 6.2831853;
        float wob = 0.55 + 0.55 * hash11(fi * 71.0);
        float rad = orbit * (0.55 + 0.55 * hash11(fi * 11.0));
        // Lissajous drift — fills the room volume, not a flat ring
        vec3 pos = clamp(vec3(
            origin.x + cos(a0 + t * 1.1 * wob) * rad + 0.18 * sin(c0 + t * 0.4),
            origin.y + 0.42 * sin(b0 + t * 0.85) * rad + 0.12 * cos(a0 * 0.7 + t * 0.55),
            origin.z + sin(a0 * 0.9 + t * 1.05 * wob) * rad + 0.15 * sin(b0 + t * 0.33)), 0.0, 1.0);
        // Asymmetric blink: long dark, short bright (firefly language)
        float blink_t = fract(t * (0.35 + 0.55 * hash11(fi * 23.0)) + a0 * 0.15);
        float blink = pow(smoothstep(0.0, 0.08, blink_t) * (1.0 - smoothstep(0.12, 0.45, blink_t)), 1.2);
        float life = 0.12 + 0.88 * blink;
        vec3 d = p01 - pos;
        float soft2 = soft * soft;
        float core = exp(-dot(d, d) / (soft2 * 0.28));
        float halo = exp(-dot(d, d) / (soft2 * 2.4));
        float w = life * (1.35 * core + 0.55 * halo);
        dens += w;
        float tint = hash11(fi * 3.0);
        vec3 gold = vec3(1.0, 0.85, 0.25);
        vec3 lime = vec3(0.55, 1.0, 0.3);
        vec3 warm = mix(gold, lime, tint);
        col += w * mix(warm * 0.45, warm, core);
    }
    dens = clamp(dens, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

const char* kMeteorBody = R"GLSL(
float hash11(float n) { return fract(sin(n) * 43758.5453123); }
void volumeMain(out vec4 out_color, vec3 p01)
{
    int n = int(u_params[6] + 0.5);
    float soft = max(0.001, u_params[5]);
    float reach = u_params[4];
    float rate = max(0.15, u_params[7]);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    for(int i = 0; i < 48; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        float sx = hash11(fi * 11.0);
        float sz = hash11(fi * 23.0);
        float phase = hash11(fi * 47.0);
        float spd = 0.65 + 0.7 * hash11(fi * 31.0);
        float age = fract(u_time * rate * spd + phase);
        // Unique dive vector per meteor
        vec3 dir = normalize(vec3(
            0.25 + 0.7 * sx,
            -0.75 - 0.2 * hash11(fi * 5.0),
            0.15 + 0.6 * sz));
        vec3 start = vec3(0.05 + sx * 0.55, 0.92 + 0.06 * hash11(fi * 9.0), 0.08 + sz * 0.55);
        vec3 head = clamp(start + dir * (age * (0.9 + 0.5 * reach)), 0.0, 1.0);
        float life = pow(1.0 - age, 1.25) * smoothstep(0.0, 0.08, age);
        // Anisotropic head+tail in dive frame
        vec3 d = p01 - head;
        float along = dot(d, dir);
        vec3 perp = d - dir * along;
        float rx = soft * 0.75;
        float ry = soft * (2.6 + 2.0 * (1.0 - age));
        float e = dot(perp, perp) / (rx * rx) + (along * along) / (ry * ry);
        float head_w = life * 1.6 * exp(-dot(d, d) / (soft * soft * 0.4));
        float tail_w = life * exp(-e);
        // Trail beads behind the head (reads as sparks on sparse grids)
        float beads = 0.0;
        for(int k = 1; k <= 3; ++k)
        {
            float fk = float(k);
            vec3 bp = clamp(head - dir * (0.05 * fk * reach), 0.0, 1.0);
            float bw = life * (0.55 / fk) * exp(-dot(p01 - bp, p01 - bp) / (soft * soft * 1.1));
            beads += bw;
        }
        float w = head_w + tail_w + beads;
        dens += w;
        vec3 white = vec3(1.0, 0.97, 0.9);
        vec3 amber = vec3(1.0, 0.5, 0.12);
        vec3 ember = vec3(0.7, 0.12, 0.02);
        vec3 mc = mix(ember, mix(amber, white, head_w / max(w, 1e-4)), life);
        col += w * mc;
    }
    dens = clamp(dens * 1.45, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

/* Slow rising embers / floating dust — ambient wow that always reads on sparse LEDs. */
const char* kEmbersBody = R"GLSL(
float hash11(float n) { return fract(sin(n) * 43758.5453123); }
void volumeMain(out vec4 out_color, vec3 p01)
{
    int n = int(u_params[6] + 0.5);
    float soft = max(0.001, u_params[5]);
    float spread = u_params[4];
    float rate = max(0.08, u_params[7]);
    float dens = 0.0;
    vec3 col = vec3(0.0);
    for(int i = 0; i < 64; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        float ux = hash11(fi * 17.0);
        float uz = hash11(fi * 29.0);
        float phase = hash11(fi * 41.0);
        float spd = 0.45 + 0.7 * hash11(fi * 13.0);
        float rise = fract(u_time * rate * spd + phase);
        float wob = 0.08 * sin(u_time * (0.7 + hash11(fi * 7.0)) + fi);
        vec3 pos = clamp(vec3(
            0.12 + ux * 0.76 + wob + (ux - 0.5) * spread * 0.15,
            rise,
            0.12 + uz * 0.76 + 0.6 * wob), 0.0, 1.0);
        float life = 0.35 + 0.65 * sin(3.14159265 * rise);
        // Rare bright pop
        float pop = pow(hash11(fi * 3.0 + floor(u_time * 0.5)), 8.0);
        life += pop * 0.9;
        vec3 d = p01 - pos;
        float soft2 = soft * soft;
        float core = exp(-dot(d, d) / (soft2 * 0.35));
        float halo = exp(-dot(d, d) / (soft2 * 2.8));
        float w = life * (1.2 * core + 0.45 * halo);
        dens += w;
        vec3 cool = vec3(0.45, 0.08, 0.01);
        vec3 hot = vec3(1.0, 0.65, 0.18);
        vec3 white = vec3(1.0, 0.9, 0.55);
        col += w * mix(cool, mix(hot, white, pop), core);
    }
    dens = clamp(dens * 1.15, 0.0, 1.0);
    if(dens < 1e-4) { out_color = vec4(0.0); return; }
    out_color = vec4(clamp(col / max(dens, 1e-4), 0.0, 1.0) * dens, dens);
}
)GLSL";

/*
 * Procedural flock: sub-swarm centers wander; agents orbit locally with
 * velocity streaks. Closed-form (no persistent sim) — sparse-LED friendly.
 */
const char* kSwarmBody = R"GLSL(
float hash11(float n) { return fract(sin(n) * 43758.5453123); }
void volumeMain(out vec4 out_color, vec3 p01)
{
    int n = int(u_params[6] + 0.5);
    float soft = max(0.001, u_params[5]);
    vec3 origin = vec3(u_params[1], u_params[2], u_params[3]);
    float orbit = u_params[4];
    float rate = max(0.12, u_params[7]);
    float t = u_time * rate;
    float dens = 0.0;
    vec3 col = vec3(0.0);
    const int flocks = 4;
    vec3 centers[4];
    for(int f = 0; f < flocks; ++f)
    {
        float ff = float(f);
        float a = t * (0.55 + 0.12 * ff) + ff * 1.7;
        float b = t * (0.42 + 0.09 * ff) + ff * 2.3;
        centers[f] = clamp(vec3(
            origin.x + cos(a) * orbit * (0.55 + 0.15 * sin(b)),
            origin.y + 0.35 * sin(b * 0.85) * orbit,
            origin.z + sin(a * 0.9 + 0.4) * orbit * (0.55 + 0.12 * cos(a))), 0.0, 1.0);
    }
    for(int i = 0; i < 72; ++i)
    {
        if(i >= n) break;
        float fi = float(i);
        int flock = int(floor(hash11(fi * 3.1) * float(flocks)));
        flock = clamp(flock, 0, flocks - 1);
        vec3 ctr = centers[flock];
        float a0 = hash11(fi * 19.0) * 6.2831853;
        float b0 = hash11(fi * 37.0) * 6.2831853;
        float rad = orbit * (0.10 + 0.22 * hash11(fi * 11.0));
        float wob = 0.65 + 0.55 * hash11(fi * 71.0);
        float local_t = t * (1.05 + 0.35 * hash11(fi * 13.0));
        vec3 pos = clamp(vec3(
            ctr.x + cos(a0 + local_t * 1.35 * wob) * rad,
            ctr.y + 0.55 * sin(b0 + local_t * 1.1) * rad,
            ctr.z + sin(a0 * 0.95 + local_t * 1.25 * wob) * rad), 0.0, 1.0);
        /* Analytic velocity for streak orientation. */
        vec3 vel = normalize(vec3(
            -sin(a0 + local_t * 1.35 * wob) * rad * 1.35 * wob,
            0.55 * cos(b0 + local_t * 1.1) * rad * 1.1,
            cos(a0 * 0.95 + local_t * 1.25 * wob) * rad * 1.25 * wob) + vec3(1e-4));
        float life = 0.55 + 0.45 * (0.5 + 0.5 * sin(local_t * 0.7 + a0));
        float accent = pow(hash11(fi * 5.0 + floor(t * 0.35)), 10.0);
        life += accent * 0.85;
        vec3 d = p01 - pos;
        float along = dot(d, vel);
        vec3 perp = d - vel * along;
        float rx = soft * 0.85;
        float ry = soft * (2.2 + 1.4 * hash11(fi * 7.0));
        float e = dot(perp, perp) / (rx * rx) + (along * along) / (ry * ry);
        float soft2 = soft * soft;
        float core = exp(-dot(d, d) / (soft2 * 0.30));
        float streak = exp(-e);
        float w = life * (1.15 * core + 0.70 * streak);
        dens += w;
        vec3 teal = vec3(0.15, 0.85, 0.95);
        vec3 violet = vec3(0.55, 0.25, 1.0);
        vec3 white = vec3(1.0, 0.97, 0.92);
        vec3 base = mix(teal, violet, hash11(fi * 2.0 + float(flock)));
        col += w * mix(base * 0.55, mix(base, white, accent), core);
    }
    dens = clamp(dens * 1.20, 0.0, 1.0);
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
    const char* body = kBurstBody;
    // Room-show defaults: fat kernels + high counts so sparse device grids still light up
    int count = std::clamp((int)std::lround(28.0f + clip.pulse_length * 56.0f), 24, 64);
    float reach_or_spread = (0.28f + clip.pulse_length * 0.55f) * size;
    float soft = (0.09f + clip.pulse_length * 0.10f) * size;
    if(mode == "rain")
    {
        body = kRainBody;
        count = std::clamp((int)std::lround(40.0f + clip.pulse_length * 55.0f), 36, 80);
        reach_or_spread = 0.55f * size;
        soft = (0.07f + clip.pulse_length * 0.08f) * size;
    }
    else if(mode == "fireflies")
    {
        body = kFirefliesBody;
        count = std::clamp((int)std::lround(18.0f + clip.pulse_length * 40.0f), 16, 48);
        reach_or_spread = (0.22f + clip.pulse_length * 0.35f) * size;
        soft = (0.08f + clip.pulse_length * 0.08f) * size;
    }
    else if(mode == "meteors")
    {
        body = kMeteorBody;
        count = std::clamp((int)std::lround(14.0f + clip.pulse_length * 34.0f), 12, 48);
        reach_or_spread = (0.35f + clip.pulse_length * 0.4f) * size;
        soft = (0.055f + clip.pulse_length * 0.07f) * size;
    }
    else if(mode == "embers")
    {
        body = kEmbersBody;
        count = std::clamp((int)std::lround(28.0f + clip.pulse_length * 40.0f), 24, 64);
        reach_or_spread = (0.4f + clip.pulse_length * 0.35f) * size;
        soft = (0.07f + clip.pulse_length * 0.08f) * size;
    }
    else if(mode == "swarm")
    {
        body = kSwarmBody;
        count = std::clamp((int)std::lround(36.0f + clip.pulse_length * 48.0f), 32, 72);
        reach_or_spread = (0.26f + clip.pulse_length * 0.38f) * size;
        soft = (0.065f + clip.pulse_length * 0.07f) * size;
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
    const std::string mode = script::ParticlesMode(effect_id);
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

} // namespace particle_field
} // namespace Action
